// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{IncomingMessage, OutgoingMessage, OutgoingPluginMessage, PluginData, PluginMetadata};
use crate::options::PluginsOptions;
use async_channel::Sender;
use async_io::Timer;
use async_process::{Child, ChildStdin, ChildStdout, Command, Stdio};
use futures::{AsyncRead, AsyncWrite};
use gettextrs::gettext;
use std::{
    borrow::Cow,
    future::Future,
    io::{Error, ErrorKind},
    path::{Path, PathBuf},
    pin::{Pin, pin},
    task::{Context, Poll},
    time::Duration,
};

pub struct PluginInstance {
    path: PathBuf,
    metadata: PluginMetadata,
    child: Option<Child>,
    errors: Vec<Error>,
    incoming: Vec<u8>,
    incoming_size: usize,
    outgoing: Vec<u8>,
    outgoing_offset: usize,
    startup_timeout: Option<Timer>,
    sender: Sender<OutgoingPluginMessage>,
}

impl PluginInstance {
    // Same restriction as browsers: maximum incoming message size is 1 MiB.
    const INCOMING_MAX_SIZE: usize = 1024 * 1024;
    const MAX_STARTUP_SECS: u64 = 10;

    pub fn new(
        path: PathBuf,
        sender: &Sender<OutgoingPluginMessage>,
        options: &PluginsOptions,
    ) -> Self {
        let metadata = options
            .metadata
            .get()
            .remove(Self::file_name_from_path(&path).as_ref())
            .unwrap_or_default();
        Self {
            path,
            metadata,
            child: None,
            errors: Vec::new(),
            incoming: Vec::new(),
            incoming_size: 0,
            outgoing: Vec::new(),
            outgoing_offset: 0,
            startup_timeout: None,
            sender: sender.clone(),
        }
    }

    fn file_name_from_path(path: &Path) -> Cow<'_, str> {
        path.file_name().unwrap_or_default().to_string_lossy()
    }

    pub fn file_name(&self) -> Cow<'_, str> {
        Self::file_name_from_path(&self.path)
    }

    pub fn is_enabled(&self) -> bool {
        self.metadata.enabled()
    }

    pub fn is_initialized(&self) -> bool {
        self.child.is_some() && self.startup_timeout.is_none()
    }

    pub fn start(&mut self) {
        if self.child.is_some() {
            return;
        }

        match Command::new(&self.path)
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .spawn()
        {
            Ok(child) => {
                self.child = Some(child);
                self.errors.clear();
                self.startup_timeout =
                    Some(Timer::after(Duration::from_secs(Self::MAX_STARTUP_SECS)));
                self.incoming
                    .resize(size_of::<u32>() + Self::INCOMING_MAX_SIZE, 0);
                self.incoming_size = 0;
                self.metadata.set_enabled(true);
                self.save_metadata();

                self.send_message(OutgoingMessage::Apis(Vec::new()));
            }
            Err(error) => {
                self.errors.push(error);
                self.metadata.set_enabled(false);
                self.save_metadata();
            }
        }
    }

    pub fn stop(&mut self) {
        let Some(child) = self.child.as_mut() else {
            return;
        };
        if let Err(error) = child.kill() {
            self.errors.push(error);
        }

        // We drop the child even if we fail to kill the process.
        // We keep the incoming buffer just in case the process is restarted shortly.
        self.child = None;
        self.startup_timeout = None;
        self.outgoing.clear();
        self.outgoing.shrink_to(0);
        self.outgoing_offset = 0;
        self.metadata.set_enabled(false);
        self.save_metadata();
    }

    fn send_message(&mut self, msg: OutgoingMessage) {
        if self.child.is_none() {
            return;
        }

        let data = match serde_json::to_vec(&msg) {
            Ok(data) => data,
            Err(error) => {
                eprintln!("Failed serializing plugin message: {error}");
                return;
            }
        };
        let size = match u32::try_from(data.len()) {
            Ok(size) => size,
            Err(_) => {
                eprintln!("Failed serializing plugin message, message too large");
                return;
            }
        };

        self.outgoing.extend(size.to_ne_bytes());
        self.outgoing.extend(data);
    }

    pub fn data(&self) -> PluginData {
        PluginData {
            metadata: self.metadata.clone(),
            initializing: self.is_enabled() && !self.is_initialized(),
            errors: self.errors.iter().map(Error::to_string).collect(),
        }
    }

    fn save_metadata(&self) {
        let options = PluginsOptions::new();
        let mut metadata = options.metadata.get();
        metadata.insert(self.file_name().to_string(), self.metadata.clone());
        if let Err(error) = options.metadata.set(metadata) {
            eprintln!("Failed saving plugin metadata: {error}");
        }
        self.notify_updated();
    }

    fn notify_updated(&self) {
        let _ = self.sender.try_send(OutgoingPluginMessage::PluginUpdated(
            self.file_name().to_string(),
            self.data(),
        ));
    }

    fn process_incoming(&mut self, message: IncomingMessage) -> bool {
        match message {
            IncomingMessage::Info(data) => {
                self.metadata.set_name(Some(&data.name));
                self.metadata.set_version(Some(&data.version));
                self.metadata.set_copyright(data.copyright.as_deref());
                self.metadata.set_comments(data.comments.as_deref());
                self.metadata.set_authors(&data.authors);
                self.metadata.set_documenters(&data.documenters);
                self.metadata.set_translators(&data.translators);
                self.metadata.set_webpage(data.webpage.as_deref());
                self.save_metadata();
            }
            IncomingMessage::Error(error) => {
                self.errors.push(Error::other(error));
                self.notify_updated();
            }
            IncomingMessage::Failed => return false,
            IncomingMessage::Ready => {
                self.startup_timeout = None;
                self.notify_updated();
            }
        }
        true
    }

    fn poll_read(
        &mut self,
        cx: &mut Context<'_>,
        stdout: &mut ChildStdout,
        desired_size: usize,
    ) -> bool {
        if self.incoming_size >= desired_size {
            return true;
        }

        match pin!(stdout).poll_read(cx, &mut self.incoming[self.incoming_size..desired_size]) {
            Poll::Ready(Ok(size)) => {
                if size > 0 {
                    self.incoming_size += size;
                    self.incoming_size >= desired_size
                } else {
                    self.errors.push(Error::other(gettext(
                        "Child process terminated unexpectedly",
                    )));
                    self.stop();
                    false
                }
            }
            Poll::Ready(Err(error)) => {
                self.errors.push(if error.kind() == ErrorKind::BrokenPipe {
                    Error::other(gettext("Child process terminated unexpectedly"))
                } else {
                    error
                });
                self.stop();
                false
            }
            Poll::Pending => false,
        }
    }

    fn poll_incoming(
        &mut self,
        cx: &mut Context<'_>,
        stdout: &mut ChildStdout,
    ) -> Poll<Result<IncomingMessage, ()>> {
        if !self.poll_read(cx, stdout, size_of::<u32>()) {
            return Poll::Pending;
        }

        let size = self.incoming[0..size_of::<u32>()]
            .try_into()
            .ok()
            .and_then(|bytes| usize::try_from(u32::from_ne_bytes(bytes)).ok())
            .unwrap_or_default();
        if size > Self::INCOMING_MAX_SIZE {
            self.errors.push(Error::other(gettext(
                "Incoming message exceeded maximum size",
            )));
            return Poll::Ready(Err(()));
        }

        if !self.poll_read(cx, stdout, size_of::<u32>() + size) {
            return Poll::Pending;
        }

        let message = match serde_json::from_slice::<IncomingMessage>(
            &self.incoming[size_of::<u32>()..size_of::<u32>() + size],
        ) {
            Ok(message) => message,
            Err(error) => {
                self.errors.push(Error::other(error));
                return Poll::Ready(Err(()));
            }
        };

        self.incoming_size = 0;
        Poll::Ready(Ok(message))
    }

    fn send_outgoing(
        &mut self,
        cx: &mut Context<'_>,
        stdin: &mut ChildStdin,
    ) -> Poll<Result<(), ()>> {
        match pin!(stdin).poll_write(cx, &self.outgoing[self.outgoing_offset..]) {
            Poll::Ready(Ok(size)) => {
                self.outgoing_offset += size;
                if self.outgoing_offset < self.outgoing.len().saturating_sub(1) {
                    Poll::Pending
                } else {
                    self.outgoing.clear();
                    self.outgoing_offset = 0;
                    Poll::Ready(Ok(()))
                }
            }
            Poll::Ready(Err(error)) => {
                self.errors.push(if error.kind() == ErrorKind::BrokenPipe {
                    Error::other(gettext("Child process terminated unexpectedly"))
                } else {
                    error
                });
                Poll::Ready(Err(()))
            }
            Poll::Pending => Poll::Pending,
        }
    }
}

impl Future for PluginInstance {
    type Output = ();

    fn poll(mut self: Pin<&mut Self>, cx: &mut Context<'_>) -> Poll<Self::Output> {
        // Enforce startup timeout
        if let Some(timeout) = &mut self.startup_timeout
            && matches!(pin!(timeout).poll(cx), Poll::Ready(_))
        {
            self.errors.push(Error::other(gettext(
                "Plugin didn't start up within maximum time",
            )));
            self.stop();
            return Poll::Pending;
        }

        if let Some(mut child) = self.child.take() {
            let mut failed = false;

            // Try sending any outgoing messages
            if !failed
                && !self.outgoing.is_empty()
                && let Some(stdin) = child.stdin.as_mut()
            {
                match self.send_outgoing(cx, stdin) {
                    Poll::Ready(Ok(_)) | Poll::Pending => {}
                    Poll::Ready(Err(_)) => {
                        failed = true;
                    }
                }
            }

            // Process any incoming messages
            if !failed && let Some(stdout) = child.stdout.as_mut() {
                loop {
                    match self.poll_incoming(cx, stdout) {
                        Poll::Ready(Ok(message)) => {
                            failed = !self.process_incoming(message);
                            if failed {
                                break;
                            }
                        }
                        Poll::Ready(Err(_)) => {
                            failed = true;
                            break;
                        }
                        Poll::Pending => break,
                    }
                }
            }

            self.child = Some(child);
            if failed {
                self.stop();
                return Poll::Pending;
            }
        }

        Poll::Pending
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::marker::Unpin;

    /// This is similar to futures::poll!() but does not consume the future.
    async fn poll_once<T>(future: &mut (impl Future<Output = T> + Unpin)) -> Option<T> {
        let mut pin = pin!(future);
        futures::future::poll_fn(move |cx| {
            Poll::Ready(match pin.as_mut().poll(cx) {
                Poll::Ready(result) => Some(result),
                Poll::Pending => None,
            })
        })
        .await
    }

    fn setup_plugin(path: &str) -> PluginInstance {
        let options = PluginsOptions::new();
        let mut metadata = options.metadata.get();
        metadata.remove(path.rsplit_once('/').map(|(_, name)| name).unwrap_or(path));
        let _ = options.metadata.set(metadata);

        let (sender, _) = async_channel::unbounded();
        let mut instance = PluginInstance::new(path.into(), &sender, &options);
        instance.start();
        instance
    }

    #[test]
    fn test_missing_plugin_startup() {
        let instance = setup_plugin("testdata/plugin-missing");
        assert!(!instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 1);
    }

    #[async_std::test]
    async fn test_invalid_plugin_startup() {
        let mut instance = setup_plugin("testdata/plugin-invalid");
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll_once(&mut instance).await;

        assert!(!instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 1);
    }

    #[async_std::test]
    async fn test_nonjson_plugin_startup() {
        let mut instance = setup_plugin("testdata/plugin-nonjson");
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll_once(&mut instance).await;

        assert!(!instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 1);
    }

    #[async_std::test]
    async fn test_basic_plugin_startup() {
        let mut instance = setup_plugin("testdata/plugin-basic");
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll_once(&mut instance).await;

        assert!(instance.is_enabled());
        assert!(instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);
        assert_eq!(instance.metadata.name(), Some("Basic plugin".to_owned()));
        assert_eq!(instance.metadata.version(), Some("0.5".to_owned()));
        assert_eq!(instance.metadata.copyright(), None);
        assert_eq!(instance.metadata.comments(), None);
        assert_eq!(instance.metadata.authors(), Vec::<String>::new());
        assert_eq!(instance.metadata.documenters(), Vec::<String>::new());
        assert_eq!(instance.metadata.translators(), Vec::<String>::new());
        assert_eq!(instance.metadata.webpage(), None);
    }

    #[async_std::test]
    async fn test_extended_plugin_startup() {
        let mut instance = setup_plugin("testdata/plugin-extended");
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll_once(&mut instance).await;

        assert!(instance.is_enabled());
        assert!(instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);
        assert_eq!(instance.metadata.name(), Some("Extended plugin".to_owned()));
        assert_eq!(instance.metadata.version(), Some("1.1".to_owned()));
        assert_eq!(
            instance.metadata.copyright(),
            Some("Copyright 2026".to_owned())
        );
        assert_eq!(
            instance.metadata.comments(),
            Some("This is super cool".to_owned())
        );
        assert_eq!(
            instance.metadata.authors(),
            vec!["Bob".to_owned(), "Jane".to_owned()]
        );
        assert_eq!(instance.metadata.documenters(), vec!["Frank".to_owned()]);
        assert_eq!(instance.metadata.translators(), vec![String::new()]);
        assert_eq!(
            instance.metadata.webpage(),
            Some("https://example.com/".to_owned())
        );
    }

    #[async_std::test]
    async fn test_failing_plugin_startup() {
        let mut instance = setup_plugin("testdata/plugin-failing");
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll_once(&mut instance).await;

        assert!(!instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 3);
        assert_eq!(instance.metadata.name(), Some("Failing plugin".to_owned()));
        assert_eq!(instance.metadata.version(), Some("3.5".to_owned()));
        assert_eq!(instance.metadata.copyright(), None);
        assert_eq!(instance.metadata.comments(), Some("Failing reliably since 2026".to_owned()));
        assert_eq!(instance.metadata.authors(), Vec::<String>::new());
        assert_eq!(instance.metadata.documenters(), Vec::<String>::new());
        assert_eq!(instance.metadata.translators(), Vec::<String>::new());
        assert_eq!(instance.metadata.webpage(), None);
    }
}

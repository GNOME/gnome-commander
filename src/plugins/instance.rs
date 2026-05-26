// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    ApiCall, ApiRequestToPlugin, ApiResponseFromPlugin, Apis, GenericDialog, IncomingResult,
    PluginData, PluginMetadata,
    protocol::{MessageFromPlugin, MessageToPlugin},
};
use crate::{
    config::{DATADIR, PACKAGE},
    options::PluginsOptions,
};
use async_io::Timer;
use async_process::{Child, ChildStdin, ChildStdout, Command, Stdio};
use futures::{AsyncRead, AsyncWrite};
use gettextrs::gettext;
use std::{
    borrow::Cow,
    collections::{HashMap, HashSet},
    ffi::OsString,
    future::Future,
    io::{Error, ErrorKind},
    path::{Path, PathBuf},
    pin::{Pin, pin},
    task::{Context, Poll, Waker},
    time::Duration,
};

#[derive(Debug)]
pub enum PluginInstanceOutput {
    Updated,
    ApiResponse {
        id: u32,
        response: Option<ApiResponseFromPlugin>,
    },
    UpdatedAndApiResponse {
        id: u32,
        response: Option<ApiResponseFromPlugin>,
    },
}

#[derive(Debug)]
pub struct PluginInstance {
    file_name: OsString,
    path: PathBuf,
    system_dir: PathBuf,
    metadata: PluginMetadata,
    child: Option<Child>,
    errors: Vec<Error>,
    incoming: Vec<u8>,
    incoming_size: usize,
    outgoing: Vec<u8>,
    outgoing_offset: usize,
    outgoing_waker: Option<Waker>,
    startup_timeout: Option<Timer>,
    apis: HashSet<Apis>,
    pending_api_requests: HashMap<u32, (ApiCall, Timer)>,
    dialogs: HashMap<u32, GenericDialog>,
}

impl PluginInstance {
    const INCOMING_MAX_SIZE: usize = 5 * 1024 * 1024; // 5 MiB
    const MAX_STARTUP_SECS: u64 = 10;
    const MAX_RESPONSE_SECS: u64 = 5;

    pub fn new(
        path: PathBuf,
        file_name: OsString,
        system_dir: &Path,
        options: &PluginsOptions,
    ) -> Self {
        let metadata = options
            .metadata
            .get()
            .remove(file_name.to_string_lossy().as_ref())
            .unwrap_or_default();
        Self {
            file_name,
            path,
            system_dir: system_dir.to_path_buf(),
            metadata,
            child: None,
            errors: Vec::new(),
            incoming: Vec::new(),
            incoming_size: 0,
            outgoing: Vec::new(),
            outgoing_offset: 0,
            outgoing_waker: None,
            startup_timeout: None,
            apis: HashSet::new(),
            pending_api_requests: HashMap::new(),
            dialogs: HashMap::new(),
        }
    }

    pub fn file_name(&self) -> Cow<'_, str> {
        self.file_name.to_string_lossy()
    }

    pub fn name(&self) -> String {
        self.metadata
            .name
            .clone()
            .unwrap_or_else(|| self.file_name().to_string())
    }

    /// Tests whether a plugin is enabled. Other than on application startup this also implies that
    /// the plugin is running.
    pub fn is_enabled(&self) -> bool {
        self.metadata.enabled
            || (
                // Plugins in system dir are enabled by default
                self.metadata.was_empty
                    && self
                        .path
                        .ancestors()
                        .any(|ancestor| ancestor == self.system_dir)
            )
    }

    /// Tests whether the plugin signaled being ready. The result is only meaningful for enabled
    /// plugins.
    pub fn is_initialized(&self) -> bool {
        self.startup_timeout.is_none()
    }

    pub fn metadata(&self) -> &PluginMetadata {
        &self.metadata
    }

    pub fn has_pending_api_requests(&self) -> bool {
        !self.pending_api_requests.is_empty()
    }

    pub fn drain_pending_api_requests(&mut self) -> Vec<u32> {
        std::mem::take(&mut self.pending_api_requests)
            .into_keys()
            .collect()
    }

    pub fn start(&mut self) {
        if self.child.is_some() {
            return;
        }

        match Command::new(&self.path)
            .env("PYTHONPATH", &self.system_dir)
            .env("GETTEXT_DOMAIN", PACKAGE)
            .env("GETTEXT_DIR", Path::new(DATADIR).join("locale"))
            .stdin(Stdio::piped())
            .stdout(Stdio::piped())
            .spawn()
        {
            Ok(child) => {
                self.child = Some(child);
                self.errors.clear();
                self.startup_timeout =
                    Some(Timer::after(Duration::from_secs(Self::MAX_STARTUP_SECS)));
                self.incoming_size = 0;
                self.metadata.enabled = true;
                self.save_metadata();

                self.send_message(MessageToPlugin::Apis(
                    Apis::all().map(|api| api.info()).collect(),
                ));
            }
            Err(error) => {
                self.errors.push(error);
                self.metadata.enabled = false;
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
        self.child = None;
        self.startup_timeout = None;
        self.incoming.clear();
        self.incoming.shrink_to(0);
        self.incoming_size = 0;
        self.outgoing.clear();
        self.outgoing.shrink_to(0);
        self.outgoing_offset = 0;
        self.outgoing_waker = None;
        self.apis.clear();
        self.metadata.enabled = false;
        self.dialogs.retain(|_, dialog| {
            dialog.cancel();
            false
        });
        self.save_metadata();
    }

    pub fn send_message(&mut self, msg: MessageToPlugin) {
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

        if let Some(waker) = self.outgoing_waker.take() {
            waker.wake();
        }
    }

    pub fn data(&self) -> PluginData {
        PluginData {
            metadata: self.metadata.clone(),
            initializing: self.is_enabled() && !self.is_initialized(),
            errors: self.errors.iter().map(Error::to_string).collect(),
        }
    }

    fn save_metadata(&self) {
        let options = PluginsOptions::instance();
        let mut metadata = options.metadata.get();
        metadata.insert(self.file_name().to_string(), self.metadata.clone());

        #[cfg(not(test))]
        if let Err(error) = options.metadata.set(metadata) {
            eprintln!("Failed saving plugin metadata: {error}");
        }
    }

    pub fn handle_api_request(&mut self, id: u32, request: &ApiRequestToPlugin) -> bool {
        if self.apis.iter().any(|api| api.accept_request(request)) {
            self.send_message(MessageToPlugin::ApiRequest(id, request.clone()));
            if request.call().expect_response() {
                self.pending_api_requests.insert(
                    id,
                    (
                        request.call(),
                        Timer::after(Duration::from_secs(Self::MAX_RESPONSE_SECS)),
                    ),
                );
            }
            true
        } else {
            false
        }
    }

    /// Processes a message from plugin. Note that `self.child` will be unset while this method
    /// runs, so it should return `false` instead of calling `stop()` to indicate failure.
    fn process_incoming(
        &mut self,
        message: MessageFromPlugin,
        cx: &mut Context<'_>,
    ) -> Result<Option<PluginInstanceOutput>, ()> {
        const UPDATED: Result<Option<PluginInstanceOutput>, ()> =
            Ok(Some(PluginInstanceOutput::Updated));

        match message {
            MessageFromPlugin::Info(data) => {
                self.metadata.name = Some(data.name);
                self.metadata.version = Some(data.version);
                self.metadata.copyright = data.copyright;
                self.metadata.comments = data.comments;
                self.metadata.authors = data.authors;
                self.metadata.documenters = data.documenters;
                self.metadata.translators = data.translators;
                self.metadata.webpage = data.webpage;
                self.save_metadata();
                UPDATED
            }
            MessageFromPlugin::Error(error) => {
                self.errors.push(Error::other(error));
                UPDATED
            }
            MessageFromPlugin::Register(info) => {
                if self.is_initialized() {
                    self.errors.push(Error::other(gettext(
                        "API registration received after initialization",
                    )));
                    return Err(());
                }
                if let Some(api) =
                    Apis::all().find(|api| info.name == api.name() && info.version == api.version())
                {
                    if self.apis.contains(&api) {
                        self.errors.push(Error::other(
                            gettext("Multiple registrations for API {}")
                                .replace("{}", &info.to_string()),
                        ));
                        Ok(None)
                    } else {
                        self.apis.insert(api);
                        UPDATED
                    }
                } else {
                    self.errors.push(Error::other(
                        gettext("Registration for unknown API {}").replace("{}", &info.to_string()),
                    ));
                    Err(())
                }
            }
            MessageFromPlugin::Failed => Err(()),
            MessageFromPlugin::Ready => {
                self.startup_timeout = None;
                UPDATED
            }
            MessageFromPlugin::ApiRequest(id, request) => {
                for api in self.apis.iter() {
                    match api.handle_incoming(&request, self) {
                        IncomingResult::Unhandled => {}
                        IncomingResult::Handled => return Ok(None),
                        IncomingResult::HandledWithResponse(response) => {
                            self.send_message(MessageToPlugin::ApiResponse(id, response));
                            return Ok(None);
                        }
                        IncomingResult::HandledWithDialog(dialog) => {
                            self.dialogs.insert(id, dialog);
                            self.poll_dialogs(cx); // Make sure we wait for the new dialog
                            return Ok(None);
                        }
                        IncomingResult::Error(error) => {
                            self.errors.push(error);
                            return UPDATED;
                        }
                    }
                }

                self.errors.push(Error::other(
                    gettext("No API to handle request {} from plugin")
                        .replace("{}", &id.to_string()),
                ));
                Err(())
            }
            MessageFromPlugin::ApiResponse(id, response) => {
                let Some((call, _)) = self.pending_api_requests.get(&id) else {
                    self.errors.push(Error::other(
                        gettext("Response to unknown API request {}")
                            .replace("{}", &id.to_string()),
                    ));
                    return Ok(None);
                };

                if call == &response.call() {
                    self.pending_api_requests.remove(&id);
                    Ok(Some(PluginInstanceOutput::ApiResponse {
                        id,
                        response: Some(response),
                    }))
                } else {
                    self.errors.push(Error::other(
                        gettext("Response to API request {} has wrong type")
                            .replace("{}", &id.to_string()),
                    ));
                    Ok(None)
                }
            }
        }
    }

    fn poll_read(
        &mut self,
        cx: &mut Context<'_>,
        mut stdout: &mut ChildStdout,
        desired_size: usize,
    ) -> Result<bool, Error> {
        if self.incoming.len() < desired_size {
            self.incoming.resize(desired_size, 0);
        }
        while self.incoming_size < desired_size {
            match Pin::new(&mut stdout)
                .poll_read(cx, &mut self.incoming[self.incoming_size..desired_size])
            {
                Poll::Ready(Ok(size)) => {
                    if size > 0 {
                        self.incoming_size += size;
                    } else {
                        return Err(Error::other(gettext(
                            "Child process terminated unexpectedly",
                        )));
                    }
                }
                Poll::Ready(Err(error)) => {
                    return Err(if error.kind() == ErrorKind::BrokenPipe {
                        Error::other(gettext("Child process terminated unexpectedly"))
                    } else {
                        error
                    });
                }
                Poll::Pending => return Ok(false),
            }
        }
        Ok(true)
    }

    fn poll_incoming(
        &mut self,
        cx: &mut Context<'_>,
        stdout: &mut ChildStdout,
    ) -> Poll<Result<MessageFromPlugin, Error>> {
        let size = match self.poll_read(cx, stdout, size_of::<u32>()) {
            Ok(true) => self.incoming[0..size_of::<u32>()]
                .try_into()
                .ok()
                .and_then(|bytes| usize::try_from(u32::from_ne_bytes(bytes)).ok())
                .unwrap_or_default(),
            Ok(false) => return Poll::Pending,
            Err(error) => return Poll::Ready(Err(error)),
        };
        if size > Self::INCOMING_MAX_SIZE {
            return Poll::Ready(Err(Error::other(gettext(
                "Incoming message exceeded maximum size",
            ))));
        }

        let message = match self.poll_read(cx, stdout, size_of::<u32>() + size) {
            Ok(true) => match serde_json::from_slice::<MessageFromPlugin>(
                &self.incoming[size_of::<u32>()..size_of::<u32>() + size],
            ) {
                Ok(message) => message,
                Err(error) => return Poll::Ready(Err(Error::other(error))),
            },
            Ok(false) => return Poll::Pending,
            Err(error) => return Poll::Ready(Err(error)),
        };

        self.incoming_size = 0;
        Poll::Ready(Ok(message))
    }

    fn send_outgoing(
        &mut self,
        cx: &mut Context<'_>,
        stdin: &mut ChildStdin,
    ) -> Poll<Result<(), Error>> {
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
            Poll::Ready(Err(error)) => Poll::Ready(Err(if error.kind() == ErrorKind::BrokenPipe {
                Error::other(gettext("Child process terminated unexpectedly"))
            } else {
                error
            })),
            Poll::Pending => Poll::Pending,
        }
    }

    fn poll_dialogs(&mut self, cx: &mut Context<'_>) {
        let mut done = Vec::new();
        for (id, dialog) in self.dialogs.iter_mut() {
            if let Poll::Ready(response) = Pin::new(dialog).poll(cx) {
                done.push((*id, response));
            }
        }

        for (id, response) in done.into_iter() {
            self.send_message(MessageToPlugin::ApiResponse(id, response));
            self.dialogs.remove(&id);
        }
    }

    pub fn poll(&mut self, cx: &mut Context<'_>) -> Poll<PluginInstanceOutput> {
        let mut output: Option<Result<PluginInstanceOutput, ()>> = None;

        // Enforce startup timeout
        if let Some(timeout) = &mut self.startup_timeout
            && matches!(pin!(timeout).poll(cx), Poll::Ready(_))
        {
            self.errors.push(Error::other(gettext(
                "Plugin didn't start up within maximum time",
            )));
            output = Some(Err(()));
        }

        // Enforce request timeouts
        if output.is_none() {
            let mut timed_out = None;
            for (id, (_, timeout)) in self.pending_api_requests.iter_mut() {
                if matches!(pin!(timeout).poll(cx), Poll::Ready(_)) {
                    timed_out = Some(*id);
                    break;
                }
            }
            if let Some(id) = timed_out {
                self.pending_api_requests.remove(&id);
                self.errors.push(Error::other(
                    gettext("No response to API request {} received within maximum time")
                        .replace("{}", &id.to_string()),
                ));
                output = Some(Ok(PluginInstanceOutput::UpdatedAndApiResponse {
                    id,
                    response: None,
                }));
            }
        }

        // Check whether any dialogs completed
        if output.is_none() {
            self.poll_dialogs(cx);
        }

        if output.is_none()
            && let Some(mut child) = self.child.take()
        {
            // Try sending any outgoing messages
            if output.is_none()
                && let Some(stdin) = child.stdin.as_mut()
            {
                if !self.outgoing.is_empty() {
                    match self.send_outgoing(cx, stdin) {
                        Poll::Ready(Ok(_)) | Poll::Pending => {}
                        Poll::Ready(Err(error)) => {
                            self.errors.push(error);
                            output = Some(Err(()));
                        }
                    }
                } else if self.outgoing_waker.is_none() {
                    self.outgoing_waker = Some(cx.waker().clone());
                }
            }

            // Process any incoming messages
            if let Some(stdout) = child.stdout.as_mut() {
                while output.is_none() {
                    match self.poll_incoming(cx, stdout) {
                        Poll::Ready(Ok(message)) => match self.process_incoming(message, cx) {
                            Ok(Some(notification)) => {
                                output = Some(Ok(notification));
                            }
                            Ok(None) => {}
                            Err(_) => {
                                output = Some(Err(()));
                            }
                        },
                        Poll::Ready(Err(error)) => {
                            self.errors.push(error);
                            output = Some(Err(()));
                        }
                        Poll::Pending => break,
                    }
                }
            }

            self.child = Some(child);
        }

        match output {
            Some(Ok(output)) => Poll::Ready(output),
            Some(Err(_)) => {
                self.stop();
                Poll::Ready(PluginInstanceOutput::Updated)
            }
            None => Poll::Pending,
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use tempfile::TempPath;

    fn setup_plugin(contents: &str) -> (PluginInstance, TempPath) {
        use std::io::Write;
        use std::os::unix::fs::PermissionsExt;

        let permissions = std::fs::Permissions::from_mode(0o755);
        let mut file = tempfile::Builder::new()
            .prefix("plugin")
            .permissions(permissions)
            .tempfile()
            .unwrap();
        file.write_all(contents.as_bytes()).unwrap();
        let (_, path) = file.into_parts();

        let mut instance = PluginInstance::new(
            path.to_path_buf(),
            "plugin".into(),
            &PathBuf::from("."),
            &PluginsOptions::instance(),
        );
        instance.start();
        (instance, path)
    }

    async fn poll(instance: &mut PluginInstance) {
        for i in 0..2 {
            if i == 1 {
                Timer::after(Duration::from_millis(100)).await;
            }
            futures::future::poll_fn(|cx| {
                while let Poll::Ready(_) = instance.poll(cx) {}
                Poll::Ready(())
            })
            .await
        }
    }

    #[test]
    fn test_missing_plugin_startup() {
        let path = tempfile::NamedTempFile::new().unwrap().path().to_path_buf();
        let mut instance = PluginInstance::new(
            path,
            "plugin".into(),
            &PathBuf::from("."),
            &PluginsOptions::instance(),
        );
        instance.start();
        assert!(!instance.is_enabled());
        assert_eq!(instance.errors.len(), 1);
    }

    #[async_std::test]
    async fn test_invalid_plugin_startup() {
        let (mut instance, _cleanup) = setup_plugin("#!/bin/sh");
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll(&mut instance).await;

        assert!(!instance.is_enabled());
        assert_eq!(instance.errors.len(), 1);
    }

    #[async_std::test]
    async fn test_nonjson_plugin_startup() {
        let (mut instance, _cleanup) = setup_plugin(
            r#"#!/bin/sh
                /usr/bin/echo "Hello there. This isn't JSON but still worth a try."
                sleep 60
            "#,
        );
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll(&mut instance).await;

        assert!(!instance.is_enabled());
        assert_eq!(instance.errors.len(), 1);
    }

    #[async_std::test]
    async fn test_basic_plugin_startup() {
        let (mut instance, _cleanup) = setup_plugin(
            r#"#!/bin/sh
                /usr/bin/echo -ne '\x47\0\0\0{"type": "info", "payload": {"name": "Basic plugin", "version": "0.5"}}'
                /usr/bin/echo -ne '\x11\0\0\0{"type": "ready"}'
                sleep 60
            "#,
        );
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll(&mut instance).await;

        assert!(instance.is_enabled());
        assert!(instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);
        assert_eq!(instance.metadata.name, Some("Basic plugin".to_owned()));
        assert_eq!(instance.metadata.version, Some("0.5".to_owned()));
        assert_eq!(instance.metadata.copyright, None);
        assert_eq!(instance.metadata.comments, None);
        assert_eq!(instance.metadata.authors, Vec::<String>::new());
        assert_eq!(instance.metadata.documenters, Vec::<String>::new());
        assert_eq!(instance.metadata.translators, Vec::<String>::new());
        assert_eq!(instance.metadata.webpage, None);
    }

    #[async_std::test]
    async fn test_extended_plugin_startup() {
        let (mut instance, _cleanup) = setup_plugin(
            r#"#!/bin/sh
                /usr/bin/echo -ne '\xF9\0\0\0{"type": "info", "payload": {"name": "Extended plugin", "version": "1.1", "copyright": "Copyright 2026", "comments": "This is super cool", "authors": ["Bob", "Jane"], "documenters": ["Frank"], "translators": [""], "webpage": "https://example.com/"}}'
                /usr/bin/echo -ne '\x4F\0\0\0{"type": "register", "payload": {"name": "extract_metadata", "version": "1.0"}}'
                /usr/bin/echo -ne '\x22\0\0\0{"type": "ready", "payload": null}'
                sleep 60
            "#,
        );
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll(&mut instance).await;

        assert!(instance.is_enabled());
        assert!(instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);
        assert_eq!(instance.metadata.name, Some("Extended plugin".to_owned()));
        assert_eq!(instance.metadata.version, Some("1.1".to_owned()));
        assert_eq!(
            instance.metadata.copyright,
            Some("Copyright 2026".to_owned())
        );
        assert_eq!(
            instance.metadata.comments,
            Some("This is super cool".to_owned())
        );
        assert_eq!(
            instance.metadata.authors,
            vec!["Bob".to_owned(), "Jane".to_owned()]
        );
        assert_eq!(instance.metadata.documenters, vec!["Frank".to_owned()]);
        assert_eq!(instance.metadata.translators, vec![String::new()]);
        assert_eq!(
            instance.metadata.webpage,
            Some("https://example.com/".to_owned())
        );
        assert_eq!(
            instance.apis.iter().copied().collect::<Vec<_>>(),
            vec![Apis::ExtractMetadata]
        );
    }

    #[async_std::test]
    async fn test_failing_plugin_startup() {
        let (mut instance, _cleanup) = setup_plugin(
            r#"#!/bin/sh
                /usr/bin/echo -ne '\x74\0\0\0{"type": "info", "payload": {"name": "Failing plugin", "version": "3.5", "comments": "Failing reliably since 2026"}}'
                /usr/bin/echo -ne '\x33\0\0\0{"type": "error", "payload": "Something is wrong."}'
                /usr/bin/echo -ne '\x35\0\0\0{"type": "error", "payload": "Meaning really wrong."}'
                /usr/bin/echo -ne '\x3D\0\0\0{"type": "error", "payload": "Yes, no recovering from that."}'
                /usr/bin/echo -ne '\x12\0\0\0{"type": "failed"}'
                sleep 60
            "#,
        );
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll(&mut instance).await;

        assert!(!instance.is_enabled());
        assert_eq!(instance.errors.len(), 3);
        assert_eq!(instance.metadata.name, Some("Failing plugin".to_owned()));
        assert_eq!(instance.metadata.version, Some("3.5".to_owned()));
        assert_eq!(instance.metadata.copyright, None);
        assert_eq!(
            instance.metadata.comments,
            Some("Failing reliably since 2026".to_owned())
        );
        assert_eq!(instance.metadata.authors, Vec::<String>::new());
        assert_eq!(instance.metadata.documenters, Vec::<String>::new());
        assert_eq!(instance.metadata.translators, Vec::<String>::new());
        assert_eq!(instance.metadata.webpage, None);
    }

    #[async_std::test]
    async fn test_invalid_registration() {
        let (mut instance, _cleanup) = setup_plugin(
            r#"#!/bin/sh
                /usr/bin/echo -ne '\x4F\0\0\0{"type": "register", "payload": {"name": "unknown_api", "version": "1.0"}}'
                /usr/bin/echo -ne '\x22\0\0\0{"type": "ready", "payload": null}'
                sleep 60
            "#,
        );
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll(&mut instance).await;

        assert!(!instance.is_enabled());
        assert_eq!(instance.errors.len(), 1);
        assert!(instance.apis.is_empty());
    }

    #[async_std::test]
    async fn test_late_registration() {
        let (mut instance, _cleanup) = setup_plugin(
            r#"#!/bin/sh
                /usr/bin/echo -ne '\x22\0\0\0{"type": "ready", "payload": null}'
                /usr/bin/echo -ne '\x4F\0\0\0{"type": "register", "payload": {"name": "extract_metadata", "version": "1.0"}}'
                sleep 60
            "#,
        );
        assert!(instance.is_enabled());
        assert!(!instance.is_initialized());
        assert_eq!(instance.errors.len(), 0);

        poll(&mut instance).await;

        assert!(!instance.is_enabled());
        assert_eq!(instance.errors.len(), 1);
        assert!(instance.apis.is_empty());
    }
}

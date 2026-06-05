// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    connection::{
        ConnectionExt,
        remote::{ConnectionMethod, ConnectionRemote, ConnectionRemoteExt},
    },
    main_win::MainWindow,
    utils::{ErrorMessage, display_help},
};
use component_framework::{
    helpers::{Grid, GridRow},
    prelude::*,
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};

#[derive(Debug, Default)]
pub struct ConnectDialogView {
    dialog: gtk::Window,
    grid: Grid,
    type_combo: gtk::DropDown,
    alias_entry: gtk::Entry,
    uri_entry: gtk::Entry,
    server_entry: gtk::Entry,
    optional_label: gtk::Label,
    port_entry: gtk::Entry,
    folder_entry: gtk::Entry,
    domain_entry: gtk::Entry,
}

impl ConnectDialogView {
    pub fn method(&self) -> ConnectionMethod {
        ConnectionMethod::from(self.type_combo.selected())
    }

    pub fn alias(&self) -> String {
        self.alias_entry.text().to_string()
    }

    pub fn server(&self) -> Result<String, ErrorMessage> {
        let server = self.server_entry.text();
        let server = server.trim();
        if server.is_empty() {
            Err(ErrorMessage::new(
                gettext("You must enter a name for the server"),
                Some(gettext("Please enter a name and try again.")),
            ))
        } else {
            Ok(server.to_string())
        }
    }

    pub fn port(&self) -> Result<i32, ErrorMessage> {
        let port = self.port_entry.text();
        let port = port.trim();
        if port.is_empty() {
            Ok(-1)
        } else {
            port.parse().map_err(|_error| {
                ErrorMessage::brief(gettext("You must enter a number for the port"))
            })
        }
    }

    pub fn domain(&self) -> Option<String> {
        let domain = self.domain_entry.text();
        let domain = domain.trim();
        if domain.is_empty() {
            None
        } else {
            Some(domain.to_string())
        }
    }

    pub fn folder(&self) -> String {
        let folder = self.folder_entry.text();
        let folder = folder.trim_start_matches('/');
        if folder.is_empty() {
            folder.to_owned()
        } else {
            format!("/{folder}")
        }
    }

    pub fn uri(&self) -> Result<glib::Uri, ErrorMessage> {
        let uri = self.uri_entry.text();
        glib::Uri::parse(&uri, glib::UriFlags::NONE).map_err(|_| {
            ErrorMessage::new(
                gettext("“{}” is not a valid location").replace("{}", &uri),
                Some(gettext("Please check spelling and try again.")),
            )
        })
    }
}

pub enum ConnectDialogInput {
    TypeChanged,
    Help,
    Accept,
}

#[derive(Debug)]
pub struct ConnectDialog {
    temporary: bool,
    alias: Option<String>,
    uri: Option<glib::Uri>,
}

impl ConnectDialog {
    fn update_row_visibility(&self, view: &ConnectDialogView) {
        let dynamic_rows = [
            view.uri_entry.upcast_ref::<gtk::Widget>(),
            view.server_entry.upcast_ref(),
            view.optional_label.upcast_ref(),
            view.port_entry.upcast_ref(),
            view.folder_entry.upcast_ref(),
            view.domain_entry.upcast_ref(),
        ];

        let layout: &[&gtk::Widget] = match view.method() {
            ConnectionMethod::Sftp
            | ConnectionMethod::Ftp
            | ConnectionMethod::AnonFtp
            | ConnectionMethod::Dav
            | ConnectionMethod::Davs => &[
                view.server_entry.upcast_ref(),
                view.optional_label.upcast_ref(),
                view.port_entry.upcast_ref(),
                view.folder_entry.upcast_ref(),
            ],
            ConnectionMethod::Smb => &[
                view.server_entry.upcast_ref(),
                view.optional_label.upcast_ref(),
                view.folder_entry.upcast_ref(),
                view.domain_entry.upcast_ref(),
            ],
            ConnectionMethod::Uri => &[view.uri_entry.upcast_ref()],
            _ => &[],
        };

        for widget in dynamic_rows {
            let visible = layout.contains(&widget);
            let (_, row, _, _) = view.grid.query_child(widget);
            if let Some(label) = view.grid.child_at(0, row) {
                label.set_visible(visible);
            }
            if let Some(entry) = view.grid.child_at(1, row) {
                entry.set_visible(visible);
            }
        }
    }

    fn get_connection_uri(&self, view: &ConnectDialogView) -> Result<glib::Uri, ErrorMessage> {
        let uri = match view.method() {
            ConnectionMethod::Sftp => glib::Uri::build(
                glib::UriFlags::NONE,
                "sftp",
                None,
                Some(&view.server()?),
                view.port()?,
                &view.folder(),
                None,
                None,
            ),
            ConnectionMethod::Ftp | ConnectionMethod::AnonFtp => glib::Uri::build(
                glib::UriFlags::NONE,
                "ftp",
                None,
                Some(&view.server()?),
                view.port()?,
                &view.folder(),
                None,
                None,
            ),
            ConnectionMethod::Smb => glib::Uri::build(
                glib::UriFlags::NON_DNS,
                "smb",
                None,
                Some(&if let Some(domain) = view.domain() {
                    format!("{domain};{}", view.server()?)
                } else {
                    view.server()?
                }),
                -1,
                &view.folder(),
                None,
                None,
            ),
            ConnectionMethod::Dav => glib::Uri::build(
                glib::UriFlags::NONE,
                "dav",
                None,
                Some(&view.server()?),
                view.port()?,
                &view.folder(),
                None,
                None,
            ),
            ConnectionMethod::Davs => glib::Uri::build(
                glib::UriFlags::NONE,
                "davs",
                None,
                Some(&view.server()?),
                view.port()?,
                &view.folder(),
                None,
                None,
            ),
            ConnectionMethod::Uri => view.uri()?,
            _ => {
                return Err(ErrorMessage::brief("Unsupported connection method"));
            }
        };
        Ok(uri)
    }

    async fn wait_for_connection(
        mut controller: ComponentController<Self>,
    ) -> Option<ConnectionRemote> {
        controller.root().present();

        let response = controller.receive().await;
        controller.root().close();

        let (alias, uri) = response.ok()??;
        let connection = ConnectionRemote::new(&alias, &uri);
        Some(connection)
    }

    /// Dialog for setting up a new remote server connection.
    pub async fn new_connection(
        parent_window: &MainWindow,
        uri: Option<glib::Uri>,
    ) -> Option<ConnectionRemote> {
        if let Some(dialog) = parent_window.get_dialog::<gtk::Window>("connect") {
            dialog.present();
            None
        } else {
            let controller = ConnectDialog {
                temporary: true,
                alias: None,
                uri,
            }
            .build();
            controller.root().set_transient_for(Some(parent_window));
            parent_window.set_dialog("connect", controller.root().clone());

            Self::wait_for_connection(controller).await
        }
    }

    pub async fn add_connection(parent_window: &gtk::Window) -> Option<ConnectionRemote> {
        let controller = ConnectDialog {
            temporary: false,
            alias: None,
            uri: None,
        }
        .build();
        controller.root().set_transient_for(Some(parent_window));
        Self::wait_for_connection(controller).await
    }

    pub async fn edit_connection(
        parent_window: &gtk::Window,
        connection: &ConnectionRemote,
    ) -> Option<ConnectionRemote> {
        let controller = ConnectDialog {
            temporary: false,
            alias: connection.alias(),
            uri: connection.uri(),
        }
        .build();
        controller.root().set_transient_for(Some(parent_window));

        if let Some(new_connection) = Self::wait_for_connection(controller).await {
            new_connection.bookmarks_from(connection);
            Some(new_connection)
        } else {
            None
        }
    }
}

impl Component for ConnectDialog {
    type View = ConnectDialogView;
    type Input = ConnectDialogInput;
    type Output = Option<(String, glib::Uri)>;
    type Root = gtk::Window;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let view = Self::View::default();

        with!(&view.dialog {
            .add_css_class("dialog");
            .set_title(Some(&if self.temporary {
                gettext("New Connection")
            } else if self.uri.is_none() {
                gettext("Add Connection")
            } else {
                gettext("Edit Connection")
            }));
            .set_modal(!self.temporary);
            .set_resizable(false);

            &view.grid {
                GridRow {
                    .labeled_with(
                        &gettext("Service _type:"),
                        |label| label.add_css_class("important")
                    );

                    &view.type_combo {
                        .set_model(Some(&create_methods_model()));
                        .set_selected(self.uri
                            .as_ref()
                            .and_then(ConnectionMethod::from_uri)
                            .unwrap_or(ConnectionMethod::Sftp)
                            .into()
                        );
                        .set_hexpand(true);
                        .set_sensitive(self.temporary || self.uri.is_none());
                        .connect_selected_notify(forward_input!(sender, Self::Input::TypeChanged));
                    }
                }

                if !self.temporary {
                    GridRow {
                        .labeled(&gettext("_Alias:"));

                        &view.alias_entry {
                            .set_activates_default(true);
                            if let Some(alias) = &self.alias {
                                .set_text(alias);
                            }
                        }
                    }
                }

                GridRow {
                    .labeled(&gettext("_Location (URI):"));

                    &view.uri_entry {
                        .set_activates_default(true);
                        if let Some(uri) = self.uri.as_ref() {
                            .set_text(&uri.to_str());
                        }
                    }
                }

                GridRow {
                    .labeled(&gettext("_Server:"));

                    &view.server_entry {
                        .set_activates_default(true);
                        if let Some(host) = self.uri.as_ref().and_then(|uri| uri.host()) {
                            .set_text(host.split_once(';').map(|(_, host)| host).unwrap_or(&host));
                        }
                    }
                }

                GridRow {
                    .spanned(2);

                    &view.optional_label {
                        .set_label(&gettext("Optional information"));
                        .add_css_class("important");
                        .set_use_markup(true);
                        .set_halign(gtk::Align::Start);
                    }
                }

                GridRow {
                    .labeled(&gettext("_Port:"));

                    &view.port_entry {
                        .set_activates_default(true);
                        if let Some(uri) = self.uri.as_ref() && uri.port() != -1 {
                            .set_text(&uri.port().to_string());
                        }
                        .connect_insert_text(|port_entry, text, pos| {
                            let new_chars = text.chars()
                                .filter(char::is_ascii_digit)
                                .collect::<String>();
                            if new_chars.is_empty() {
                                return;
                            }

                            // We are mixing character and byte positions here. However, since we are
                            // only dealing with ASCII digits this is fine.
                            let mut text = port_entry.text().to_string();
                            text.insert_str(*pos as usize, &new_chars);
                            port_entry.set_text(&text);
                            *pos += new_chars.len() as i32;
                        });
                    }
                }

                GridRow {
                    .labeled(&gettext("_Folder:"));

                    &view.folder_entry {
                        .set_activates_default(true);
                        if let Some(uri) = self.uri.as_ref() {
                            .set_text(&uri.path());
                        }
                    }
                }

                GridRow {
                    .labeled(&gettext("_Domain name:"));

                    &view.domain_entry {
                        .set_activates_default(true);
                        if let Some(host) = self.uri.as_ref().and_then(|uri| uri.host()) {
                            .set_text(
                                host.split_once(';').map(|(domain, _)| domain).unwrap_or_default()
                            );
                        }
                    }
                }

                GridRow {
                    .spanned(2);

                    gtk::Box {
                        .add_css_class("spacing");
                        .set_orientation(gtk::Orientation::Horizontal);

                        gtk::Button {
                            .set_label(&gettext("_Help"));
                            .set_use_underline(true);
                            .connect_clicked(forward_input!(sender, Self::Input::Help));
                        }

                        gtk::Button {
                            .set_label(&gettext("_Cancel"));
                            .set_use_underline(true);
                            .set_as_cancel();
                            .set_hexpand(true);
                            .set_halign(gtk::Align::End);
                            .connect_clicked(forward_output!(sender, None));
                        }

                        gtk::Button {
                            .set_label(&if self.temporary {
                                gettext("C_onnect")
                            } else if self.uri.is_none() {
                                gettext("A_dd Connection")
                            } else {
                                gettext("_Update Connection")
                            });
                            .set_use_underline(true);
                            .set_as_default();
                            .connect_clicked(forward_input!(sender, Self::Input::Accept));
                        }
                    }
                }
            }
        });

        self.update_row_visibility(&view);

        view
    }

    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
        &view.dialog
    }

    async fn update(
        &mut self,
        msg: Self::Input,
        sender: &ComponentSender<Self>,
        view: &mut Self::View,
    ) {
        match msg {
            Self::Input::TypeChanged => {
                self.update_row_visibility(view);
            }
            Self::Input::Help => {
                let dialog = view.dialog.clone();
                glib::spawn_future_local(async move {
                    display_help(&dialog, Some("gnome-commander-config-remote-connections")).await;
                });
            }
            Self::Input::Accept => match self.get_connection_uri(view) {
                Ok(uri) => sender.output(Some((view.alias(), uri))),
                Err(error) => error.show(&view.dialog).await,
            },
        }
    }
}

fn create_methods_model() -> gio::ListModel {
    gtk::StringList::new(&[
        &gettext("SSH"),
        &gettext("FTP (with login)"),
        &gettext("Public FTP"),
        &gettext("Windows share"),
        &gettext("WebDAV (HTTP)"),
        &gettext("Secure WebDAV (HTTPS)"),
        &gettext("Custom location"),
    ])
    .upcast()
}

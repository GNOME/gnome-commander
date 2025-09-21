/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * For more details see the file COPYING.
 */

use crate::connection::{
    connection::{ConnectionExt, ConnectionInterface},
    remote::{ConnectionMethodID, ConnectionRemote, ConnectionRemoteExt},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::path::Path;

mod imp {
    use super::*;
    use crate::utils::{ErrorMessage, SenderExt, dialog_button_box, display_help};

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

    #[derive(Clone, Copy, PartialEq)]
    enum GridRow {
        TypeSelector = 0,
        Alias,
        Uri,
        Server,
        SubTitleOptionals,
        Port,
        Folder,
        Domain,
        Buttons,
    }

    const DYNAMIC_ROWS: &[GridRow] = &[
        GridRow::Uri,
        GridRow::Server,
        GridRow::SubTitleOptionals,
        GridRow::Port,
        GridRow::Folder,
        GridRow::Domain,
    ];

    pub struct ConnectDialog {
        pub grid: gtk::Grid,
        pub type_combo: gtk::DropDown,
        pub alias_entry: gtk::Entry,
        pub uri_entry: gtk::Entry,
        pub server_entry: gtk::Entry,
        pub port_entry: gtk::Entry,
        pub folder_entry: gtk::Entry,
        pub domain_entry: gtk::Entry,
        sender: async_channel::Sender<Option<glib::Uri>>,
        pub receiver: async_channel::Receiver<Option<glib::Uri>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ConnectDialog {
        const NAME: &'static str = "GnomeCmdConnectDialog";
        type Type = super::ConnectDialog;
        type ParentType = gtk::Window;

        fn new() -> Self {
            let (sender, receiver) = async_channel::bounded(1);
            Self {
                grid: gtk::Grid::builder()
                    .margin_top(12)
                    .margin_bottom(12)
                    .margin_start(12)
                    .margin_end(12)
                    .row_spacing(6)
                    .column_spacing(12)
                    .build(),
                type_combo: gtk::DropDown::builder()
                    .model(&create_methods_model())
                    .build(),
                alias_entry: gtk::Entry::builder().activates_default(true).build(),
                uri_entry: gtk::Entry::builder().activates_default(true).build(),
                server_entry: gtk::Entry::builder().activates_default(true).build(),
                port_entry: gtk::Entry::builder().activates_default(true).build(),
                folder_entry: gtk::Entry::builder().activates_default(true).build(),
                domain_entry: gtk::Entry::builder().activates_default(true).build(),
                sender,
                receiver,
            }
        }
    }

    impl ObjectImpl for ConnectDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();

            obj.set_title(Some(&gettext("Remote Server")));
            obj.set_resizable(false);

            obj.set_child(Some(&self.grid));

            let label = gtk::Label::builder()
                .label(format!("<b>{}</b>", gettext("Service _type:")))
                .use_underline(true)
                .use_markup(true)
                .mnemonic_widget(&self.type_combo)
                .halign(gtk::Align::Start)
                .valign(gtk::Align::Center)
                .build();

            self.grid
                .attach(&label, 0, GridRow::TypeSelector as i32, 1, 1);
            self.grid
                .attach(&self.type_combo, 1, GridRow::TypeSelector as i32, 1, 1);

            self.type_combo.connect_selected_notify(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    let method = imp.get_method();
                    imp.setup_for_type(method);
                }
            ));

            self.attach_entry(&gettext("_Alias:"), &self.alias_entry, GridRow::Alias);
            self.attach_entry(&gettext("_Location (URI):"), &self.uri_entry, GridRow::Uri);
            self.attach_entry(&gettext("_Server:"), &self.server_entry, GridRow::Server);

            let subtitle = gtk::Label::builder()
                .label(format!("<b>{}</b>", gettext("Optional information")))
                .use_markup(true)
                .halign(gtk::Align::Start)
                .valign(gtk::Align::Center)
                .build();
            self.grid
                .attach(&subtitle, 0, GridRow::SubTitleOptionals as i32, 2, 1);

            self.attach_entry(&gettext("_Port:"), &self.port_entry, GridRow::Port);
            // g_signal_connect (dialog.imp().port_entry, "insert-text", G_CALLBACK (port_insert_text), NULL);
            self.attach_entry(&gettext("_Folder:"), &self.folder_entry, GridRow::Folder);
            self.attach_entry(
                &gettext("_Domain name:"),
                &self.domain_entry,
                GridRow::Domain,
            );

            let help_btn = gtk::Button::builder()
                .label(gettext("_Help"))
                .use_underline(true)
                .build();
            help_btn.connect_clicked(glib::clone!(
                #[weak]
                obj,
                move |_| {
                    glib::spawn_future_local(async move {
                        display_help(
                            obj.upcast_ref(),
                            Some("gnome-commander-config-remote-connections"),
                        )
                        .await;
                    });
                }
            ));

            let cancel_btn = gtk::Button::builder()
                .label(gettext("_Cancel"))
                .use_underline(true)
                .build();
            cancel_btn.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.sender.toss(None)
            ));

            let ok_btn = gtk::Button::builder()
                .label(gettext("_OK"))
                .use_underline(true)
                .build();
            ok_btn.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.ok_clicked().await });
                }
            ));

            self.grid.attach(
                &dialog_button_box(&[&help_btn], &[&cancel_btn, &ok_btn]),
                0,
                GridRow::Buttons as i32,
                2,
                1,
            );

            obj.set_default_widget(Some(&ok_btn));
        }
    }

    impl WidgetImpl for ConnectDialog {}
    impl WindowImpl for ConnectDialog {}

    impl ConnectDialog {
        fn attach_entry(&self, label: &str, entry: &gtk::Entry, row: GridRow) {
            let label = label_for(label, entry);
            self.grid.attach(&label, 0, row as i32, 1, 1);
            self.grid.attach(entry, 1, row as i32, 1, 1);
        }

        pub fn setup_for_type(&self, method: Option<ConnectionMethodID>) {
            let layout: &[GridRow] = match method {
                Some(ConnectionMethodID::CON_SFTP)
                | Some(ConnectionMethodID::CON_FTP)
                | Some(ConnectionMethodID::CON_ANON_FTP)
                | Some(ConnectionMethodID::CON_DAV)
                | Some(ConnectionMethodID::CON_DAVS) => &[
                    GridRow::Server,
                    GridRow::SubTitleOptionals,
                    GridRow::Port,
                    GridRow::Folder,
                ],
                Some(ConnectionMethodID::CON_SMB) => &[
                    GridRow::Server,
                    GridRow::SubTitleOptionals,
                    GridRow::Folder,
                    GridRow::Domain,
                ],
                Some(ConnectionMethodID::CON_URI) => &[GridRow::Uri],
                _ => &[],
            };

            for row in DYNAMIC_ROWS {
                let visible = layout.contains(row);
                if let Some(label) = self.grid.child_at(0, *row as i32) {
                    label.set_visible(visible);
                }
                if let Some(entry) = self.grid.child_at(1, *row as i32) {
                    entry.set_visible(visible);
                }
            }
        }

        pub fn get_method(&self) -> Option<ConnectionMethodID> {
            let method = self.type_combo.selected();
            ConnectionMethodID::from_repr(method)
        }

        pub fn set_method(&self, method: Option<ConnectionMethodID>) {
            self.type_combo
                .set_selected(method.unwrap_or(ConnectionMethodID::CON_URI) as u32);
            self.setup_for_type(method);
        }

        pub fn get_server(&self) -> Result<String, ErrorMessage> {
            let server = self.server_entry.text();
            let server = server.trim();
            if server.is_empty() {
                Err(ErrorMessage {
                    message: gettext("You must enter a name for the server"),
                    secondary_text: Some(gettext("Please enter a name and try again.")),
                })
            } else {
                Ok(server.to_string())
            }
        }

        pub fn get_port(&self) -> Result<i32, ErrorMessage> {
            let port = self.port_entry.text();
            let port = port.trim();
            if port.is_empty() {
                Ok(-1)
            } else {
                port.parse::<i32>().map_err(|_error| ErrorMessage {
                    message: gettext("You must enter a number for the port"),
                    secondary_text: None,
                })
            }
        }

        pub fn get_domain(&self) -> Option<String> {
            let domain = self.domain_entry.text();
            let domain = domain.trim();
            if domain.is_empty() {
                None
            } else {
                Some(domain.to_string())
            }
        }

        pub fn get_folder(&self) -> String {
            let folder = self.folder_entry.text();
            let folder = folder.trim_start_matches('/');
            if folder.is_empty() {
                folder.to_owned()
            } else {
                format!("/{folder}")
            }
        }

        pub fn get_connection_uri(&self) -> Result<glib::Uri, ErrorMessage> {
            let method = self.get_method().ok_or_else(|| ErrorMessage {
                message: gettext("Connection method is not selected"),
                secondary_text: None,
            })?;
            let uri = match method {
                ConnectionMethodID::CON_SFTP => glib::Uri::build(
                    glib::UriFlags::NONE,
                    "sftp",
                    None,
                    Some(&self.get_server()?),
                    self.get_port()?,
                    &self.get_folder(),
                    None,
                    None,
                ),
                ConnectionMethodID::CON_FTP | ConnectionMethodID::CON_ANON_FTP => glib::Uri::build(
                    glib::UriFlags::NONE,
                    "ftp",
                    None,
                    Some(&self.get_server()?),
                    self.get_port()?,
                    &self.get_folder(),
                    None,
                    None,
                ),
                ConnectionMethodID::CON_SMB => {
                    let mut host = self.get_server()?;
                    if let Some(domain) = self.get_domain() {
                        host = format!("{domain};{host}");
                    }
                    glib::Uri::build(
                        glib::UriFlags::NON_DNS,
                        "smb",
                        None,
                        Some(&host),
                        -1,
                        &self.get_folder(),
                        None,
                        None,
                    )
                }
                ConnectionMethodID::CON_DAV => glib::Uri::build(
                    glib::UriFlags::NONE,
                    "dav",
                    None,
                    Some(&self.get_server()?),
                    self.get_port()?,
                    &self.get_folder(),
                    None,
                    None,
                ),
                ConnectionMethodID::CON_DAVS => glib::Uri::build(
                    glib::UriFlags::NONE,
                    "davs",
                    None,
                    Some(&self.get_server()?),
                    self.get_port()?,
                    &self.get_folder(),
                    None,
                    None,
                ),
                ConnectionMethodID::CON_URI => {
                    let uri = self.uri_entry.text();
                    glib::Uri::parse(&uri, glib::UriFlags::NONE).map_err(|_| ErrorMessage {
                        message: gettext("“{}” is not a valid location").replace("{}", &uri),
                        secondary_text: Some(gettext("Please check spelling and try again.")),
                    })?
                }
                _ => {
                    return Err(ErrorMessage {
                        message: "Unsupported connection method".to_string(),
                        secondary_text: None,
                    });
                }
            };
            Ok(uri)
        }

        async fn ok_clicked(&self) {
            match self.get_connection_uri() {
                Ok(data) => self.sender.toss(Some(data)),
                Err(error) => error.show(self.obj().upcast_ref()).await,
            }
        }
    }

    fn label_for(text: &str, widget: &impl IsA<gtk::Widget>) -> gtk::Label {
        gtk::Label::builder()
            .label(text)
            .use_underline(true)
            .halign(gtk::Align::Start)
            .valign(gtk::Align::Center)
            .mnemonic_widget(widget)
            .build()
    }
}

glib::wrapper! {
    pub struct ConnectDialog(ObjectSubclass<imp::ConnectDialog>)
        @extends gtk::Window, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native;
}

/*
fn port_insert_text(editable: &GtkEditable, new_text: &str, new_text_length: i32, position: &i32) {
    if new_text_length < 0 {
        new_text_length = strlen(new_text);
    }

    if (new_text_length != 1 || g_ascii_isdigit(new_text[0])) {
        return;
    }

    gdk_display_beep(gtk_widget_get_display(GTK_WIDGET(editable)));
    g_signal_stop_emission_by_name(editable, "insert-text");
}
*/

impl Default for ConnectDialog {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ConnectDialog {
    /// Dialog for setting up a new remote server connection.
    pub async fn new_connection(
        parent_window: &gtk::Window,
        with_alias: bool,
        uri: Option<glib::Uri>,
    ) -> Option<ConnectionRemote> {
        let dialog = ConnectDialog::default();
        dialog.set_transient_for(Some(parent_window));
        dialog.imp().alias_entry.set_sensitive(with_alias);

        if let Some(uri) = uri {
            dialog.set_uri(&uri);
        } else {
            dialog.imp().set_method(Some(ConnectionMethodID::CON_SFTP));
        }

        dialog.present();
        let response = dialog.imp().receiver.recv().await;
        let connection: Option<ConnectionRemote> = match response {
            Ok(Some(uri)) => {
                let alias = dialog.imp().alias_entry.text();
                let con = ConnectionRemote::new(&alias, &uri);
                Some(con)
            }
            _ => None,
        };
        dialog.close();
        connection
    }

    pub async fn edit_connection(parent_window: &gtk::Window, con: &ConnectionRemote) -> bool {
        let dialog: ConnectDialog = glib::Object::builder().build();
        dialog.set_transient_for(Some(parent_window));

        dialog.imp().type_combo.set_sensitive(false);

        if let Some(alias) = con.alias() {
            dialog.imp().alias_entry.set_text(&alias);
        } else {
            dialog.imp().alias_entry.set_sensitive(false);
        }

        if let Some(uri) = con.uri() {
            dialog.set_uri(&uri);
        }

        dialog.present();
        let mut result = false;

        let response = dialog.imp().receiver.recv().await;
        if let Ok(Some(uri)) = response {
            let alias = dialog.imp().alias_entry.text();
            con.set_alias(Some(&alias));
            con.set_uri(Some(&uri));

            let path = uri.path();
            con.set_base_path(Some(con.create_path(if path.is_empty() {
                Path::new("/")
            } else {
                Path::new(&path)
            })));

            result = true;
        }
        dialog.close();
        result
    }

    fn set_uri(&self, uri: &glib::Uri) {
        self.imp().set_method(ConnectionMethodID::from_uri(uri));

        self.imp().uri_entry.set_text(&uri.to_str());

        let full_host = uri.host().unwrap_or_default();
        let (domain, host) = full_host
            .split_once(';')
            .unwrap_or_else(|| ("", &full_host));

        self.imp().server_entry.set_text(host);

        let port = uri.port();
        if port != -1 {
            self.imp().port_entry.set_text(&port.to_string());
        }

        self.imp().folder_entry.set_text(&uri.path());
        self.imp().domain_entry.set_text(domain);
    }
}

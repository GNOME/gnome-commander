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

use crate::{
    connection::{
        connection::{Connection, ConnectionExt, ConnectionMethodID},
        remote::ConnectionRemote,
        smb::ConnectionSmb,
    },
    main_win::{ffi::GnomeCmdMainWin, MainWindow},
    types::FileSelectorID,
    utils::{display_help, show_error_message},
};
use gettextrs::gettext;
use gtk::{glib, glib::translate::from_glib_none, prelude::*, subclass::prelude::*};
use std::path::Path;

mod imp {
    use super::*;
    use crate::utils::{show_error_message, ErrorMessage, Gtk3to4BoxCompat};
    use gtk::prelude::{DialogExt, GtkWindowExt};

    fn create_methods_model() -> gtk::ListStore {
        let store = gtk::ListStore::new(&[String::static_type(), i32::static_type()]);
        store.set(
            &store.append(),
            &[
                (0, &gettext("SSH")),
                (1, &(ConnectionMethodID::CON_SFTP as i32)),
            ],
        );
        store.set(
            &store.append(),
            &[
                (0, &gettext("FTP (with login)")),
                (1, &(ConnectionMethodID::CON_FTP as i32)),
            ],
        );
        store.set(
            &store.append(),
            &[
                (0, &gettext("Public FTP")),
                (1, &(ConnectionMethodID::CON_ANON_FTP as i32)),
            ],
        );
        store.set(
            &store.append(),
            &[
                (0, &gettext("Windows share")),
                (1, &(ConnectionMethodID::CON_SMB as i32)),
            ],
        );
        store.set(
            &store.append(),
            &[
                (0, &gettext("WebDAV (HTTP)")),
                (1, &(ConnectionMethodID::CON_DAV as i32)),
            ],
        );
        store.set(
            &store.append(),
            &[
                (0, &gettext("Secure WebDAV (HTTPS)")),
                (1, &(ConnectionMethodID::CON_DAVS as i32)),
            ],
        );
        store.set(
            &store.append(),
            &[
                (0, &gettext("Custom location")),
                (1, &(ConnectionMethodID::CON_URI as i32)),
            ],
        );
        store
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
        pub type_combo: gtk::ComboBox,
        pub alias_entry: gtk::Entry,
        pub uri_entry: gtk::Entry,
        pub server_entry: gtk::Entry,
        pub port_entry: gtk::Entry,
        pub folder_entry: gtk::Entry,
        pub domain_entry: gtk::Entry,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ConnectDialog {
        const NAME: &'static str = "GnomeCmdConnectDialog";
        type Type = super::ConnectDialog;
        type ParentType = gtk::Dialog;

        fn new() -> Self {
            Self {
                grid: gtk::Grid::builder()
                    .row_spacing(6)
                    .column_spacing(12)
                    .build(),
                type_combo: gtk::ComboBox::with_model(&create_methods_model()),
                alias_entry: gtk::Entry::builder().activates_default(true).build(),
                uri_entry: gtk::Entry::builder().activates_default(true).build(),
                server_entry: gtk::Entry::builder().activates_default(true).build(),
                port_entry: gtk::Entry::builder().activates_default(true).build(),
                folder_entry: gtk::Entry::builder().activates_default(true).build(),
                domain_entry: gtk::Entry::builder().activates_default(true).build(),
            }
        }
    }

    impl ObjectImpl for ConnectDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();

            obj.set_title(&gettext("Remote Server"));
            obj.set_resizable(false);

            let content_area = obj.content_area();
            content_area.set_margin_top(12);
            content_area.set_margin_bottom(12);
            content_area.set_margin_start(12);
            content_area.set_margin_end(12);
            content_area.set_spacing(6);
            content_area.append(&self.grid);

            let label = gtk::Label::builder()
                .label(format!("<b>{}</b>", gettext("Service _type:")))
                .use_underline(true)
                .use_markup(true)
                .mnemonic_widget(&self.type_combo)
                .halign(gtk::Align::Start)
                .valign(gtk::Align::Center)
                .build();

            let renderer = gtk::CellRendererText::new();
            self.type_combo.pack_start(&renderer, true);
            self.type_combo.add_attribute(&renderer, "text", 0);

            self.grid
                .attach(&label, 0, GridRow::TypeSelector as i32, 1, 1);
            self.grid
                .attach(&self.type_combo, 1, GridRow::TypeSelector as i32, 1, 1);

            self.type_combo
                .connect_changed(glib::clone!(@weak self as imp => move |_| {
                    let method = imp.get_method();
                    imp.setup_for_type(method);
                }));

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

            let buttonbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(6)
                .margin_top(6)
                .hexpand(true)
                .build();
            let buttonbox_size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Horizontal);
            self.grid
                .attach(&buttonbox, 0, GridRow::Buttons as i32, 2, 1);

            let help_btn = gtk::Button::builder()
                .label(gettext("_Help"))
                .use_underline(true)
                .hexpand(true)
                .halign(gtk::Align::Start)
                .build();
            help_btn.connect_clicked(glib::clone!(@weak obj => move |_| {
                display_help(
                    obj.upcast_ref(),
                    Some("gnome-commander-config-remote-connections")
                );
            }));
            buttonbox.append(&help_btn);
            buttonbox_size_group.add_widget(&help_btn);

            let cancel_btn = gtk::Button::builder()
                .label(gettext("_Cancel"))
                .use_underline(true)
                .build();
            cancel_btn.connect_clicked(
                glib::clone!(@weak obj => move |_| obj.response(gtk::ResponseType::Cancel)),
            );
            buttonbox.append(&cancel_btn);
            buttonbox_size_group.add_widget(&cancel_btn);

            let ok_btn = gtk::Button::builder()
                .label(gettext("_OK"))
                .use_underline(true)
                .can_default(true)
                .build();
            ok_btn.connect_clicked(glib::clone!(@weak obj => move |_| {
                match obj.imp().get_connection_uri() {
                    Ok(_) => obj.response(gtk::ResponseType::Ok),
                    Err(error) => show_error_message(obj.upcast_ref(), &error),
                }
            }));
            buttonbox.append(&ok_btn);
            buttonbox_size_group.add_widget(&ok_btn);

            obj.set_default_response(gtk::ResponseType::Ok);

            //  g_signal_connect (dialog, "response", G_CALLBACK (response_callback), dialog);
        }
    }

    impl WidgetImpl for ConnectDialog {}
    impl ContainerImpl for ConnectDialog {}
    impl BinImpl for ConnectDialog {}
    impl WindowImpl for ConnectDialog {}
    impl DialogImpl for ConnectDialog {}

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
            let iter = self.type_combo.active_iter()?;
            let method: i32 = self.type_combo.model()?.value(&iter, 1).get().ok()?;
            ConnectionMethodID::from_repr(method)
        }

        pub fn set_method(&self, method: ConnectionMethodID) {
            self.type_combo.set_active(Some(method as u32));
            self.setup_for_type(Some(method));
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

        pub fn get_connection_uri(&self) -> Result<glib::Uri, ErrorMessage> {
            let method = self.get_method().ok_or_else(|| ErrorMessage {
                message: gettext("Connection method is not selected"),
                secondary_text: None,
            })?;
            let folder = self.folder_entry.text();
            let folder = folder.trim_start_matches('/');
            match method {
                ConnectionMethodID::CON_SFTP => Ok(glib::Uri::build(
                    glib::UriFlags::NONE,
                    "sftp://",
                    None,
                    Some(&self.get_server()?),
                    self.get_port()?,
                    folder,
                    None,
                    None,
                )),
                ConnectionMethodID::CON_FTP | ConnectionMethodID::CON_ANON_FTP => {
                    Ok(glib::Uri::build(
                        glib::UriFlags::NONE,
                        "ftp://",
                        None,
                        Some(&self.get_server()?),
                        self.get_port()?,
                        folder,
                        None,
                        None,
                    ))
                }
                ConnectionMethodID::CON_SMB => {
                    let mut host = self.get_server()?;
                    if let Some(domain) = self.get_domain() {
                        host = format!("{domain};{host}");
                    }
                    Ok(glib::Uri::build(
                        glib::UriFlags::NON_DNS,
                        "smb://",
                        None,
                        Some(&host),
                        -1,
                        folder,
                        None,
                        None,
                    ))
                }
                ConnectionMethodID::CON_DAV => Ok(glib::Uri::build(
                    glib::UriFlags::NONE,
                    "dav://",
                    None,
                    Some(&self.get_server()?),
                    self.get_port()?,
                    folder,
                    None,
                    None,
                )),
                ConnectionMethodID::CON_DAVS => Ok(glib::Uri::build(
                    glib::UriFlags::NONE,
                    "davs://",
                    None,
                    Some(&self.get_server()?),
                    self.get_port()?,
                    folder,
                    None,
                    None,
                )),
                ConnectionMethodID::CON_URI => {
                    let uri = self.uri_entry.text();
                    glib::Uri::parse(&uri, glib::UriFlags::NONE).map_err(|_| ErrorMessage {
                        message: gettext!("“{}” is not a valid location", uri),
                        secondary_text: Some(gettext("Please check spelling and try again.")),
                    })
                }
                _ => Err(ErrorMessage {
                    message: "Unsupported connection method".to_string(),
                    secondary_text: None,
                }),
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
        @extends gtk::Dialog, gtk::Window, gtk::Widget;
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
    ) -> Option<ConnectionRemote> {
        let dialog = ConnectDialog::default();
        dialog.set_transient_for(Some(parent_window));
        dialog.show_all();
        dialog.imp().alias_entry.set_sensitive(with_alias);

        dialog.imp().set_method(ConnectionMethodID::CON_SFTP);

        dialog.present();
        let mut connection: Option<ConnectionRemote> = None;
        loop {
            let response = dialog.run_future().await;
            match response {
                gtk::ResponseType::Ok => {
                    let Some(method) = dialog.imp().get_method() else {
                        continue;
                    };
                    match (method, dialog.imp().get_connection_uri()) {
                        (ConnectionMethodID::CON_SMB, Ok(uri)) => {
                            let alias = dialog.imp().alias_entry.text();
                            let con = ConnectionSmb::default();
                            con.upcast_ref::<Connection>().set_alias(Some(&alias));
                            con.upcast_ref::<Connection>().set_uri(Some(&uri));
                            connection = Some(con.upcast::<ConnectionRemote>());
                        }
                        (_, Ok(uri)) => {
                            let alias = dialog.imp().alias_entry.text();
                            let con = ConnectionRemote::default();
                            con.upcast_ref::<Connection>().set_alias(Some(&alias));
                            con.upcast_ref::<Connection>().set_uri(Some(&uri));
                            connection = Some(con);
                        }
                        (_, Err(error)) => show_error_message(dialog.upcast_ref(), &error),
                    }
                }
                gtk::ResponseType::Cancel | gtk::ResponseType::DeleteEvent => {
                    break;
                }
                _ => {}
            }
        }
        dialog.close();
        connection
    }

    pub async fn edit_connection(parent_window: &gtk::Window, con: &ConnectionRemote) -> bool {
        let dialog: ConnectDialog = glib::Object::builder().build();
        dialog.set_transient_for(Some(parent_window));
        dialog.show_all();

        let connection = con.upcast_ref::<Connection>();

        dialog.imp().set_method(connection.method());
        dialog.imp().type_combo.set_sensitive(false);

        if let Some(alias) = connection.alias() {
            dialog.imp().alias_entry.set_text(&alias);
        } else {
            dialog.imp().alias_entry.set_sensitive(false);
        }

        if let Some(uri) = connection.uri() {
            dialog.imp().uri_entry.set_text(&uri.to_str());

            let full_host = uri.host().unwrap_or_default();
            let (domain, host) = full_host
                .split_once(';')
                .unwrap_or_else(|| ("", &full_host));

            dialog.imp().server_entry.set_text(host);

            let port = uri.port();
            if port != -1 {
                dialog.imp().port_entry.set_text(&port.to_string());
            }

            dialog.imp().folder_entry.set_text(&uri.path());
            dialog.imp().domain_entry.set_text(domain);
        }

        dialog.present();
        let mut result = false;
        loop {
            let response = dialog.run_future().await;
            match response {
                gtk::ResponseType::Ok => match dialog.imp().get_connection_uri() {
                    Ok(uri) => {
                        let alias = dialog.imp().alias_entry.text();
                        connection.set_alias(Some(&alias));
                        connection.set_uri(Some(&uri));

                        let path = uri.path();
                        connection.set_base_path(connection.create_path(if path.is_empty() {
                            Path::new("/")
                        } else {
                            Path::new(&path)
                        }));

                        result = true;
                        break;
                    }
                    Err(error) => show_error_message(dialog.upcast_ref(), &error),
                },
                gtk::ResponseType::Cancel | gtk::ResponseType::DeleteEvent => {
                    break;
                }
                _ => {}
            }
        }
        dialog.close();
        result
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_quick_connect_dialog(main_win: *mut GnomeCmdMainWin) {
    let main_win: MainWindow = unsafe { from_glib_none(main_win) };
    glib::MainContext::default().spawn_local(async move {
        // let con: GnomeCmdConRemote = gnome_cmd_data.get_quick_connect();
        if let Some(connection) = ConnectDialog::new_connection(main_win.upcast_ref(), false).await
        {
            let fs = main_win.file_selector(FileSelectorID::ACTIVE);
            if fs.file_list().is_locked() {
                fs.new_tab();
            }
            fs.set_connection(connection.upcast_ref(), None);
        }
    });
}

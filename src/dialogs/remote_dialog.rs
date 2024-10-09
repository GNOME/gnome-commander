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
    connection::list::ConnectionList, i18n::I18N_CONTEXT_ACTION, main_win::MainWindow,
    types::FileSelectorID,
};
use gettextrs::{gettext, pgettext};
use gtk::{glib, pango, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{
        connection::{connection::ConnectionExt, remote::ConnectionRemote},
        dialogs::connect_dialog::ConnectDialog,
        utils::{dialog_button_box, display_help},
    };
    use std::{cell::OnceCell, time::Duration};

    const COL_METHOD: i32 = 0;
    const COL_NAME: i32 = 1;
    const COL_CON: i32 = 2;

    const SORTID_METHOD: gtk::SortColumn = gtk::SortColumn::Index(0);
    const SORTID_NAME: gtk::SortColumn = gtk::SortColumn::Index(1);

    fn create_model() -> gtk::ListStore {
        let store = gtk::ListStore::new(&[
            glib::Type::STRING, // COL_METHOD
            glib::Type::STRING, // COL_NAME
            glib::Type::OBJECT, // COL_CON
        ]);
        store.set_sort_func(SORTID_METHOD, |model, iter1, iter2| {
            let c1 = model
                .get_value(iter1, COL_CON)
                .get::<'_, ConnectionRemote>()
                .ok()
                .map(|c| (c.method(), c.alias().map(|a| a.to_uppercase())));
            let c2 = model
                .get_value(iter2, COL_CON)
                .get::<'_, ConnectionRemote>()
                .ok()
                .map(|c| (c.method(), c.alias().map(|a| a.to_uppercase())));
            c1.cmp(&c2).into()
        });
        store.set_sort_func(SORTID_NAME, |model, iter1, iter2| {
            let c1 = model
                .get_value(iter1, COL_CON)
                .get::<'_, ConnectionRemote>()
                .ok()
                .map(|c| (c.alias().map(|a| a.to_uppercase()), c.method()));
            let c2 = model
                .get_value(iter2, COL_CON)
                .get::<'_, ConnectionRemote>()
                .ok()
                .map(|c| (c.alias().map(|a| a.to_uppercase()), c.method()));
            c1.cmp(&c2).into()
        });
        store.set_sort_column_id(SORTID_METHOD, gtk::SortType::Ascending);
        store
    }

    pub fn set_connection(
        store: &gtk::ListStore,
        iter: &gtk::TreeIter,
        connection: &ConnectionRemote,
    ) {
        store.set_value(iter, COL_METHOD as u32, &connection.icon_name().to_value());
        store.set_value(iter, COL_NAME as u32, &connection.alias().to_value());
        store.set_value(iter, COL_CON as u32, &connection.to_value());
    }

    pub struct RemoteDialog {
        pub model: gtk::ListStore,
        pub view: gtk::TreeView,
        edit_button: gtk::Button,
        remove_button: gtk::Button,
        connect_button: gtk::Button,
        pub main_win: OnceCell<MainWindow>,
        pub connections: OnceCell<ConnectionList>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for RemoteDialog {
        const NAME: &'static str = "GnomeCmdRemoteDialog";
        type Type = super::RemoteDialog;
        type ParentType = gtk::Dialog;

        fn new() -> Self {
            Self {
                model: create_model(),
                view: gtk::TreeView::builder()
                    .enable_search(true)
                    .search_column(COL_NAME)
                    .height_request(240)
                    .build(),
                edit_button: gtk::Button::builder()
                    .label(&gettext("_Edit"))
                    .use_underline(true)
                    .build(),
                remove_button: gtk::Button::builder()
                    .label(&gettext("_Remove"))
                    .use_underline(true)
                    .build(),
                connect_button: gtk::Button::builder()
                    .label(&pgettext(I18N_CONTEXT_ACTION, "C_onnect"))
                    .use_underline(true)
                    .build(),
                main_win: Default::default(),
                connections: Default::default(),
            }
        }
    }

    impl ObjectImpl for RemoteDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_title(Some(&gettext("Remote Connections")));
            obj.set_resizable(true);

            obj.content_area().set_margin_top(12);
            obj.content_area().set_margin_bottom(12);
            obj.content_area().set_margin_start(12);
            obj.content_area().set_margin_end(12);
            obj.content_area().set_spacing(6);

            let hbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(12)
                .vexpand(true)
                .build();
            obj.content_area().append(&hbox);

            {
                let renderer = gtk::CellRendererPixbuf::new();
                let col = gtk::TreeViewColumn::with_attributes(
                    "",
                    &renderer,
                    &[("icon-name", COL_METHOD)],
                );
                col.set_clickable(true);
                col.set_sort_column_id(COL_METHOD);
                col.button()
                    .set_tooltip_text(Some(&gettext("Network protocol")));
                self.view.append_column(&col);
            }
            {
                let renderer = gtk::CellRendererText::builder()
                    .ellipsize(pango::EllipsizeMode::End)
                    .ellipsize_set(true)
                    .build();
                let col = gtk::TreeViewColumn::with_attributes(
                    &gettext("Name"),
                    &renderer,
                    &[("text", COL_NAME)],
                );
                col.set_clickable(true);
                col.set_resizable(true);
                col.set_sort_column_id(COL_NAME);
                col.button()
                    .set_tooltip_text(Some(&gettext("Connection name")));
                self.view.append_column(&col);
            }

            self.view.set_model(Some(&self.model));
            self.view.selection().set_mode(gtk::SelectionMode::Browse);

            let sw = gtk::ScrolledWindow::builder()
                .hscrollbar_policy(gtk::PolicyType::Automatic)
                .vscrollbar_policy(gtk::PolicyType::Automatic)
                .has_frame(true)
                .hexpand(true)
                .child(&self.view)
                .build();
            hbox.append(&sw);

            let bbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .spacing(12)
                .build();
            hbox.append(&bbox);

            let add_button = gtk::Button::builder()
                .label(&gettext("_Add"))
                .use_underline(true)
                .build();
            add_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move {
                        imp.on_new_btn_clicked().await;
                    });
                }
            ));
            bbox.append(&add_button);

            self.edit_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move {
                        imp.on_edit_btn_clicked().await;
                    });
                }
            ));
            bbox.append(&self.edit_button);

            self.remove_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.on_remove_btn_clicked()
            ));
            bbox.append(&self.remove_button);

            let help_button = gtk::Button::builder()
                .label(&gettext("_Help"))
                .use_underline(true)
                .build();
            help_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.on_help_btn_clicked()
            ));

            let close_button = gtk::Button::builder()
                .label(&gettext("_Close"))
                .use_underline(true)
                .build();
            close_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.on_close_btn_clicked()
            ));

            self.connect_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.on_connect_btn_clicked()
            ));

            obj.content_area().append(&dialog_button_box(
                &[&help_button],
                &[&close_button, &self.connect_button],
            ));

            self.view.connect_row_activated(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, _, _| imp.on_list_row_activated()
            ));
            self.model.connect_row_inserted(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, _, _| imp.on_list_row_inserted()
            ));
            self.model.connect_row_deleted(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, _| imp.on_list_row_deleted()
            ));
        }
    }

    impl WidgetImpl for RemoteDialog {}
    impl WindowImpl for RemoteDialog {}
    impl DialogImpl for RemoteDialog {}

    impl RemoteDialog {
        pub fn fill_model(&self) {
            let Some(list) = self.connections.get().map(|l| l.all_remote()) else {
                return;
            };
            for connection in list
                .iter()
                .flat_map(|c| c.downcast_ref::<ConnectionRemote>())
            {
                let iter = self.model.append();
                set_connection(&self.model, &iter, &connection);
            }

            let empty = self.is_empty();
            self.edit_button.set_sensitive(!empty);
            self.remove_button.set_sensitive(!empty);
            self.connect_button.set_sensitive(!empty);
        }

        fn is_empty(&self) -> bool {
            self.model.iter_first().is_none()
        }

        pub fn get_selected_connection(&self) -> Option<(gtk::TreeIter, ConnectionRemote)> {
            let (model, iter) = self.view.selection().selected()?;
            let connection = model
                .get_value(&iter, COL_CON)
                .get::<'_, ConnectionRemote>()
                .ok()?;
            Some((iter, connection))
        }

        async fn on_new_btn_clicked(&self) {
            let Some(connection) =
                ConnectDialog::new_connection(self.obj().upcast_ref(), true).await
            else {
                return;
            };

            self.connections.get().unwrap().add_remote(&connection);
            let iter = self.model.append();
            set_connection(&self.model, &iter, &connection);
        }

        async fn on_edit_btn_clicked(&self) {
            let Some((iter, connection)) = self.get_selected_connection() else {
                return;
            };

            if ConnectDialog::edit_connection(self.obj().upcast_ref(), &connection).await {
                set_connection(&self.model, &iter, &connection);
            }
        }

        fn on_remove_btn_clicked(&self) {
            let Some((iter, connection)) = self.get_selected_connection() else {
                eprintln!("{}", gettext("No server selected"));
                return;
            };

            self.model.remove(&iter);
            self.connections.get().unwrap().remove_remote(&connection);
        }

        fn on_list_row_activated(&self) {
            self.do_connect();
        }

        fn on_list_row_inserted(&self) {
            self.edit_button.set_sensitive(true);
            self.remove_button.set_sensitive(true);
            self.connect_button.set_sensitive(true);
        }

        fn on_list_row_deleted(&self) {
            if self.is_empty() {
                self.edit_button.set_sensitive(false);
                self.remove_button.set_sensitive(false);
                self.connect_button.set_sensitive(false);
            }
        }

        fn on_help_btn_clicked(&self) {
            display_help(
                self.obj().upcast_ref(),
                Some("gnome-commander-remote-connections"),
            );
        }

        fn on_close_btn_clicked(&self) {
            self.obj().close();
        }

        fn on_connect_btn_clicked(&self) {
            self.do_connect();
        }

        fn do_connect(&self) {
            let Some((_iter, connection)) = self.get_selected_connection() else {
                eprintln!("{}", gettext("No server selected"));
                return;
            };

            let Some(main_win) = self.main_win.get() else {
                eprintln!("No main window");
                return;
            };

            let fs = main_win.file_selector(FileSelectorID::ACTIVE);

            self.obj().close();

            glib::timeout_add_local_once(Duration::from_millis(1), move || {
                if fs.file_list().is_locked() {
                    fs.new_tab();
                }
                fs.set_connection(connection.upcast_ref(), None);
            });
        }
    }
}

glib::wrapper! {
    pub struct RemoteDialog(ObjectSubclass<imp::RemoteDialog>)
        @extends gtk::Dialog, gtk::Window, gtk::Widget;
}

impl RemoteDialog {
    pub fn new(main_window: &MainWindow) -> Self {
        let dialog: RemoteDialog = glib::Object::builder().build();
        dialog.set_transient_for(Some(main_window));
        dialog.imp().main_win.set(main_window.clone()).ok().unwrap();
        dialog
            .imp()
            .connections
            .set(ConnectionList::get())
            .ok()
            .unwrap();
        dialog.imp().fill_model();

        // select first
        if let Some(iter) = dialog.imp().model.iter_first() {
            dialog.imp().view.selection().select_iter(&iter);
        }
        dialog.imp().view.grab_focus();

        dialog
    }
}

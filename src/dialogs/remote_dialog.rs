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
        connection::{Connection, ConnectionExt},
        list::ConnectionList,
        remote::ConnectionRemote,
    },
    i18n::I18N_CONTEXT_ACTION,
    main_win::MainWindow,
    types::FileSelectorID,
};
use gettextrs::{gettext, pgettext};
use gtk::{gio, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{
        dialogs::{connect_dialog::ConnectDialog, order_utils::ordering_buttons},
        utils::{dialog_button_box, display_help},
    };
    use std::{cell::OnceCell, time::Duration};

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::RemoteDialog)]
    pub struct RemoteDialog {
        selection: gtk::SingleSelection,
        pub view: gtk::ColumnView,
        edit_button: gtk::Button,
        remove_button: gtk::Button,
        connect_button: gtk::Button,
        #[property(get, construct_only)]
        main_window: OnceCell<MainWindow>,
        #[property(get, construct_only)]
        connections: OnceCell<ConnectionList>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for RemoteDialog {
        const NAME: &'static str = "GnomeCmdRemoteDialog";
        type Type = super::RemoteDialog;
        type ParentType = gtk::Window;

        fn new() -> Self {
            Self {
                selection: gtk::SingleSelection::new(None::<gio::ListModel>),
                view: gtk::ColumnView::builder()
                    .width_request(400)
                    .height_request(250)
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
                main_window: Default::default(),
                connections: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for RemoteDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_title(Some(&gettext("Remote Connections")));
            obj.set_resizable(true);

            let grid = gtk::Grid::builder()
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .row_spacing(6)
                .column_spacing(12)
                .build();
            obj.set_child(Some(&grid));

            self.view.append_column(
                &gtk::ColumnViewColumn::builder()
                    .factory(&connection_icon_factory())
                    .build(),
            );
            self.view.append_column(
                &gtk::ColumnViewColumn::builder()
                    .title(gettext("Name"))
                    .factory(&connection_alias_factory())
                    .expand(true)
                    .build(),
            );

            self.view.set_model(Some(&self.selection));

            let sw = gtk::ScrolledWindow::builder()
                .hscrollbar_policy(gtk::PolicyType::Automatic)
                .vscrollbar_policy(gtk::PolicyType::Automatic)
                .has_frame(true)
                .hexpand(true)
                .vexpand(true)
                .child(&self.view)
                .build();
            grid.attach(&sw, 0, 0, 1, 1);

            let bbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .spacing(6)
                .build();
            grid.attach(&bbox, 1, 0, 1, 1);

            let add_button = gtk::Button::builder()
                .label(&gettext("_Add"))
                .use_underline(true)
                .build();
            add_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.on_new_btn_clicked().await });
                }
            ));
            bbox.append(&add_button);

            self.edit_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.on_edit_btn_clicked().await });
                }
            ));
            bbox.append(&self.edit_button);

            self.remove_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.on_remove_btn_clicked()
            ));
            bbox.append(&self.remove_button);

            let (up_button, down_button) = ordering_buttons(&self.selection);
            bbox.append(&up_button);
            bbox.append(&down_button);

            let help_button = gtk::Button::builder()
                .label(&gettext("_Help"))
                .use_underline(true)
                .build();
            help_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.on_help_btn_clicked().await });
                }
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

            grid.attach(
                &dialog_button_box(&[&help_button], &[&close_button, &self.connect_button]),
                0,
                1,
                2,
                1,
            );

            self.view.connect_activate(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, _| imp.do_connect()
            ));

            self.selection.connect_selected_notify(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.selection_changed()
            ));
        }
    }

    impl WidgetImpl for RemoteDialog {}
    impl WindowImpl for RemoteDialog {}

    impl RemoteDialog {
        pub fn fill_model(&self) {
            let Some(list) = self.connections.get().map(|l| l.all()) else {
                return;
            };
            let filter = gtk::FilterListModel::new(
                Some(list),
                Some(gtk::CustomFilter::new(|item| {
                    item.downcast_ref::<ConnectionRemote>().is_some()
                })),
            );
            self.selection.set_model(Some(&filter));
        }

        fn selection_changed(&self) {
            let has_selection = self.selection.selected() != gtk::INVALID_LIST_POSITION;
            self.edit_button.set_sensitive(has_selection);
            self.remove_button.set_sensitive(has_selection);
        }

        pub fn selected_connection(&self) -> Option<(u32, ConnectionRemote)> {
            let position = self.selection.selected();
            if position == gtk::INVALID_LIST_POSITION {
                return None;
            };
            let connection = self
                .selection
                .item(position)
                .and_downcast::<ConnectionRemote>()?;
            Some((position, connection))
        }

        async fn on_new_btn_clicked(&self) {
            let Some(connection) =
                ConnectDialog::new_connection(self.obj().upcast_ref(), true, None).await
            else {
                return;
            };

            let connections = self.obj().connections();
            connections.add(&connection);

            let position = self.selection.n_items() - 1;
            self.selection.set_selected(position);
            self.view
                .scroll_to(position, None, gtk::ListScrollFlags::all(), None);
            self.view.grab_focus();
        }

        async fn on_edit_btn_clicked(&self) {
            let Some((position, connection)) = self.selected_connection() else {
                return;
            };

            if ConnectDialog::edit_connection(self.obj().upcast_ref(), &connection).await {
                self.obj().connections().refresh(&connection);
                self.selection.set_selected(position);
                self.view
                    .scroll_to(position, None, gtk::ListScrollFlags::all(), None);
                self.view.grab_focus();
            }
        }

        fn on_remove_btn_clicked(&self) {
            let Some((_, connection)) = self.selected_connection() else {
                eprintln!("{}", gettext("No server selected"));
                return;
            };
            self.obj().connections().remove(&connection);
        }

        async fn on_help_btn_clicked(&self) {
            display_help(
                self.obj().upcast_ref(),
                Some("gnome-commander-remote-connections"),
            )
            .await;
        }

        fn on_close_btn_clicked(&self) {
            self.obj().close();
        }

        fn on_connect_btn_clicked(&self) {
            self.do_connect();
        }

        fn do_connect(&self) {
            let Some((_iter, connection)) = self.selected_connection() else {
                eprintln!("{}", gettext("No server selected"));
                return;
            };

            let fs = self
                .obj()
                .main_window()
                .file_selector(FileSelectorID::ACTIVE);

            self.obj().close();

            glib::timeout_add_local_once(Duration::from_millis(1), move || {
                if fs.is_current_tab_locked() {
                    fs.new_tab();
                }
                fs.file_list().set_connection(&connection, None);
            });
        }
    }
}

glib::wrapper! {
    pub struct RemoteDialog(ObjectSubclass<imp::RemoteDialog>)
        @extends gtk::Window, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native;
}

impl RemoteDialog {
    pub fn new(main_window: &MainWindow) -> Self {
        let dialog: RemoteDialog = glib::Object::builder()
            .property("transient-for", main_window)
            .property("connections", ConnectionList::get())
            .property("main-window", main_window)
            .build();
        dialog.imp().fill_model();
        dialog.imp().view.grab_focus();
        dialog
    }
}

fn connection_icon_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        if let Some(list_item) = item.downcast_ref::<gtk::ListItem>() {
            list_item.set_child(Some(&gtk::Image::builder().build()));
        }
    });
    factory.connect_bind(|_, item| {
        let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(image) = list_item.child().and_downcast::<gtk::Image>() else {
            return;
        };
        image.set_icon_name(None);
        if let Some(icon) = list_item
            .item()
            .and_downcast::<Connection>()
            .and_then(|c| c.open_icon())
        {
            image.set_from_gicon(&icon);
        };
    });
    factory.upcast()
}

fn connection_alias_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        if let Some(list_item) = item.downcast_ref::<gtk::ListItem>() {
            list_item.set_child(Some(&gtk::Label::builder().xalign(0.0).build()));
        }
    });
    factory.connect_bind(|_, item| {
        let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(label) = list_item.child().and_downcast::<gtk::Label>() else {
            return;
        };
        label.set_label("");
        if let Some(device) = list_item.item().and_downcast::<Connection>() {
            label.set_label(&device.alias().unwrap_or_else(|| device.uuid()));
        };
    });
    factory.upcast()
}

/*
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

use super::edit_bookmark_dialog::edit_bookmark_dialog;
use crate::{
    connection::{
        bookmark::Bookmark,
        connection::{Connection, ConnectionExt},
        list::ConnectionList,
    },
    dir::Directory,
    file::File,
    options::options::GeneralOptions,
    shortcuts::Shortcuts,
    utils::{ErrorMessage, bold},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{
        options::utils::remember_window_size,
        utils::{SenderExt, dialog_button_box, display_help},
    };
    use std::cell::RefCell;

    pub struct BookmarksDialog {
        pub shortcuts: RefCell<Option<Shortcuts>>,
        pub flatten_model: gtk::FlattenListModel,
        pub selection_model: gtk::SingleSelection,
        pub view: gtk::ColumnView,
        pub edit_button: gtk::Button,
        pub remove_button: gtk::Button,
        pub up_button: gtk::Button,
        pub down_button: gtk::Button,
        jump_button: gtk::Button,
        sender: async_channel::Sender<Option<TaggedBookmark>>,
        pub receiver: async_channel::Receiver<Option<TaggedBookmark>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for BookmarksDialog {
        const NAME: &'static str = "GnomeCmdBookmarksDialog";
        type Type = super::BookmarksDialog;
        type ParentType = gtk::Window;

        fn new() -> Self {
            let flatten_model = gtk::FlattenListModel::new(None::<gio::ListModel>);
            let selection_model = gtk::SingleSelection::new(Some(flatten_model.clone()));
            let view = gtk::ColumnView::builder()
                .width_request(400)
                .height_request(250)
                .model(&selection_model)
                .build();
            let (sender, receiver) = async_channel::bounded(1);
            Self {
                shortcuts: Default::default(),
                flatten_model,
                selection_model,
                view,
                edit_button: gtk::Button::builder()
                    .label(gettext("_Edit"))
                    .use_underline(true)
                    .sensitive(false)
                    .build(),
                remove_button: gtk::Button::builder()
                    .label(gettext("_Remove"))
                    .use_underline(true)
                    .sensitive(false)
                    .build(),
                up_button: gtk::Button::builder()
                    .label(gettext("_Up"))
                    .use_underline(true)
                    .sensitive(false)
                    .build(),
                down_button: gtk::Button::builder()
                    .label(gettext("_Down"))
                    .use_underline(true)
                    .sensitive(false)
                    .build(),
                jump_button: gtk::Button::builder()
                    .label(gettext("_Jump to"))
                    .use_underline(true)
                    .sensitive(false)
                    .build(),
                sender,
                receiver,
            }
        }
    }

    impl ObjectImpl for BookmarksDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let dialog = self.obj();

            dialog.set_title(Some(&gettext("Bookmarks")));
            dialog.set_resizable(true);
            dialog.set_destroy_with_parent(true);

            let options = GeneralOptions::new();
            remember_window_size(
                &*dialog,
                &options.bookmarks_window_width,
                &options.bookmarks_window_height,
            );

            let grid = gtk::Grid::builder()
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .row_spacing(6)
                .column_spacing(12)
                .build();
            dialog.set_child(Some(&grid));

            self.view.set_header_factory(Some(&self.header_factory()));
            self.view.append_column(
                &gtk::ColumnViewColumn::builder()
                    .title(gettext("Name"))
                    .factory(&self.name_factory())
                    .resizable(true)
                    .build(),
            );
            self.view.append_column(
                &gtk::ColumnViewColumn::builder()
                    .title(gettext("Shortcut"))
                    .factory(&self.shortcut_factory())
                    .resizable(true)
                    .build(),
            );
            self.view.append_column(
                &gtk::ColumnViewColumn::builder()
                    .title(gettext("Path"))
                    .factory(&self.path_factory())
                    .resizable(true)
                    .expand(true)
                    .build(),
            );
            self.view.connect_activate(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, _| imp.jump_to_clicked()
            ));

            let scrolled_window = gtk::ScrolledWindow::builder()
                .hscrollbar_policy(gtk::PolicyType::Never)
                .vscrollbar_policy(gtk::PolicyType::Automatic)
                .has_frame(true)
                .hexpand(true)
                .vexpand(true)
                .child(&self.view)
                .build();
            grid.attach(&scrolled_window, 0, 0, 1, 1);

            self.selection_model.connect_selected_notify(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.selection_changed()
            ));

            let vbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .spacing(6)
                .build();
            vbox.append(&self.edit_button);
            vbox.append(&self.remove_button);
            vbox.append(&self.up_button);
            vbox.append(&self.down_button);
            grid.attach(&vbox, 1, 0, 1, 1);

            self.edit_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.edit_clicked().await });
                }
            ));
            self.remove_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.remove_clicked()
            ));
            self.up_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.up_clicked()
            ));
            self.down_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.down_clicked()
            ));

            let help_button = gtk::Button::builder()
                .label(gettext("_Help"))
                .use_underline(true)
                .build();
            let close_button = gtk::Button::builder()
                .label(gettext("_Close"))
                .use_underline(true)
                .build();

            grid.attach(
                &dialog_button_box(&[&help_button], &[&close_button, &self.jump_button]),
                0,
                1,
                2,
                1,
            );

            help_button.connect_clicked(glib::clone!(
                #[weak]
                dialog,
                move |_| {
                    glib::spawn_future_local(async move {
                        display_help(dialog.upcast_ref(), Some("gnome-commander-bookmarks")).await
                    });
                }
            ));

            close_button.connect_clicked(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_| sender.toss(None)
            ));

            self.jump_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.jump_to_clicked()
            ));

            dialog.connect_close_request(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_| {
                    sender.toss(None);
                    glib::Propagation::Proceed
                }
            ));

            dialog.set_default_widget(Some(&self.jump_button));
        }
    }

    impl WidgetImpl for BookmarksDialog {}
    impl WindowImpl for BookmarksDialog {}

    impl BookmarksDialog {
        fn header_factory(&self) -> gtk::ListItemFactory {
            let factory = gtk::SignalListItemFactory::new();
            factory.connect_setup(|_, item| {
                let Some(item) = item.downcast_ref::<gtk::ListHeader>() else {
                    return;
                };
                item.set_child(Some(
                    &gtk::Label::builder()
                        .xalign(0.0)
                        .margin_start(6)
                        .margin_end(6)
                        .margin_top(12)
                        .margin_bottom(6)
                        .build(),
                ));
            });
            factory.connect_bind(|_, item| {
                let Some(item) = item.downcast_ref::<gtk::ListHeader>() else {
                    return;
                };
                let Some(label) = item.child().and_downcast::<gtk::Label>() else {
                    return;
                };
                label.set_text("");
                let Some(obj) = item.item().and_downcast::<glib::BoxedAnyObject>() else {
                    return;
                };
                let Ok(node) = obj.try_borrow::<TaggedBookmark>() else {
                    return;
                };
                label.set_markup(&bold(
                    &node
                        .connection
                        .alias()
                        .unwrap_or_else(|| node.connection.uuid()),
                ));
            });
            factory.upcast()
        }

        fn name_factory(&self) -> gtk::ListItemFactory {
            let factory = gtk::SignalListItemFactory::new();
            factory.connect_setup(|_, item| {
                let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
                    return;
                };
                item.set_child(Some(&gtk::Label::builder().xalign(0.0).build()));
            });
            factory.connect_bind(|_, item| {
                let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
                    return;
                };
                let Some(label) = item.child().and_downcast::<gtk::Label>() else {
                    return;
                };
                label.set_text("");
                let Some(obj) = item.item().and_downcast::<glib::BoxedAnyObject>() else {
                    return;
                };
                let Ok(node) = obj.try_borrow::<TaggedBookmark>() else {
                    return;
                };
                label.set_text(&node.bookmark.name());
            });
            factory.upcast()
        }

        fn shortcut_factory(&self) -> gtk::ListItemFactory {
            let factory = gtk::SignalListItemFactory::new();
            factory.connect_setup(|_, list_item| {
                let Some(list_item) = list_item.downcast_ref::<gtk::ListItem>() else {
                    return;
                };
                let bx = gtk::Box::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .spacing(6)
                    .build();
                list_item.set_child(Some(&bx));
            });
            factory.connect_bind(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, list_item| {
                    let Some(list_item) = list_item.downcast_ref::<gtk::ListItem>() else {
                        return;
                    };
                    let Some(obj) = list_item.item().and_downcast::<glib::BoxedAnyObject>() else {
                        return;
                    };
                    let Ok(node) = obj.try_borrow::<TaggedBookmark>() else {
                        return;
                    };
                    let Some(bx) = list_item.child().and_downcast::<gtk::Box>() else {
                        return;
                    };

                    while let Some(child) = bx.first_child() {
                        child.unparent();
                    }
                    let shortcuts = imp.shortcuts.borrow();
                    if let Some(shortcuts) = shortcuts
                        .as_ref()
                        .map(|s| s.bookmark_shortcuts(&node.bookmark.name()))
                    {
                        for shortcut in shortcuts {
                            bx.append(
                                &gtk::ShortcutLabel::builder()
                                    .accelerator(shortcut.name())
                                    .build(),
                            );
                        }
                    }
                }
            ));
            factory.upcast()
        }

        fn path_factory(&self) -> gtk::ListItemFactory {
            let factory = gtk::SignalListItemFactory::new();
            factory.connect_setup(|_, item| {
                let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
                    return;
                };
                item.set_child(Some(&gtk::Label::builder().xalign(0.0).build()));
            });
            factory.connect_bind(|_, item| {
                let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
                    return;
                };
                let Some(label) = item.child().and_downcast::<gtk::Label>() else {
                    return;
                };
                label.set_text("");
                let Some(obj) = item.item().and_downcast::<glib::BoxedAnyObject>() else {
                    return;
                };
                let Ok(node) = obj.try_borrow::<TaggedBookmark>() else {
                    return;
                };
                label.set_text(&node.bookmark.path());
                label.set_tooltip_text(Some(&node.bookmark.path()));
            });
            factory.upcast()
        }

        fn selected(&self) -> Option<TaggedBookmark> {
            Some(
                self.selection_model
                    .selected_item()
                    .and_downcast::<glib::BoxedAnyObject>()?
                    .try_borrow::<TaggedBookmark>()
                    .ok()?
                    .clone(),
            )
        }

        fn selection_changed(&self) {
            if let Some(bookmark) = self.selected() {
                self.jump_button.set_sensitive(true);
                self.edit_button.set_sensitive(true);
                self.remove_button.set_sensitive(true);
                self.up_button.set_sensitive(!bookmark.is_first());
                self.down_button.set_sensitive(!bookmark.is_last());
            } else {
                self.jump_button.set_sensitive(false);
                self.edit_button.set_sensitive(false);
                self.remove_button.set_sensitive(false);
                self.up_button.set_sensitive(false);
                self.down_button.set_sensitive(false);
            }
        }

        async fn edit_clicked(&self) {
            if let Some(bookmark) = self.selected() {
                if let Some(changed_bookmark) = edit_bookmark_dialog(
                    self.obj().upcast_ref(),
                    &gettext("Edit Bookmark"),
                    &bookmark.bookmark,
                )
                .await
                {
                    bookmark
                        .connection
                        .replace_bookmark(&bookmark.bookmark, changed_bookmark);
                }
            }
        }

        fn remove_clicked(&self) {
            if let Some(bookmark) = self.selected() {
                bookmark.connection.remove_bookmark(&bookmark.bookmark);
                self.selection_changed();
            }
        }

        fn up_clicked(&self) {
            if let Some(bookmark) = self.selected() {
                if let Some(position) = bookmark.connection.move_bookmark_up(&bookmark.bookmark) {
                    self.selection_model.set_selected(position);
                    self.selection_changed();
                }
            }
        }

        fn down_clicked(&self) {
            if let Some(bookmark) = self.selected() {
                if let Some(position) = bookmark.connection.move_bookmark_down(&bookmark.bookmark) {
                    self.selection_model.set_selected(position);
                    self.selection_changed();
                }
            }
        }

        fn jump_to_clicked(&self) {
            if let Some(bookmark) = self.selected() {
                self.sender.toss(Some(bookmark));
            }
        }
    }
}

glib::wrapper! {
    pub struct BookmarksDialog(ObjectSubclass<imp::BookmarksDialog>)
        @extends gtk::Window, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native;
}

impl BookmarksDialog {
    fn new(
        parent_window: &gtk::Window,
        connection_list: &ConnectionList,
        shortcuts: &Shortcuts,
    ) -> Self {
        let dialog: Self = glib::Object::builder()
            .property("transient-for", parent_window)
            .build();
        *dialog.imp().shortcuts.borrow_mut() = Some(shortcuts.clone());
        dialog.update_model(connection_list);
        dialog
    }

    fn update_model(&self, connection_list: &ConnectionList) {
        let model = gtk::MapListModel::new(Some(connection_list.all()), move |connection| {
            let connection: Connection = connection.clone().downcast().unwrap();
            gtk::MapListModel::new(Some(connection.bookmarks()), move |bookmark| {
                let bookmark: Bookmark = bookmark.clone().downcast().unwrap();
                glib::BoxedAnyObject::new(TaggedBookmark {
                    connection: connection.clone(),
                    bookmark,
                })
                .upcast()
            })
            .upcast()
        });
        self.imp().flatten_model.set_model(Some(&model));
    }

    pub async fn show(
        parent_window: &gtk::Window,
        connection_list: &ConnectionList,
        shortcuts: &Shortcuts,
    ) -> Option<TaggedBookmark> {
        let dialog = Self::new(parent_window, connection_list, shortcuts);
        dialog.present();
        dialog.imp().view.grab_focus();
        let result = dialog.imp().receiver.recv().await.ok().flatten();
        dialog.close();
        result
    }
}

#[derive(Clone)]
pub struct TaggedBookmark {
    pub connection: Connection,
    pub bookmark: Bookmark,
}

impl TaggedBookmark {
    fn is_first(&self) -> bool {
        self.connection.bookmarks().item(0).as_ref() == Some(self.bookmark.upcast_ref())
    }

    fn is_last(&self) -> bool {
        let bookmarks = self.connection.bookmarks();
        let last_index = bookmarks.n_items().saturating_sub(1);
        bookmarks.item(last_index).as_ref() == Some(self.bookmark.upcast_ref())
    }
}

pub async fn bookmark_directory(window: &gtk::Window, dir: &Directory, options: &GeneralOptions) {
    let file = dir.upcast_ref::<File>();
    let is_local = file.is_local();
    let path = if is_local {
        file.get_real_path()
    } else {
        Some(file.get_path_string_through_parent())
    };
    let Some(path) = path else {
        eprintln!("Failed to get path for bookmarking");
        return;
    };

    let Some(path_str) = path.to_str() else {
        ErrorMessage::new(gettext("To bookmark a directory the whole search path to the directory must be in valid UTF-8 encoding"), None::<String>)
            .show(window).await;
        return;
    };

    let bookmark = Bookmark::new(
        path.file_name()
            .and_then(|n| n.to_str())
            .unwrap_or_default(),
        path_str,
    );

    if let Some(changed_bookmark) =
        edit_bookmark_dialog(window, &gettext("New Bookmark"), &bookmark).await
    {
        let connection_list = ConnectionList::get();

        let con = if is_local {
            connection_list.home().upcast()
        } else {
            dir.connection()
        };
        con.add_bookmark(&changed_bookmark);

        if let Err(error) = options.bookmarks.set(connection_list.save_bookmarks()) {
            eprintln!("Failed to save bookmarks: {error}");
        }
    }
}

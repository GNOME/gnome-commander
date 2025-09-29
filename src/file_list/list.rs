/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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

use super::{
    cell::FileListCell, file_attr_sorter::FileAttrSorter, file_type_sorter::FileTypeSorter,
    item::FileListItem, popup::file_popup_menu, quick_search::QuickSearch,
};
use crate::{
    connection::{
        connection::{Connection, ConnectionExt, ConnectionInterface},
        list::ConnectionList,
    },
    dialogs::{delete_dialog::show_delete_dialog, rename_popover::show_rename_popover},
    dir::{Directory, DirectoryState},
    file::File,
    filter::{Filter, fnmatch},
    imageloader::icon_cache,
    layout::{
        PREF_COLORS,
        ls_colors::{LsPalletteColor, ls_colors_get},
    },
    libgcmd::file_descriptor::FileDescriptorExt,
    main_win::MainWindow,
    open_connection::open_connection,
    options::options::{ColorOptions, ConfirmOptions, FiltersOptions, GeneralOptions},
    tags::tags::FileMetadataService,
    types::{ExtensionDisplayMode, GraphicalLayoutMode, SizeDisplayMode},
    utils::{ErrorMessage, size_to_string, time_to_string},
};
use gettextrs::{gettext, ngettext};
use gtk::{gdk, gio, glib, graphene, prelude::*, subclass::prelude::*};
use std::{collections::HashSet, path::Path};
use strum::VariantArray;

mod imp {
    use super::*;
    use crate::{
        app::App,
        connection::list::ConnectionList,
        debug::debug,
        dialogs::pattern_selection_dialog::select_by_pattern,
        file_list::{
            actions::{
                Script, file_list_action_execute, file_list_action_execute_script,
                file_list_action_file_edit, file_list_action_file_view, file_list_action_open_with,
                file_list_action_open_with_default, file_list_action_open_with_other,
            },
            popup::list_popup_menu,
        },
        tags::tags::FileMetadataService,
        transfer::{copy_files, link_files, move_files},
        types::{
            ConfirmOverwriteMode, DndMode, ExtensionDisplayMode, GnomeCmdTransferType,
            GraphicalLayoutMode, LeftMouseButtonMode, MiddleMouseButtonMode, PermissionDisplayMode,
            QuickSearchShortcut, RightMouseButtonMode,
        },
        utils::{
            ALT, ALT_SHIFT, CONTROL, CONTROL_ALT, CONTROL_SHIFT, NO_MOD, SHIFT,
            get_modifiers_state, permissions_to_numbers, permissions_to_text,
        },
        weak_map::WeakMap,
    };
    use std::{
        cell::{Cell, OnceCell, RefCell},
        collections::BTreeMap,
        ffi::OsStr,
        path::PathBuf,
        rc::Rc,
        sync::OnceLock,
        time::Duration,
    };

    pub type CellsMap = Rc<WeakMap<FileListItem, FileListCell>>;

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::FileList)]
    pub struct FileList {
        pub quick_search: glib::WeakRef<QuickSearch>,
        #[property(get, construct_only)]
        pub file_metadata_service: OnceCell<FileMetadataService>,
        #[property(get, set, nullable)]
        pub base_dir: RefCell<Option<PathBuf>>,

        #[property(get, set)]
        pub font_name: RefCell<String>,

        #[property(get, set)]
        pub row_height: Cell<u32>,

        #[property(get, set, builder(ExtensionDisplayMode::default()))]
        pub extension_display_mode: Cell<ExtensionDisplayMode>,

        #[property(get, set, builder(GraphicalLayoutMode::default()))]
        graphical_layout_mode: Cell<GraphicalLayoutMode>,

        #[property(get, set, builder(SizeDisplayMode::default()))]
        size_display_mode: Cell<SizeDisplayMode>,

        #[property(get, set, builder(PermissionDisplayMode::default()))]
        permissions_display_mode: Cell<PermissionDisplayMode>,

        #[property(get, set)]
        date_display_format: RefCell<String>,

        #[property(get, set)]
        pub use_ls_colors: Cell<bool>,

        #[property(get, set, default = true)]
        case_sensitive: Cell<bool>,

        #[property(get, set)]
        symbolic_links_as_regular_files: Cell<bool>,

        #[property(get, set, builder(LeftMouseButtonMode::default()))]
        left_mouse_button_mode: Cell<LeftMouseButtonMode>,

        #[property(get, set, builder(MiddleMouseButtonMode::default()))]
        middle_mouse_button_mode: Cell<MiddleMouseButtonMode>,

        #[property(get, set, builder(RightMouseButtonMode::default()))]
        right_mouse_button_mode: Cell<RightMouseButtonMode>,

        #[property(get, set, default = true)]
        left_mouse_button_unselects: Cell<bool>,

        #[property(get, set, builder(DndMode::default()))]
        dnd_mode: Cell<DndMode>,

        #[property(get, set, default = true)]
        select_dirs: Cell<bool>,

        #[property(get, set, builder(QuickSearchShortcut::default()))]
        quick_search_shortcut: Cell<QuickSearchShortcut>,

        #[property(get)]
        pub store: gio::ListStore,

        pub sort_model: gtk::SortListModel,
        pub sorting: Cell<Option<(ColumnID, gtk::SortType)>>,

        #[property(get)]
        pub selection: gtk::SingleSelection,
        #[property(get)]
        pub view: gtk::ColumnView,
        pub columns: RefCell<Vec<gtk::ColumnViewColumn>>,

        pub cells_map: BTreeMap<ColumnID, CellsMap>,

        pub shift_down: Cell<bool>,
        pub shift_down_row: Cell<Option<u32>>,
        pub shift_down_key: Cell<Option<gdk::Key>>,

        modifier_click: Cell<Option<gdk::ModifierType>>,

        pub focus_later: RefCell<Option<PathBuf>>,

        pub connection: RefCell<Option<Connection>>,
        pub connection_handlers: RefCell<Vec<glib::SignalHandlerId>>,

        pub directory: RefCell<Option<Directory>>,
        pub directory_handlers: RefCell<Vec<glib::SignalHandlerId>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileList {
        const NAME: &'static str = "GnomeCmdFileList";
        type Type = super::FileList;
        type ParentType = gtk::Widget;

        fn class_init(klass: &mut Self::Class) {
            klass.install_action_async("fl.refresh", None, |obj, _, _| async move {
                obj.reload().await
            });
            klass.install_action_async("fl.file-view", None, |obj, _, parameter| async move {
                let use_internal_viewer = parameter.and_then(|v| v.get::<bool>());
                file_list_action_file_view(&obj, use_internal_viewer).await;
            });
            klass.install_action_async("fl.file-edit", None, |obj, _, _| async move {
                file_list_action_file_edit(&obj).await;
            });
            klass.install_action_async("fl.open-with-default", None, |obj, _, _| async move {
                file_list_action_open_with_default(&obj).await;
            });
            klass.install_action_async("fl.open-with-other", None, |obj, _, _| async move {
                file_list_action_open_with_other(&obj).await;
            });
            klass.install_action_async(
                "fl.open-with",
                Some(&App::static_variant_type()),
                |obj, _, parameter| async move {
                    if let Some(app) = parameter.as_ref().and_then(App::from_variant) {
                        file_list_action_open_with(&obj, app).await;
                    } else {
                        eprintln!("Cannot load app from a variant");
                    }
                },
            );
            klass.install_action_async("fl.execute", None, |obj, _, _| async move {
                file_list_action_execute(&obj).await;
            });
            klass.install_action_async(
                "fl.execute-script",
                Some(&Script::static_variant_type()),
                |obj, _, parameter| async move {
                    if let Some(script) = parameter.as_ref().and_then(Script::from_variant) {
                        file_list_action_execute_script(&obj, script).await;
                    } else {
                        eprintln!("Cannot load script from a variant");
                    }
                },
            );
            klass.install_action_async(
                "fl.drop-files",
                Some(&DroppingFiles::static_variant_type()),
                |obj, _, parameter| async move {
                    if let Some(transfer) = parameter.as_ref().and_then(DroppingFiles::from_variant)
                    {
                        obj.imp().drop_files(transfer).await;
                    }
                },
            );
            klass.install_action("fl.drop-files-cancel", None, |_, _, _| { /* do nothing */ });
        }

        fn new() -> Self {
            Self {
                quick_search: Default::default(),
                file_metadata_service: Default::default(),
                base_dir: Default::default(),

                font_name: Default::default(),
                row_height: Default::default(),
                extension_display_mode: Default::default(),
                graphical_layout_mode: Default::default(),
                size_display_mode: Default::default(),
                permissions_display_mode: Default::default(),
                date_display_format: Default::default(),
                use_ls_colors: Default::default(),
                case_sensitive: Default::default(),
                symbolic_links_as_regular_files: Default::default(),
                left_mouse_button_mode: Default::default(),
                middle_mouse_button_mode: Default::default(),
                right_mouse_button_mode: Default::default(),
                left_mouse_button_unselects: Default::default(),
                dnd_mode: Default::default(),
                select_dirs: Default::default(),
                quick_search_shortcut: Default::default(),

                store: gio::ListStore::new::<FileListItem>(),

                sort_model: Default::default(),
                sorting: Default::default(),

                selection: Default::default(),
                view: gtk::ColumnView::builder()
                    .show_column_separators(false)
                    .show_row_separators(false)
                    .build(),
                columns: Default::default(),

                cells_map: ColumnID::VARIANTS
                    .iter()
                    .map(|c| (*c, Default::default()))
                    .collect(),

                shift_down: Default::default(),
                shift_down_row: Default::default(),
                shift_down_key: Default::default(),
                modifier_click: Default::default(),

                focus_later: Default::default(),

                connection: Default::default(),
                connection_handlers: Default::default(),

                directory: Default::default(),
                directory_handlers: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileList {
        fn constructed(&self) {
            self.parent_constructed();

            let fl = self.obj();
            fl.set_layout_manager(Some(gtk::BinLayout::new()));

            self.sort_model.set_model(Some(&self.store));
            self.selection.set_model(Some(&self.sort_model));
            self.view.set_model(Some(&self.selection));
            self.view.add_css_class("gnome-cmd-file-list");

            let scrolled_window = gtk::ScrolledWindow::builder()
                .hscrollbar_policy(gtk::PolicyType::Automatic)
                .vscrollbar_policy(gtk::PolicyType::Automatic)
                .hexpand(true)
                .vexpand(true)
                .child(&self.view)
                .build();
            scrolled_window.set_parent(&*fl);

            for (column_id, title, factory, sorter) in [
                (ColumnID::COLUMN_ICON, "", create_icon_factory(), None),
                (
                    ColumnID::COLUMN_NAME,
                    &gettext("name"),
                    create_name_factory(&self.cells_map[&ColumnID::COLUMN_NAME]),
                    Some(FileAttrSorter::by_name()),
                ),
                (
                    ColumnID::COLUMN_EXT,
                    &gettext("ext"),
                    create_ext_factory(&self.cells_map[&ColumnID::COLUMN_EXT]),
                    Some(FileAttrSorter::by_ext()),
                ),
                (
                    ColumnID::COLUMN_DIR,
                    &gettext("dir"),
                    create_dir_factory(&self.cells_map[&ColumnID::COLUMN_DIR]),
                    Some(FileAttrSorter::by_dir()),
                ),
                (
                    ColumnID::COLUMN_SIZE,
                    &gettext("size"),
                    create_size_factory(&self.cells_map[&ColumnID::COLUMN_SIZE]),
                    Some(FileAttrSorter::by_size()),
                ),
                (
                    ColumnID::COLUMN_DATE,
                    &gettext("date"),
                    create_date_factory(&self.cells_map[&ColumnID::COLUMN_DATE]),
                    Some(FileAttrSorter::by_date()),
                ),
                (
                    ColumnID::COLUMN_PERM,
                    &gettext("perm"),
                    create_perm_factory(&self.cells_map[&ColumnID::COLUMN_PERM]),
                    Some(FileAttrSorter::by_perm()),
                ),
                (
                    ColumnID::COLUMN_OWNER,
                    &gettext("uid"),
                    create_owner_factory(&self.cells_map[&ColumnID::COLUMN_OWNER]),
                    Some(FileAttrSorter::by_owner()),
                ),
                (
                    ColumnID::COLUMN_GROUP,
                    &gettext("gid"),
                    create_group_factory(&self.cells_map[&ColumnID::COLUMN_GROUP]),
                    Some(FileAttrSorter::by_group()),
                ),
            ] {
                let column = gtk::ColumnViewColumn::builder()
                    .title(title)
                    .factory(&factory)
                    .resizable(true)
                    .expand(column_id != ColumnID::COLUMN_ICON)
                    .build();
                column.set_sorter(sorter.as_ref());
                self.view.append_column(&column);
                self.columns.borrow_mut().push(column);
            }

            let sorter = gtk::MultiSorter::new();
            sorter.append(FileTypeSorter::default());
            sorter.append(
                self.view
                    .sorter()
                    .and_downcast::<gtk::ColumnViewSorter>()
                    .unwrap(),
            );

            self.sort_model.set_sorter(Some(&sorter));
            self.view
                .sort_by_column(self.columns.borrow().get(1), gtk::SortType::Ascending);
            self.sorting
                .set(Some((ColumnID::COLUMN_NAME, gtk::SortType::Ascending)));

            self.selection.connect_selected_notify(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.cursor_changed()
            ));

            let key_controller = gtk::EventControllerKey::builder()
                .propagation_phase(gtk::PropagationPhase::Capture)
                .build();
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.key_pressed(key, state)
            ));
            fl.add_controller(key_controller);

            let click_controller = gtk::GestureClick::builder()
                .button(0)
                .propagation_phase(gtk::PropagationPhase::Capture)
                .build();
            click_controller.connect_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |gesture, n_press, x, y| imp.on_button_press(
                    gesture.current_button(),
                    n_press,
                    x,
                    y
                )
            ));
            click_controller.connect_released(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |gesture, n_press, x, y| imp.on_button_release(
                    gesture.current_button(),
                    n_press,
                    x,
                    y
                )
            ));
            self.view.add_controller(click_controller);

            let drag_source = gtk::DragSource::new();
            drag_source.connect_prepare(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                None,
                move |_, x, y| imp.drag_prepare(x, y)
            ));
            drag_source.connect_drag_end(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, drag, delete| imp.drag_end(drag, delete)
            ));
            // TODO: implement
            // fl.add_controller(drag_source);

            let drop_target = gtk::DropTarget::new(String::static_type(), gdk::DragAction::all());
            drop_target.connect_drop(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                false,
                move |_, value, x, y| imp.drop(value, x, y)
            ));
            // TODO: implement
            // fl.add_controller(drop_target);

            let general_options = GeneralOptions::new();
            general_options.list_font.bind(&*fl, "font-name").build();
            general_options
                .list_row_height
                .bind(&*fl, "row-height")
                .build();
            general_options
                .extension_display_mode
                .bind(&*fl, "extension-display-mode")
                .build();
            general_options
                .graphical_layout_mode
                .bind(&*fl, "graphical-layout-mode")
                .build();
            general_options
                .size_display_mode
                .bind(&*fl, "size-display-mode")
                .build();
            general_options
                .permissions_display_mode
                .bind(&*fl, "permissions-display-mode")
                .build();
            general_options
                .date_display_format
                .bind(&*fl, "date-display-format")
                .build();
            general_options
                .case_sensitive
                .bind(&*fl, "case-sensitive")
                .build();
            general_options
                .symbolic_links_as_regular_files
                .bind(&*fl, "symbolic-links-as-regular-files")
                .build();
            general_options
                .left_mouse_button_mode
                .bind(&*fl, "left-mouse-button-mode")
                .build();
            general_options
                .middle_mouse_button_mode
                .bind(&*fl, "middle-mouse-button-mode")
                .build();
            general_options
                .right_mouse_button_mode
                .bind(&*fl, "right-mouse-button-mode")
                .build();
            general_options
                .left_mouse_button_unselects
                .bind(&*fl, "left-mouse-button-unselects")
                .build();
            general_options
                .select_dirs
                .bind(&*fl, "select-dirs")
                .build();
            general_options
                .quick_search_shortcut
                .bind(&*fl, "quick-search-shortcut")
                .build();

            let confirm_options = ConfirmOptions::new();
            confirm_options.dnd_mode.bind(&*fl, "dnd-mode").build();

            for (column, key) in fl
                .view()
                .columns()
                .iter::<gtk::ColumnViewColumn>()
                .flatten()
                .zip(&general_options.list_column_width)
            {
                key.bind(&column, "fixed-width").build();
            }

            let color_options = ColorOptions::new();
            color_options
                .use_ls_colors
                .bind(&*fl, "use-ls-colors")
                .build();
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
            if let Some(directory) = self.directory.take() {
                for handler_id in self.directory_handlers.take() {
                    directory.disconnect(handler_id);
                }
            }
            if let Some(connection) = self.connection.take() {
                for handler_id in self.connection_handlers.take() {
                    connection.disconnect(handler_id);
                }
            }
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    // The file list widget was clicked
                    glib::subclass::Signal::builder("list-clicked")
                        .param_types([u32::static_type(), Option::<File>::static_type()])
                        .build(),
                    // The visible content of the file list has changed (files have been: selected, created, deleted or modified)
                    glib::subclass::Signal::builder("files-changed").build(),
                    // The current directory has been changed
                    glib::subclass::Signal::builder("dir-changed")
                        .param_types([Directory::static_type()])
                        .build(),
                    // The current connection has been changed
                    glib::subclass::Signal::builder("con-changed")
                        .param_types([Connection::static_type()])
                        .build(),
                    // A file in the list has been activated for opening
                    glib::subclass::Signal::builder("file-activated")
                        .param_types([File::static_type()])
                        .build(),
                    // A text to be added to a command line
                    glib::subclass::Signal::builder("cmdline-append")
                        .param_types([String::static_type()])
                        .build(),
                    // Execute a command in a command line
                    glib::subclass::Signal::builder("cmdline-execute")
                        .return_type::<bool>()
                        .build(),
                ]
            })
        }
    }

    impl WidgetImpl for FileList {
        fn grab_focus(&self) -> bool {
            self.view.grab_focus()
        }
    }

    impl FileList {
        pub fn set_connection(&self, connection: &Connection) {
            if Some(connection) == self.connection.borrow().as_ref() {
                return;
            }

            let previous_connection = self.connection.replace(Some(connection.clone()));

            if let Some(previous_connection) = previous_connection {
                for handler_id in self.connection_handlers.take() {
                    previous_connection.disconnect(handler_id);
                }
            }

            self.connection_handlers
                .borrow_mut()
                .push(connection.connect_closure(
                    "close",
                    false,
                    glib::closure_local!(
                        #[weak(rename_to = imp)]
                        self,
                        move |c: &Connection| {
                            if Some(c) == imp.connection.borrow().as_ref() {
                                imp.obj()
                                    .set_connection(&ConnectionList::get().home(), None);
                            }
                        }
                    ),
                ));

            self.obj().emit_by_name::<()>("con-changed", &[&connection]);
        }

        pub fn set_directory(&self, directory: &Directory) {
            if Some(directory) == self.directory.borrow().as_ref() {
                return;
            }

            let previous_directory = self.directory.replace(Some(directory.clone()));
            if let Some(previous_directory) = previous_directory {
                previous_directory.cancel_monitoring();
                for handler_id in self.directory_handlers.take() {
                    previous_directory.disconnect(handler_id);
                }
                if previous_directory.upcast_ref::<File>().is_local()
                    && !previous_directory.is_monitored()
                    && previous_directory.needs_mtime_update()
                {
                    previous_directory.update_mtime();
                }
            }

            self.directory_handlers
                .borrow_mut()
                .push(directory.connect_closure(
                    "list-ok",
                    false,
                    glib::closure_local!(
                        #[weak(rename_to = imp)]
                        self,
                        move |d: &Directory| imp.on_dir_list_ok(d)
                    ),
                ));
            self.directory_handlers
                .borrow_mut()
                .push(directory.connect_closure(
                    "list-failed",
                    false,
                    glib::closure_local!(
                        #[weak(rename_to = imp)]
                        self,
                        move |d: &Directory, e: &glib::Error| imp.on_dir_list_failed(d, e)
                    ),
                ));
            self.directory_handlers
                .borrow_mut()
                .push(directory.connect_closure(
                    "dir-deleted",
                    false,
                    glib::closure_local!(
                        #[weak(rename_to = imp)]
                        self,
                        move |d: &Directory| imp.on_directory_deleted(d)
                    ),
                ));
            self.directory_handlers
                .borrow_mut()
                .push(directory.connect_closure(
                    "file-created",
                    false,
                    glib::closure_local!(
                        #[weak(rename_to = imp)]
                        self,
                        move |_: &Directory, f: &File| imp.on_dir_file_created(f)
                    ),
                ));
            self.directory_handlers
                .borrow_mut()
                .push(directory.connect_closure(
                    "file-deleted",
                    false,
                    glib::closure_local!(
                        #[weak(rename_to = imp)]
                        self,
                        move |d: &Directory, f: &File| imp.on_dir_file_deleted(d, f)
                    ),
                ));
            self.directory_handlers
                .borrow_mut()
                .push(directory.connect_closure(
                    "file-changed",
                    false,
                    glib::closure_local!(
                        #[weak(rename_to = imp)]
                        self,
                        move |_: &Directory, f: &File| imp.on_dir_file_changed(f)
                    ),
                ));
            self.directory_handlers
                .borrow_mut()
                .push(directory.connect_closure(
                    "file-renamed",
                    false,
                    glib::closure_local!(
                        #[weak(rename_to = imp)]
                        self,
                        move |_: &Directory, f: &File| imp.on_dir_file_renamed(f)
                    ),
                ));

            directory.start_monitoring();

            self.obj().emit_by_name::<()>("dir-changed", &[directory]);
        }

        fn on_dir_list_ok(&self, dir: &Directory) {
            debug!('l', "on_dir_list_ok");
            self.obj().emit_by_name::<()>("dir-changed", &[dir]);
        }

        fn on_dir_list_failed(&self, _dir: &Directory, error: &glib::Error) {
            debug!('l', "on_dir_list_failed: {}", error);
        }

        fn on_directory_deleted(&self, dir: &Directory) {
            if let Some(parent_dir) = dir.existing_parent() {
                self.obj().goto_directory(&parent_dir.path().path());
            } else {
                self.obj().goto_directory(&Path::new("~"));
            }
        }

        fn on_dir_file_created(&self, f: &File) {
            if self.insert_file(&*f) {
                self.obj().emit_files_changed();
            }
        }

        fn on_dir_file_deleted(&self, dir: &Directory, f: &File) {
            if self.directory.borrow().as_ref() == Some(dir) {
                if self.obj().remove_file(&*f) {
                    self.obj().emit_files_changed();
                }
            }
        }

        fn on_dir_file_changed(&self, f: &File) {
            if self.obj().has_file(&*f) {
                self.update_file(&*f);
                self.obj().emit_files_changed();
            }
        }

        fn on_dir_file_renamed(&self, f: &File) {
            if let Some(item) = self.obj().get_row_from_file(&*f) {
                item.update();
            }
        }

        pub fn items_iter(&self) -> impl Iterator<Item = FileListItem> + use<'_> {
            self.selection
                .iter::<glib::Object>()
                .flatten()
                .filter_map(|o| o.downcast::<FileListItem>().ok())
        }

        pub fn item_at(&self, position: u32) -> Option<FileListItem> {
            self.selection.item(position).and_downcast::<FileListItem>()
        }

        pub fn add_file(&self, f: &File) {
            let item = FileListItem::new(f);
            self.store.append(&item);

            // If we have been waiting for this file to show up, focus it
            if self.focus_later.borrow().as_ref() == Some(&f.file_info().name()) {
                self.focus_later.replace(None);
                self.obj().focus_file_at_row(&item);
            }
        }

        pub fn insert_file(&self, f: &File) -> bool {
            let options = FiltersOptions::new();
            if file_is_wanted(f, &options) {
                self.add_file(f);
                true
            } else {
                false
            }
        }

        pub fn update_file(&self, f: &File) {
            if f.needs_update() {
                if let Some(item) = self.obj().get_row_from_file(f) {
                    item.update();
                }
            }
        }

        fn cursor_changed(&self) {
            let Some(cursor) = self.obj().focused_file_position() else {
                return;
            };
            let shift_down_key = self.shift_down_key.take();
            let Some(shift_down_row) = self.shift_down_row.get() else {
                return;
            };

            match shift_down_key {
                Some(gdk::Key::Page_Up) => {
                    self.toggle_file_range(shift_down_row, cursor, false);
                }
                Some(gdk::Key::Page_Down) => {
                    self.toggle_file_range(shift_down_row, cursor, false);
                }
                Some(gdk::Key::Home) => {
                    self.toggle_file_range(shift_down_row, cursor, true);
                }
                Some(gdk::Key::End) => {
                    self.toggle_file_range(shift_down_row, cursor, true);
                }
                _ => {}
            }
        }

        fn toggle_file_range(&self, start_row: u32, end_row: u32, closed_end: bool) {
            let start = u32::min(start_row, end_row);
            let end = u32::max(start_row, end_row);
            let range = if closed_end {
                start..(end + 1)
            } else {
                start..end
            };
            for position in range {
                self.toggle_file(position);
            }
        }

        fn select_file_range(&self, start_row: u32, end_row: u32) {
            let start = u32::min(start_row, end_row);
            let end = u32::max(start_row, end_row);
            for position in start..=end {
                if let Some(item) = self.item_at(position).filter(|f| !f.file().is_dotdot()) {
                    item.set_selected(true);
                }
            }
            self.obj().emit_files_changed();
        }

        pub fn select_with_mouse(&self, positions: u32) {
            if let Some(start_row) = self.shift_down_row.get() {
                self.select_file_range(start_row, positions);
                self.shift_down.set(false);
                self.shift_down_key.set(None);
            }
        }

        fn key_pressed(&self, key: gdk::Key, state: gdk::ModifierType) -> glib::Propagation {
            match (state, key) {
                (ALT, gdk::Key::Return | gdk::Key::KP_Enter) => {
                    let _ = self.obj().activate_action("win.file-properties", None);
                    return glib::Propagation::Stop;
                }
                (ALT, gdk::Key::KP_Add) => {
                    self.obj().toggle_files_with_same_extension(true);
                    return glib::Propagation::Stop;
                }
                (ALT, gdk::Key::KP_Subtract) => {
                    self.obj().toggle_files_with_same_extension(false);
                    return glib::Propagation::Stop;
                }
                (SHIFT, gdk::Key::F6) => {
                    let this = self.obj().clone();
                    glib::spawn_future_local(async move {
                        this.show_rename_dialog().await;
                    });
                    return glib::Propagation::Stop;
                }
                (SHIFT, gdk::Key::F10) => {
                    self.obj().show_file_popup(None);
                    return glib::Propagation::Stop;
                }
                (
                    SHIFT,
                    gdk::Key::Left | gdk::Key::KP_Left | gdk::Key::Right | gdk::Key::KP_Right,
                ) => {
                    return glib::Propagation::Proceed;
                }
                (SHIFT, gdk::Key::Page_Up | gdk::Key::KP_Page_Up | gdk::Key::KP_9) => {
                    self.shift_down_key.set(Some(gdk::Key::Page_Up));
                    self.shift_down_row
                        .replace(self.obj().focused_file_position());
                    return glib::Propagation::Proceed;
                }
                (SHIFT, gdk::Key::Page_Down | gdk::Key::KP_Page_Down | gdk::Key::KP_3) => {
                    self.shift_down_key.set(Some(gdk::Key::Page_Down));
                    self.shift_down_row
                        .replace(self.obj().focused_file_position());
                    return glib::Propagation::Proceed;
                }
                (
                    SHIFT,
                    gdk::Key::Up
                    | gdk::Key::KP_Up
                    | gdk::Key::KP_8
                    | gdk::Key::Down
                    | gdk::Key::KP_Down
                    | gdk::Key::KP_2,
                ) => {
                    self.shift_down_key.set(Some(gdk::Key::Up));
                    if let Some(focused) = self.obj().focused_file_position() {
                        self.toggle_file(focused);
                    }
                    return glib::Propagation::Proceed;
                }
                (SHIFT, gdk::Key::Home | gdk::Key::KP_Home | gdk::Key::KP_7) => {
                    self.shift_down_key.set(Some(gdk::Key::Home));
                    self.shift_down_row
                        .replace(self.obj().focused_file_position());
                    return glib::Propagation::Proceed;
                }
                (SHIFT, gdk::Key::End | gdk::Key::KP_End | gdk::Key::KP_1) => {
                    self.shift_down_key.set(Some(gdk::Key::End));
                    self.shift_down_row
                        .replace(self.obj().focused_file_position());
                    return glib::Propagation::Proceed;
                }
                (SHIFT, gdk::Key::Delete | gdk::Key::KP_Delete) => {
                    let this = self.obj().clone();
                    glib::spawn_future_local(async move {
                        this.show_delete_dialog(true).await;
                    });
                    return glib::Propagation::Stop;
                }
                (ALT_SHIFT, gdk::Key::Return | gdk::Key::KP_Enter) => {
                    self.obj().show_visible_tree_sizes();
                    return glib::Propagation::Stop;
                }
                (CONTROL, gdk::Key::F3) => {
                    self.obj().sort_by(ColumnID::COLUMN_NAME);
                    return glib::Propagation::Stop;
                }
                (CONTROL, gdk::Key::F4) => {
                    self.obj().sort_by(ColumnID::COLUMN_EXT);
                    return glib::Propagation::Stop;
                }
                (CONTROL, gdk::Key::F5) => {
                    self.obj().sort_by(ColumnID::COLUMN_DATE);
                    return glib::Propagation::Stop;
                }
                (CONTROL, gdk::Key::F6) => {
                    self.obj().sort_by(ColumnID::COLUMN_SIZE);
                    return glib::Propagation::Stop;
                }
                (CONTROL, gdk::Key::P | gdk::Key::p) => {
                    self.add_cwd_to_cmdline();
                    return glib::Propagation::Stop;
                }
                (CONTROL, gdk::Key::Return | gdk::Key::KP_Enter) => {
                    self.add_file_to_cmdline(false);
                    return glib::Propagation::Stop;
                }
                (CONTROL_SHIFT, gdk::Key::Return | gdk::Key::KP_Enter) => {
                    self.add_file_to_cmdline(true);
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::Return | gdk::Key::KP_Enter) => {
                    let event_processed = self.obj().emit_by_name::<bool>("cmdline-execute", &[]);
                    if !event_processed {
                        if let Some(file) = self.obj().focused_file() {
                            self.obj().emit_by_name::<()>("file-activated", &[&file]);
                        }
                    }
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::space) => {
                    self.obj()
                        .set_cursor(gdk::Cursor::from_name("wait", None).as_ref());

                    self.obj().toggle();
                    if let Some(position) = self.obj().focused_file_position() {
                        self.obj().show_dir_tree_size(position);
                    }
                    self.obj().emit_files_changed();

                    self.obj().set_cursor(None);
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::KP_Add | gdk::Key::plus | gdk::Key::equal) => {
                    let this = self.obj().clone();
                    glib::spawn_future_local(async move {
                        select_by_pattern(&this, true).await;
                    });
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::KP_Subtract | gdk::Key::minus) => {
                    let this = self.obj().clone();
                    glib::spawn_future_local(async move {
                        select_by_pattern(&this, false).await;
                    });
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::KP_Multiply) => {
                    self.obj().invert_selection();
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::KP_Divide) => {
                    self.obj().restore_selection();
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::Insert | gdk::Key::KP_Insert) => {
                    self.obj().toggle();
                    if let Some(position) = self.obj().focused_file_position() {
                        if position + 1 < self.selection.n_items() {
                            self.obj().select_row(position + 1);
                        }
                    }
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::Delete | gdk::Key::KP_Delete) => {
                    let this = self.obj().clone();
                    glib::spawn_future_local(async move {
                        this.show_delete_dialog(false).await;
                    });
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::Shift_L | gdk::Key::Shift_R) => {
                    if !self.shift_down.get() {
                        self.shift_down_row
                            .replace(self.obj().focused_file_position());
                    }
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::Menu) => {
                    self.obj().show_file_popup(None);
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::F3) => {
                    let _ = self.obj().activate_action("fl.file-view", None);
                    return glib::Propagation::Stop;
                }
                (NO_MOD, gdk::Key::F4) => {
                    let _ = self.obj().activate_action("fl.file-edit", None);
                    return glib::Propagation::Stop;
                }
                _ => {}
            }

            if is_quicksearch_starting_character(key) {
                if is_quicksearch_starting_modifier(self.quick_search_shortcut.get(), state) {
                    self.obj().show_quick_search(Some(key));
                } else if let Some(text) = key.to_unicode().map(|c| c.to_string()) {
                    self.obj().emit_by_name::<()>("cmdline-append", &[&text]);
                }
                return glib::Propagation::Stop;
            }
            glib::Propagation::Proceed
        }

        fn get_modifiers_state(&self) -> Option<gdk::ModifierType> {
            self.obj()
                .root()
                .and_downcast::<gtk::Window>()
                .as_ref()
                .and_then(get_modifiers_state)
        }

        fn on_button_press(&self, button: u32, n_press: i32, x: f64, y: f64) {
            let cell = self.row_at_coords(x, y);

            self.obj().emit_by_name::<()>(
                "list-clicked",
                &[&button, &cell.as_ref().map(|f| f.1.file())],
            );

            if let Some((_, item)) = cell.as_ref() {
                let state = self.get_modifiers_state();

                self.modifier_click.set(state);

                if n_press == 2
                    && button == 1
                    && self.left_mouse_button_mode.get()
                        == LeftMouseButtonMode::OpensWithDoubleClick
                {
                    self.obj()
                        .emit_by_name::<()>("file-activated", &[&item.file()]);
                } else if n_press == 1 && button == 1 {
                    if state == Some(SHIFT) {
                        if let Some(position) = self
                            .items_iter()
                            .position(|i| &i == item)
                            .and_then(|p| p.try_into().ok())
                        {
                            self.select_with_mouse(position);
                        }
                    } else if state == Some(CONTROL) {
                        item.toggle_selected();
                    }
                    if state == Some(NO_MOD) {
                        if !item.selected() && self.left_mouse_button_unselects.get() {
                            self.obj().unselect_all();
                        }
                    }
                } else if n_press == 1 && button == 3 {
                    if !item.file().is_dotdot() {
                        if self.right_mouse_button_mode.get() == RightMouseButtonMode::Selects {
                            if self
                                .selection
                                .selected_item()
                                .map_or(false, |focused| &focused == item)
                            {
                                item.set_selected(true);
                                self.obj().emit_files_changed();
                                self.obj().show_file_popup(None);
                            } else {
                                item.toggle_selected();
                                self.obj().emit_files_changed();
                            }
                        } else {
                            let this = self.obj().clone();
                            glib::timeout_add_local_once(Duration::from_millis(1), move || {
                                this.show_file_popup(Some(&gdk::Rectangle::new(
                                    x as i32, y as i32, 0, 0,
                                )));
                            });
                        }
                    }
                }
            } else if n_press == 1 && button == 3 {
                self.show_list_popup(x, y);
            }
        }

        fn on_button_release(&self, button: u32, n_press: i32, x: f64, y: f64) {
            let Some((_, item)) = self.row_at_coords(x, y) else {
                return;
            };
            if n_press == 1
                && button == 1
                && self.modifier_click.get() == Some(NO_MOD)
                && self.left_mouse_button_mode.get() == LeftMouseButtonMode::OpensWithSingleClick
            {
                self.obj()
                    .emit_by_name::<()>("file-activated", &[&item.file()]);
            }
        }

        fn row_at_coords(&self, x: f64, y: f64) -> Option<(ColumnID, FileListItem)> {
            let mut widget = self.view.pick(x, y, gtk::PickFlags::DEFAULT)?;
            let cell = loop {
                match widget.downcast_ref::<FileListCell>() {
                    Some(cell) => break Some(cell.clone()),
                    None => match widget.parent() {
                        Some(parent) => widget = parent,
                        None => break None,
                    },
                }
            }?;
            for (column_id, cells_map) in &self.cells_map {
                if let Some(item) = cells_map.find_by_value(&cell) {
                    return Some((*column_id, item.clone()));
                }
            }
            None
        }

        fn show_list_popup(&self, x: f64, y: f64) {
            let menu = list_popup_menu();
            let popover = gtk::PopoverMenu::from_model(Some(&menu));
            popover.set_parent(&*self.obj());
            popover.set_position(gtk::PositionType::Bottom);
            popover.set_pointing_to(Some(&gdk::Rectangle::new(x as i32, y as i32, 0, 0)));
            popover.present();
            popover.popup();
        }

        pub fn toggle_file(&self, position: u32) {
            if position == gtk::INVALID_LIST_POSITION {
                return;
            }
            if let Some(item) = self.item_at(position).filter(|i| !i.file().is_dotdot()) {
                item.toggle_selected();
                self.selection.items_changed(position, 1, 1);
                self.obj().emit_files_changed();
            }
        }

        fn add_to_cmdline(&self, value: impl AsRef<OsStr>) {
            if let Some(text) = glib::shell_quote(value).to_str() {
                self.obj().emit_by_name::<()>("cmdline-append", &[&text]);
            }
        }

        fn add_cwd_to_cmdline(&self) {
            if let Some(path) = self
                .obj()
                .directory()
                .and_upcast::<File>()
                .and_then(|d| d.get_real_path())
            {
                self.add_to_cmdline(path);
            }
        }

        fn add_file_to_cmdline(&self, fullpath: bool) {
            if let Some(file) = self.obj().selected_file() {
                if fullpath {
                    if let Some(path) = file.get_real_path() {
                        self.add_to_cmdline(path);
                    }
                } else {
                    self.add_to_cmdline(file.get_name());
                }
            }
        }

        fn drag_prepare(&self, _x: f64, _y: f64) -> Option<gdk::ContentProvider> {
            let files = self
                .obj()
                .selected_files()
                .into_iter()
                .map(|f| f.get_uri_str())
                .collect::<Vec<_>>();
            if files.is_empty() {
                return None;
            }

            let bytes = glib::Bytes::from_owned(files.join("\r\n"));

            Some(gdk::ContentProvider::for_bytes("text/uri-list", &bytes))
        }

        fn drag_end(&self, _drag: &gdk::Drag, delete: bool) {
            if delete {
                for f in self.obj().selected_files() {
                    self.obj().remove_file(&f);
                }
            }
        }

        fn drop(&self, value: &glib::Value, x: f64, y: f64) -> bool {
            let Ok(data) = value.get::<String>() else {
                return false;
            };

            let row = self.row_at_coords(x, y);
            let file = row.map(|(_, item)| item.file());

            let destination = if file.as_ref().is_some_and(|f| f.is_dotdot()) {
                DroppingFilesDestination::Parent
            } else if let Some(dir) = file.and_downcast::<Directory>() {
                DroppingFilesDestination::Child(dir.file_info().name())
            } else {
                DroppingFilesDestination::This
            };

            // transform the drag data to a list with `gio::File`s
            let files: Vec<_> = data
                .split("\r\n")
                .filter(|s| !s.is_empty())
                .map(|s| s.to_owned())
                // .map(|uri| gio::File::for_uri(uri))
                .collect();

            let mask = self.get_modifiers_state();
            let shift_or_control = mask
                .map(|m| m.contains(SHIFT) || m.contains(CONTROL))
                .unwrap_or_default();

            match (self.dnd_mode.get(), shift_or_control) {
                (DndMode::Query, _) | (_, true) => {
                    let menu = create_dnd_popup(&files, &destination);

                    let dnd_popover = gtk::PopoverMenu::from_model(Some(&menu));
                    dnd_popover.set_parent(&*self.obj());
                    dnd_popover
                        .set_pointing_to(Some(&gdk::Rectangle::new(x as i32, y as i32, 1, 1)));
                    dnd_popover.present();
                    dnd_popover.popup();
                }
                (DndMode::Copy, false) => {
                    let this = self.obj().clone();
                    glib::spawn_future_local(async move {
                        this.imp()
                            .drop_files(DroppingFiles {
                                transfer_type: GnomeCmdTransferType::COPY,
                                files,
                                destination,
                            })
                            .await;
                    });
                }
                (DndMode::Move, false) => {
                    let this = self.obj().clone();
                    glib::spawn_future_local(async move {
                        this.imp()
                            .drop_files(DroppingFiles {
                                transfer_type: GnomeCmdTransferType::MOVE,
                                files,
                                destination,
                            })
                            .await;
                    });
                }
            }

            true
        }

        async fn drop_files(&self, data: DroppingFiles) {
            let Some(window) = self.obj().root().and_downcast::<gtk::Window>() else {
                return;
            };

            let files: Vec<gio::File> = data
                .files
                .iter()
                .map(|uri| gio::File::for_uri(uri))
                .collect();

            let to = match data.destination {
                DroppingFilesDestination::Parent => self.obj().directory().and_then(|d| d.parent()),
                DroppingFilesDestination::Child(name) => self
                    .obj()
                    .find_file_by_name(&name)
                    .and_then(|position| self.item_at(position))
                    .map(|item| item.file())
                    .and_downcast::<Directory>(),
                DroppingFilesDestination::This => self.obj().directory(),
            };
            let Some(dir) = to else { return };

            let _result = match data.transfer_type {
                GnomeCmdTransferType::COPY => {
                    copy_files(
                        window.clone(),
                        files,
                        dir.clone(),
                        None,
                        gio::FileCopyFlags::NONE,
                        ConfirmOverwriteMode::Query,
                    )
                    .await
                }
                GnomeCmdTransferType::MOVE => {
                    move_files(
                        window.clone(),
                        files,
                        dir.clone(),
                        None,
                        gio::FileCopyFlags::NONE,
                        ConfirmOverwriteMode::Query,
                    )
                    .await
                }
                GnomeCmdTransferType::LINK => {
                    link_files(
                        window.clone(),
                        files,
                        dir.clone(),
                        None,
                        gio::FileCopyFlags::NONE,
                        ConfirmOverwriteMode::Query,
                    )
                    .await
                }
            };

            if let Err(error) = dir.relist_files(&window, false).await {
                error.show(&window).await;
            }
            // main_win.focus_file_lists();
        }
    }

    fn is_quicksearch_starting_character(key: gdk::Key) -> bool {
        (key >= gdk::Key::A && key <= gdk::Key::Z)
            || (key >= gdk::Key::a && key <= gdk::Key::z)
            || (key >= gdk::Key::_0 && key <= gdk::Key::_9)
            || key == gdk::Key::period
            || key == gdk::Key::question
            || key == gdk::Key::asterisk
            || key == gdk::Key::bracketleft
    }

    fn is_quicksearch_starting_modifier(
        quick_search: QuickSearchShortcut,
        state: gdk::ModifierType,
    ) -> bool {
        match quick_search {
            QuickSearchShortcut::CtrlAlt => state == CONTROL_ALT,
            QuickSearchShortcut::Alt => state == ALT,
            QuickSearchShortcut::JustACharacter => state == NO_MOD,
        }
    }

    pub fn type_string(file_type: gio::FileType) -> &'static str {
        match file_type {
            gio::FileType::Regular => " ",
            gio::FileType::Directory => "/",
            gio::FileType::SymbolicLink => "@",
            gio::FileType::Special => "S",
            gio::FileType::Shortcut => "K",
            gio::FileType::Mountable => "M",
            _ => "?",
        }
    }

    pub fn display_permissions(permissions: u32, mode: PermissionDisplayMode) -> String {
        match mode {
            PermissionDisplayMode::Number => permissions_to_numbers(permissions),
            PermissionDisplayMode::Text => permissions_to_text(permissions),
        }
    }

    #[derive(glib::Variant)]
    pub struct DroppingFiles {
        pub transfer_type: GnomeCmdTransferType,
        pub files: Vec<String>,
        pub destination: DroppingFilesDestination,
    }

    #[derive(Clone, glib::Variant)]
    pub enum DroppingFilesDestination {
        This,
        Parent,
        Child(PathBuf),
    }

    fn create_dnd_popup(files: &[String], destination: &DroppingFilesDestination) -> gio::Menu {
        let menu = gio::Menu::new();
        let section = gio::Menu::new();

        let item = gio::MenuItem::new(Some(&gettext("_Copy here")), None);
        item.set_action_and_target_value(
            Some("fl.drop-files"),
            Some(
                &DroppingFiles {
                    transfer_type: GnomeCmdTransferType::COPY,
                    files: files.to_vec(),
                    destination: destination.to_owned(),
                }
                .to_variant(),
            ),
        );
        section.append_item(&item);

        let item = gio::MenuItem::new(Some(&gettext("_Move here")), None);
        item.set_action_and_target_value(
            Some("fl.drop-files"),
            Some(
                &DroppingFiles {
                    transfer_type: GnomeCmdTransferType::MOVE,
                    files: files.to_vec(),
                    destination: destination.to_owned(),
                }
                .to_variant(),
            ),
        );
        section.append_item(&item);

        let item = gio::MenuItem::new(Some(&gettext("_Link here")), None);
        item.set_action_and_target_value(
            Some("fl.drop-files"),
            Some(
                &DroppingFiles {
                    transfer_type: GnomeCmdTransferType::LINK,
                    files: files.to_vec(),
                    destination: destination.to_owned(),
                }
                .to_variant(),
            ),
        );
        section.append_item(&item);

        menu.append_section(None, &section);
        menu.append(Some(&gettext("C_ancel")), Some("fl.drop-files-cancel"));
        menu
    }
}

glib::wrapper! {
    pub struct FileList(ObjectSubclass<imp::FileList>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

#[allow(non_camel_case_types)]
#[derive(Clone, Copy, PartialOrd, Ord, PartialEq, Eq, strum::FromRepr, strum::VariantArray)]
#[repr(C)]
pub enum ColumnID {
    COLUMN_ICON = 0,
    COLUMN_NAME,
    COLUMN_EXT,
    COLUMN_DIR,
    COLUMN_SIZE,
    COLUMN_DATE,
    COLUMN_PERM,
    COLUMN_OWNER,
    COLUMN_GROUP,
}

impl ColumnID {
    pub fn title(self) -> Option<String> {
        match self {
            Self::COLUMN_ICON => None,
            Self::COLUMN_NAME => Some(gettext("name")),
            Self::COLUMN_EXT => Some(gettext("ext")),
            Self::COLUMN_DIR => Some(gettext("dir")),
            Self::COLUMN_SIZE => Some(gettext("size")),
            Self::COLUMN_DATE => Some(gettext("date")),
            Self::COLUMN_PERM => Some(gettext("perm")),
            Self::COLUMN_OWNER => Some(gettext("uid")),
            Self::COLUMN_GROUP => Some(gettext("gid")),
        }
    }

    pub fn xalign(self) -> f32 {
        match self {
            Self::COLUMN_ICON => 0.5,
            Self::COLUMN_SIZE => 1.0,
            _ => 0.0,
        }
    }

    pub fn default_sort_direction(self) -> gtk::SortType {
        match self {
            Self::COLUMN_ICON => gtk::SortType::Ascending,
            Self::COLUMN_NAME => gtk::SortType::Ascending,
            Self::COLUMN_EXT => gtk::SortType::Ascending,
            Self::COLUMN_DIR => gtk::SortType::Ascending,
            Self::COLUMN_SIZE => gtk::SortType::Descending,
            Self::COLUMN_DATE => gtk::SortType::Descending,
            Self::COLUMN_PERM => gtk::SortType::Ascending,
            Self::COLUMN_OWNER => gtk::SortType::Ascending,
            Self::COLUMN_GROUP => gtk::SortType::Ascending,
        }
    }
}

impl FileList {
    pub fn new(file_metadata_service: &FileMetadataService) -> Self {
        glib::Object::builder()
            .property("file-metadata-service", file_metadata_service)
            .build()
    }

    pub fn size(&self) -> usize {
        self.imp()
            .selection
            .n_items()
            .try_into()
            .ok()
            .unwrap_or_default()
    }

    pub fn show_dir_tree_size(&self, position: u32) {
        if let Some(item) = self.imp().item_at(position) {
            item.set_size(
                item.file()
                    .tree_size()
                    .and_then(|s| s.try_into().ok())
                    .unwrap_or(-1),
            );
        }
    }

    pub fn show_visible_tree_sizes(&self) {
        for item in self.imp().items_iter() {
            item.set_size(
                item.file()
                    .tree_size()
                    .and_then(|s| s.try_into().ok())
                    .unwrap_or(-1),
            );
        }
        self.emit_files_changed();
    }

    fn remove_file(&self, f: &File) -> bool {
        let Some(store_position) = self
            .imp()
            .store
            .iter::<FileListItem>()
            .flatten()
            .position(|item| item.file() == *f)
        else {
            return false;
        };
        let view_position = self.imp().items_iter().position(|item| item.file() == *f);

        self.imp().store.remove(store_position as u32);
        if let Some(view_position) = view_position {
            self.select_row(view_position as u32);
        }
        true
    }

    pub fn clear(&self) {
        self.imp().store.remove_all();
    }

    pub fn visible_files(&self) -> glib::List<File> {
        self.imp().items_iter().map(|item| item.file()).collect()
    }

    pub fn selected_files(&self) -> glib::List<File> {
        let mut list: glib::List<File> = self
            .imp()
            .items_iter()
            .filter(|item| item.selected())
            .map(|item| item.file())
            .collect();
        if list.is_empty() {
            if let Some(file) = self.selected_file() {
                list.push_back(file);
            }
        }
        list
    }

    pub fn sorting(&self) -> (ColumnID, gtk::SortType) {
        let cv_sorter = self
            .view()
            .sorter()
            .and_downcast::<gtk::ColumnViewSorter>()
            .unwrap();
        let col = self
            .imp()
            .columns
            .borrow()
            .iter()
            .position(|c| Some(c) == cv_sorter.primary_sort_column().as_ref())
            .and_then(ColumnID::from_repr)
            .unwrap_or(ColumnID::COLUMN_NAME);
        (col, cv_sorter.primary_sort_order())
    }

    pub fn set_sorting(&self, column: ColumnID, order: gtk::SortType) {
        if let Some(column) = self.imp().columns.borrow().get(column as usize) {
            self.view().sort_by_column(Some(column), order);
        }
    }

    pub fn show_column(&self, col: ColumnID, value: bool) {
        if let Some(column) = self.imp().columns.borrow().get(col as usize) {
            column.set_visible(value);
        }
    }

    pub fn has_file(&self, f: &File) -> bool {
        self.imp().items_iter().any(|item| item.file() == *f)
    }

    fn focused_file_position(&self) -> Option<u32> {
        Some(self.imp().selection.selected()).filter(|p| *p != gtk::INVALID_LIST_POSITION)
    }

    pub fn focused_file(&self) -> Option<File> {
        let position = self.focused_file_position()?;
        let file = self.imp().item_at(position)?.file();
        Some(file)
    }

    pub fn selected_file(&self) -> Option<File> {
        self.focused_file().filter(|f| !f.is_dotdot())
    }

    pub fn toggle(&self) {
        if let Some(position) = self.focused_file_position() {
            self.imp().toggle_file(position);
        }
    }

    pub fn toggle_and_step(&self) {
        if let Some(position) = self.focused_file_position() {
            self.imp().toggle_file(position);
            if position + 1 < self.imp().selection.n_items() {
                self.select_row(position + 1);
            }
        }
    }

    pub fn select_all(&self) {
        if self.select_dirs() {
            for item in self.imp().items_iter() {
                if !item.file().is_dotdot() {
                    item.set_selected(true);
                }
            }
        } else {
            for item in self.imp().items_iter() {
                let file = item.file();
                if !file.is_dotdot() && !file.is::<Directory>() {
                    item.set_selected(true);
                }
            }
        }
        self.emit_files_changed();
    }

    pub fn select_all_files(&self) {
        for item in self.imp().items_iter() {
            let file = item.file();
            if !file.is_dotdot() {
                item.set_selected(!file.is::<Directory>());
            }
        }
        self.emit_files_changed();
    }

    pub fn unselect_all_files(&self) {
        for item in self.imp().items_iter() {
            if !item.file().is::<Directory>() {
                item.set_selected(false);
            }
        }
        self.emit_files_changed();
    }

    pub fn unselect_all(&self) {
        for item in self.imp().items_iter() {
            item.set_selected(false);
        }
        self.emit_files_changed();
    }

    pub fn focus_prev(&self) {
        let position = self.imp().selection.selected();
        if position > 0 {
            self.select_row(position - 1);
        }
    }

    pub fn focus_next(&self) {
        let position = self.imp().selection.selected();
        if position + 1 < self.imp().selection.n_items() {
            self.select_row(position + 1)
        };
    }

    pub fn connection(&self) -> Option<Connection> {
        self.imp().connection.borrow().clone()
    }

    pub fn set_connection(&self, connection: &impl IsA<Connection>, start_dir: Option<&Directory>) {
        let this = self.clone();
        let connection = connection.as_ref().clone();
        let start_dir = start_dir.cloned();
        glib::spawn_future_local(async move {
            this.set_connection_async(&connection, start_dir).await;
        });
    }

    pub async fn set_connection_async(
        &self,
        connection: &impl IsA<Connection>,
        start_dir: Option<Directory>,
    ) {
        let connection = connection.as_ref();
        if self.connection().as_ref() == Some(connection) {
            let directory = if !connection.should_remember_dir() {
                connection.default_dir()
            } else {
                start_dir
            };
            if let Some(directory) = directory {
                self.set_directory_async(&directory).await;
            }
            return;
        }

        let opened = if connection.is_open() {
            true
        } else {
            if let Some(window) = self.root().and_downcast::<gtk::Window>() {
                open_connection(&window, connection).await
            } else {
                eprintln!("No window");
                false
            }
        };

        if opened {
            self.imp().set_connection(connection);
        }

        if let Some(directory) = start_dir.or_else(|| connection.default_dir()) {
            self.set_directory_async(&directory).await;
        }
    }

    pub fn directory(&self) -> Option<Directory> {
        self.imp().directory.borrow().clone()
    }

    pub fn set_directory(&self, directory: &Directory) {
        let this = self.clone();
        let directory = directory.clone();
        glib::spawn_future_local(async move {
            this.set_directory_async(&directory).await;
        });
    }

    pub async fn set_directory_async(&self, directory: &Directory) {
        if Some(directory) == self.directory().as_ref() {
            return;
        }

        let Some(window) = self.root().and_downcast::<gtk::Window>() else {
            return;
        };

        // this.set_sensitive(false);
        self.set_cursor(gdk::Cursor::from_name("wait", None).as_ref());

        let result = match directory.state() {
            DirectoryState::Empty => directory.list_files(&window, true).await,
            DirectoryState::Listing | DirectoryState::Canceling => Ok(()),
            DirectoryState::Listed => {
                // check if the dir has up-to-date file list; if not and it's a local dir - relist it
                if directory.upcast_ref::<File>().is_local()
                    && !directory.is_monitored()
                    && directory.update_mtime()
                {
                    directory.relist_files(&window, true).await
                } else {
                    Ok(())
                }
            }
        };

        self.set_cursor(None);
        // self.set_sensitive(true);

        if let Err(error) = result {
            error.show(&window).await;
            // g_timeout_add (1, (GSourceFunc) set_home_connection, fl);
            return;
        }

        self.imp().set_directory(directory);
    }

    pub async fn reload(&self) {
        let Some(directory) = self.directory() else {
            return;
        };
        let Some(window) = self.root().and_downcast::<gtk::Window>() else {
            eprintln!("No window");
            return;
        };

        self.unselect_all();
        if let Err(error) = directory.relist_files(&window, true).await {
            error.show(&window).await;
        }
    }

    pub fn append_file(&self, file: &File) {
        self.imp().add_file(file);
    }

    pub(super) fn get_row_from_file(&self, f: &File) -> Option<FileListItem> {
        for item in self.imp().items_iter() {
            if item.file() == *f {
                return Some(item);
            }
        }
        None
    }

    pub(super) fn focus_file_at_row(&self, row: &FileListItem) {
        for (pos, item) in self.imp().items_iter().enumerate() {
            if item == *row {
                self.select_row(pos as u32);
                break;
            }
        }
    }

    pub fn find_file_by_name(&self, name: &Path) -> Option<u32> {
        self.imp()
            .items_iter()
            .position(|item| item.file().file_info().name() == name)
            .and_then(|p| p.try_into().ok())
    }

    pub fn focus_file(&self, focus_file: &Path, _scroll_to_file: bool) {
        match self.find_file_by_name(focus_file) {
            Some(position) => {
                self.select_row(position);
            }
            None => {
                /* The file was not found, remember the filename in case the file gets
                added to the list in the future (after a FAM event etc). */
                self.imp().focus_later.replace(Some(focus_file.to_owned()));
            }
        }
    }

    pub fn select_row(&self, pos: u32) {
        if pos != gtk::INVALID_LIST_POSITION {
            self.imp().view.scroll_to(
                pos,
                None,
                gtk::ListScrollFlags::FOCUS | gtk::ListScrollFlags::SELECT,
                None,
            );
        }
    }

    pub fn toggle_with_pattern(&self, pattern: &Filter, mode: bool) {
        let select_dirs = self.select_dirs();
        for item in self.imp().items_iter() {
            let file = item.file();
            if !file.is_dotdot()
                && (select_dirs || file.downcast_ref::<Directory>().is_none())
                && pattern.matches(&file.file_info().display_name())
            {
                item.set_selected(mode);
            }
        }
        self.emit_files_changed();
    }

    pub fn goto_directory(&self, dir: &Path) {
        let this = self.clone();
        let dir = dir.to_path_buf();
        glib::spawn_future_local(async move {
            this.goto_directory_async(&dir).await;
        });
    }

    pub async fn goto_directory_async(&self, dir: &Path) {
        if let Err(error) = self.goto_directory_actual(dir).await {
            match self.root().and_downcast::<gtk::Window>() {
                Some(window) => error.show(&window).await,
                None => eprintln!("{error}"),
            }
        }
    }

    pub async fn goto_directory_actual(&self, dir: &Path) -> Result<(), ErrorMessage> {
        let dir = if let Ok(relative) = dir.strip_prefix("~") {
            glib::home_dir().join(relative)
        } else {
            glib::shell_unquote(dir)
                .as_ref()
                .map(|q| Path::new(q))
                .unwrap_or(dir)
                .to_path_buf()
        };

        let new_dir;
        let focus_dir;
        if dir == Path::new("..") {
            let cwd = self
                .directory()
                .ok_or_else(|| ErrorMessage::brief(gettext("Current directory is not set.")))?;

            // let's get the parent directory
            new_dir = Some(
                cwd.parent()
                    .ok_or_else(|| ErrorMessage::brief(gettext("No parent directory")))?,
            );
            focus_dir = Some(cwd);
        } else {
            focus_dir = None;
            if dir.is_absolute() {
                let connection = self
                    .connection()
                    .unwrap_or_else(|| ConnectionList::get().home().upcast());
                new_dir = Some(Directory::try_new(
                    &connection,
                    connection.create_path(&dir),
                )?);
            } else if dir.starts_with("\\\\") {
                let connection = ConnectionList::get().smb().ok_or_else(|| {
                    ErrorMessage::brief(gettext("No SAMBA connection is available"))
                })?;
                new_dir = Some(Directory::try_new(
                    &connection,
                    connection.create_path(&dir),
                )?);
            } else {
                let child_dir = self
                    .directory()
                    .ok_or_else(|| ErrorMessage::brief(gettext("Current directory is not set.")))
                    .and_then(|cwd| cwd.child(&dir))?;
                new_dir = Some(child_dir);
            }
        }

        if let Some(new_dir) = new_dir {
            self.set_directory_async(&new_dir).await;
        }

        // focus the current dir when going back to the parent dir
        if let Some(focus_dir) = focus_dir {
            self.focus_file(&focus_dir.file_info().name(), false);
        }

        Ok(())
    }

    fn emit_files_changed(&self) {
        self.emit_by_name::<()>("files-changed", &[]);
    }

    pub fn set_selected_files(&self, files: &HashSet<File>) {
        for item in self.imp().items_iter() {
            item.set_selected(files.contains(&item.file()));
        }
        self.emit_files_changed();
    }

    pub fn toggle_files_with_same_extension(&self, select: bool) {
        let Some(ext) = self.selected_file().and_then(|f| f.extension()) else {
            return;
        };
        for item in self.imp().items_iter() {
            let file = item.file();
            if !file.is_dotdot() && file.extension().as_ref() == Some(&ext) {
                item.set_selected(select);
            }
        }
        self.emit_files_changed();
    }

    pub fn invert_selection(&self) {
        let select_dirs = self.select_dirs();
        for item in self.imp().items_iter() {
            let file = item.file();
            if !file.is_dotdot() && (select_dirs || file.downcast_ref::<Directory>().is_none()) {
                item.set_selected(!item.selected());
            }
        }
        self.emit_files_changed();
    }

    pub fn restore_selection(&self) {
        // TODO: implement
    }

    pub fn stats(&self) -> FileListStats {
        let mut stats = FileListStats::default();
        for item in self.imp().items_iter() {
            let file = item.file();
            if !file.is_dotdot() {
                let info = file.file_info();
                let selected = item.selected();
                let cached_size: Option<u64> = item.size().try_into().ok();

                match info.file_type() {
                    gio::FileType::Directory => {
                        stats.total.directories += 1;
                        if selected {
                            stats.selected.directories += 1;
                        }
                        if let Some(cached_size) = cached_size {
                            stats.total.bytes += cached_size;
                            if selected {
                                stats.selected.bytes += cached_size;
                            }
                        }
                    }
                    gio::FileType::Regular => {
                        let size: u64 = info.size().try_into().unwrap_or_default();
                        stats.total.files += 1;
                        stats.total.bytes += size;
                        if selected {
                            stats.selected.files += 1;
                            stats.selected.bytes += size;
                        }
                    }
                    _ => {}
                }
            }
        }
        stats
    }

    pub fn stats_str(&self, mode: SizeDisplayMode) -> String {
        let stats = self.stats();

        let sentence1 = gettext("A total of {selected_bytes} has been selected.").replace(
            "{selected_bytes}",
            &size_to_string(stats.selected.bytes, mode),
        );

        let sentence2 = ngettext(
            "{selected_files} file selected.",
            "{selected_files} files selected.",
            stats.selected.files as u32,
        )
        .replace("{selected_files}", &stats.selected.files.to_string());

        let sentence3 = ngettext(
            "{selected_dirs} directory selected.",
            "{selected_dirs} directories selected.",
            stats.selected.directories as u32,
        )
        .replace("{selected_dirs}", &stats.selected.directories.to_string());

        format!("{sentence1} {sentence2} {sentence3}")
    }

    pub async fn show_delete_dialog(&self, force: bool) {
        let Some(window) = self.root().and_downcast::<gtk::Window>() else {
            eprintln!("No window");
            return;
        };
        let files = self.selected_files();
        if !files.is_empty() {
            let general_options = GeneralOptions::new();
            let confirm_options = ConfirmOptions::new();
            show_delete_dialog(&window, &files, force, &general_options, &confirm_options).await;
        }
    }

    pub async fn show_rename_dialog(&self) {
        let Some(window) = self.root().and_downcast::<gtk::Window>() else {
            eprintln!("No window");
            return;
        };
        let Some(file) = self.selected_file() else {
            return;
        };
        let Some(rect) = self.focus_row_coordinates() else {
            eprintln!("Selected file is not visible");
            return;
        };

        if let Some(new_name) = show_rename_popover(&file.get_name(), self, &rect).await {
            match file.rename(&new_name) {
                Ok(_) => {
                    self.focus_file(&Path::new(&new_name), true);
                    self.grab_focus();
                }
                Err(error) => {
                    ErrorMessage::with_error(
                        gettext("Cannot rename a file to {new_name}")
                            .replace("{new_name}", &new_name),
                        &error,
                    )
                    .show(&window)
                    .await;
                }
            }
            self.grab_focus();
        }
    }

    pub fn show_quick_search(&self, key: Option<gdk::Key>) {
        let popup_is_visible = self
            .imp()
            .quick_search
            .upgrade()
            .map(|popup| popup.is_visible())
            .unwrap_or_default();

        if !popup_is_visible {
            let popup = QuickSearch::new(self);
            self.imp().quick_search.set(Some(&popup));
            popup.popup();

            if let Some(initial_text) = key.and_then(|k| k.to_unicode()).map(|c| c.to_string()) {
                popup.set_text(&initial_text);
            }
        }
    }

    pub fn update_style(&self) {
        // TODO: Maybe??? gtk_cell_renderer_set_fixed_size (priv->columns[1], )
        // gtk_clist_set_row_height (*this, gnome_cmd_data.options.list_row_height);

        // let font_desc = pango::FontDescription::from_string(&self.font_name());
        // gtk_widget_override_font (*this, font_desc);

        self.queue_draw();
    }

    fn sort_by(&self, col: ColumnID) {
        if let Some(column) = self.imp().columns.borrow().get(col as usize) {
            let cv_sorter = self
                .view()
                .sorter()
                .and_downcast::<gtk::ColumnViewSorter>()
                .unwrap();
            let direction = if cv_sorter.primary_sort_column().as_ref() == Some(column) {
                match cv_sorter.primary_sort_order() {
                    gtk::SortType::Ascending => gtk::SortType::Descending,
                    _ => gtk::SortType::Ascending,
                }
            } else {
                col.default_sort_direction()
            };
            self.view().sort_by_column(Some(column), direction);
        }
    }

    pub fn show_files(&self, dir: &Directory) {
        let options = FiltersOptions::new();

        let mut items: Vec<_> = dir
            .files()
            .iter::<File>()
            .flatten()
            .filter(|f| file_is_wanted(f, &options))
            .map(|f| FileListItem::new(&f))
            .collect();
        if dir.parent().is_some() {
            items.insert(0, FileListItem::new(&File::dotdot(dir)));
        }

        self.imp().sort_model.set_model(gio::ListModel::NONE);
        let store = &self.imp().store;
        store.remove_all();
        store.splice(0, 0, &items);
        self.imp().sort_model.set_model(Some(store));

        if self.is_realized() {
            // If we have been waiting for this file to show up, focus it
            let focus_later = self.imp().focus_later.take();
            let focus_later_item = items
                .into_iter()
                .find(|item| focus_later.as_ref() == Some(&item.file().file_info().name()));

            if let Some(item) = focus_later_item {
                self.focus_file_at_row(&item);
            } else {
                self.select_row(0);
            }
        }
    }

    pub fn connect_con_changed<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &Connection) + 'static,
    {
        self.connect_closure(
            "con-changed",
            false,
            glib::closure_local!(move |this, connection| (callback)(this, connection)),
        )
    }

    pub fn connect_dir_changed<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &Directory) + 'static,
    {
        self.connect_closure(
            "dir-changed",
            false,
            glib::closure_local!(move |this, directory| (callback)(this, directory)),
        )
    }

    pub fn connect_files_changed<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self) + 'static,
    {
        self.connect_closure(
            "files-changed",
            false,
            glib::closure_local!(move |this| (callback)(this)),
        )
    }

    pub fn connect_file_activated<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &File) + 'static,
    {
        self.connect_closure(
            "file-activated",
            false,
            glib::closure_local!(move |this, file| (callback)(this, file)),
        )
    }

    pub fn connect_cmdline_append<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &str) + 'static,
    {
        self.connect_closure(
            "cmdline-append",
            false,
            glib::closure_local!(move |this, text| (callback)(this, text)),
        )
    }

    pub fn connect_cmdline_execute<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self) -> bool + 'static,
    {
        self.connect_closure(
            "cmdline-execute",
            false,
            glib::closure_local!(move |this| (callback)(this)),
        )
    }

    pub fn connect_list_clicked<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, u32, Option<File>) + 'static,
    {
        self.connect_closure(
            "list-clicked",
            false,
            glib::closure_local!(move |this, button, file| (callback)(this, button, file)),
        )
    }

    fn item_rect(&self, item: &FileListItem, column: ColumnID) -> Option<graphene::Rect> {
        self.imp()
            .cells_map
            .get(&column)?
            .find_by_key(item)
            .and_then(|c| c.compute_bounds(self))
    }

    fn focus_row_coordinates(&self) -> Option<gdk::Rectangle> {
        let selected = self
            .selection()
            .selected_item()
            .and_downcast::<FileListItem>()?;

        let name_rect = self.item_rect(&selected, ColumnID::COLUMN_NAME)?;
        let rect = if self.extension_display_mode() != ExtensionDisplayMode::Both {
            if let Some(ext_rect) = self.item_rect(&selected, ColumnID::COLUMN_EXT) {
                name_rect.union(&ext_rect)
            } else {
                name_rect
            }
        } else {
            name_rect
        };

        Some(gdk::Rectangle::new(
            rect.x() as i32,
            rect.y() as i32,
            rect.width() as i32,
            rect.height() as i32,
        ))
    }

    fn show_file_popup(&self, point_to: Option<&gdk::Rectangle>) {
        let Some(main_win) = self.root().and_downcast::<MainWindow>() else {
            return;
        };
        let Some(menu) = file_popup_menu(&main_win, self) else {
            return;
        };

        let popover = gtk::PopoverMenu::from_model(Some(&menu));
        popover.set_parent(self);
        popover.set_position(gtk::PositionType::Bottom);
        popover.set_flags(gtk::PopoverMenuFlags::NESTED);
        popover.set_pointing_to(
            point_to
                .cloned()
                .or_else(|| self.focus_row_coordinates())
                .as_ref(),
        );
        popover.present();
        popover.popup();
    }
}

#[derive(Default)]
pub struct Stats {
    pub files: u64,
    pub directories: u64,
    pub bytes: u64,
}

#[derive(Default)]
pub struct FileListStats {
    pub total: Stats,
    pub selected: Stats,
}

fn file_is_wanted(file: &File, options: &FiltersOptions) -> bool {
    match file.file().query_info(
        "standard::*",
        gio::FileQueryInfoFlags::NOFOLLOW_SYMLINKS,
        gio::Cancellable::NONE,
    ) {
        Ok(file_info) => {
            let basename = file.file().basename();
            let Some(basename) = basename.as_ref().and_then(|b| b.to_str()) else {
                return false;
            };

            if basename == "." {
                return false;
            }
            if file.is_dotdot() {
                return false;
            }
            let hide = match file_info.file_type() {
                gio::FileType::Unknown => options.hide_unknown.get(),
                gio::FileType::Regular => options.hide_regular.get(),
                gio::FileType::Directory => options.hide_directory.get(),
                gio::FileType::SymbolicLink => options.hide_symlink.get(),
                gio::FileType::Special => options.hide_special.get(),
                gio::FileType::Shortcut => options.hide_shortcut.get(),
                gio::FileType::Mountable => options.hide_mountable.get(),
                _ => false,
            };
            if hide {
                return false;
            }
            if options.hide_symlink.get() && file_info.is_symlink() {
                return false;
            }
            if options.hide_hidden.get() && file_info.is_hidden() {
                return false;
            }
            if options.hide_virtual.get()
                && file_info.boolean(gio::FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL)
            {
                return false;
            }
            if options.hide_volatile.get()
                && file_info.boolean(gio::FILE_ATTRIBUTE_STANDARD_IS_VOLATILE)
            {
                return false;
            }
            if options.hide_backup.get() && matches_pattern(basename, &options.backup_pattern.get())
            {
                return false;
            }
            true
        }
        Err(error) => {
            eprintln!(
                "file_is_wanted: retrieving file info for {} failed: {}",
                file.file().uri(),
                error.message()
            );
            true
        }
    }
}

fn matches_pattern(file: &str, patterns: &str) -> bool {
    patterns
        .split(';')
        .any(|pattern| fnmatch(pattern, file, false))
}
fn create_icon_factory() -> gtk::ListItemFactory {
    let icon_cache = icon_cache();

    let color_settings = gio::Settings::new(PREF_COLORS);

    let global_options = GeneralOptions::new();

    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, obj| {
        if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
            let stack = gtk::Stack::builder().hexpand(true).build();
            stack.add_css_class("cell");

            let image = gtk::Image::builder()
                .hexpand(true)
                .halign(gtk::Align::Center)
                .build();
            stack.add_named(&image, Some("image"));

            let label = gtk::Label::builder()
                .hexpand(true)
                .halign(gtk::Align::Center)
                .xalign(0.5)
                .build();
            stack.add_named(&label, Some("label"));

            list_item.set_child(Some(&stack));
        }
    });
    factory.connect_bind(move |_, obj| {
        let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(stack) = list_item.child().and_downcast::<gtk::Stack>() else {
            return;
        };
        let Some(item) = list_item.item().and_downcast::<FileListItem>() else {
            return;
        };
        let image = stack
            .child_by_name("image")
            .and_downcast::<gtk::Image>()
            .unwrap();
        let label = stack
            .child_by_name("label")
            .and_downcast::<gtk::Label>()
            .unwrap();

        image.clear();
        label.set_text("");

        let file = item.file();

        let use_ls_colors = color_settings.boolean("use-ls-colors");
        apply_css(&item, use_ls_colors, label.upcast_ref());

        match global_options.graphical_layout_mode.get() {
            GraphicalLayoutMode::Text => {
                label.set_text(imp::type_string(file.file_info().file_type()));
                stack.set_visible_child(&label);
            }
            mode => {
                if let Some(icon) = icon_cache.file_icon(&file, mode) {
                    image.set_from_gicon(&icon);
                }
                stack.set_visible_child(&image);
            }
        }
    });
    factory.upcast()
}

fn create_name_factory(cells: &imp::CellsMap) -> gtk::ListItemFactory {
    let global_options = GeneralOptions::new();
    create_text_cell_factory(0.0, cells, move |cell, item| {
        let prop = if matches!(
            global_options.extension_display_mode.get(),
            ExtensionDisplayMode::Stripped
        ) {
            "stem"
        } else {
            "name"
        };
        cell.bind(&item);
        cell.add_binding(
            item.bind_property(prop, &cell, "text")
                .sync_create()
                .build(),
        );
    })
}

fn create_ext_factory(cells: &imp::CellsMap) -> gtk::ListItemFactory {
    let global_options = GeneralOptions::new();
    create_text_cell_factory(0.0, cells, move |cell, item| {
        let prop = if matches!(
            global_options.extension_display_mode.get(),
            ExtensionDisplayMode::WithFileName
        ) {
            "empty"
        } else {
            "extension"
        };
        cell.bind(&item);
        cell.add_binding(
            item.bind_property(prop, &cell, "text")
                .sync_create()
                .build(),
        );
    })
}

fn create_dir_factory(cells: &imp::CellsMap) -> gtk::ListItemFactory {
    create_text_cell_factory(0.0, cells, move |cell, item| {
        cell.bind(&item);
        cell.add_binding(
            item.bind_property("directory", &cell, "text")
                .sync_create()
                .build(),
        );
    })
}

fn create_size_factory(cells: &imp::CellsMap) -> gtk::ListItemFactory {
    let global_options = GeneralOptions::new();
    create_text_cell_factory(1.0, cells, move |cell, item| {
        let mode = global_options.size_display_mode.get();
        let is_directory = item.file().file_info().file_type() == gio::FileType::Directory;
        cell.bind(&item);
        cell.add_binding(
            item.bind_property("size", &cell, "text")
                .transform_to(move |_, size: i64| {
                    if size >= 0 {
                        Some(size_to_string(size as u64, mode))
                    } else if is_directory {
                        Some(gettext("<DIR>"))
                    } else {
                        None
                    }
                })
                .sync_create()
                .build(),
        );
    })
}

fn create_date_factory(cells: &imp::CellsMap) -> gtk::ListItemFactory {
    let global_options = GeneralOptions::new();
    create_text_cell_factory(0.0, cells, move |cell, item| {
        let format = global_options.date_display_format.get();

        cell.bind(&item);
        cell.add_binding(
            item.bind_property("modification-time", &cell, "text")
                .transform_to(move |_, dt: Option<glib::DateTime>| {
                    time_to_string(dt?, &format).ok()
                })
                .sync_create()
                .build(),
        );
        cell.bind(&item);
    })
}

fn create_perm_factory(cells: &imp::CellsMap) -> gtk::ListItemFactory {
    let global_options = GeneralOptions::new();
    create_text_cell_factory(0.0, cells, move |cell, item| {
        let mode = global_options.permissions_display_mode.get();

        cell.bind(&item);
        cell.add_binding(
            item.bind_property("permissions", &cell, "text")
                .transform_to(move |_, permissions: u32| {
                    if permissions != u32::MAX {
                        Some(imp::display_permissions(permissions, mode))
                    } else {
                        None
                    }
                })
                .sync_create()
                .build(),
        );
        cell.bind(&item);
    })
}

fn create_owner_factory(cells: &imp::CellsMap) -> gtk::ListItemFactory {
    create_text_cell_factory(0.0, cells, move |cell, item| {
        cell.bind(&item);
        cell.add_binding(
            item.bind_property("owner", &cell, "text")
                .transform_to(|_, v: Option<String>| v)
                .sync_create()
                .build(),
        );
        cell.bind(&item);
    })
}

fn create_group_factory(cells: &imp::CellsMap) -> gtk::ListItemFactory {
    create_text_cell_factory(0.0, cells, move |cell, item| {
        cell.bind(&item);
        cell.add_binding(
            item.bind_property("group", &cell, "text")
                .transform_to(|_, v: Option<String>| v)
                .sync_create()
                .build(),
        );
        cell.bind(&item);
    })
}

fn create_text_cell_factory(
    alignment: f32,
    cells: &imp::CellsMap,
    bind: impl Fn(FileListCell, FileListItem) + 'static,
) -> gtk::ListItemFactory {
    let color_settings = gio::Settings::new(PREF_COLORS);

    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(move |_, obj| {
        if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
            let cell = FileListCell::default();
            cell.set_xalign(alignment);

            color_settings
                .bind("use-ls-colors", &cell, "use-ls-colors")
                .build();

            list_item.set_child(Some(&cell));
        }
    });
    factory.connect_bind(glib::clone!(
        #[strong]
        cells,
        move |_, obj| {
            let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() else {
                return;
            };
            let Some(cell) = list_item.child().and_downcast::<FileListCell>() else {
                return;
            };
            let Some(item) = list_item.item().and_downcast::<FileListItem>() else {
                return;
            };
            cells.add(&item, &cell);
            (bind)(cell, item);
        }
    ));
    factory.connect_unbind(glib::clone!(
        #[strong]
        cells,
        move |_, obj| {
            if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
                if let Some(cell) = list_item.child().and_downcast::<FileListCell>() {
                    cell.unbind();
                    cells.remove_value(&cell);
                }
            }
        }
    ));
    factory.upcast()
}

fn apply_css(item: &FileListItem, use_ls_colors: bool, widget: &gtk::Widget) {
    for c in LsPalletteColor::VARIANTS {
        widget.remove_css_class(&format!("fg-{}", c.as_ref()));
        widget.remove_css_class(&format!("bg-{}", c.as_ref()));
    }

    if item.selected() {
        widget.add_css_class("sel");
    } else {
        widget.remove_css_class("sel");
        if use_ls_colors {
            if let Some(colors) = ls_colors_get(&item.file().file_info()) {
                if let Some(class) = colors.fg.map(|fg| format!("fg-{}", fg.as_ref())) {
                    widget.add_css_class(&class);
                }
                if let Some(class) = colors.bg.map(|fg| format!("bg-{}", fg.as_ref())) {
                    widget.add_css_class(&class);
                }
            }
        }
    }
}

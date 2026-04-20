// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    cell::FileListCell,
    file_attr_sorter::FileAttrSorter,
    file_type_sorter::FileTypeSorter,
    item::FileListItem,
    popup::file_popup_menu,
    quick_search::{QuickSearch, QuickSearchMode},
};
use crate::{
    connection::{Connection, ConnectionExt, ConnectionInterface, list::ConnectionList},
    dialogs::{delete_dialog::show_delete_dialog, rename_popover::show_rename_popover},
    dir::{Directory, DirectoryState},
    file::{File, FileOps},
    filter::{Filter, fnmatch},
    history::History,
    imageloader::icon_cache,
    layout::{
        PREF_COLORS,
        ls_colors::{LsPalletteColor, ls_colors_get},
    },
    main_win::MainWindow,
    open_connection::open_connection,
    options::{ColorOptions, ConfirmOptions, FiltersOptions, GeneralOptions},
    tags::FileMetadataService,
    types::{ExtensionDisplayMode, GraphicalLayoutMode, SizeDisplayMode},
    utils::{ErrorMessage, size_to_string, time_to_string, u32_enum},
};
use gettextrs::gettext;
use gtk::{gdk, gio, glib, graphene, prelude::*, subclass::prelude::*};
use std::{
    collections::HashSet,
    ffi::OsStr,
    path::{Path, PathBuf},
};

mod imp {
    use super::*;
    use crate::{
        app::App,
        connection::list::ConnectionList,
        debug::debug,
        file_list::{
            actions::{
                Script, file_list_action_execute, file_list_action_execute_script,
                file_list_action_file_edit, file_list_action_file_view, file_list_action_open_with,
                file_list_action_open_with_default, file_list_action_open_with_other,
            },
            popup::list_popup_menu,
        },
        tags::FileMetadataService,
        transfer::{copy_files, link_files, move_files},
        types::{
            ConfirmOverwriteMode, DndMode, ExtensionDisplayMode, GnomeCmdTransferType,
            GraphicalLayoutMode, LeftMouseButtonMode, MiddleMouseButtonMode, PermissionDisplayMode,
            QuickSearchShortcut, RightMouseButtonMode,
        },
        utils::{
            ALT, CONTROL, CONTROL_ALT, ListRowSelector, NO_MOD, SHIFT, get_modifiers_state,
            permissions_to_numbers, permissions_to_text,
        },
        weak_map::WeakMap,
    };
    use std::{
        cell::{Cell, OnceCell, RefCell},
        collections::BTreeMap,
        rc::Rc,
        sync::OnceLock,
        time::Duration,
    };

    pub type CellsMap = Rc<WeakMap<FileListItem, FileListCell>>;

    #[derive(Debug, Default, Copy, Clone, PartialEq, Eq)]
    struct TextTarget {
        pub quick_search: bool,
        pub command_line: bool,
    }

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

        #[property(get, set)]
        pub extension_display_mode: Cell<ExtensionDisplayMode>,

        #[property(get, set)]
        graphical_layout_mode: Cell<GraphicalLayoutMode>,

        #[property(get, set)]
        size_display_mode: Cell<SizeDisplayMode>,

        #[property(get, set)]
        permissions_display_mode: Cell<PermissionDisplayMode>,

        #[property(get, set)]
        date_display_format: RefCell<String>,

        #[property(get, set)]
        pub use_ls_colors: Cell<bool>,

        #[property(get, set, default = true)]
        case_sensitive: Cell<bool>,

        #[property(get, set)]
        symbolic_links_as_regular_files: Cell<bool>,

        #[property(get, set)]
        left_mouse_button_mode: Cell<LeftMouseButtonMode>,

        #[property(get, set)]
        middle_mouse_button_mode: Cell<MiddleMouseButtonMode>,

        #[property(get, set)]
        right_mouse_button_mode: Cell<RightMouseButtonMode>,

        #[property(get, set, default = true)]
        left_mouse_button_unselects: Cell<bool>,

        #[property(get, set)]
        dnd_mode: Cell<DndMode>,

        #[property(get, set, default = true)]
        select_dirs: Cell<bool>,

        #[property(get, set)]
        pub(super) quick_search_shortcut: Cell<QuickSearchShortcut>,

        #[property(get)]
        pub store: gio::ListStore,

        pub sort_model: gtk::SortListModel,
        pub(super) filter_model: gtk::FilterListModel,
        pub(super) filter: RefCell<Option<Filter>>,

        #[property(get)]
        pub selection: gtk::SingleSelection,
        #[property(get)]
        pub view: gtk::ColumnView,
        pub columns: RefCell<Vec<gtk::ColumnViewColumn>>,
        pub row_selector: Rc<ListRowSelector>,

        pub cells_map: BTreeMap<ColumnID, CellsMap>,

        pub shift_down: Cell<bool>,
        pub range_selection_start: Cell<Option<u32>>,
        pub range_selection_closed: Cell<Option<bool>>,

        modifier_click: Cell<Option<gdk::ModifierType>>,

        pub focus_later: RefCell<Option<PathBuf>>,

        pub connection: RefCell<Option<Connection>>,
        pub connection_handlers: RefCell<Vec<glib::SignalHandlerId>>,

        pub directory: RefCell<Option<Directory>>,
        pub directory_handlers: RefCell<Vec<glib::SignalHandlerId>>,
        pub history: History<(Connection, PathBuf)>,

        pub current_size_calculation: Cell<Option<gio::Cancellable>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileList {
        const NAME: &'static str = "GnomeCmdFileList";
        type Type = super::FileList;
        type ParentType = gtk::Widget;

        fn class_init(klass: &mut Self::Class) {
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
            let store = gio::ListStore::new::<FileListItem>();
            let filter_model = gtk::FilterListModel::builder().model(&store).build();
            let sort_model = gtk::SortListModel::builder().model(&filter_model).build();
            let selection = gtk::SingleSelection::builder().model(&sort_model).build();

            let view = gtk::ColumnView::builder()
                .model(&selection)
                .show_column_separators(false)
                .show_row_separators(false)
                .build();
            let row_selector = ListRowSelector::new(&view);
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

                store,

                sort_model,
                filter_model,
                filter: Default::default(),

                selection,
                view,
                columns: Default::default(),
                row_selector,

                cells_map: ColumnID::all().map(|c| (c, Default::default())).collect(),

                shift_down: Default::default(),
                range_selection_start: Default::default(),
                range_selection_closed: Default::default(),
                modifier_click: Default::default(),

                focus_later: Default::default(),

                connection: Default::default(),
                connection_handlers: Default::default(),

                directory: Default::default(),
                directory_handlers: Default::default(),
                history: History::new(20),

                current_size_calculation: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileList {
        fn constructed(&self) {
            self.parent_constructed();

            let fl = self.obj();
            fl.set_layout_manager(Some(gtk::BinLayout::new()));

            self.filter_model
                .set_filter(Some(&gtk::CustomFilter::new(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    #[upgrade_or]
                    false,
                    move |obj| imp.filter_row(obj),
                ))));
            self.view.add_css_class("gnome-cmd-file-list");

            let scrolled_window = gtk::ScrolledWindow::builder()
                .hscrollbar_policy(gtk::PolicyType::Automatic)
                .vscrollbar_policy(gtk::PolicyType::Automatic)
                .hexpand(true)
                .vexpand(true)
                .child(&self.view)
                .build();

            let capture_event_box = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .build();
            capture_event_box.append(&scrolled_window);
            capture_event_box.set_parent(&*fl);

            for (column_id, title, factory, sorter) in [
                (ColumnID::Icon, "", create_icon_factory(), None),
                (
                    ColumnID::Name,
                    &gettext("name"),
                    create_name_factory(&self.cells_map[&ColumnID::Name]),
                    Some(FileAttrSorter::by_name()),
                ),
                (
                    ColumnID::Ext,
                    &gettext("ext"),
                    create_ext_factory(&self.cells_map[&ColumnID::Ext]),
                    Some(FileAttrSorter::by_ext()),
                ),
                (
                    ColumnID::Dir,
                    &gettext("dir"),
                    create_dir_factory(&self.cells_map[&ColumnID::Dir]),
                    Some(FileAttrSorter::by_dir()),
                ),
                (
                    ColumnID::Size,
                    &gettext("size"),
                    create_size_factory(&self.cells_map[&ColumnID::Size]),
                    Some(FileAttrSorter::by_size()),
                ),
                (
                    ColumnID::Date,
                    &gettext("date"),
                    create_date_factory(&self.cells_map[&ColumnID::Date]),
                    Some(FileAttrSorter::by_date()),
                ),
                (
                    ColumnID::Perm,
                    &gettext("perm"),
                    create_perm_factory(&self.cells_map[&ColumnID::Perm]),
                    Some(FileAttrSorter::by_perm()),
                ),
                (
                    ColumnID::Owner,
                    &gettext("uid"),
                    create_owner_factory(&self.cells_map[&ColumnID::Owner]),
                    Some(FileAttrSorter::by_owner()),
                ),
                (
                    ColumnID::Group,
                    &gettext("gid"),
                    create_group_factory(&self.cells_map[&ColumnID::Group]),
                    Some(FileAttrSorter::by_group()),
                ),
            ] {
                let column = gtk::ColumnViewColumn::builder()
                    .title(title)
                    .factory(&factory)
                    .resizable(true)
                    .expand(column_id != ColumnID::Icon)
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
            fl.set_sorting(ColumnID::Name, gtk::SortType::Ascending);

            self.selection.connect_selected_notify(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.cursor_changed()
            ));

            let quick_search_controller = gtk::EventControllerKey::builder()
                .propagation_phase(gtk::PropagationPhase::Capture)
                .build();
            quick_search_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.key_pressed_quick_search(key, state)
            ));
            capture_event_box.connect_root_notify(move |this| {
                if let Some(root) = this.root() {
                    root.add_controller(quick_search_controller.clone());
                }
            });

            let key_capture_controller = gtk::EventControllerKey::builder()
                .propagation_phase(gtk::PropagationPhase::Capture)
                .build();
            key_capture_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.key_pressed_capture(key, state)
            ));
            capture_event_box.add_controller(key_capture_controller);

            let key_controller = gtk::EventControllerKey::new();
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.key_pressed(key, state)
            ));
            capture_event_box.add_controller(key_controller);

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
                .bind_enum(&*fl, "extension-display-mode");
            general_options
                .graphical_layout_mode
                .bind_enum(&*fl, "graphical-layout-mode");
            general_options
                .size_display_mode
                .bind_enum(&*fl, "size-display-mode");
            general_options
                .permissions_display_mode
                .bind_enum(&*fl, "permissions-display-mode");
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
                .bind_enum(&*fl, "left-mouse-button-mode");
            general_options
                .middle_mouse_button_mode
                .bind_enum(&*fl, "middle-mouse-button-mode");
            general_options
                .right_mouse_button_mode
                .bind_enum(&*fl, "right-mouse-button-mode");
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
                .bind_enum(&*fl, "quick-search-shortcut");

            let confirm_options = ConfirmOptions::new();
            confirm_options.dnd_mode.bind_enum(&*fl, "dnd-mode");

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
            if let Some(prev_value) = self.current_size_calculation.take() {
                prev_value.cancel();
            }
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
                    // Quick Search needs to be displayed
                    glib::subclass::Signal::builder("show-quick-search")
                        .param_types([gtk::Widget::static_type()])
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
        fn filter_row(&self, obj: &glib::Object) -> bool {
            let Some(ref filter) = *self.filter.borrow() else {
                return true;
            };
            let Some(item) = obj.downcast_ref::<FileListItem>() else {
                return false;
            };
            item.file().is_dotdot() || filter.matches(&item.name())
        }

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

        fn add_to_history(&self, directory: &Directory) {
            if let Some(connection) = &*self.connection.borrow() {
                self.history
                    .add((connection.clone(), directory.path_from_root()));
            }

            self.obj().emit_by_name::<()>("dir-changed", &[directory]);
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
                if previous_directory.is_local()
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
                    "dir-renamed",
                    false,
                    glib::closure_local!(
                        #[weak(rename_to = imp)]
                        self,
                        move |d: &Directory| imp.on_directory_renamed(d)
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
            self.add_to_history(directory);
        }

        fn on_dir_list_ok(&self, dir: &Directory) {
            debug!('l', "on_dir_list_ok");
            self.add_to_history(dir);
        }

        fn on_dir_list_failed(&self, _dir: &Directory, error: &glib::Error) {
            debug!('l', "on_dir_list_failed: {}", error);
        }

        fn on_directory_deleted(&self, dir: &Directory) {
            if let Some(parent_dir) = dir.existing_parent()
                && let Some(path) = parent_dir.local_path()
            {
                self.obj().goto_directory(&path);
            } else {
                self.obj().goto_directory(Path::new("~"));
            }
        }

        fn on_directory_renamed(&self, dir: &Directory) {
            self.obj().emit_by_name::<()>("dir-changed", &[dir]);
        }

        fn on_dir_file_created(&self, f: &File) {
            if self.insert_file(f) {
                self.obj().emit_files_changed();
            }
        }

        fn on_dir_file_deleted(&self, dir: &Directory, f: &File) {
            if self.directory.borrow().as_ref() == Some(dir) {
                self.obj().remove_file(f);
            }
        }

        fn on_dir_file_changed(&self, f: &File) {
            if self.obj().has_file(f) {
                self.update_file(f);
                self.obj().emit_files_changed();
            }
        }

        fn on_dir_file_renamed(&self, f: &File) {
            for (position, item) in self.store.iter::<FileListItem>().flatten().enumerate() {
                if &item.file() == f {
                    item.update();
                    self.store.items_changed(position as u32, 1, 1);
                }
            }
        }

        pub fn items_iter(&self) -> impl DoubleEndedIterator<Item = FileListItem> {
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
            if self.focus_later.borrow().as_ref() == Some(&f.path_name()) {
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
            if f.needs_update()
                && let Some(item) = self.obj().get_row_from_file(f)
            {
                item.update();
            }
        }

        fn cursor_changed(&self) {
            if let Some(prev_value) = self.current_size_calculation.take() {
                prev_value.cancel();
            }
            let Some(cursor) = self.obj().focused_file_position() else {
                return;
            };
            let Some(range_selection_closed) = self.range_selection_closed.take() else {
                return;
            };
            let Some(range_selection_start) = self.range_selection_start.get() else {
                return;
            };

            self.toggle_file_range(range_selection_start, cursor, range_selection_closed);
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
            if let Some(start_row) = self.range_selection_start.get() {
                self.select_file_range(start_row, positions);
                self.shift_down.set(false);
                self.range_selection_closed.set(None);
            }
        }

        fn potential_text_target(&self) -> TextTarget {
            let Some(focus) = self.obj().root().as_ref().and_then(gtk::Root::focus) else {
                return Default::default();
            };
            if focus.is_ancestor(&*self.obj()) {
                // Key presses within the file list go to either Quick Search or command line
                TextTarget {
                    quick_search: true,
                    command_line: true,
                }
            } else if self.quick_search_shortcut.get() != QuickSearchShortcut::JustACharacter
                && let Some(quick_search) = self.quick_search.upgrade()
                && focus.is_ancestor(&quick_search)
            {
                // Continue handling Alt+letter and Ctrl+Alt+letter within Quick Search
                TextTarget {
                    quick_search: true,
                    ..Default::default()
                }
            } else {
                Default::default()
            }
        }

        fn key_pressed_quick_search(
            &self,
            key: gdk::Key,
            state: gdk::ModifierType,
        ) -> glib::Propagation {
            if is_quicksearch_starting_character(key) {
                let target = self.potential_text_target();
                if target.quick_search
                    && is_quicksearch_starting_modifier(self.quick_search_shortcut.get(), state)
                {
                    self.obj().show_quick_search(Some(key), None);
                    return glib::Propagation::Stop;
                } else if target.command_line
                    && state.difference(SHIFT) == NO_MOD
                    && let Some(text) = key.to_unicode()
                {
                    self.obj()
                        .emit_by_name::<()>("cmdline-append", &[&text.to_string()]);
                    return glib::Propagation::Stop;
                }
            }

            glib::Propagation::Proceed
        }

        pub(super) fn start_range_selection(&self, closed: bool) {
            self.range_selection_closed.set(Some(closed));
            self.range_selection_start
                .replace(self.obj().focused_file_position());
        }

        fn key_pressed_capture(
            &self,
            key: gdk::Key,
            state: gdk::ModifierType,
        ) -> glib::Propagation {
            match (state, key) {
                (
                    SHIFT,
                    gdk::Key::Page_Up
                    | gdk::Key::KP_Page_Up
                    | gdk::Key::KP_9
                    | gdk::Key::Page_Down
                    | gdk::Key::KP_Page_Down
                    | gdk::Key::KP_3
                    | gdk::Key::Up
                    | gdk::Key::KP_Up
                    | gdk::Key::KP_8
                    | gdk::Key::Down
                    | gdk::Key::KP_Down
                    | gdk::Key::KP_2,
                ) => {
                    self.start_range_selection(false);
                    glib::Propagation::Proceed
                }
                (
                    SHIFT,
                    gdk::Key::Home
                    | gdk::Key::KP_Home
                    | gdk::Key::KP_7
                    | gdk::Key::End
                    | gdk::Key::KP_End
                    | gdk::Key::KP_1,
                ) => {
                    self.start_range_selection(true);
                    glib::Propagation::Proceed
                }
                (NO_MOD, gdk::Key::Shift_L | gdk::Key::Shift_R) => {
                    if !self.shift_down.get() {
                        self.range_selection_start
                            .replace(self.obj().focused_file_position());
                    }
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
        }

        fn key_pressed(&self, key: gdk::Key, state: gdk::ModifierType) -> glib::Propagation {
            match (state, key) {
                (SHIFT, gdk::Key::Tab | gdk::Key::ISO_Left_Tab) => glib::Propagation::Stop,
                (SHIFT, gdk::Key::F10) => {
                    self.obj().show_file_popup(None);
                    glib::Propagation::Stop
                }
                (NO_MOD, gdk::Key::Menu) => {
                    self.obj().show_file_popup(None);
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
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
                    && self.left_mouse_button_mode.get() == LeftMouseButtonMode::DoubleClick
                {
                    self.obj()
                        .emit_by_name::<()>("file-activated", &[&item.file()]);
                } else if n_press == 1 && button == 1 {
                    match state {
                        Some(SHIFT) => {
                            if let Some(position) = self
                                .items_iter()
                                .position(|i| &i == item)
                                .and_then(|p| p.try_into().ok())
                            {
                                self.select_with_mouse(position);
                            }
                        }
                        Some(CONTROL) => item.toggle_selected(),
                        Some(NO_MOD) => {
                            if !item.selected() && self.left_mouse_button_unselects.get() {
                                self.obj().unselect_all();
                            }
                        }
                        _ => {}
                    }
                } else if n_press == 1 && button == 3 && !item.file().is_dotdot() {
                    if self.right_mouse_button_mode.get() == RightMouseButtonMode::Selects {
                        if self
                            .selection
                            .selected_item()
                            .is_some_and(|focused| &focused == item)
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
                && self.left_mouse_button_mode.get() == LeftMouseButtonMode::SingleClick
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

        fn drag_prepare(&self, _x: f64, _y: f64) -> Option<gdk::ContentProvider> {
            let files = self
                .obj()
                .selected_files()
                .into_iter()
                .map(|f| f.uri())
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
            } else if let Some(file) = &file
                && file.is_directory()
            {
                DroppingFilesDestination::Child(file.path_name())
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
                                transfer_type: GnomeCmdTransferType::Copy,
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
                                transfer_type: GnomeCmdTransferType::Move,
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
                DroppingFilesDestination::Child(name) => {
                    self.obj().directory().map(|d| d.child(&name))
                }
                DroppingFilesDestination::This => self.obj().directory(),
            };
            let Some(dir) = to else { return };

            let _result = match data.transfer_type {
                GnomeCmdTransferType::Copy => {
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
                GnomeCmdTransferType::Move => {
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
                GnomeCmdTransferType::Link => {
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
        let state = state.difference(SHIFT);
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
                    transfer_type: GnomeCmdTransferType::Copy,
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
                    transfer_type: GnomeCmdTransferType::Move,
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
                    transfer_type: GnomeCmdTransferType::Link,
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

u32_enum! {
pub enum ColumnID {
    Icon,
    #[default]
    Name,
    Ext,
    Dir,
    Size,
    Date,
    Perm,
    Owner,
    Group,
}
}

impl ColumnID {
    pub fn name(&self) -> &'static str {
        match self {
            Self::Icon => "icon",
            Self::Name => "name",
            Self::Ext => "ext",
            Self::Dir => "dir",
            Self::Size => "size",
            Self::Date => "date",
            Self::Perm => "perm",
            Self::Owner => "uid",
            Self::Group => "gid",
        }
    }

    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "icon" => Some(Self::Icon),
            "name" => Some(Self::Name),
            "ext" => Some(Self::Ext),
            "dir" => Some(Self::Dir),
            "size" => Some(Self::Size),
            "date" => Some(Self::Date),
            "perm" => Some(Self::Perm),
            "uid" => Some(Self::Owner),
            "gid" => Some(Self::Group),
            _ => None,
        }
    }

    pub fn title(self) -> Option<String> {
        match self {
            Self::Icon => None,
            Self::Name => Some(gettext("name")),
            Self::Ext => Some(gettext("ext")),
            Self::Dir => Some(gettext("dir")),
            Self::Size => Some(gettext("size")),
            Self::Date => Some(gettext("date")),
            Self::Perm => Some(gettext("perm")),
            Self::Owner => Some(gettext("uid")),
            Self::Group => Some(gettext("gid")),
        }
    }

    pub fn xalign(self) -> f32 {
        match self {
            Self::Icon => 0.5,
            Self::Size => 1.0,
            _ => 0.0,
        }
    }

    pub fn default_sort_direction(self) -> gtk::SortType {
        match self {
            Self::Icon => gtk::SortType::Ascending,
            Self::Name => gtk::SortType::Ascending,
            Self::Ext => gtk::SortType::Ascending,
            Self::Dir => gtk::SortType::Ascending,
            Self::Size => gtk::SortType::Descending,
            Self::Date => gtk::SortType::Descending,
            Self::Perm => gtk::SortType::Ascending,
            Self::Owner => gtk::SortType::Ascending,
            Self::Group => gtk::SortType::Ascending,
        }
    }
}

impl FileList {
    pub fn new(file_metadata_service: &FileMetadataService) -> Self {
        glib::Object::builder()
            .property("file-metadata-service", file_metadata_service)
            .build()
    }

    pub fn set_filter(&self, filter: Option<Filter>) {
        self.imp().filter.replace(filter);
        if let Some(filter) = self.imp().filter_model.filter() {
            filter.changed(gtk::FilterChange::Different);
        }
    }

    pub fn start_range_selection(&self, closed: bool) {
        self.imp().start_range_selection(closed);
    }

    pub fn size(&self) -> usize {
        self.imp()
            .selection
            .n_items()
            .try_into()
            .ok()
            .unwrap_or_default()
    }

    fn new_size_calculation_cancellable(&self) -> gio::Cancellable {
        let cancellable = gio::Cancellable::new();
        if let Some(prev_value) = self
            .imp()
            .current_size_calculation
            .replace(Some(cancellable.clone()))
        {
            prev_value.cancel();
        }
        cancellable
    }

    pub fn show_dir_tree_size(&self, position: u32) {
        if let Some(item) = self.imp().item_at(position) {
            let cancellable = self.new_size_calculation_cancellable();
            glib::spawn_future_local(async move {
                item.set_size(
                    item.file()
                        .tree_size(
                            glib::clone!(
                                #[weak]
                                item,
                                move |size| {
                                    if let Ok(size) = i64::try_from(size) {
                                        item.set_size(size)
                                    }
                                }
                            ),
                            cancellable,
                        )
                        .await
                        .and_then(|s| s.try_into().ok())
                        .unwrap_or(-1),
                );
            });
        }
    }

    pub fn show_visible_tree_sizes(&self) {
        let this = self.clone();
        let cancellable = self.new_size_calculation_cancellable();
        glib::spawn_future_local(async move {
            for item in this.imp().items_iter() {
                if cancellable.is_cancelled() {
                    break;
                }
                if item.file().is_dotdot() {
                    continue;
                }
                item.set_size(
                    item.file()
                        .tree_size(
                            glib::clone!(
                                #[weak]
                                item,
                                move |size| {
                                    if let Ok(size) = i64::try_from(size) {
                                        item.set_size(size)
                                    }
                                }
                            ),
                            cancellable.clone(),
                        )
                        .await
                        .and_then(|s| s.try_into().ok())
                        .unwrap_or(-1),
                );
            }
            this.emit_files_changed();
        });
    }

    pub fn remove_file(&self, f: &File) -> bool {
        let Some(store_position) = self
            .imp()
            .store
            .iter::<FileListItem>()
            .flatten()
            .position(|item| item.file() == *f)
        else {
            return false;
        };

        let was_focused = self
            .root()
            .as_ref()
            .and_then(gtk::Root::focus)
            .is_some_and(|widget| widget.is_ancestor(self));
        self.imp().store.remove(store_position as u32);
        self.emit_files_changed();
        if was_focused {
            // Removing focused row makes Gtk transfer focus away from the list, restore it.
            self.grab_focus();
        }

        true
    }

    pub fn clear(&self) {
        self.imp().store.remove_all();
    }

    pub fn visible_files(&self) -> glib::List<File> {
        self.imp().items_iter().map(|item| item.file()).collect()
    }

    pub fn files(&self) -> impl DoubleEndedIterator<Item = File> {
        self.imp().items_iter().map(|item| item.file())
    }

    pub fn selected_files(&self) -> glib::List<File> {
        let mut list: glib::List<File> = self
            .imp()
            .items_iter()
            .filter(|item| item.selected())
            .map(|item| item.file())
            .collect();
        if list.is_empty()
            && let Some(file) = self.selected_file()
        {
            list.push_back(file);
        }
        list
    }

    pub fn sorting(&self) -> (ColumnID, gtk::SortType) {
        let cv_sorter = self.view().sorter().and_downcast::<gtk::ColumnViewSorter>();
        let sort_column = cv_sorter
            .as_ref()
            .and_then(|sorter| sorter.primary_sort_column());

        let col = self
            .imp()
            .columns
            .borrow()
            .iter()
            .position(|c| sort_column.as_ref() == Some(c))
            .and_then(|pos| u32::try_from(pos).ok().map(ColumnID::from))
            .unwrap_or_default();
        let direction = cv_sorter
            .map(|sorter| sorter.primary_sort_order())
            .unwrap_or(col.default_sort_direction());
        (col, direction)
    }

    pub fn set_sorting(&self, column: ColumnID, order: gtk::SortType) {
        if let Some(column) = self.imp().columns.borrow().get(column as usize) {
            self.view().sort_by_column(None, order);
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
            self.show_dir_tree_size(position);
            self.emit_files_changed();
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
                if !file.is_dotdot() && !file.is_directory() {
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
                item.set_selected(!file.is_directory());
            }
        }
        self.emit_files_changed();
    }

    pub fn unselect_all_files(&self) {
        for item in self.imp().items_iter() {
            if !item.file().is_directory() {
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
        } else if let Some(window) = self.root().and_downcast::<gtk::Window>() {
            open_connection(&window, connection).await
        } else {
            eprintln!("No window");
            false
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
                if directory.is_local() && !directory.is_monitored() && directory.update_mtime() {
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

    pub fn dir_history(&self) -> &History<(Connection, PathBuf)> {
        &self.imp().history
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
        self.imp().items_iter().find(|item| item.file() == *f)
    }

    pub(super) fn focus_file_at_row(&self, row: &FileListItem) {
        if let Some(pos) = self.imp().items_iter().position(|item| item == *row) {
            self.select_row(pos as u32);
        }
    }

    pub fn find_file_by_name(&self, name: &Path) -> Option<u32> {
        self.imp()
            .items_iter()
            .position(|item| item.file().path_name() == name)
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
        self.imp().row_selector.select_row(pos);
    }

    pub fn toggle_with_pattern(&self, pattern: &Filter, mode: bool) {
        let select_dirs = self.select_dirs();
        for item in self.imp().items_iter() {
            let file = item.file();
            if !file.is_dotdot()
                && (select_dirs || !file.is_directory())
                && pattern.matches(&file.name())
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
                .map(Path::new)
                .unwrap_or(dir)
                .to_path_buf()
        };

        let (new_dir, focus_dir) = if dir == Path::new("..") {
            let cwd = self
                .directory()
                .ok_or_else(|| ErrorMessage::brief(gettext("Current directory is not set.")))?;

            // let's get the parent directory
            (
                cwd.parent()
                    .ok_or_else(|| ErrorMessage::brief(gettext("No parent directory")))?,
                Some(cwd),
            )
        } else {
            (
                if dir.is_absolute() {
                    let connection = self
                        .connection()
                        .unwrap_or_else(|| ConnectionList::get().home().upcast());
                    Directory::new(&connection, &connection.create_uri(&dir))
                } else if dir.starts_with("\\\\") {
                    let connection = ConnectionList::get().smb().ok_or_else(|| {
                        ErrorMessage::brief(gettext("No SAMBA connection is available"))
                    })?;
                    Directory::new(&connection, &connection.create_uri(&dir))
                } else {
                    self.directory()
                        .ok_or_else(|| {
                            ErrorMessage::brief(gettext("Current directory is not set."))
                        })
                        .map(|cwd| cwd.child(&dir))?
                },
                None,
            )
        };

        self.set_directory_async(&new_dir).await;

        // focus the current dir when going back to the parent dir
        if let Some(focus_dir) = focus_dir {
            self.focus_file(&focus_dir.path_name(), false);
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
            if !file.is_dotdot() && (select_dirs || !file.is_directory()) {
                item.set_selected(!item.selected());
            }
        }
        self.emit_files_changed();
    }

    pub fn stats(&self) -> FileListStats {
        let mut stats = FileListStats::default();
        for item in self.imp().items_iter() {
            let file = item.file();
            if !file.is_dotdot() {
                let selected = item.selected();
                let cached_size: Option<u64> = item.size().try_into().ok();

                match file.file_type() {
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
                        let size = file.size().unwrap_or_default();
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

        let sentence1 = gettext("Selected {selected_bytes} of {total_bytes}.")
            .replace(
                "{selected_bytes}",
                &size_to_string(stats.selected.bytes, mode),
            )
            .replace("{total_bytes}", &size_to_string(stats.total.bytes, mode));

        let sentence2 = gettext("Files: {selected_files} of {total_files}.")
            .replace("{selected_files}", &stats.selected.files.to_string())
            .replace("{total_files}", &stats.total.files.to_string());

        let sentence3 = gettext("Directories: {selected_dirs} of {total_dirs}.")
            .replace("{selected_dirs}", &stats.selected.directories.to_string())
            .replace("{total_dirs}", &stats.total.directories.to_string());

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
            show_delete_dialog(
                &window,
                self.connection().as_ref(),
                &files,
                force,
                &general_options,
                &confirm_options,
            )
            .await;
        }
    }

    pub async fn show_rename_dialog(&self) {
        let Some(window) = self.root().and_downcast::<gtk::Window>() else {
            eprintln!("No window");
            return;
        };
        let Some(selected) = self
            .selection()
            .selected_item()
            .and_downcast::<FileListItem>()
        else {
            return;
        };
        let Some(rect) = self.focus_row_coordinates(&selected) else {
            eprintln!("Selected file is not visible");
            return;
        };

        let file = selected.file();
        if let Some(new_name) = show_rename_popover(&file.name(), self, &rect).await {
            match file.rename(&new_name) {
                Ok(_) => {
                    selected.update();
                    self.focus_file(Path::new(&new_name), true);
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

    pub fn show_quick_search(&self, key: Option<gdk::Key>, mode: Option<QuickSearchMode>) {
        let quick_search = self.imp().quick_search.upgrade().unwrap_or_else(|| {
            // We display the quick search box outside the file list widget to make sure keyboard
            // shortcut processing doesn't interfere with text entry.
            let quick_search = QuickSearch::new(self);
            self.emit_by_name::<()>(
                "show-quick-search",
                &[quick_search.upcast_ref::<gtk::Widget>()],
            );
            quick_search.set_mode(
                mode.unwrap_or_else(|| GeneralOptions::new().quick_search_default_mode.get()),
            );
            self.imp().quick_search.set(Some(&quick_search));
            quick_search
        });
        if let Some(mode) = mode {
            // If a mode was passed in explicitly, overwrite even if Quick Search is already open.
            quick_search.set_mode(mode);
        }
        if let Some(text) = key.and_then(|k| k.to_unicode()).map(|c| c.to_string()) {
            quick_search.add_text(&text);
        }
        quick_search.grab_focus_without_selecting();
    }

    pub fn update_style(&self) {
        // TODO: Maybe??? gtk_cell_renderer_set_fixed_size (priv->columns[1], )
        // gtk_clist_set_row_height (*this, gnome_cmd_data.options.list_row_height);

        // let font_desc = pango::FontDescription::from_string(&self.font_name());
        // gtk_widget_override_font (*this, font_desc);

        if let Some(main_win) = self.root().and_downcast::<MainWindow>()
            && main_win.color_themes().has_theme()
        {
            self.view().add_css_class("themed");
        } else {
            self.view().remove_css_class("themed");
        }

        self.queue_draw();
    }

    pub fn sort_by(&self, col: ColumnID) {
        let (current_col, current_direction) = self.sorting();
        let direction = if current_col == col {
            match current_direction {
                gtk::SortType::Ascending => gtk::SortType::Descending,
                _ => gtk::SortType::Ascending,
            }
        } else {
            col.default_sort_direction()
        };
        self.set_sorting(col, direction);
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

        let store = &self.imp().store;
        store.remove_all();
        store.splice(0, 0, &items);

        if self.is_realized() {
            // If we have been waiting for this file to show up, focus it
            let focus_later = self.imp().focus_later.take();
            let focus_later_item = items
                .into_iter()
                .find(|item| focus_later.as_ref() == Some(&item.file().path_name()));

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
        // Silence linting false positive, https://github.com/gtk-rs/gtk-rs-core/issues/1912
        #[allow(clippy::redundant_closure)]
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
        // Silence linting false positive, https://github.com/gtk-rs/gtk-rs-core/issues/1912
        #[allow(clippy::redundant_closure)]
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
        // Silence linting false positive, https://github.com/gtk-rs/gtk-rs-core/issues/1912
        #[allow(clippy::redundant_closure)]
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
        // Silence linting false positive, https://github.com/gtk-rs/gtk-rs-core/issues/1912
        #[allow(clippy::redundant_closure)]
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
        // Silence linting false positive, https://github.com/gtk-rs/gtk-rs-core/issues/1912
        #[allow(clippy::redundant_closure)]
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
        // Silence linting false positive, https://github.com/gtk-rs/gtk-rs-core/issues/1912
        #[allow(clippy::redundant_closure)]
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
        // Silence linting false positive, https://github.com/gtk-rs/gtk-rs-core/issues/1912
        #[allow(clippy::redundant_closure)]
        self.connect_closure(
            "list-clicked",
            false,
            glib::closure_local!(move |this, button, file| (callback)(this, button, file)),
        )
    }

    pub fn connect_show_quick_search<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, gtk::Widget) + 'static,
    {
        // Silence linting false positive, https://github.com/gtk-rs/gtk-rs-core/issues/1912
        #[allow(clippy::redundant_closure)]
        self.connect_closure(
            "show-quick-search",
            false,
            glib::closure_local!(move |this, widget| (callback)(this, widget)),
        )
    }

    fn item_rect(&self, item: &FileListItem, column: ColumnID) -> Option<graphene::Rect> {
        self.imp()
            .cells_map
            .get(&column)?
            .find_by_key(item)
            .and_then(|c| c.compute_bounds(self))
    }

    fn focus_row_coordinates(&self, item: &FileListItem) -> Option<gdk::Rectangle> {
        let name_rect = self.item_rect(item, ColumnID::Name)?;
        let rect = if self.extension_display_mode() != ExtensionDisplayMode::Both {
            if let Some(ext_rect) = self.item_rect(item, ColumnID::Ext) {
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
                .or_else(|| {
                    let selected = self
                        .selection()
                        .selected_item()
                        .and_downcast::<FileListItem>()?;
                    self.focus_row_coordinates(&selected)
                })
                .as_ref(),
        );
        popover.present();
        popover.popup();
    }

    pub fn open_file(&self) {
        let event_processed = self.emit_by_name::<bool>("cmdline-execute", &[]);
        if !event_processed && let Some(file) = self.focused_file() {
            self.emit_by_name::<()>("file-activated", &[&file]);
        }
    }

    fn add_to_cmdline(&self, value: impl AsRef<OsStr>) {
        if let Some(text) = glib::shell_quote(value).to_str() {
            self.emit_by_name::<()>("cmdline-append", &[&text]);
        }
    }

    pub fn add_cwd_to_cmdline(&self) {
        if let Some(path) = self.directory().and_then(|d| d.local_path()) {
            self.add_to_cmdline(path);
        }
    }

    pub fn add_file_to_cmdline(&self, fullpath: bool) {
        if let Some(file) = self.selected_file() {
            if fullpath {
                if let Some(path) = file.local_path() {
                    self.add_to_cmdline(path);
                }
            } else {
                self.add_to_cmdline(file.name());
            }
        }
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

            let image = gtk::Image::builder().build();
            let overlay = gtk::Overlay::builder()
                .child(&image)
                .hexpand(true)
                .halign(gtk::Align::Center)
                .build();
            stack.add_named(&overlay, Some("overlay"));

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
        if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>()
            && let Some(stack) = list_item.child().and_downcast::<gtk::Stack>()
            && let Some(item) = list_item.item().and_downcast::<FileListItem>()
            && let Some(overlay) = stack
                .child_by_name("overlay")
                .and_downcast::<gtk::Overlay>()
            && let Some(image) = overlay.child().and_downcast::<gtk::Image>()
            && let Some(label) = stack.child_by_name("label").and_downcast::<gtk::Label>()
        {
            image.clear();
            label.set_text("");

            // Remove existing overlay image if any
            if let Some(mut child) = overlay.first_accessible_child() {
                loop {
                    if let Some(widget) = child.downcast_ref::<gtk::Widget>()
                        && widget != &image
                    {
                        overlay.remove_overlay(widget);
                        break;
                    }

                    child = match child.next_accessible_sibling() {
                        Some(child) => child,
                        None => break,
                    }
                }
            }

            let file = item.file();
            if !file.is_dotdot() && file.file_info().is_symlink() {
                overlay.add_overlay(
                    &gtk::Image::builder()
                        .gicon(icon_cache.symlink_overlay())
                        .pixel_size(9)
                        .halign(gtk::Align::End)
                        .valign(gtk::Align::End)
                        .build(),
                );
            }

            let use_ls_colors = color_settings.boolean("use-ls-colors");
            apply_css(&item, use_ls_colors, label.upcast_ref());

            match global_options.graphical_layout_mode.get() {
                GraphicalLayoutMode::Text => {
                    label.set_text(imp::type_string(file.file_type()));
                    stack.set_visible_child(&label);
                }
                mode => {
                    if let Some(icon) = icon_cache.file_icon(&file, mode) {
                        image.set_from_gicon(&icon);
                    }
                    stack.set_visible_child(&overlay);
                }
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
        cell.bind(&item);
        if !matches!(
            global_options.extension_display_mode.get(),
            ExtensionDisplayMode::WithFname
        ) {
            cell.add_binding(
                item.bind_property("extension", &cell, "text")
                    .sync_create()
                    .build(),
            );
        }
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
        let is_directory = item.file().is_directory();
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
            if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>()
                && let Some(cell) = list_item.child().and_downcast::<FileListCell>()
            {
                cell.unbind();
                cell.clear_bindings();
                cells.remove_value(&cell);
            }
        }
    ));
    factory.upcast()
}

pub fn apply_css(item: &FileListItem, use_ls_colors: bool, widget: &gtk::Widget) {
    for c in LsPalletteColor::all() {
        widget.remove_css_class(&format!("fg-{}", c.name()));
        widget.remove_css_class(&format!("bg-{}", c.name()));
    }

    let mut current_parent = widget.clone().upcast::<gtk::Accessible>();
    while let Some(parent) = current_parent.accessible_parent() {
        current_parent = parent;
        if current_parent.accessible_role() == gtk::AccessibleRole::Row {
            break;
        }
    }

    if let Ok(parent_widget) = current_parent.downcast::<gtk::Widget>() {
        if item.selected() {
            parent_widget.add_css_class("sel");
        } else {
            parent_widget.remove_css_class("sel");
        }
    }

    if !item.selected()
        && use_ls_colors
        && let Some(colors) = ls_colors_get(&item.file().file_info())
    {
        if let Some(class) = colors.fg.map(|fg| format!("fg-{}", fg.name())) {
            widget.add_css_class(&class);
        }
        if let Some(class) = colors.bg.map(|fg| format!("bg-{}", fg.name())) {
            widget.add_css_class(&class);
        }
    }
}

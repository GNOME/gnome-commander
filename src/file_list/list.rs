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
    file_attr_sorter::{reverse_sorter, FileAttrSorter},
    file_type_sorter::FileTypeSorter,
    popup::file_popup_menu,
    quick_search::QuickSearch,
};
use crate::{
    connection::connection::Connection,
    data::{
        ColorOptions, ConfirmOptions, FiltersOptions, FiltersOptionsRead, GeneralOptions,
        GeneralOptionsRead,
    },
    dialogs::{delete_dialog::show_delete_dialog, rename_popover::show_rename_popover},
    dir::Directory,
    file::{ffi::GnomeCmdFile, File},
    filter::{fnmatch, Filter},
    layout::{color_themes::ColorThemes, ls_colors_palette::load_palette},
    libgcmd::file_descriptor::FileDescriptorExt,
    main_win::MainWindow,
    tags::tags::FileMetadataService,
    types::{ExtensionDisplayMode, SizeDisplayMode},
    utils::{size_to_string, ErrorMessage},
};
use gettextrs::{gettext, ngettext};
use gtk::{
    gdk, gio,
    glib::{
        self,
        ffi::{gboolean, GType},
        translate::{from_glib_borrow, from_glib_none, Borrowed, IntoGlib, ToGlibPtr},
    },
    pango,
    prelude::*,
    subclass::prelude::*,
};
use std::{
    collections::HashSet,
    ffi::c_char,
    ops::ControlFlow,
    path::{Path, PathBuf},
};

mod imp {
    use super::*;
    use crate::{
        app::App,
        data::ColorOptions,
        dialogs::pattern_selection_dialog::select_by_pattern,
        file_list::{
            actions::{
                file_list_action_execute, file_list_action_execute_script,
                file_list_action_file_edit, file_list_action_file_view, file_list_action_open_with,
                file_list_action_open_with_default, file_list_action_open_with_other, Script,
            },
            popup::list_popup_menu,
        },
        imageloader::icon_cache,
        layout::{
            color_themes::ColorTheme,
            ls_colors::{ls_colors_get, LsPallettePlane},
            ls_colors_palette::LsColorsPalette,
        },
        tags::tags::FileMetadataService,
        transfer::{gnome_cmd_copy_gfiles, gnome_cmd_link_gfiles, gnome_cmd_move_gfiles},
        types::{
            ConfirmOverwriteMode, DndMode, ExtensionDisplayMode, GnomeCmdTransferType,
            GraphicalLayoutMode, LeftMouseButtonMode, MiddleMouseButtonMode, PermissionDisplayMode,
            QuickSearchShortcut, RightMouseButtonMode,
        },
        utils::{
            get_modifiers_state, permissions_to_numbers, permissions_to_text, time_to_string, ALT,
            ALT_SHIFT, CONTROL, CONTROL_ALT, CONTROL_SHIFT, NO_MOD, SHIFT,
        },
    };
    use std::{
        borrow::Cow,
        cell::{Cell, OnceCell, RefCell},
        cmp,
        ffi::OsStr,
        path::PathBuf,
        sync::OnceLock,
        time::Duration,
    };
    use strum::VariantArray;

    #[derive(Default, glib::Properties)]
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

        pub color_theme: RefCell<Option<Cow<'static, ColorTheme>>>,
        pub ls_palette: RefCell<LsColorsPalette>,

        #[property(get)]
        view: OnceCell<gtk::TreeView>,
        #[property(get)]
        store: OnceCell<gtk::ListStore>,

        pub sorting: Cell<Option<(ColumnID, gtk::SortType)>>,
        #[property(get, set)]
        pub sorter: RefCell<Option<gtk::Sorter>>,

        pub shift_down: Cell<bool>,
        pub shift_down_row: RefCell<Option<gtk::TreeIter>>,
        pub shift_down_key: Cell<Option<gdk::Key>>,

        modifier_click: Cell<Option<gdk::ModifierType>>,

        pub focus_later: RefCell<Option<PathBuf>>,
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
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileList {
        fn constructed(&self) {
            self.parent_constructed();

            let fl = self.obj();
            fl.set_layout_manager(Some(gtk::BinLayout::new()));

            self.color_theme.replace(ColorThemes::new().theme());
            self.ls_palette
                .replace(load_palette(&ColorOptions::new().0));

            let view = gtk::TreeView::builder().build();
            view.add_css_class("gnome-cmd-file-list");
            view.selection().set_mode(gtk::SelectionMode::Browse);

            let scrolled_window = gtk::ScrolledWindow::builder()
                .hscrollbar_policy(gtk::PolicyType::Automatic)
                .vscrollbar_policy(gtk::PolicyType::Automatic)
                .hexpand(true)
                .vexpand(true)
                .child(&view)
                .build();
            scrolled_window.set_parent(&*fl);
            self.view.set(view.clone()).unwrap();

            let store = gtk::ListStore::new(&[
                gio::Icon::static_type(), // COLUMN_ICON
                String::static_type(),    // COLUMN_NAME
                String::static_type(),    // COLUMN_EXT
                String::static_type(),    // COLUMN_DIR
                String::static_type(),    // COLUMN_SIZE
                String::static_type(),    // COLUMN_DATE
                String::static_type(),    // COLUMN_PERM
                String::static_type(),    // COLUMN_OWNER
                String::static_type(),    // COLUMN_GROUP
                File::static_type(),      // DATA_COLUMN_FILE
                String::static_type(),    // DATA_COLUMN_ICON_NAME
                bool::static_type(),      // DATA_COLUMN_SELECTED
                u64::static_type(),       // DATA_COLUMN_SIZE
            ]);
            self.store.set(store.clone()).unwrap();

            self.sorting
                .set(Some((ColumnID::COLUMN_NAME, gtk::SortType::Ascending)));
            fl.update_sorter();

            for (i, column_id) in ColumnID::VARIANTS.iter().cloned().enumerate() {
                let column = gtk::TreeViewColumn::builder()
                    .title(column_id.title().unwrap_or_default())
                    .sizing(gtk::TreeViewColumnSizing::Fixed)
                    .resizable(true)
                    .build();

                view.insert_column(&column, i as i32);

                match column_id {
                    ColumnID::COLUMN_ICON => {
                        column.set_clickable(false);

                        let renderer = gtk::CellRendererPixbuf::new();
                        column.pack_start(&renderer, true);
                        column.set_cell_data_func(
                            &renderer,
                            glib::clone!(
                                #[weak(rename_to = imp)]
                                self,
                                move |_, cell, _, iter| imp.cell_data(cell, iter, column_id as i32)
                            ),
                        );

                        let renderer = gtk::CellRendererText::new();
                        column.pack_start(&renderer, true);
                        column.set_cell_data_func(
                            &renderer,
                            glib::clone!(
                                #[weak(rename_to = imp)]
                                self,
                                move |_, cell, _, iter| imp.cell_data(
                                    cell,
                                    iter,
                                    DataColumns::DATA_COLUMN_ICON_NAME as i32
                                )
                            ),
                        );
                    }
                    _ => {
                        column.set_clickable(true);
                        // column.set_sort_column_id(i);
                        column.set_sort_indicator(true);

                        let renderer = gtk::CellRendererText::new();
                        renderer.set_xalign(column_id.xalign());
                        column.pack_start(&renderer, true);
                        column.set_cell_data_func(
                            &renderer,
                            glib::clone!(
                                #[weak(rename_to = imp)]
                                self,
                                move |_, cell, _, iter| imp.cell_data(cell, iter, column_id as i32)
                            ),
                        );
                    }
                }

                column.connect_clicked(glib::clone!(
                    #[weak]
                    fl,
                    move |_| fl.sort_by(column_id)
                ));
            }

            unsafe {
                ffi::gnome_cmd_file_list_init(fl.to_glib_none().0);
            }

            view.set_model(Some(&store));

            view.connect_cursor_changed(glib::clone!(
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

            let click_controller = gtk::GestureClick::builder().button(0).build();
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
            view.add_controller(click_controller);

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
            general_options
                .0
                .bind("list-font", &*fl, "font-name")
                .build();
            general_options
                .0
                .bind("list-row-height", &*fl, "row-height")
                .build();
            general_options
                .0
                .bind("extension-display-mode", &*fl, "extension-display-mode")
                .build();
            general_options
                .0
                .bind("graphical-layout-mode", &*fl, "graphical-layout-mode")
                .build();
            general_options
                .0
                .bind("size-display-mode", &*fl, "size-display-mode")
                .build();
            general_options
                .0
                .bind("perm-display-mode", &*fl, "permissions-display-mode")
                .build();
            general_options
                .0
                .bind("date-disp-format", &*fl, "date-display-format")
                .build();
            general_options
                .0
                .bind("case-sensitive", &*fl, "case-sensitive")
                .build();
            general_options
                .0
                .bind(
                    "symbolic-links-as-regular-files",
                    &*fl,
                    "symbolic-links-as-regular-files",
                )
                .build();
            general_options
                .0
                .bind("clicks-to-open-item", &*fl, "left-mouse-button-mode")
                .build();
            general_options
                .0
                .bind("middle-mouse-btn-mode", &*fl, "middle-mouse-button-mode")
                .build();
            general_options
                .0
                .bind("right-mouse-btn-mode", &*fl, "right-mouse-button-mode")
                .build();
            general_options
                .0
                .bind(
                    "left-mouse-btn-unselects",
                    &*fl,
                    "left-mouse-button-unselects",
                )
                .build();
            general_options
                .0
                .bind("select-dirs", &*fl, "select-dirs")
                .build();
            general_options
                .0
                .bind("quick-search", &*fl, "quick-search-shortcut")
                .build();

            let confirm_options = ConfirmOptions::new();
            confirm_options
                .0
                .bind("mouse-drag-and-drop", &*fl, "dnd-mode")
                .build();

            for (column, key) in fl.view().columns().iter().zip([
                "column-width-icon",
                "column-width-name",
                "column-width-ext",
                "column-width-dir",
                "column-width-size",
                "column-width-date",
                "column-width-perm",
                "column-width-owner",
                "column-width-group",
            ]) {
                general_options.0.bind(key, column, "fixed-width").build();
            }

            let color_options = ColorOptions::new();
            color_options
                .0
                .bind("use-ls-colors", &*fl, "use-ls-colors")
                .build();
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
            unsafe {
                ffi::gnome_cmd_file_list_finalize(self.obj().to_glib_none().0);
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
            self.obj().view().grab_focus()
        }

        fn realize(&self) {
            self.parent_realize();
            self.update_column_sort_arrows();
        }
    }

    impl FileList {
        pub fn is_selected_iter(&self, iter: &gtk::TreeIter) -> bool {
            let selected: bool = TreeModelExtManual::get(
                &self.obj().store(),
                iter,
                DataColumns::DATA_COLUMN_SELECTED as i32,
            );
            selected
        }

        fn file_at_iter(&self, iter: &gtk::TreeIter) -> File {
            let file: File = TreeModelExtManual::get(
                &self.obj().store(),
                iter,
                DataColumns::DATA_COLUMN_FILE as i32,
            );
            file
        }

        pub fn set_model_row(&self, iter: &gtk::TreeIter, f: &File, tree_size: bool) {
            let model = self.obj().store();

            match self.graphical_layout_mode.get() {
                GraphicalLayoutMode::Text => {
                    model.set(
                        iter,
                        &[(DataColumns::DATA_COLUMN_ICON_NAME as u32, &type_string(f))],
                    );
                }
                mode => {
                    let icon = icon_cache().file_icon(f, mode);
                    model.set(iter, &[(ColumnID::COLUMN_ICON as u32, &icon)]);
                }
            }

            let file_name = f.file_info().name();
            let name = match self.extension_display_mode.get() {
                ExtensionDisplayMode::Stripped
                    if f.file_info().file_type() == gio::FileType::Regular =>
                {
                    file_name
                        .file_stem()
                        .map(|n| n.to_string_lossy().to_string())
                }
                _ => Some(file_name.to_string_lossy().to_string()),
            };
            let ext = match self.extension_display_mode.get() {
                ExtensionDisplayMode::Stripped | ExtensionDisplayMode::Both
                    if f.file_info().file_type() == gio::FileType::Regular =>
                {
                    file_name
                        .extension()
                        .map(|n| n.to_string_lossy().to_string())
                }
                _ => None,
            };
            model.set(
                iter,
                &[
                    (ColumnID::COLUMN_NAME as u32, &name),
                    (ColumnID::COLUMN_EXT as u32, &ext),
                ],
            );

            let path = f.get_path_string_through_parent();
            let dir = path.parent();
            if let Some(relative) = self
                .obj()
                .base_dir()
                .and_then(|b| dir.and_then(|t| t.strip_prefix(b).ok()))
                .map(|r| Path::new(".").join(r))
            {
                model.set(iter, &[(ColumnID::COLUMN_DIR as u32, &relative)]);
            } else {
                model.set(iter, &[(ColumnID::COLUMN_DIR as u32, &dir)]);
            }

            if f.is_dotdot() {
                model.set(
                    iter,
                    &[
                        (DataColumns::DATA_COLUMN_SIZE as u32, &u64::MAX),
                        (ColumnID::COLUMN_SIZE as u32, &gettext("<DIR>")),
                    ],
                );
            } else if f.file_info().file_type() == gio::FileType::Directory {
                let dir_size = tree_size.then(|| f.tree_size()).flatten();
                let size_str = Some(
                    dir_size
                        .map(|size| size_to_string(size, self.size_display_mode.get()))
                        .unwrap_or_else(|| gettext("<DIR>")),
                );
                model.set(
                    iter,
                    &[
                        (
                            DataColumns::DATA_COLUMN_SIZE as u32,
                            &dir_size.unwrap_or(u64::MAX),
                        ),
                        (ColumnID::COLUMN_SIZE as u32, &size_str),
                    ],
                );
            } else {
                let file_size = f.size();
                let size_str =
                    file_size.map(|size| size_to_string(size, self.size_display_mode.get()));
                model.set(
                    iter,
                    &[
                        (
                            DataColumns::DATA_COLUMN_SIZE as u32,
                            &file_size.unwrap_or(u64::MAX),
                        ),
                        (ColumnID::COLUMN_SIZE as u32, &size_str),
                    ],
                );
            }

            if f.file_info().file_type() != gio::FileType::Directory || !f.is_dotdot() {
                model.set(
                    iter,
                    &[(
                        ColumnID::COLUMN_DATE as u32,
                        &f.modification_date().and_then(|dt| {
                            time_to_string(dt, &self.date_display_format.borrow()).ok()
                        }),
                    )],
                );
                model.set(
                    iter,
                    &[(
                        ColumnID::COLUMN_PERM as u32,
                        &display_permissions(f.permissions(), self.permissions_display_mode.get()),
                    )],
                );
                model.set(iter, &[(ColumnID::COLUMN_OWNER as u32, &f.owner())]);
                model.set(iter, &[(ColumnID::COLUMN_GROUP as u32, &f.group())]);
            }

            model.set(
                iter,
                &[
                    (DataColumns::DATA_COLUMN_FILE as u32, f),
                    (DataColumns::DATA_COLUMN_SELECTED as u32, &false),
                ],
            );
        }

        pub fn add_file(&self, f: &File, next: Option<&gtk::TreeIter>) {
            let iter = if let Some(next) = next {
                self.obj().store().insert_before(Some(next))
            } else {
                self.obj().store().append()
            };
            self.set_model_row(&iter, f, false);

            // If we have been waiting for this file to show up, focus it
            if self.focus_later.borrow().as_ref() == Some(&f.file_info().name()) {
                self.focus_later.replace(None);
                self.obj().focus_file_at_row(&iter);
            }
        }

        pub fn insert_file(&self, f: &File) -> bool {
            let options = FiltersOptions::new();
            if !file_is_wanted(f, &options) {
                return false;
            }

            let next_iter = if let Some(sorter) = self.obj().sorter() {
                let result = self
                    .obj()
                    .traverse_files::<gtk::TreeIter>(|f2, iter, _store| {
                        if sorter.compare(f2, f) == gtk::Ordering::Larger {
                            ControlFlow::Break(iter.clone())
                        } else {
                            ControlFlow::Continue(())
                        }
                    });
                match result {
                    ControlFlow::Break(iter) => Some(iter),
                    ControlFlow::Continue(()) => None,
                }
            } else {
                None
            };

            self.add_file(f, next_iter.as_ref());
            true
        }

        pub fn update_file(&self, f: &File) {
            if f.needs_update() {
                if let Some(row) = self.obj().get_row_from_file(f) {
                    self.set_model_row(&row, f, false);
                }
            }
        }

        fn cell_data(&self, cell: &gtk::CellRenderer, iter: &gtk::TreeIter, column: i32) {
            let file = self.file_at_iter(iter);
            let selected = self.is_selected_iter(iter);

            let layout = self.graphical_layout_mode.get();

            let has_foreground;
            if column == ColumnID::COLUMN_ICON as i32 {
                match layout {
                    GraphicalLayoutMode::Text => {
                        cell.set_property("gicon", gio::Icon::NONE);
                    }
                    _ => {
                        let icon: Option<gio::Icon> =
                            TreeModelExtManual::get(&self.obj().store(), iter, column);
                        cell.set_property("gicon", icon);
                    }
                }
                has_foreground = false;
            } else if column == DataColumns::DATA_COLUMN_ICON_NAME as i32 {
                match layout {
                    GraphicalLayoutMode::Text => {
                        cell.set_property("text", None::<String>);
                    }
                    _ => {
                        let icon: Option<String> =
                            TreeModelExtManual::get(&self.obj().store(), iter, column);
                        cell.set_property("text", icon);
                    }
                }
                has_foreground = false;
            } else {
                let value: Option<String> =
                    TreeModelExtManual::get(&self.obj().store(), iter, column);
                cell.set_property("text", value);
                has_foreground = true;
            }

            if selected {
                if let Some(color_theme) = self.color_theme.borrow().as_ref() {
                    if has_foreground {
                        cell.set_property("foreground-rgba", color_theme.sel_fg);
                        cell.set_property("foreground-set", true);
                    }
                    cell.set_property("cell-background-rgba", color_theme.sel_bg);
                    cell.set_property("cell-background-set", true);
                } else {
                    // TODO: Consider better ways to highlight selected files
                    cell.set_property("weight-rgba", pango::Weight::Bold);
                    cell.set_property("weight-set", true);
                }
            } else {
                if let Some(color_theme) = self.color_theme.borrow().as_ref() {
                    if has_foreground {
                        cell.set_property("foreground-rgba", color_theme.norm_fg);
                        cell.set_property("foreground-set", true);
                    }
                    cell.set_property("cell-background-rgba", color_theme.norm_bg);
                    cell.set_property("cell-background-set", true);
                }

                self.paint_cell_with_ls_colors(cell, &file, has_foreground);
            }
        }

        fn paint_cell_with_ls_colors(
            &self,
            cell: &gtk::CellRenderer,
            file: &File,
            has_foreground: bool,
        ) {
            if !self.use_ls_colors.get() {
                return;
            }
            if let Some(colors) = ls_colors_get(&file.file_info()) {
                if has_foreground {
                    if let Some(fg) = colors.fg {
                        cell.set_property(
                            "foreground-rgba",
                            self.ls_palette
                                .borrow()
                                .color(LsPallettePlane::Foreground, fg),
                        );
                        cell.set_property("foreground-set", true);
                    }
                }
                if let Some(bg) = colors.bg {
                    cell.set_property(
                        "cell-background-rgba",
                        self.ls_palette
                            .borrow()
                            .color(LsPallettePlane::Background, bg),
                    );
                    cell.set_property("cell-background-set", true);
                }
            }
        }

        pub fn update_column_sort_arrows(&self) {
            let (col, order) = self.obj().sorting();
            for (i, column) in self.obj().view().columns().into_iter().enumerate() {
                if i == col as usize {
                    column.set_sort_indicator(true);
                    column.set_sort_order(order);
                } else {
                    column.set_sort_indicator(false);
                }
            }
        }

        pub fn set_selected_at_iter(&self, iter: &gtk::TreeIter, selected: bool) {
            self.obj().store().set_value(
                iter,
                DataColumns::DATA_COLUMN_SELECTED as u32,
                &selected.to_value(),
            );
            self.obj().emit_files_changed();
        }

        fn cursor_changed(&self) {
            let Some(cursor) = self.obj().focused_file_iter() else {
                return;
            };
            let shift_down_key = self.shift_down_key.take();
            let Some(shift_down_row) = self.shift_down_row.borrow().clone() else {
                return;
            };

            match shift_down_key {
                Some(gdk::Key::Page_Up) => {
                    self.toggle_file_range(&shift_down_row, &cursor, false);
                }
                Some(gdk::Key::Page_Down) => {
                    self.toggle_file_range(&shift_down_row, &cursor, false);
                }
                Some(gdk::Key::Home) => {
                    self.toggle_file_range(&shift_down_row, &cursor, true);
                }
                Some(gdk::Key::End) => {
                    self.toggle_file_range(&shift_down_row, &cursor, true);
                }
                _ => {}
            }
        }

        fn toggle_file_range(
            &self,
            start_row: &gtk::TreeIter,
            end_row: &gtk::TreeIter,
            closed_end: bool,
        ) {
            let mut state = 0;
            let _ = self.obj().traverse_files(|_file, iter, store| {
                let matchez = iter_compare(store, iter, start_row) == cmp::Ordering::Equal
                    || iter_compare(store, iter, end_row) == cmp::Ordering::Equal;
                if state == 0 {
                    if matchez {
                        self.toggle_file(iter);
                        state = 1;
                    }
                } else if state == 1 {
                    if matchez {
                        if closed_end {
                            self.toggle_file(iter);
                        }
                        return ControlFlow::Break(());
                    } else {
                        self.toggle_file(iter);
                    }
                }
                ControlFlow::Continue(())
            });
        }

        fn select_file_range(&self, start_row: &gtk::TreeIter, end_row: &gtk::TreeIter) {
            let mut state = 0;
            let _ = self.obj().traverse_files(|file, iter, store| {
                let matchez = iter_compare(store, iter, start_row) == cmp::Ordering::Equal
                    || iter_compare(store, iter, end_row) == cmp::Ordering::Equal;
                if state == 0 {
                    if matchez {
                        state = 1;
                        if !file.is_dotdot() {
                            self.set_selected_at_iter(iter, true);
                        }
                    }
                } else {
                    if !file.is_dotdot() {
                        self.set_selected_at_iter(iter, true);
                    }
                    if matchez {
                        return ControlFlow::Break(());
                    }
                }
                ControlFlow::Continue(())
            });
            self.obj().emit_files_changed();
        }

        pub fn select_with_mouse(&self, iter: &gtk::TreeIter) {
            if let Some(start_iter) = self.shift_down_row.borrow().clone() {
                self.select_file_range(&start_iter, iter);
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
                    self.shift_down_row.replace(self.obj().focused_file_iter());
                    return glib::Propagation::Proceed;
                }
                (SHIFT, gdk::Key::Page_Down | gdk::Key::KP_Page_Down | gdk::Key::KP_3) => {
                    self.shift_down_key.set(Some(gdk::Key::Page_Down));
                    self.shift_down_row.replace(self.obj().focused_file_iter());
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
                    if let Some(focused) = self.obj().focused_file_iter() {
                        self.toggle_file(&focused);
                    }
                    return glib::Propagation::Proceed;
                }
                (SHIFT, gdk::Key::Home | gdk::Key::KP_Home | gdk::Key::KP_7) => {
                    self.shift_down_key.set(Some(gdk::Key::Home));
                    self.shift_down_row.replace(self.obj().focused_file_iter());
                    return glib::Propagation::Proceed;
                }
                (SHIFT, gdk::Key::End | gdk::Key::KP_End | gdk::Key::KP_1) => {
                    self.shift_down_key.set(Some(gdk::Key::End));
                    self.shift_down_row.replace(self.obj().focused_file_iter());
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
                    if let Some(file) = self.obj().selected_file() {
                        self.obj().show_dir_tree_size(&file);
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
                    if let Some(iter) = self.obj().focused_file_iter() {
                        if self.obj().store().iter_next(&iter) {
                            self.obj().focus_file_at_row(&iter);
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
                        self.shift_down_row.replace(self.obj().focused_file_iter());
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
            let row = self.row_at_coords(x, y);
            let row_and_file = row.and_then(|row| Some((row, self.obj().file_at_row(&row)?)));

            self.obj().emit_by_name::<()>(
                "list-clicked",
                &[&button, &row_and_file.as_ref().map(|f| &f.1)],
            );

            if let Some((row, file)) = row_and_file.as_ref() {
                let state = self.get_modifiers_state();

                self.modifier_click.set(state);

                if n_press == 2
                    && button == 1
                    && self.left_mouse_button_mode.get()
                        == LeftMouseButtonMode::OpensWithDoubleClick
                {
                    self.obj().emit_by_name::<()>("file-activated", &[file]);
                } else if n_press == 1 && button == 1 {
                    if state == Some(SHIFT) {
                        self.select_with_mouse(&row);
                    } else if state == Some(CONTROL) {
                        self.toggle_file(&row);
                    }
                    if state == Some(NO_MOD) {
                        if !self.is_selected_iter(&row) && self.left_mouse_button_unselects.get() {
                            self.obj().unselect_all();
                        }
                    }
                } else if n_press == 1 && button == 3 {
                    if !file.is_dotdot() {
                        if self.right_mouse_button_mode.get() == RightMouseButtonMode::Selects {
                            if self.obj().focused_file_iter().map_or(false, |focus_iter| {
                                iter_compare(&self.obj().store(), &focus_iter, &row)
                                    == cmp::Ordering::Equal
                            }) {
                                self.set_selected_at_iter(&row, true);
                                self.obj().emit_files_changed();
                                self.obj().show_file_popup(None);
                            } else {
                                if !self.is_selected_iter(&row) {
                                    self.set_selected_at_iter(&row, true);
                                } else {
                                    self.set_selected_at_iter(&row, false);
                                }
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
            let Some(row) = self.row_at_coords(x, y) else {
                return;
            };
            if let Some(file) = self.obj().file_at_row(&row) {
                if n_press == 1
                    && button == 1
                    && self.modifier_click.get() == Some(NO_MOD)
                    && self.left_mouse_button_mode.get()
                        == LeftMouseButtonMode::OpensWithSingleClick
                {
                    self.obj().emit_by_name::<()>("file-activated", &[&file]);
                }
            }
        }

        fn row_at_coords(&self, x: f64, y: f64) -> Option<gtk::TreeIter> {
            let (path, _) = self.obj().view().dest_row_at_pos(x as i32, y as i32)?;
            let iter = self.obj().store().iter(&path?)?;
            Some(iter)
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

        pub fn toggle_file(&self, iter: &gtk::TreeIter) {
            let file = self.file_at_iter(iter);
            if !file.is_dotdot() {
                self.set_selected_at_iter(iter, !self.is_selected_iter(iter));
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
                .map(|d| d.get_real_path())
            {
                self.add_to_cmdline(path);
            }
        }

        fn add_file_to_cmdline(&self, fullpath: bool) {
            if let Some(file) = self.obj().selected_file() {
                if fullpath {
                    self.add_to_cmdline(file.get_real_path());
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
                .filter_map(|f| f.get_uri_str())
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
            let file = row.and_then(|iter| self.obj().file_at_row(&iter));

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

            let files: glib::List<gio::File> = data
                .files
                .iter()
                .map(|uri| gio::File::for_uri(uri))
                .collect();

            let to = match data.destination {
                DroppingFilesDestination::Parent => self.obj().directory().and_then(|d| d.parent()),
                DroppingFilesDestination::Child(name) => self
                    .obj()
                    .find_file_by_name(&name)
                    .and_then(|iter| self.obj().file_at_row(&iter))
                    .and_downcast::<Directory>(),
                DroppingFilesDestination::This => self.obj().directory(),
            };
            let Some(dir) = to else { return };

            let _result = match data.transfer_type {
                GnomeCmdTransferType::COPY => {
                    gnome_cmd_copy_gfiles(
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
                    gnome_cmd_move_gfiles(
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
                    gnome_cmd_link_gfiles(
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

            dir.relist_files(&window, false).await;
            // main_win.focus_file_lists();
        }
    }

    fn iter_compare(
        store: &gtk::ListStore,
        iter1: &gtk::TreeIter,
        iter2: &gtk::TreeIter,
    ) -> cmp::Ordering {
        let path1 = store.path(iter1);
        let path2 = store.path(iter2);
        path1.cmp(&path2)
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

    fn type_string(file: &File) -> &'static str {
        match file.file_info().file_type() {
            gio::FileType::Regular => " ",
            gio::FileType::Directory => "/",
            gio::FileType::SymbolicLink => "@",
            gio::FileType::Special => "S",
            gio::FileType::Shortcut => "K",
            gio::FileType::Mountable => "M",
            _ => "?",
        }
    }

    fn display_permissions(permissions: u32, mode: PermissionDisplayMode) -> String {
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

pub mod ffi {
    use super::*;
    use crate::{connection::connection::ffi::GnomeCmdCon, dir::ffi::GnomeCmdDir};

    pub type GnomeCmdFileList = <super::FileList as glib::object::ObjectType>::GlibType;

    extern "C" {
        pub fn gnome_cmd_file_list_init(fl: *mut GnomeCmdFileList);
        pub fn gnome_cmd_file_list_finalize(fl: *mut GnomeCmdFileList);

        pub fn gnome_cmd_file_list_get_connection(fl: *mut GnomeCmdFileList) -> *mut GnomeCmdCon;

        pub fn gnome_cmd_file_list_get_directory(fl: *mut GnomeCmdFileList) -> *mut GnomeCmdDir;
        pub fn gnome_cmd_file_list_set_directory(fl: *mut GnomeCmdFileList, dir: *mut GnomeCmdDir);

        pub fn gnome_cmd_file_list_set_connection(
            fl: *mut GnomeCmdFileList,
            con: *mut GnomeCmdCon,
            start_dir: *mut GnomeCmdDir,
        );

        pub fn gnome_cmd_file_list_goto_directory(fl: *mut GnomeCmdFileList, dir: *const c_char);
    }
}

glib::wrapper! {
    pub struct FileList(ObjectSubclass<imp::FileList>)
        @extends gtk::Widget;
}

#[allow(non_camel_case_types)]
#[derive(Clone, Copy, PartialEq, Eq, strum::FromRepr, strum::VariantArray)]
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

#[allow(non_camel_case_types)]
enum DataColumns {
    DATA_COLUMN_FILE = 9,
    DATA_COLUMN_ICON_NAME,
    DATA_COLUMN_SELECTED,
    DATA_COLUMN_SIZE,
}

impl FileList {
    pub fn new(file_metadata_service: &FileMetadataService) -> Self {
        glib::Object::builder()
            .property("file-metadata-service", file_metadata_service)
            .build()
    }

    pub fn size(&self) -> usize {
        self.store()
            .iter_n_children(None)
            .try_into()
            .ok()
            .unwrap_or_default()
    }

    pub fn show_dir_tree_size(&self, f: &File) {
        if let Some(iter) = self.get_row_from_file(f) {
            self.imp().set_model_row(&iter, f, true);
        }
    }

    pub fn show_visible_tree_sizes(&self) {
        let _ = self.traverse_files::<()>(|file, iter, _store| {
            self.imp().set_model_row(iter, file, true);
            ControlFlow::Continue(())
        });
        self.emit_files_changed();
    }

    fn remove_file(&self, f: &File) -> bool {
        match self.get_row_from_file(f) {
            Some(iter) => {
                if self.store().remove(&iter) {
                    self.focus_file_at_row(&iter);
                }
                true
            }
            None => false,
        }
    }

    pub fn clear(&self) {
        self.store().clear();
    }

    pub fn visible_files(&self) -> glib::List<File> {
        let mut files = glib::List::new();
        let _ = self.traverse_files::<()>(|file, _iter, _store| {
            files.push_back(file.clone());
            ControlFlow::Continue(())
        });
        files
    }

    pub fn selected_files(&self) -> glib::List<File> {
        let mut list = glib::List::<File>::new();
        let _ = self.traverse_files::<()>(|file, iter, _store| {
            if self.imp().is_selected_iter(iter) {
                list.push_back(file.clone());
            }
            ControlFlow::Continue(())
        });
        if list.is_empty() {
            if let Some(file) = self.selected_file() {
                list.push_back(file);
            }
        }
        list
    }

    pub fn sorting(&self) -> (ColumnID, gtk::SortType) {
        self.imp()
            .sorting
            .get()
            .unwrap_or((ColumnID::COLUMN_NAME, gtk::SortType::Ascending))
    }

    pub fn set_sorting(&self, column: ColumnID, order: gtk::SortType) {
        self.imp().sorting.set(Some((column, order)));
        self.update_sorter();
    }

    pub fn show_column(&self, col: ColumnID, value: bool) {
        if let Some(column) = self.view().column(col as i32) {
            column.set_visible(value);
        }
    }

    pub fn has_file(&self, f: &File) -> bool {
        self.traverse_files::<()>(|file, _iter, _store| {
            if file == f {
                ControlFlow::Break(())
            } else {
                ControlFlow::Continue(())
            }
        })
        .is_break()
    }

    pub fn file_at_row(&self, iter: &gtk::TreeIter) -> Option<File> {
        self.store()
            .get_value(iter, DataColumns::DATA_COLUMN_FILE as i32)
            .get()
            .ok()
    }

    pub fn focused_file_iter(&self) -> Option<gtk::TreeIter> {
        let path = TreeViewExt::cursor(&self.view()).0?;
        let iter = self.store().iter(&path)?;
        Some(iter)
    }

    pub fn focused_file(&self) -> Option<File> {
        let iter = self.focused_file_iter()?;
        let file = self.file_at_row(&iter)?;
        Some(file)
    }

    pub fn selected_file(&self) -> Option<File> {
        self.focused_file().filter(|f| !f.is_dotdot())
    }

    pub fn toggle(&self) {
        if let Some(iter) = self.focused_file_iter() {
            self.imp().toggle_file(&iter);
        }
    }

    pub fn toggle_and_step(&self) {
        if let Some(iter) = self.focused_file_iter() {
            self.imp().toggle_file(&iter);
            if self.store().iter_next(&iter) {
                self.focus_file_at_row(&iter);
            }
        }
    }

    pub fn select_all(&self) {
        if self.select_dirs() {
            let _ = self.traverse_files::<()>(|file, iter, _store| {
                if !file.is_dotdot() {
                    self.imp().set_selected_at_iter(iter, true);
                }
                ControlFlow::Continue(())
            });
        } else {
            let _ = self.traverse_files::<()>(|file, iter, _store| {
                if !file.is_dotdot() && !file.is::<Directory>() {
                    self.imp().set_selected_at_iter(iter, true);
                }
                ControlFlow::Continue(())
            });
        }
        self.emit_files_changed();
    }

    pub fn select_all_files(&self) {
        let _ = self.traverse_files::<()>(|file, iter, _store| {
            if !file.is_dotdot() {
                self.imp()
                    .set_selected_at_iter(iter, !file.is::<Directory>());
            }
            ControlFlow::Continue(())
        });
    }

    pub fn unselect_all_files(&self) {
        let _ = self.traverse_files::<()>(|file, iter, _store| {
            if !file.is::<Directory>() {
                self.imp().set_selected_at_iter(iter, false);
            }
            ControlFlow::Continue(())
        });
    }

    pub fn unselect_all(&self) {
        let _ = self.traverse_files::<()>(|_file, iter, _store| {
            self.imp().set_selected_at_iter(iter, false);
            ControlFlow::Continue(())
        });
    }

    pub fn focus_prev(&self) {
        let view = self.view();
        if let Some(mut path) = TreeViewExt::cursor(&view).0 {
            if path.prev() {
                TreeViewExt::set_cursor(&view, &path, None, false);
            }
        }
    }

    pub fn focus_next(&self) {
        let view = self.view();
        if let Some(mut path) = TreeViewExt::cursor(&view).0 {
            path.next();
            TreeViewExt::set_cursor(&view, &path, None, false);
        }
    }

    pub fn connection(&self) -> Option<Connection> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_list_get_connection(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn directory(&self) -> Option<Directory> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_file_list_get_directory(
                self.to_glib_none().0,
            ))
        }
    }

    pub fn set_directory(&self, directory: &Directory) {
        unsafe {
            ffi::gnome_cmd_file_list_set_directory(
                self.to_glib_none().0,
                directory.to_glib_none().0,
            )
        }
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
        directory.relist_files(&window, true).await;
    }

    pub fn append_file(&self, file: &File) {
        self.imp().add_file(file, None);
    }

    pub fn set_connection(&self, connection: &impl IsA<Connection>, start_dir: Option<&Directory>) {
        unsafe {
            ffi::gnome_cmd_file_list_set_connection(
                self.to_glib_none().0,
                connection.as_ref().to_glib_none().0,
                start_dir.to_glib_none().0,
            )
        }
    }

    pub(super) fn get_row_from_file(&self, f: &File) -> Option<gtk::TreeIter> {
        let result = self.traverse_files::<gtk::TreeIter>(|file, iter, _store| {
            if file == f {
                ControlFlow::Break(iter.clone())
            } else {
                ControlFlow::Continue(())
            }
        });
        match result {
            ControlFlow::Break(iter) => Some(iter),
            ControlFlow::Continue(()) => None,
        }
    }

    pub(super) fn focus_file_at_row(&self, row: &gtk::TreeIter) {
        let path = self.store().path(row);
        gtk::prelude::TreeViewExt::set_cursor(&self.view(), &path, None, false);
    }

    fn find_file_by_name(&self, name: &Path) -> Option<gtk::TreeIter> {
        let result = self.traverse_files::<gtk::TreeIter>(|f, iter, _store| {
            if f.file_info().name() == name {
                ControlFlow::Break(iter.clone())
            } else {
                ControlFlow::Continue(())
            }
        });
        match result {
            ControlFlow::Break(iter) => Some(iter),
            ControlFlow::Continue(()) => None,
        }
    }

    pub fn focus_file(&self, focus_file: &Path, scroll_to_file: bool) {
        match self.find_file_by_name(focus_file) {
            Some(iter) => {
                self.focus_file_at_row(&iter);
                if scroll_to_file {
                    let path = self.store().path(&iter);
                    self.view()
                        .scroll_to_cell(Some(&path), None, false, 0.0, 0.0);
                }
            }
            None => {
                /* The file was not found, remember the filename in case the file gets
                added to the list in the future (after a FAM event etc). */
                self.imp().focus_later.replace(Some(focus_file.to_owned()));
            }
        }
    }

    pub fn select_row(&self, row: Option<gtk::TreeIter>) {
        if let Some(row) = row.or_else(|| self.store().iter_first()) {
            self.focus_file_at_row(&row);
        }
    }

    pub fn toggle_with_pattern(&self, pattern: &Filter, mode: bool) {
        let select_dirs = self.select_dirs();
        let _ = self.traverse_files::<()>(|file, iter, store| {
            if !file.is_dotdot()
                && (select_dirs || file.downcast_ref::<Directory>().is_none())
                && pattern.matches(&file.file_info().display_name())
            {
                store.set(iter, &[(DataColumns::DATA_COLUMN_SELECTED as u32, &mode)]);
            }
            ControlFlow::Continue(())
        });
        self.emit_files_changed();
    }

    pub fn goto_directory(&self, dir: &Path) {
        unsafe {
            ffi::gnome_cmd_file_list_goto_directory(self.to_glib_none().0, dir.to_glib_none().0)
        }
    }

    pub fn traverse_files<T>(
        &self,
        mut visitor: impl FnMut(&File, &gtk::TreeIter, &gtk::ListStore) -> ControlFlow<T>,
    ) -> ControlFlow<T> {
        let store = self.store();
        if let Some(iter) = store.iter_first() {
            loop {
                let file: File =
                    TreeModelExtManual::get(&store, &iter, DataColumns::DATA_COLUMN_FILE as i32);
                (visitor)(&file, &iter, &store)?;
                if !store.iter_next(&iter) {
                    break;
                }
            }
        }
        ControlFlow::Continue(())
    }

    fn emit_files_changed(&self) {
        self.emit_by_name::<()>("files-changed", &[]);
    }

    pub fn set_selected_files(&self, files: &HashSet<File>) {
        let _ = self.traverse_files::<()>(|file, iter, store| {
            let selected = files.contains(file);
            store.set(
                iter,
                &[(DataColumns::DATA_COLUMN_SELECTED as u32, &selected)],
            );
            ControlFlow::Continue(())
        });
        self.emit_files_changed();
    }

    pub fn toggle_files_with_same_extension(&self, select: bool) {
        let Some(ext) = self.selected_file().and_then(|f| f.extension()) else {
            return;
        };
        let _ = self.traverse_files::<()>(|file, iter, store| {
            if !file.is_dotdot() && file.extension().as_ref() == Some(&ext) {
                store.set(iter, &[(DataColumns::DATA_COLUMN_SELECTED as u32, &select)]);
            }
            ControlFlow::Continue(())
        });
        self.emit_files_changed();
    }

    pub fn invert_selection(&self) {
        let select_dirs = self.select_dirs();
        let _ = self.traverse_files::<()>(|file, iter, store| {
            if !file.is_dotdot() && (select_dirs || file.downcast_ref::<Directory>().is_none()) {
                let selected: bool =
                    TreeModelExtManual::get(store, iter, DataColumns::DATA_COLUMN_SELECTED as i32);
                store.set(
                    iter,
                    &[(DataColumns::DATA_COLUMN_SELECTED as u32, &!selected)],
                );
            }
            ControlFlow::Continue(())
        });
    }

    pub fn restore_selection(&self) {
        // TODO: implement
    }

    pub fn stats(&self) -> FileListStats {
        let mut stats = FileListStats::default();
        let _ = self.traverse_files::<()>(|file, iter, store| {
            if !file.is_dotdot() {
                let info = file.file_info();
                let selected: bool =
                    TreeModelExtManual::get(store, iter, DataColumns::DATA_COLUMN_SELECTED as i32);
                let cached_size: u64 =
                    TreeModelExtManual::get(store, iter, DataColumns::DATA_COLUMN_SIZE as i32);

                match info.file_type() {
                    gio::FileType::Directory => {
                        stats.total.directories += 1;
                        if selected {
                            stats.selected.directories += 1;
                        }
                        if cached_size != u64::MAX {
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
            ControlFlow::Continue(())
        });
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
        self.imp().color_theme.replace(ColorThemes::new().theme());
        self.imp()
            .ls_palette
            .replace(load_palette(&ColorOptions::new().0));

        self.update_sorter();

        // TODO: Maybe??? gtk_cell_renderer_set_fixed_size (priv->columns[1], )
        // gtk_clist_set_row_height (*this, gnome_cmd_data.options.list_row_height);

        // let font_desc = pango::FontDescription::from_string(&self.font_name());
        // gtk_widget_override_font (*this, font_desc);

        self.queue_draw();
    }

    fn update_sorter(&self) {
        let (column, sort_type) = self.sorting();

        let file_sorter = match column {
            ColumnID::COLUMN_ICON => return,
            ColumnID::COLUMN_NAME => FileAttrSorter::by_name(),
            ColumnID::COLUMN_EXT => FileAttrSorter::by_ext(),
            ColumnID::COLUMN_DIR => FileAttrSorter::by_dir(),
            ColumnID::COLUMN_SIZE => FileAttrSorter::by_size(),
            ColumnID::COLUMN_DATE => FileAttrSorter::by_date(),
            ColumnID::COLUMN_PERM => FileAttrSorter::by_perm(),
            ColumnID::COLUMN_OWNER => FileAttrSorter::by_owner(),
            ColumnID::COLUMN_GROUP => FileAttrSorter::by_group(),
        };

        let full_sorter = gtk::MultiSorter::new();
        full_sorter.append(FileTypeSorter::default());
        if sort_type == gtk::SortType::Descending {
            full_sorter.append(reverse_sorter(file_sorter));
        } else {
            full_sorter.append(file_sorter);
        }

        self.set_sorter(full_sorter);
    }

    fn sort_by(&self, col: ColumnID) {
        let (column, order) = self.sorting();
        let sort_type = if column == col {
            match order {
                gtk::SortType::Ascending => gtk::SortType::Descending,
                _ => gtk::SortType::Ascending,
            }
        } else {
            col.default_sort_direction()
        };
        self.set_sorting(col, sort_type);
        self.imp().update_column_sort_arrows();
        self.sort();
    }

    fn sort(&self) {
        let selfile = self.selected_file();

        let store = self.store();
        let sorter = self.sorter().unwrap();

        let size = store.iter_n_children(None) as u32;
        let mut indexes: Vec<u32> = (0..size).collect();
        indexes.sort_by(|index1, index2| {
            let iter1 = store.iter_nth_child(None, *index1 as i32).unwrap();
            let iter2 = store.iter_nth_child(None, *index2 as i32).unwrap();

            let file1: File =
                TreeModelExtManual::get(&store, &iter1, DataColumns::DATA_COLUMN_FILE as i32);
            let file2: File =
                TreeModelExtManual::get(&store, &iter2, DataColumns::DATA_COLUMN_FILE as i32);

            sorter.compare(&file1, &file2).into()
        });
        store.reorder(&indexes);

        // refocus the previously selected file if this file list has the focus
        if self.has_focus() {
            if let Some(row) = selfile.and_then(|f| self.get_row_from_file(&f)) {
                self.focus_file_at_row(&row);
            }
        }
    }

    pub fn show_files(&self, dir: &Directory) {
        self.clear();

        let options = FiltersOptions::new();

        let mut files: Vec<_> = dir
            .files()
            .iter::<File>()
            .flatten()
            .filter(|f| file_is_wanted(f, &options))
            .collect();
        if dir.parent().is_some() {
            files.insert(0, File::dotdot(dir));
        }
        if let Some(sorter) = self.sorter() {
            files.sort_by(|a, b| sorter.compare(a, b).into());
        }
        for f in files {
            self.append_file(&f);
        }

        if self.is_realized() {
            self.view().scroll_to_point(0, 0);
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

    fn focus_row_coordinates(&self) -> Option<gdk::Rectangle> {
        let options = GeneralOptions::new();

        let view = self.view();
        let path = gtk::prelude::TreeViewExt::cursor(&view).0?;
        let name_rect = view.cell_area(
            Some(&path),
            Some(&view.column(ColumnID::COLUMN_NAME as i32)?),
        );
        let mut rect = if options.extension_display_mode() != ExtensionDisplayMode::Both {
            let ext_rect = view.cell_area(
                Some(&path),
                Some(&view.column(ColumnID::COLUMN_EXT as i32)?),
            );
            name_rect.union(&ext_rect)
        } else {
            name_rect
        };
        let (x, y) = view.convert_bin_window_to_widget_coords(rect.x(), rect.y());
        rect.set_x(x);
        rect.set_y(y);
        Some(rect)
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

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_get_type() -> GType {
    FileList::static_type().into_glib()
}

fn file_is_wanted(file: &File, options: &dyn FiltersOptionsRead) -> bool {
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
                gio::FileType::Unknown => options.hide_unknown(),
                gio::FileType::Regular => options.hide_regular(),
                gio::FileType::Directory => options.hide_directory(),
                gio::FileType::SymbolicLink => options.hide_symlink(),
                gio::FileType::Special => options.hide_special(),
                gio::FileType::Shortcut => options.hide_shortcut(),
                gio::FileType::Mountable => options.hide_mountable(),
                _ => false,
            };
            if hide {
                return false;
            }
            if options.hide_symlink() && file_info.is_symlink() {
                return false;
            }
            if options.hide_hidden() && file_info.is_hidden() {
                return false;
            }
            if options.hide_virtual() && file_info.boolean(gio::FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL)
            {
                return false;
            }
            if options.hide_volatile()
                && file_info.boolean(gio::FILE_ATTRIBUTE_STANDARD_IS_VOLATILE)
            {
                return false;
            }
            if options.hide_backup() && matches_pattern(basename, &options.backup_pattern()) {
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

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_get_sort_column(fl: *mut ffi::GnomeCmdFileList) -> i32 {
    let fl: Borrowed<FileList> = unsafe { from_glib_borrow(fl) };
    fl.sorting().0 as i32
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_sort(fl: *mut ffi::GnomeCmdFileList) {
    let fl: Borrowed<FileList> = unsafe { from_glib_borrow(fl) };
    fl.sort();
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_insert_file(
    fl: *mut ffi::GnomeCmdFileList,
    f: *mut GnomeCmdFile,
) -> gboolean {
    let fl: Borrowed<FileList> = unsafe { from_glib_borrow(fl) };
    let f: Borrowed<File> = unsafe { from_glib_borrow(f) };
    fl.imp().insert_file(&*f).into_glib()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_focus_file(
    fl: *mut ffi::GnomeCmdFileList,
    f: *const c_char,
    scroll: gboolean,
) {
    let fl: Borrowed<FileList> = unsafe { from_glib_borrow(fl) };
    let f: PathBuf = unsafe { from_glib_none(f) };
    fl.focus_file(&f, scroll != 0);
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_remove_file(
    fl: *mut ffi::GnomeCmdFileList,
    f: *mut GnomeCmdFile,
) -> gboolean {
    let fl: Borrowed<FileList> = unsafe { from_glib_borrow(fl) };
    let f: Borrowed<File> = unsafe { from_glib_borrow(f) };
    fl.remove_file(&*f).into_glib()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_has_file(
    fl: *mut ffi::GnomeCmdFileList,
    f: *mut GnomeCmdFile,
) -> gboolean {
    let fl: Borrowed<FileList> = unsafe { from_glib_borrow(fl) };
    let f: Borrowed<File> = unsafe { from_glib_borrow(f) };
    fl.has_file(&*f).into_glib()
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_list_update_file(
    fl: *mut ffi::GnomeCmdFileList,
    f: *mut GnomeCmdFile,
) {
    let fl: Borrowed<FileList> = unsafe { from_glib_borrow(fl) };
    let f: Borrowed<File> = unsafe { from_glib_borrow(f) };
    fl.imp().update_file(&*f);
}

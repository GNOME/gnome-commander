// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    search_progress_dialog::SearchProgressDialog,
    searcher::{SearchProgress, Searcher},
    text_render::TextRender,
};
use crate::{
    file::{File, FileOps},
    tags::FileMetadataService,
    utils::{MenuBuilderExt, extract_menu_shortcuts, pending, u32_enum},
};
use gettextrs::{gettext, ngettext};
use gtk::{
    gio,
    glib::{self, translate::*},
    prelude::*,
    subclass::prelude::*,
};

const IMAGE_SCALE_FACTORS: &[f64] = &[
    0.1, 0.2, 0.33, 0.5, 0.67, 1.0, 1.25, 1.50, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
];

u32_enum! {
    pub enum DisplayMode {
        #[default]
        Text,
        Binary,
        Hexdump,
        Image,
    }
}

impl DisplayMode {
    pub fn guess(file: &File) -> Option<Self> {
        let content_type = file.content_type().map(|ct| ct.to_lowercase())?;
        if content_type.starts_with("text/") {
            Some(Self::Text)
        } else if content_type.starts_with("image/") {
            Some(Self::Image)
        } else if content_type.starts_with("application/") {
            Some(Self::Binary)
        } else {
            None
        }
    }
}

mod imp {
    use super::*;
    use crate::{
        file_metainfo_view::FileMetainfoView,
        intviewer::{
            image_render::{ImageOperation, ImageRender},
            search_dialog::{SearchDialog, SearchRequest},
            text_render::TextRenderDisplayMode,
        },
        options::{ViewerOptions, utils::remember_window_size},
        utils::display_help,
    };
    use gtk::gdk;
    use std::cell::{Cell, OnceCell, RefCell};

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::ViewerWindow)]
    pub struct ViewerWindow {
        menubar: gtk::PopoverMenuBar,
        shortcuts_controller: RefCell<gtk::ShortcutController>,
        pub stack: gtk::Stack,
        pub text_render: TextRender,
        pub image_render: ImageRender,
        pub status_label: gtk::Label,
        pub metadata_view: OnceCell<FileMetainfoView>,

        #[property(get, construct_only)]
        pub file_metadata_service: OnceCell<FileMetadataService>,

        #[property(get, set)]
        pub file: RefCell<Option<File>>,
        pub searcher: RefCell<Option<Searcher>>,
        pub search: RefCell<SearchRequest>,
        pub current_scale_index: Cell<usize>,
        pub img_initialized: Cell<bool>,

        #[property(get, set = Self::set_display_mode)]
        display_mode: Cell<DisplayMode>,

        #[property(get, set)]
        pub wrap_lines: Cell<bool>,
        #[property(get, set)]
        pub encoding: RefCell<String>,
        #[property(get, set)]
        pub chars_per_line: Cell<i32>,
        #[property(get, set)]
        pub hexadecimal_offset: Cell<bool>,
        #[property(get, set)]
        pub metadata_visible: Cell<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ViewerWindow {
        const NAME: &'static str = "GnomeCmdViewerWindow";
        type Type = super::ViewerWindow;
        type ParentType = gtk::Window;

        fn class_init(klass: &mut Self::Class) {
            klass.install_action("viewer.copy-text-selection", None, |obj, _, _| {
                obj.imp().copy_selection()
            });
            klass.install_action("viewer.close", None, |obj, _, _| obj.close());
            klass.install_action("viewer.zoom-in", None, |obj, _, _| obj.imp().zoom_in());
            klass.install_action("viewer.zoom-out", None, |obj, _, _| obj.imp().zoom_out());
            klass.install_action("viewer.normal-size", None, |obj, _, _| {
                obj.imp().zoom_normal()
            });
            klass.install_action("viewer.best-fit", None, |obj, _, _| {
                obj.imp().zoom_best_fit()
            });
            klass.install_action_async("viewer.find", None, |obj, _, _| async move {
                obj.imp().find().await
            });
            klass.install_action_async("viewer.find-next", None, |obj, _, _| async move {
                obj.imp().find_next().await
            });
            klass.install_action_async("viewer.find-previous", None, |obj, _, _| async move {
                obj.imp().find_prev().await
            });
            klass.install_property_action("viewer.display-mode", "display-mode");
            klass.install_property_action("viewer.wrap-lines", "wrap-lines");
            klass.install_property_action("viewer.encoding", "encoding");
            klass.install_action(
                "viewer.imageop",
                Some(glib::VariantTy::INT32),
                |obj, _, p| obj.imp().image_operation(p),
            );
            klass.install_property_action("viewer.chars-per-line", "chars-per-line");
            klass.install_property_action("viewer.hexadecimal-offset", "hexadecimal-offset");
            klass.install_property_action("viewer.metadata-visible", "metadata-visible");
            klass.install_action_async("viewer.quick-help", None, |w, _, _| async move {
                display_help(w.upcast_ref(), Some("gnome-commander-internal-viewer")).await
            });
            klass.install_action_async("viewer.keyboard-shortcuts", None, |w, _, _| async move {
                display_help(
                    w.upcast_ref(),
                    Some("gnome-commander-internal-viewer-keyboard"),
                )
                .await
            });
        }

        fn new() -> Self {
            let image_render = ImageRender::new();
            image_render.set_best_fit(true);
            image_render.set_scale_factor(1.0);

            let display_mode = Default::default();
            let menu = create_menu(display_mode);
            Self {
                menubar: gtk::PopoverMenuBar::from_model(Some(&menu)),
                shortcuts_controller: RefCell::new(gtk::ShortcutController::for_model(
                    &extract_menu_shortcuts(menu.upcast_ref()),
                )),
                stack: gtk::Stack::builder().vexpand(true).build(),
                text_render: TextRender::new(),
                image_render,
                status_label: gtk::Label::default(),
                metadata_view: Default::default(),

                file_metadata_service: Default::default(),

                file: Default::default(),
                searcher: Default::default(),
                search: RefCell::new(SearchRequest::Text {
                    pattern: glib::GString::new(),
                    case_sensitive: true,
                }),
                current_scale_index: Cell::new(5),
                img_initialized: Default::default(),
                display_mode: Cell::new(display_mode),

                wrap_lines: Cell::new(true),
                encoding: Default::default(),
                hexadecimal_offset: Cell::new(true),
                chars_per_line: Cell::new(20),
                metadata_visible: Cell::new(true),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for ViewerWindow {
        fn constructed(&self) {
            self.parent_constructed();

            let window = self.obj();

            window.set_icon_name(Some("gnome-commander-internal-viewer"));
            window.set_title(Some("GViewer"));

            let vbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .spacing(6)
                .build();
            window.set_child(Some(&vbox));

            vbox.append(&self.menubar);

            let tscrollbox = gtk::ScrolledWindow::builder()
                .child(&self.text_render)
                .hexpand(true)
                .vexpand(true)
                .build();

            let iscrollbox = gtk::ScrolledWindow::builder()
                .child(&self.image_render)
                .hexpand(true)
                .vexpand(true)
                .build();

            self.stack.add_named(&tscrollbox, Some("text"));
            self.stack.add_named(&iscrollbox, Some("image"));
            self.stack.set_visible_child_name("text");
            vbox.append(&self.stack);

            self.text_render.connect_text_status_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.text_status_update()
            ));
            self.image_render.connect_image_status_changed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.image_status_update()
            ));

            let button_gesture = gtk::GestureClick::builder().button(3).build();
            button_gesture.connect_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, n_press, x, y| imp.on_text_viewer_button_pressed(n_press, x, y)
            ));
            self.text_render.add_controller(button_gesture);

            let statusbar = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(6)
                .margin_start(12)
                .margin_end(12)
                .build();
            statusbar.append(&self.status_label);
            vbox.append(&statusbar);

            let metadata_view = FileMetainfoView::new(&window.file_metadata_service());
            metadata_view.set_vexpand(true);
            window
                .bind_property("metadata-visible", &metadata_view, "visible")
                .bidirectional()
                .build();
            vbox.append(&metadata_view);
            self.metadata_view.set(metadata_view).ok().unwrap();

            let options = ViewerOptions::new();

            remember_window_size(&*window, &options.window_width, &options.window_height);

            options
                .font_size
                .bind(&self.text_render, "font-size")
                .build();
            options.tab_size.bind(&self.text_render, "tab-size").build();

            options.wrap_mode.bind(&*window, "wrap-lines").build();
            window
                .bind_property("wrap-lines", &self.text_render, "wrap-mode")
                .bidirectional()
                .sync_create()
                .build();

            options.encoding.bind(&*window, "encoding").build();
            window
                .bind_property("encoding", &self.text_render, "encoding")
                .bidirectional()
                .sync_create()
                .build();

            options
                .binary_bytes_per_line
                .bind(&*window, "chars-per-line")
                .build();
            window
                .bind_property("chars-per-line", &self.text_render, "fixed-limit")
                .bidirectional()
                .sync_create()
                .build();

            options
                .metadata_visible
                .bind(&*window, "metadata-visible")
                .build();

            options
                .display_hex_offset
                .bind(&*window, "hexadecimal-offset")
                .build();
            window
                .bind_property(
                    "hexadecimal-offset",
                    &self.text_render,
                    "hexadecimal-offset",
                )
                .bidirectional()
                .sync_create()
                .build();

            let key_controller = gtk::EventControllerKey::new();
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.key_pressed(key, state)
            ));
            window.add_controller(key_controller);

            window.add_controller(self.shortcuts_controller.borrow().clone());
        }
    }

    impl WidgetImpl for ViewerWindow {}
    impl WindowImpl for ViewerWindow {}

    impl ViewerWindow {
        fn set_display_mode(&self, mode: DisplayMode) {
            self.display_mode.set(mode);

            if mode == DisplayMode::Image && !self.img_initialized.get() {
                // do lazy-initialization of the image render, only when the user first asks to display the file as image
                if let Some(path) = self.obj().file().and_then(|f| f.local_path()) {
                    self.img_initialized.set(true);
                    self.image_render.load_file(&path);
                } else {
                    eprintln!("ViewerWindow::set_display_mode: file path is None");
                }
            }

            match mode {
                DisplayMode::Text => {
                    self.text_render
                        .set_display_mode(TextRenderDisplayMode::Text);
                    self.stack.set_visible_child_name("text");
                    self.text_render.notify_status_changed();
                }
                DisplayMode::Binary => {
                    self.text_render
                        .set_display_mode(TextRenderDisplayMode::Binary);
                    self.stack.set_visible_child_name("text");
                    self.text_render.notify_status_changed();
                }
                DisplayMode::Hexdump => {
                    self.text_render
                        .set_display_mode(TextRenderDisplayMode::Hexdump);
                    self.stack.set_visible_child_name("text");
                    self.text_render.notify_status_changed();
                }
                DisplayMode::Image => {
                    self.stack.set_visible_child_name("image");
                    self.image_render.notify_status_changed();
                }
            }

            let menu = create_menu(mode);
            self.menubar.set_menu_model(Some(&menu));
            self.obj()
                .remove_controller(&self.shortcuts_controller.replace(
                    gtk::ShortcutController::for_model(&extract_menu_shortcuts(menu.upcast_ref())),
                ));
            self.obj()
                .add_controller(self.shortcuts_controller.borrow().clone());
        }

        fn text_status_update(&self) {
            let position = gettext("Position: {offset} of {size}")
                .replace("{offset}", &self.text_render.current_offset().to_string())
                .replace("{size}", &self.text_render.size().to_string());

            let column =
                gettext("Column: {}").replace("{}", &self.text_render.column().to_string());

            let wrap = if self.text_render.wrap_mode() {
                &gettext("Wrap")
            } else {
                ""
            };

            self.status_label
                .set_text(&format!("{position}\t{column}\t{wrap}"));
        }

        fn image_status_update(&self) {
            let Some(pixbuf) = self.image_render.origin_pixbuf() else {
                self.status_label.set_text("");
                return;
            };

            let width = pixbuf.width();
            let height = pixbuf.height();
            let bits_per_sample = pixbuf.bits_per_sample();

            let size = ngettext(
                "{width} x {height} pixel",
                "{width} x {height} pixels",
                height as u32,
            )
            .replace("{width}", &width.to_string())
            .replace("{height}", &height.to_string());

            let depth = ngettext("{} bit/sample", "{} bits/sample", bits_per_sample as u32)
                .replace("{}", &bits_per_sample.to_string());

            let scale = if !self.image_render.best_fit() {
                format!("{}%", (self.image_render.scale_factor() * 100.0) as i64)
            } else {
                gettext("(fit to window)")
            };

            self.status_label
                .set_text(&format!("{size}\t{depth}\t{scale}"));
        }

        fn copy_selection(&self) {
            if self.display_mode.get() != DisplayMode::Image {
                self.text_render.copy_selection();
            }
        }

        fn zoom_in(&self) {
            match self.display_mode.get() {
                DisplayMode::Text | DisplayMode::Binary | DisplayMode::Hexdump => {
                    let size = self.text_render.font_size();
                    if size != 0 && size <= 32 {
                        self.text_render.set_font_size(size + 1);
                    }
                }
                DisplayMode::Image => {
                    self.image_render.set_best_fit(false);

                    let index = (self.current_scale_index.get() + 1)
                        .clamp(0, IMAGE_SCALE_FACTORS.len() - 1);
                    self.current_scale_index.set(index);

                    let scale_factor = IMAGE_SCALE_FACTORS[index];
                    if self.image_render.scale_factor() != scale_factor {
                        self.image_render.set_scale_factor(scale_factor);
                    }
                }
            }
        }

        fn zoom_out(&self) {
            match self.display_mode.get() {
                DisplayMode::Text | DisplayMode::Binary | DisplayMode::Hexdump => {
                    let size = self.text_render.font_size();
                    if size >= 4 {
                        self.text_render.set_font_size(size - 1);
                    }
                }
                DisplayMode::Image => {
                    self.image_render.set_best_fit(false);

                    let index = (self.current_scale_index.get() - 1)
                        .clamp(0, IMAGE_SCALE_FACTORS.len() - 1);
                    self.current_scale_index.set(index);

                    let scale_factor = IMAGE_SCALE_FACTORS[index];
                    if self.image_render.scale_factor() != scale_factor {
                        self.image_render.set_scale_factor(scale_factor);
                    }
                }
            }
        }

        fn zoom_normal(&self) {
            match self.display_mode.get() {
                DisplayMode::Text | DisplayMode::Binary | DisplayMode::Hexdump => {
                    self.text_render.set_font_size(12);
                }
                DisplayMode::Image => {
                    self.image_render.set_best_fit(true);
                    self.current_scale_index.set(5); // index of 1.0 in IMAGE_SCALE_FACTORS
                    self.image_render.set_scale_factor(1.0);
                }
            }
        }

        fn zoom_best_fit(&self) {
            match self.display_mode.get() {
                DisplayMode::Text | DisplayMode::Binary | DisplayMode::Hexdump => {
                    // nothing to do
                }
                DisplayMode::Image => {
                    self.image_render.set_best_fit(true);
                }
            }
        }

        #[async_recursion::async_recursion(?Send)]
        async fn find(&self) {
            if self.display_mode.get() == DisplayMode::Image {
                return;
            }

            // Show the Search Dialog
            let Some(request) = SearchDialog::show(self.obj().upcast_ref()).await else {
                return;
            };

            let searcher = match &request {
                SearchRequest::Text {
                    pattern,
                    case_sensitive,
                } => Searcher::new_text_search(
                    self.text_render.input_mode().unwrap(),
                    self.text_render.current_offset(),
                    self.text_render
                        .input_mode()
                        .map(|f| f.max_offset())
                        .unwrap_or_default(),
                    pattern,
                    *case_sensitive,
                ),
                SearchRequest::Binary { pattern, .. } => Searcher::new_hex_search(
                    self.text_render.input_mode().unwrap(),
                    self.text_render.current_offset(),
                    self.text_render
                        .input_mode()
                        .map(|f| f.max_offset())
                        .unwrap_or_default(),
                    pattern,
                ),
            };

            if let Some(searcher) = searcher {
                self.search.replace(request);
                self.searcher.replace(Some(searcher));

                // call "find_next" to actually do the search
                self.find_next().await;
            }
        }

        async fn find_next(&self) {
            if self.display_mode.get() == DisplayMode::Image {
                return;
            }

            if self.searcher.borrow().is_none() {
                // if no search is active, call "menu_edit_find". (which will call "menu_edit_find_next" again
                self.find().await;
            } else {
                start_search(&self.obj(), true).await;
            }
        }

        async fn find_prev(&self) {
            if self.display_mode.get() == DisplayMode::Image {
                return;
            }

            if self.searcher.borrow().is_some() {
                start_search(&self.obj(), false).await;
            }
        }

        fn image_operation(&self, op: Option<&glib::Variant>) {
            let Some(operation) = op
                .and_then(|v| v.get::<i32>())
                .and_then(ImageOperation::from_repr)
            else {
                return;
            };
            self.image_render.operation(operation);
            self.image_render.queue_draw();
        }

        fn key_pressed(&self, key: gdk::Key, state: gdk::ModifierType) -> glib::Propagation {
            match key {
                gdk::Key::plus | gdk::Key::KP_Add | gdk::Key::equal => {
                    self.zoom_in();
                    glib::Propagation::Stop
                }

                gdk::Key::minus | gdk::Key::KP_Subtract => {
                    self.zoom_out();
                    glib::Propagation::Stop
                }
                gdk::Key::Q | gdk::Key::q if state == gdk::ModifierType::CONTROL_MASK => {
                    self.obj().close();
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
        }

        fn on_text_viewer_button_pressed(&self, n_press: i32, x: f64, y: f64) {
            if n_press == 1 {
                let menu = gio::Menu::new();
                menu.append(
                    Some(&gettext("_Copy selection")),
                    Some("viewer.copy-text-selection"),
                );

                let popover = gtk::PopoverMenu::from_model(Some(&menu));
                popover.set_parent(&*self.obj());
                popover.set_position(gtk::PositionType::Bottom);
                popover.set_pointing_to(Some(&gdk::Rectangle::new(x as i32, y as i32, 0, 0)));
                popover.popup();
            }
        }
    }
}

glib::wrapper! {
    pub struct ViewerWindow(ObjectSubclass<imp::ViewerWindow>)
        @extends gtk::Widget, gtk::Window,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Native, gtk::Root;
}

impl ViewerWindow {
    pub fn file_view(file: &File, file_metadata_service: &FileMetadataService) -> Self {
        let w: Self = glib::Object::builder()
            .property("file-metadata-service", file_metadata_service)
            .build();
        w.load_file(file);
        w.imp().stack.grab_focus();
        w
    }

    fn load_file(&self, file: &File) {
        let Some(path) = file.local_path() else {
            eprintln!("ViewerWindow::load_file: file path is None");
            return;
        };

        self.set_file(file);

        self.imp().text_render.load_file(&path);
        self.set_display_mode(DisplayMode::guess(file).unwrap_or_default());

        self.imp()
            .metadata_view
            .get()
            .unwrap()
            .set_property("file", file);

        self.set_title(path.to_str());
    }
}

async fn say_not_found(parent: &gtk::Window, search_pattern: &str) {
    let _answer = gtk::AlertDialog::builder()
        .modal(true)
        .message(gettext("Pattern “{}” was not found").replace("{}", search_pattern))
        .buttons([gettext("_Dismiss")])
        .cancel_button(0)
        .default_button(0)
        .build()
        .choose_future(Some(parent))
        .await;
}

async fn start_search(window: &ViewerWindow, forward: bool) {
    let Some(searcher) = window.imp().searcher.borrow().clone() else {
        return;
    };
    let search = window.imp().search.borrow().clone();

    let (sender, receiver) = async_channel::bounded::<SearchProgress>(1);

    let thread = std::thread::spawn(glib::clone!(
        #[strong]
        searcher,
        move || {
            let sender1 = sender.clone();
            let found = searcher.search(forward, move |p| {
                sender1.send_blocking(SearchProgress::Progress(p)).unwrap()
            });
            sender.send_blocking(SearchProgress::Done(found)).unwrap();
        }
    ));

    let progress_dlg = SearchProgressDialog::new(window.upcast_ref(), &search.to_string());
    progress_dlg.connect_stop(glib::clone!(
        #[weak]
        searcher,
        move || searcher.abort()
    ));

    progress_dlg.present();
    let found = loop {
        let message = receiver.recv().await.unwrap();
        match message {
            SearchProgress::Progress(progress) => progress_dlg.set_progress(progress),
            SearchProgress::Done(found) => break found,
        }
    };
    thread.join().unwrap();
    progress_dlg.close();

    pending().await;

    match found {
        Some(result) => {
            let text_render = &window.imp().text_render;
            text_render.set_marker(
                result,
                if forward {
                    result + search.len()
                } else {
                    result - search.len()
                },
            );
            text_render.ensure_offset_visible(result);
        }
        None => {
            say_not_found(window.upcast_ref(), &search.to_string()).await;
        }
    }
}

fn create_menu(display_mode: DisplayMode) -> gio::Menu {
    let mut menu = gio::Menu::new()
        .submenu(
            gettext("_File"),
            gio::Menu::new().item_accel(gettext("_Close"), "viewer.close", "Escape"),
        )
        .submenu(
            gettext("_View"),
            {
                let menu = gio::Menu::new();
                for (display_mode, label, accel) in [
                    (DisplayMode::Text, gettext("_Text"), "1"),
                    (DisplayMode::Binary, gettext("_Binary"), "2"),
                    (DisplayMode::Hexdump, gettext("_Hexadecimal"), "3"),
                    (DisplayMode::Image, gettext("_Image"), "4"),
                ] {
                    let item = gio::MenuItem::new(Some(&label), None);
                    item.set_action_and_target_value(
                        Some("viewer.display-mode"),
                        Some(&display_mode.to_variant()),
                    );
                    item.set_attribute_value("accel", Some(&accel.to_variant()));
                    menu.append_item(&item);
                }
                menu
            }
            .section(
                gio::Menu::new()
                    .item_accel(gettext("_Zoom In"), "viewer.zoom-in", "<Control>plus")
                    .item_accel(gettext("_Zoom Out"), "viewer.zoom-out", "<Control>minus")
                    .item_accel(gettext("_Normal Size"), "viewer.normal-size", "<Control>0")
                    .item_accel(gettext("_Best Fit"), "viewer.best-fit", "<Control>period"),
            ),
        );

    if matches!(
        display_mode,
        DisplayMode::Text | DisplayMode::Binary | DisplayMode::Hexdump
    ) {
        menu = menu.submenu(
            gettext("_Text"),
            gio::Menu::new()
                .item_accel(
                    gettext("_Copy Text Selection"),
                    "viewer.copy-text-selection",
                    "<Control>C",
                )
                .item_accel(gettext("Find…"), "viewer.find", "<Control>F")
                .item_accel(gettext("Find Next"), "viewer.find-next", "F3")
                .item_accel(
                    gettext("Find Previous"),
                    "viewer.find-previous",
                    "<Shift>F3",
                )
                .section(gio::Menu::new().item_accel(
                    gettext("_Wrap lines"),
                    "viewer.wrap-lines",
                    "<Control>W",
                ))
                .submenu(
                    gettext("_Encoding"),
                    gio::Menu::new()
                        .item_accel(gettext("_UTF-8"), "viewer.encoding('UTF8')", "U")
                        .item_accel(
                            gettext("English (US-_ASCII)"),
                            "viewer.encoding('ASCII')",
                            "A",
                        )
                        .item_accel(gettext("Terminal (CP437)"), "viewer.encoding('CP437')", "Q")
                        .item(
                            gettext("Arabic (ISO-8859-6)"),
                            "viewer.encoding('ISO-8859-6')",
                        )
                        .item(
                            gettext("Arabic ( Windows, CP1256)"),
                            "viewer.encoding('ARABIC')",
                        )
                        .item(gettext("Arabic (Dos, CP864)"), "viewer.encoding('CP864')")
                        .item(
                            gettext("Baltic (ISO-8859-4)"),
                            "viewer.encoding('ISO-8859-4')",
                        )
                        .item(
                            gettext("Central European (ISO-8859-2)"),
                            "viewer.encoding('ISO-8859-2')",
                        )
                        .item(
                            gettext("Central European (CP1250)"),
                            "viewer.encoding('CP1250')",
                        )
                        .item(
                            gettext("Cyrillic (ISO-8859-5)"),
                            "viewer.encoding('ISO-8859-5')",
                        )
                        .item(gettext("Cyrillic (CP1251)"), "viewer.encoding('CP1251')")
                        .item(
                            gettext("Greek (ISO-8859-7)"),
                            "viewer.encoding('ISO-8859-7')",
                        )
                        .item(gettext("Greek (CP1253)"), "viewer.encoding('CP1253')")
                        .item(
                            gettext("Hebrew (Windows, CP1255)"),
                            "viewer.encoding('HEBREW')",
                        )
                        .item(gettext("Hebrew (Dos, CP862)"), "viewer.encoding('CP862')")
                        .item(
                            gettext("Hebrew (ISO-8859-8)"),
                            "viewer.encoding('ISO-8859-8')",
                        )
                        .item(
                            gettext("Latin 9 (ISO-8859-15)"),
                            "viewer.encoding('ISO-8859-15')",
                        )
                        .item(
                            gettext("Maltese (ISO-8859-3)"),
                            "viewer.encoding('ISO-8859-3')",
                        )
                        .item(
                            gettext("Turkish (ISO-8859-9)"),
                            "viewer.encoding('ISO-8859-9')",
                        )
                        .item(gettext("Turkish (CP1254)"), "viewer.encoding('CP1254')")
                        .item(gettext("Western (CP1252)"), "viewer.encoding('CP1252')")
                        .item(
                            gettext("Western (ISO-8859-1)"),
                            "viewer.encoding('ISO-8859-1')",
                        ),
                ),
        );
    }

    if display_mode == DisplayMode::Image {
        menu = menu.submenu(
            gettext("_Image"),
            gio::Menu::new()
                .item_accel(
                    gettext("Rotate Clockwise"),
                    "viewer.imageop(0)",
                    "<Control>R",
                )
                .item(gettext("Rotate Counter Clockwis_e"), "viewer.imageop(1)")
                .item_accel(
                    gettext("Rotate 180\u{00B0}"),
                    "viewer.imageop(2)",
                    "<Control><Shift>R",
                )
                .item(gettext("Flip _Vertical"), "viewer.imageop(3)")
                .item(gettext("Flip _Horizontal"), "viewer.imageop(4)"),
        );
    }

    menu.submenu(
        gettext("_Settings"),
        gio::Menu::new()
            .submenu(
                gettext("_Binary Mode"),
                gio::Menu::new()
                    .item_accel(
                        gettext("_20 chars/line"),
                        "viewer.chars-per-line(20)",
                        "<Control>2",
                    )
                    .item_accel(
                        gettext("_40 chars/line"),
                        "viewer.chars-per-line(40)",
                        "<Control>4",
                    )
                    .item_accel(
                        gettext("_80 chars/line"),
                        "viewer.chars-per-line(80)",
                        "<Control>8",
                    ),
            )
            .section(
                gio::Menu::new()
                    .item_accel(
                        gettext("Show Metadata _Tags"),
                        "viewer.metadata-visible",
                        "T",
                    )
                    .item_accel(
                        gettext("_Hexadecimal Offset"),
                        "viewer.hexadecimal-offset",
                        "<Control>D",
                    ),
            ),
    )
    .submenu(
        gettext("_Help"),
        gio::Menu::new()
            .item_accel(gettext("Quick _Help"), "viewer.quick-help", "F1")
            .item(gettext("_Keyboard Shortcuts"), "viewer.keyboard-shortcuts"),
    )
}

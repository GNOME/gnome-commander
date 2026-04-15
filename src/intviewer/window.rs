// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    search_bar::{SearchBar, SearchSettings},
    searcher::Searcher,
    text_render::TextRender,
};
use crate::{
    file::{File, FileOps},
    intviewer::image_render::ImageOperation,
    tags::FileMetadataService,
    utils::{MenuBuilderExt, SenderExt, extract_menu_shortcuts, pending, u32_enum},
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
            image_render::ImageRender, input_modes::preprocess_for_cp437_conversion,
            text_render::TextRenderDisplayMode,
        },
        options::{ViewerOptions, utils::remember_window_size},
        utils::{display_help, hbox_builder, vbox_builder},
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
        pub(super) searchbar: SearchBar,
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
            klass.install_action("viewer.select-all", None, |obj, _, _| {
                obj.imp().select_all()
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
            klass.install_action("viewer.find", None, |obj, _, _| obj.imp().find());
            klass.install_action_async("viewer.find-next", None, |obj, _, _| async move {
                obj.imp().start_search(true).await
            });
            klass.install_action_async("viewer.find-previous", None, |obj, _, _| async move {
                obj.imp().start_search(false).await
            });
            klass.install_property_action("viewer.display-mode", "display-mode");
            klass.install_property_action("viewer.wrap-lines", "wrap-lines");
            klass.install_property_action("viewer.encoding", "encoding");
            klass.install_action(
                "viewer.imageop",
                Some(&ImageOperation::static_variant_type()),
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
                searchbar: SearchBar::new(),
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

            window.set_child(Some(
                &vbox_builder()
                    .add_css_class("spacing")
                    .append(&self.menubar)
                    .append({
                        let tscrollbox = gtk::ScrolledWindow::builder()
                            .child(&self.text_render)
                            .hexpand(true)
                            .vexpand(true)
                            .build();
                        self.text_render.connect_text_status_changed(glib::clone!(
                            #[weak(rename_to = imp)]
                            self,
                            move |_| imp.text_status_update()
                        ));

                        let button_gesture = gtk::GestureClick::builder().button(3).build();
                        button_gesture.connect_pressed(glib::clone!(
                            #[weak(rename_to = imp)]
                            self,
                            move |_, n_press, x, y| imp
                                .on_text_viewer_button_pressed(n_press, x, y)
                        ));
                        self.text_render.add_controller(button_gesture);

                        let iscrollbox = gtk::ScrolledWindow::builder()
                            .child(&self.image_render)
                            .hexpand(true)
                            .vexpand(true)
                            .build();
                        self.image_render.connect_image_status_changed(glib::clone!(
                            #[weak(rename_to = imp)]
                            self,
                            move |_| imp.image_status_update()
                        ));

                        let button_gesture = gtk::GestureClick::builder().button(3).build();
                        button_gesture.connect_pressed(glib::clone!(
                            #[weak(rename_to = imp)]
                            self,
                            move |_, n_press, x, y| imp
                                .on_image_viewer_button_pressed(n_press, x, y)
                        ));
                        self.image_render.add_controller(button_gesture);

                        self.stack.add_named(&tscrollbox, Some("text"));
                        self.stack.add_named(&iscrollbox, Some("image"));
                        self.stack.set_visible_child_name("text");

                        &self.stack
                    })
                    .append({
                        self.searchbar.connect_search(glib::clone!(
                            #[weak(rename_to = imp)]
                            self,
                            move |_, forward| {
                                glib::spawn_future_local(async move {
                                    imp.start_search(forward).await;
                                });
                            }
                        ));
                        &self.searchbar
                    })
                    .append(
                        &hbox_builder()
                            .add_css_class("spacing")
                            .add_css_class("offset")
                            .append(&self.status_label)
                            .build(),
                    )
                    .append(&{
                        let metadata_view = FileMetainfoView::new(&window.file_metadata_service());
                        metadata_view.set_vexpand(true);
                        window
                            .bind_property("metadata-visible", &metadata_view, "visible")
                            .bidirectional()
                            .build();
                        let _ = self.metadata_view.set(metadata_view.clone());
                        metadata_view
                    })
                    .build(),
            ));

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
                    self.searchbar.hide();
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

            self.obj()
                .action_set_enabled("viewer.best-fit", mode == DisplayMode::Image);
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

        fn select_all(&self) {
            if self.display_mode.get() != DisplayMode::Image {
                self.text_render.set_marker(
                    0,
                    self.text_render
                        .input_mode()
                        .map(|f| f.max_offset())
                        .unwrap_or_default(),
                );
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
                    let index = if self.image_render.best_fit() {
                        let current_factor = self.image_render.real_scale_factor();
                        let mut index = 0;
                        for (i, factor) in IMAGE_SCALE_FACTORS.iter().enumerate() {
                            index = i;
                            if *factor > current_factor {
                                break;
                            }
                        }
                        self.image_render.set_best_fit(false);
                        index
                    } else {
                        let current_index = self.current_scale_index.get();
                        if current_index >= IMAGE_SCALE_FACTORS.len() - 1 {
                            return;
                        }
                        current_index + 1
                    };

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
                    let index = if self.image_render.best_fit() {
                        let current_factor = self.image_render.real_scale_factor();
                        let mut index = 0;
                        for (i, factor) in IMAGE_SCALE_FACTORS.iter().enumerate().rev() {
                            index = i;
                            if *factor < current_factor {
                                break;
                            }
                        }
                        self.image_render.set_best_fit(false);
                        index
                    } else {
                        let current_index = self.current_scale_index.get();
                        if current_index == 0 {
                            return;
                        }
                        current_index - 1
                    };

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

        fn find(&self) {
            if self.display_mode.get() == DisplayMode::Image {
                return;
            }

            self.searchbar.show(true);
        }

        async fn start_search(&self, forward: bool) {
            #[derive(Debug, Clone, Copy)]
            enum SearchProgress {
                Progress(u32),
                Done(Option<u64>),
            }

            if self.display_mode.get() == DisplayMode::Image {
                return;
            }

            let Some(settings) = self.searchbar.settings() else {
                // No search active, just show and focus search bar
                self.searchbar.show(true);
                return;
            };

            self.searchbar.show(false);
            self.searchbar.add_to_history();

            let input_mode = self.text_render.input_mode().unwrap();
            let end_offset = self
                .text_render
                .input_mode()
                .map(|f| f.max_offset())
                .unwrap_or_default();
            let start_offset = if self.searchbar.showing_not_found() {
                // Wrap the search around after “Not found”
                if forward { 0 } else { end_offset }
            } else if let Some((start_marker, end_marker)) = self.text_render.marker() {
                // Start at the marked text if there is any
                if forward {
                    start_marker + 1
                } else {
                    end_marker - 1
                }
            } else {
                // Default to starting at the edge of the current screen
                if forward {
                    self.text_render.current_offset()
                } else {
                    self.text_render.end_offset()
                }
            };

            let (pattern, match_case) = match settings {
                SearchSettings::Text {
                    pattern,
                    match_case,
                } => {
                    let encoding = self.encoding.borrow();
                    let pattern = if encoding.eq_ignore_ascii_case("CP437") {
                        preprocess_for_cp437_conversion(&pattern)
                    } else {
                        pattern.into()
                    };
                    let Some((slice, _)) = glib::IConv::new(&*encoding, "UTF8")
                        .and_then(|mut iconv| iconv.convert(pattern.as_bytes()).ok())
                    else {
                        self.searchbar.show_encoding_error();
                        return;
                    };
                    (slice.to_vec(), match_case)
                }
                SearchSettings::Binary {
                    pattern,
                    match_case,
                } => (pattern, match_case),
            };
            let pattern_len = pattern.len();

            let cancellable = gio::Cancellable::new();
            let abort_handler = self.searchbar.connect_abort(glib::clone!(
                #[strong]
                cancellable,
                move |_| cancellable.cancel()
            ));

            let (sender, receiver) = async_channel::bounded::<SearchProgress>(1);

            let thread = std::thread::spawn(glib::clone!(
                #[strong]
                cancellable,
                move || {
                    let searcher = Searcher::new(
                        input_mode,
                        start_offset,
                        end_offset,
                        &pattern,
                        match_case,
                        forward,
                        cancellable,
                    );

                    let found = searcher.search(|p| {
                        sender.toss(SearchProgress::Progress(p));
                    });
                    sender.toss(SearchProgress::Done(found));
                }
            ));

            let found = loop {
                let message = receiver.recv().await.unwrap();
                match message {
                    SearchProgress::Progress(progress) => self.searchbar.show_progress(progress),
                    SearchProgress::Done(found) => break found,
                }
            };
            self.searchbar.disconnect(abort_handler);
            let _ = thread.join();
            pending().await;

            self.searchbar.hide_progress();
            match found {
                Some(result) => {
                    self.text_render.set_marker(
                        result,
                        if forward {
                            result + pattern_len as u64
                        } else {
                            result - pattern_len as u64
                        },
                    );
                    self.text_render.ensure_offset_visible(result);
                }
                None => {
                    if !cancellable.is_cancelled() {
                        self.searchbar.show_not_found();
                    }
                }
            }
        }

        fn image_operation(&self, op: Option<&glib::Variant>) {
            let Some(operation) = op.and_then(ImageOperation::from_variant) else {
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

        fn show_context_menu(
            &self,
            widget: &impl IsA<gtk::Widget>,
            menu: gio::Menu,
            x: f64,
            y: f64,
        ) {
            let popover = gtk::PopoverMenu::from_model(Some(&menu));
            popover.set_parent(widget);
            popover.set_position(gtk::PositionType::Bottom);
            popover.set_pointing_to(Some(&gdk::Rectangle::new(x as i32, y as i32, 0, 0)));
            popover.connect_closed(|this| {
                let this = this.clone();
                glib::spawn_future_local(async move {
                    this.unparent();
                });
            });
            popover.popup();
        }

        fn on_text_viewer_button_pressed(&self, n_press: i32, x: f64, y: f64) {
            if n_press == 1 {
                let menu = gio::Menu::new()
                    .item(gettext("_Copy Selection"), "viewer.copy-text-selection")
                    .item(gettext("_Select All"), "viewer.select-all");
                self.show_context_menu(&self.text_render, menu, x, y);
            }
        }

        fn on_image_viewer_button_pressed(&self, n_press: i32, x: f64, y: f64) {
            if n_press == 1 {
                let menu = gio::Menu::new()
                    .item_param(
                        gettext("_Rotate Clockwise"),
                        "viewer.imageop",
                        ImageOperation::RotateClockwise,
                    )
                    .item_param(
                        gettext("Rotate Counter Clockwis_e"),
                        "viewer.imageop",
                        ImageOperation::RotateCounterclockwise,
                    )
                    .item_param(
                        gettext("Rotate 180\u{00B0}"),
                        "viewer.imageop",
                        ImageOperation::RotateUpsideDown,
                    )
                    .item_param(
                        gettext("Flip _Vertical"),
                        "viewer.imageop",
                        ImageOperation::FlipVertical,
                    )
                    .item_param(
                        gettext("Flip _Horizontal"),
                        "viewer.imageop",
                        ImageOperation::FlipHorizontal,
                    );
                self.show_context_menu(&self.image_render, menu, x, y);
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

        if let Some(metadata_view) = self.imp().metadata_view.get() {
            metadata_view.set_property("file", file);
        }

        self.set_title(path.to_str());
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
            gio::Menu::new()
                .item_param_accel(
                    gettext("_Text"),
                    "viewer.display-mode",
                    DisplayMode::Text,
                    "1",
                )
                .item_param_accel(
                    gettext("_Binary"),
                    "viewer.display-mode",
                    DisplayMode::Binary,
                    "2",
                )
                .item_param_accel(
                    gettext("_Hexadecimal"),
                    "viewer.display-mode",
                    DisplayMode::Hexdump,
                    "3",
                )
                .item_param_accel(
                    gettext("_Image"),
                    "viewer.display-mode",
                    DisplayMode::Image,
                    "4",
                )
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
                .item_accel(gettext("_Select All"), "viewer.select-all", "<Control>A")
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
                .item_param_accel(
                    gettext("_Rotate Clockwise"),
                    "viewer.imageop",
                    ImageOperation::RotateClockwise,
                    "<Control>R",
                )
                .item_param(
                    gettext("Rotate Counter Clockwis_e"),
                    "viewer.imageop",
                    ImageOperation::RotateCounterclockwise,
                )
                .item_param_accel(
                    gettext("Rotate 180\u{00B0}"),
                    "viewer.imageop",
                    ImageOperation::RotateUpsideDown,
                    "<Control><Shift>R",
                )
                .item_param(
                    gettext("Flip _Vertical"),
                    "viewer.imageop",
                    ImageOperation::FlipVertical,
                )
                .item_param(
                    gettext("Flip _Horizontal"),
                    "viewer.imageop",
                    ImageOperation::FlipHorizontal,
                ),
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

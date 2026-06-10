// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    image_render::{ImageOperation, ImageRender},
    input_modes::preprocess_for_cp437_conversion,
    search_bar::{SearchBar, SearchSettings},
    searcher::Searcher,
    text_render::{TextRender, TextRenderDisplayMode},
};
use crate::{
    file::{File, FileOps},
    file_metainfo_view::FileMetainfoView,
    options::{ViewerOptions, types::WriteResult, utils::remember_window_state},
    tags::{FileMetadataService, file_metadata::FileMetadata},
    utils::{MenuBuilderExt, display_help, u32_enum},
};
use component_framework::prelude::*;
use gettextrs::{gettext, ngettext};
use gtk::{gdk, gio, glib, pango, prelude::*};
use std::{
    cell::Cell,
    path::{Path, PathBuf},
    time::{Duration, Instant},
};

const TEXT_SCALE_FACTORS: &[f64] = &[
    0.3, 0.5, 0.67, 0.8, 0.9, 1.0, 1.1, 1.2, 1.33, 1.5, 1.7, 2.0, 2.4, 3.0, 4.0, 5.0,
];
const DEFAULT_TEXT_SCALE_INDEX: usize = 5;

const IMAGE_SCALE_FACTORS: &[f64] = &[
    0.1, 0.2, 0.33, 0.5, 0.67, 1.0, 1.25, 1.50, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
];
const DEFAULT_IMAGE_SCALE_INDEX: usize = 5;

u32_enum! {
    pub enum DisplayMode {
        #[default]
        Text,
        FixedWidth,
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
            Some(Self::FixedWidth)
        } else {
            None
        }
    }
}

trait ShortcutControllerExt {
    fn action_shortcut(&self, action: &str, shortcut: &str);
    fn action_shortcut_with_param(&self, action: &str, param: impl ToVariant, shortcut: &str);
}

impl ShortcutControllerExt for gtk::ShortcutController {
    fn action_shortcut(&self, action: &str, shortcut: &str) {
        self.add_shortcut(gtk::Shortcut::new(
            gtk::ShortcutTrigger::parse_string(shortcut),
            Some(gtk::NamedAction::new(action)),
        ));
    }

    fn action_shortcut_with_param(&self, action: &str, param: impl ToVariant, shortcut: &str) {
        let shortcut = gtk::Shortcut::new(
            gtk::ShortcutTrigger::parse_string(shortcut),
            Some(gtk::NamedAction::new(action)),
        );
        shortcut.set_arguments(Some(&param.to_variant()));
        self.add_shortcut(shortcut);
    }
}

#[derive(Debug)]
pub struct ViewerWindowView {
    window: gtk::Window,
    action_group: gio::SimpleActionGroup,
    menubar: gtk::PopoverMenuBar,
    stack: gtk::Stack,
    text_render: TextRender,
    image_render: ImageRender,
    searchbar: SearchBar,
    status_label: gtk::Label,
    metadata_view: FileMetainfoView,
}

impl ViewerWindowView {
    pub fn text_status_update(&self) {
        let position = gettext("Position: {offset} of {size}")
            .replace("{offset}", &self.text_render.current_offset().to_string())
            .replace("{size}", &self.text_render.size().to_string());

        let column = gettext("Column: {}").replace("{}", &self.text_render.column().to_string());

        let scale = format!("{}%", (self.text_render.scale_factor() * 100.0) as i64);

        let wrap = if self.text_render.wrap_mode() {
            &gettext("Wrap")
        } else {
            ""
        };

        self.status_label
            .set_text(&format!("{position}\t{column}\t{scale}\t{wrap}"));
    }

    pub fn image_status_update(&self) {
        let Some(pixbuf) = self.image_render.origin_pixbuf() else {
            self.status_label
                .set_text(&gettext("Could not recognize the image file format"));
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
            format!(
                "{}%\t{}",
                (self.image_render.real_scale_factor() * 100.0) as i64,
                gettext("(fit to window)")
            )
        };

        self.status_label
            .set_text(&format!("{size}\t{depth}\t{scale}"));
    }

    fn show_context_menu(widget: &impl IsA<gtk::Widget>, menu: gio::Menu, x: f64, y: f64) {
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

    pub fn text_viewer_context_menu(&self, x: f64, y: f64) {
        let menu = gio::Menu::new()
            .item(gettext("_Copy Selection"), "viewer.copy-text-selection")
            .item(gettext("_Select All"), "viewer.select-all");
        Self::show_context_menu(&self.text_render, menu, x, y);
    }

    pub fn image_viewer_context_menu(&self, x: f64, y: f64) {
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
        Self::show_context_menu(&self.image_render, menu, x, y);
    }

    pub fn update_tab_size(&self) {
        self.text_render
            .set_tab_size(ViewerOptions::instance().tab_size.get());
    }

    pub fn update_wrap_mode(&self) {
        let value = ViewerOptions::instance().wrap_mode.get();
        self.text_render.set_wrap_mode(value);
        self.action_group
            .change_action_state("wrap-lines", &value.to_variant())
    }

    pub fn update_encoding(&self) {
        let value = ViewerOptions::instance().encoding.get();
        self.text_render.set_encoding(value.as_str());
        self.action_group
            .change_action_state("encoding", &value.to_variant())
    }

    pub fn update_monospaced_font(&self) {
        self.text_render
            .set_monospaced_font(ViewerOptions::instance().monospaced_font.get());
    }

    pub fn update_chars_per_line(&self) {
        let value = ViewerOptions::instance().binary_bytes_per_line.get();
        self.text_render.set_fixed_limit(value);
        self.action_group
            .change_action_state("chars-per-line", &value.to_variant())
    }

    pub fn update_metadata(&self) {
        let value = ViewerOptions::instance().metadata_visible.get();
        self.metadata_view.set_visible(value);
        self.action_group
            .change_action_state("metadata-visible", &value.to_variant())
    }

    pub fn update_hex_offset(&self) {
        let value = ViewerOptions::instance().display_hex_offset.get();
        self.text_render.set_hexadecimal_offset(value);
        self.action_group
            .change_action_state("hexadecimal-offset", &value.to_variant())
    }
}

#[derive(Debug)]
pub enum ViewerWindowInput {
    SetMetadata(FileMetadata),
    FocusContent,
    TextStatusUpdate,
    TextViewerContextMenu(f64, f64),
    ImageStatusUpdate,
    ImageViewerContextMenu(f64, f64),
    CopySelection,
    SelectAll,
    ZoomIn,
    ZoomOut,
    ZoomBestFit,
    ZoomNormal,
    Find,
    StartSearch(bool),
    CancelSearch,
    SearchProgress(u32),
    SearchDone(Option<u64>),
    DisplayMode(DisplayMode),
    ToggleWrapMode,
    Encoding(String),
    ImageOp(ImageOperation),
    ChooseFont,
    CharsPerLine(u32),
    ToggleHexOffset,
    ToggleMetadata,
    QuickHelp,
    KeyboardShortcuts,

    UpdateTabSize,
    UpdateWrapMode,
    UpdateEncoding,
    UpdateMonospacedFont,
    UpdateCharsPerLine,
    UpdateMetadata,
    UpdateHexOffset,
}

#[derive(Debug)]
pub struct ViewerWindow {
    option_handlers: Vec<glib::SignalHandlerId>,
    file: PathBuf,
    display_mode: DisplayMode,
    current_scale_index: usize,
    current_search: Option<(gio::Cancellable, bool, usize)>,
}

impl Drop for ViewerWindow {
    fn drop(&mut self) {
        let options = ViewerOptions::instance();
        for handler in self.option_handlers.drain(..) {
            options.encoding.disconnect(handler);
        }

        if let Some((cancellable, _, _)) = self.current_search.as_ref() {
            cancellable.cancel();
        }
    }
}

impl Component for ViewerWindow {
    type View = ViewerWindowView;
    type Root = gtk::Window;
    type Input = ViewerWindowInput;
    type Output = ();

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let view = Self::View {
            window: Default::default(),
            action_group: Default::default(),
            menubar: gtk::PopoverMenuBar::from_model(gio::MenuModel::NONE),
            stack: Default::default(),
            text_render: Default::default(),
            image_render: Default::default(),
            searchbar: Default::default(),
            status_label: Default::default(),
            metadata_view: Default::default(),
        };

        with!(&view.window {
            .set_icon_name(Some("gnome-commander-internal-viewer"));
            .set_title(self.file.to_str());

            .insert_action_group("viewer", Some(with!(&view.action_group {
                gio::SimpleAction::new("copy-text-selection", None) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::CopySelection)));
                }
                gio::SimpleAction::new("select-all", None) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::SelectAll)));
                }
                gio::SimpleAction::new("close", None) {
                    .connect_activate(forward!(|_, _| sender.output(())));
                }
                gio::SimpleAction::new("zoom-in", None) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::ZoomIn)));
                }
                gio::SimpleAction::new("zoom-out", None) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::ZoomOut)));
                }
                gio::SimpleAction::new("normal-size", None) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::ZoomNormal)));
                }
                gio::SimpleAction::new("best-fit", None) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::ZoomBestFit)));
                }
                gio::SimpleAction::new("find", None) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::Find)));
                }
                gio::SimpleAction::new("find-next", None) {
                    .connect_activate(forward!(
                        |_, _| sender.input(Self::Input::StartSearch(true))
                    ));
                }
                gio::SimpleAction::new("find-previous", None) {
                    .connect_activate(forward!(
                        |_, _| sender.input(Self::Input::StartSearch(false))
                    ));
                }
                gio::SimpleAction::new_stateful(
                    "display-mode",
                    Some(&DisplayMode::static_variant_type()),
                    &self.display_mode.to_variant(),
                ) {
                    .connect_activate(forward!(|_, param| sender.input(Self::Input::DisplayMode(
                        param.and_then(DisplayMode::from_variant).unwrap_or_default()
                    ))));
                }
                gio::SimpleAction::new_stateful(
                    "wrap-lines",
                    None,
                    &false.to_variant(),
                ) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::ToggleWrapMode)));
                }
                gio::SimpleAction::new_stateful(
                    "encoding",
                    Some(&String::static_variant_type()),
                    &"".to_variant(),
                ) {
                    .connect_activate(forward!(|_, param| sender.input(Self::Input::Encoding(
                        param.and_then(String::from_variant).unwrap_or_else(|| String::from("UTF8"))
                    ))));
                }
                gio::SimpleAction::new("imageop", Some(&ImageOperation::static_variant_type())) {
                    .connect_activate(forward!(|_, param| sender.input(Self::Input::ImageOp(
                        param.and_then(ImageOperation::from_variant).unwrap_or_default()
                    ))));
                }
                gio::SimpleAction::new("choose-font", None) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::ChooseFont)));
                }
                gio::SimpleAction::new_stateful(
                    "chars-per-line",
                    Some(&u32::static_variant_type()),
                    &0u32.to_variant(),
                ) {
                    .connect_activate(forward!(|_, param| sender.input(Self::Input::CharsPerLine(
                        param.and_then(u32::from_variant).unwrap_or(80)
                    ))));
                }
                gio::SimpleAction::new_stateful(
                    "hexadecimal-offset",
                    None,
                    &false.to_variant(),
                ) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::ToggleHexOffset)));
                }
                gio::SimpleAction::new_stateful(
                    "metadata-visible",
                    None,
                    &false.to_variant(),
                ) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::ToggleMetadata)));
                }
                gio::SimpleAction::new("quick-help", None) {
                    .connect_activate(forward!(|_, _| sender.input(Self::Input::QuickHelp)));
                }
                gio::SimpleAction::new("keyboard-shortcuts", None) {
                    .connect_activate(forward!(
                        |_, _| sender.input(Self::Input::KeyboardShortcuts)
                    ));
                }
            })));

            .add_controller(with!(gtk::ShortcutController {
                .action_shortcut("viewer.close", "<Control>Q");
                .action_shortcut("viewer.close", "Escape");
                .action_shortcut_with_param("viewer.display-mode", DisplayMode::Text, "1");
                .action_shortcut_with_param(
                    "viewer.display-mode", DisplayMode::FixedWidth, "2"
                );
                .action_shortcut_with_param("viewer.display-mode", DisplayMode::Hexdump, "3");
                .action_shortcut_with_param("viewer.display-mode", DisplayMode::Image, "4");
                .action_shortcut("viewer.zoom-in", "<Control>equal");
                .action_shortcut("viewer.zoom-in", "<Control>KP_Add");
                .action_shortcut("viewer.zoom-in", "<Control>plus");
                .action_shortcut("viewer.zoom-out", "<Control>KP_Subtract");
                .action_shortcut("viewer.zoom-out", "<Control>minus");
                .action_shortcut("viewer.normal-size", "<Control>0");
                .action_shortcut("viewer.best-fit", "<Control>period");
                .action_shortcut("viewer.copy-text-selection", "<Control>C");
                .action_shortcut("viewer.select-all", "<Control>A");
                .action_shortcut("viewer.find", "<Control>F");
                .action_shortcut("viewer.find-next", "F3");
                .action_shortcut("viewer.find-previous", "<Shift>F3");
                .action_shortcut("viewer.wrap-lines", "<Control>W");
                .action_shortcut_with_param("viewer.encoding", "UTF8", "U");
                .action_shortcut_with_param("viewer.encoding", "ASCII", "A");
                .action_shortcut_with_param("viewer.encoding", "CP437", "Q");
                .action_shortcut_with_param(
                    "viewer.imageop",
                    ImageOperation::RotateClockwise,
                    "<Control>R",
                );
                .action_shortcut_with_param(
                    "viewer.imageop",
                    ImageOperation::RotateUpsideDown,
                    "<Control><Shift>R",
                );
                .action_shortcut_with_param("viewer.chars-per-line", 20u32, "<Control>2");
                .action_shortcut_with_param("viewer.chars-per-line", 40u32, "<Control>4");
                .action_shortcut_with_param("viewer.chars-per-line", 80u32, "<Control>8");
                .action_shortcut("viewer.metadata-visible", "T");
                .action_shortcut("viewer.hexadecimal-offset", "<Control>D");
                .action_shortcut("viewer.quick-help", "F1");
            }));

            // Make sure we signal that we are done even if close action isn't used.
            .connect_unmap(forward!(sender.output(())));

            .connect_focus_widget_notify({
                let sender = sender.clone();
                move |window| {
                    if gtk::prelude::GtkWindowExt::focus(window)
                        .is_some_and(|widget| widget.widget_name() == "GtkPopoverMenuBarItem")
                    {
                        // Individual menu bar items should never be focused, return focus to the
                        // content area. This is quite a hack to compensate for Gtk's shortcomings
                        // but worst-case scenario is that this won't work in some future Gtk
                        // version and focus simply remains on the menu requiring the user to click.
                        sender.input(Self::Input::FocusContent);
                    }
                }
            });

            gtk::Box {
                .set_orientation(gtk::Orientation::Vertical);
                .add_css_class("spacing");

                &view.menubar {}

                &view.stack {
                    .add_named(&with!(gtk::ScrolledWindow {
                        .set_hexpand(true);
                        .set_vexpand(true);

                        &view.text_render {
                            .load_file(&self.file);

                            .connect_text_status_changed(
                                forward!(sender.input(Self::Input::TextStatusUpdate))
                            );

                            .add_controller(with!(gtk::GestureClick {
                                .set_button(3);
                                .connect_pressed({
                                    let sender = sender.clone();
                                    move |_, n_press, x, y| {
                                        if n_press == 1 {
                                            sender.input(Self::Input::TextViewerContextMenu(x, y));
                                        }
                                    }
                                });
                            }));
                        }
                    }), Some("text"));

                    .add_named(&with!(gtk::ScrolledWindow {
                        .set_hexpand(true);
                        .set_vexpand(true);

                        &view.image_render {
                            .set_best_fit(true);
                            .set_scale_factor(1.0);

                            .connect_image_status_changed(
                                forward!(sender.input(Self::Input::ImageStatusUpdate))
                            );

                            .add_controller(with!(gtk::GestureClick {
                                .set_button(3);
                                .connect_pressed({
                                    let sender = sender.clone();
                                    move |_, n_press, x, y| {
                                        if n_press == 1 {
                                            sender.input(Self::Input::ImageViewerContextMenu(x, y));
                                        }
                                    }
                                });
                            }));
                        }
                    }), Some("image"));
                }

                &view.searchbar {
                    .connect_search({
                        let sender = sender.clone();
                        move |_, forward| sender.input(Self::Input::StartSearch(forward))
                    });
                    .connect_closed(forward!(sender.input(Self::Input::FocusContent)));
                    .connect_abort(forward!(sender.input(Self::Input::CancelSearch)));
                }

                gtk::Box {
                    .set_orientation(gtk::Orientation::Horizontal);
                    .add_css_class("spacing");
                    .add_css_class("offset");

                    &view.status_label {}
                }

                &view.metadata_view {
                    .set_vexpand(true);
                }
            }

            .present();
        });

        let options = ViewerOptions::instance();

        remember_window_state(
            &view.window,
            &options.window_width,
            &options.window_height,
            &options.window_state,
        );

        with!(&mut self.option_handlers {
            .push(options.tab_size.connect_changed(
                forward!(sender.input(Self::Input::UpdateTabSize))
            ));
            .push(options.wrap_mode.connect_changed(
                forward!(sender.input(Self::Input::UpdateWrapMode))
            ));
            .push(options.encoding.connect_changed(
                forward!(sender.input(Self::Input::UpdateEncoding))
            ));
            .push(options.monospaced_font.connect_changed(
                forward!(sender.input(Self::Input::UpdateMonospacedFont))
            ));
            .push(options.binary_bytes_per_line.connect_changed(
                forward!(sender.input(Self::Input::UpdateCharsPerLine))
            ));
            .push(options.metadata_visible.connect_changed(
                forward!(sender.input(Self::Input::UpdateMetadata))
            ));
            .push(options.display_hex_offset.connect_changed(
                forward!(sender.input(Self::Input::UpdateHexOffset))
            ));
        });

        self.update_display_mode(&view);
        view.update_tab_size();
        view.update_wrap_mode();
        view.update_encoding();
        view.update_monospaced_font();
        view.update_chars_per_line();
        view.update_metadata();
        view.update_hex_offset();

        view
    }

    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
        &view.window
    }

    async fn update(
        &mut self,
        msg: Self::Input,
        sender: &ComponentSender<Self>,
        view: &mut Self::View,
    ) {
        match msg {
            Self::Input::SetMetadata(mut metadata) => {
                view.metadata_view.set_metadata(&mut metadata)
            }
            Self::Input::FocusContent => {
                if self.display_mode == DisplayMode::Image {
                    view.image_render.grab_focus();
                } else {
                    view.text_render.grab_focus();
                }
            }
            Self::Input::TextStatusUpdate => view.text_status_update(),
            Self::Input::TextViewerContextMenu(x, y) => view.text_viewer_context_menu(x, y),
            Self::Input::ImageStatusUpdate => view.image_status_update(),
            Self::Input::ImageViewerContextMenu(x, y) => view.image_viewer_context_menu(x, y),
            Self::Input::CopySelection => {
                if self.display_mode != DisplayMode::Image {
                    view.text_render.copy_selection();
                }
            }
            Self::Input::SelectAll => {
                if self.display_mode != DisplayMode::Image {
                    view.text_render.set_marker(0, view.text_render.size());
                }
            }
            Self::Input::ZoomIn => self.zoom_in(view),
            Self::Input::ZoomOut => self.zoom_out(view),
            Self::Input::ZoomBestFit => {
                if self.display_mode == DisplayMode::Image {
                    view.image_render.set_best_fit(true);
                }
            }
            Self::Input::ZoomNormal => self.zoom_normal(view),
            Self::Input::Find => {
                if self.display_mode != DisplayMode::Image {
                    view.searchbar.show(true);
                }
            }
            Self::Input::StartSearch(forward) => self.start_search(sender, view, forward),
            Self::Input::CancelSearch => {
                if let Some((cancellable, _, _)) = self.current_search.as_ref() {
                    cancellable.cancel();
                }
            }
            Self::Input::SearchProgress(progress) => view.searchbar.show_progress(progress),
            Self::Input::SearchDone(found) => self.search_done(view, found),
            Self::Input::DisplayMode(mode) => {
                self.display_mode = mode;
                self.update_display_mode(view);
            }
            Self::Input::ToggleWrapMode => {
                let options = ViewerOptions::instance();
                handle_write_result(options.wrap_mode.set(!options.wrap_mode.get()));
            }
            Self::Input::Encoding(encoding) => {
                let options = ViewerOptions::instance();
                handle_write_result(options.encoding.set(encoding));
            }
            Self::Input::ImageOp(operation) => {
                if self.display_mode == DisplayMode::Image {
                    view.image_render.operation(operation);
                    view.image_render.queue_draw();
                }
            }
            Self::Input::ChooseFont => Self::choose_font(view).await,
            Self::Input::CharsPerLine(num_chars) => {
                let options = ViewerOptions::instance();
                handle_write_result(options.binary_bytes_per_line.set(num_chars));
            }
            Self::Input::ToggleHexOffset => {
                let options = ViewerOptions::instance();
                handle_write_result(
                    options
                        .display_hex_offset
                        .set(!options.display_hex_offset.get()),
                );
            }
            Self::Input::ToggleMetadata => {
                let options = ViewerOptions::instance();
                handle_write_result(
                    options
                        .metadata_visible
                        .set(!options.metadata_visible.get()),
                );
            }
            Self::Input::QuickHelp => {
                let window = view.window.clone();
                glib::spawn_future_local(async move {
                    display_help(&window, Some("gnome-commander-internal-viewer")).await;
                });
            }
            Self::Input::KeyboardShortcuts => {
                let window = view.window.clone();
                glib::spawn_future_local(async move {
                    display_help(&window, Some("gnome-commander-internal-viewer-keyboard")).await;
                });
            }

            Self::Input::UpdateTabSize => view.update_tab_size(),
            Self::Input::UpdateWrapMode => view.update_wrap_mode(),
            Self::Input::UpdateEncoding => view.update_encoding(),
            Self::Input::UpdateMonospacedFont => view.update_monospaced_font(),
            Self::Input::UpdateCharsPerLine => view.update_chars_per_line(),
            Self::Input::UpdateMetadata => view.update_metadata(),
            Self::Input::UpdateHexOffset => view.update_hex_offset(),
        }
    }
}

impl ViewerWindow {
    fn new(file: &File, path: &Path) -> Self {
        Self {
            option_handlers: Vec::new(),
            file: path.to_path_buf(),
            current_scale_index: DEFAULT_IMAGE_SCALE_INDEX,
            display_mode: DisplayMode::guess(file).unwrap_or_default(),
            current_search: None,
        }
    }

    pub async fn file_view(file: &File, file_metadata_service: &FileMetadataService) {
        use futures::FutureExt;

        let Some(path) = file.local_path() else {
            eprintln!("Cannot view file: no local path");
            return;
        };

        let mut viewer = Self::new(file, &path).build();
        viewer.sender().input(ViewerWindowInput::FocusContent);

        futures::select!(
            _ = viewer.receive().fuse() => {
                viewer.root().close();
                return;
            },
            metadata = file_metadata_service.extract_metadata(file).fuse() => {
                viewer.sender().input(ViewerWindowInput::SetMetadata(metadata));
            }
        );

        let _ = viewer.receive().await;
        viewer.root().close();
    }

    fn update_display_mode(&self, view: &ViewerWindowView) {
        match self.display_mode {
            DisplayMode::Text => {
                view.text_render
                    .set_display_mode(TextRenderDisplayMode::Text);
                view.stack.set_visible_child_name("text");
                view.text_render.notify_status_changed();
            }
            DisplayMode::FixedWidth => {
                view.text_render
                    .set_display_mode(TextRenderDisplayMode::FixedWidth);
                view.stack.set_visible_child_name("text");
                view.text_render.notify_status_changed();
            }
            DisplayMode::Hexdump => {
                view.text_render
                    .set_display_mode(TextRenderDisplayMode::Hexdump);
                view.stack.set_visible_child_name("text");
                view.text_render.notify_status_changed();
            }
            DisplayMode::Image => {
                if view.image_render.origin_pixbuf().is_none() {
                    // Image render not initialized yet, initialize now.
                    view.image_render.load_file(&self.file)
                }
                view.stack.set_visible_child_name("image");
                view.image_render.notify_status_changed();
                view.searchbar.hide();
            }
        }

        view.menubar
            .set_menu_model(Some(&create_menu(self.display_mode)));

        view.action_group
            .change_action_state("display-mode", &self.display_mode.to_variant());
        if let Some(action) = view
            .action_group
            .lookup_action("best-fit")
            .and_downcast::<gio::SimpleAction>()
        {
            action.set_enabled(self.display_mode == DisplayMode::Image);
        }
    }

    fn zoom_in(&mut self, view: &ViewerWindowView) {
        match self.display_mode {
            DisplayMode::Text | DisplayMode::FixedWidth | DisplayMode::Hexdump => {
                let current_factor = view.text_render.scale_factor();
                let current_index = TEXT_SCALE_FACTORS
                    .iter()
                    .position(|f| *f == current_factor)
                    .unwrap_or(DEFAULT_TEXT_SCALE_INDEX);
                if current_index < TEXT_SCALE_FACTORS.len() - 1 {
                    view.text_render
                        .set_scale_factor(TEXT_SCALE_FACTORS[current_index + 1]);
                }
            }
            DisplayMode::Image => {
                let index = if view.image_render.best_fit() {
                    let current_factor = view.image_render.real_scale_factor();
                    let mut index = 0;
                    for (i, factor) in IMAGE_SCALE_FACTORS.iter().enumerate() {
                        index = i;
                        if *factor > current_factor {
                            break;
                        }
                    }
                    view.image_render.set_best_fit(false);
                    index
                } else {
                    if self.current_scale_index >= IMAGE_SCALE_FACTORS.len() - 1 {
                        return;
                    }
                    self.current_scale_index + 1
                };

                self.current_scale_index = index;

                let scale_factor = IMAGE_SCALE_FACTORS[index];
                if view.image_render.scale_factor() != scale_factor {
                    view.image_render.set_scale_factor(scale_factor);
                }
            }
        }
    }

    fn zoom_out(&mut self, view: &ViewerWindowView) {
        match self.display_mode {
            DisplayMode::Text | DisplayMode::FixedWidth | DisplayMode::Hexdump => {
                let current_factor = view.text_render.scale_factor();
                let current_index = TEXT_SCALE_FACTORS
                    .iter()
                    .position(|f| *f == current_factor)
                    .unwrap_or(DEFAULT_TEXT_SCALE_INDEX);
                if current_index > 0 {
                    view.text_render
                        .set_scale_factor(TEXT_SCALE_FACTORS[current_index - 1]);
                }
            }
            DisplayMode::Image => {
                let index = if view.image_render.best_fit() {
                    let current_factor = view.image_render.real_scale_factor();
                    let mut index = 0;
                    for (i, factor) in IMAGE_SCALE_FACTORS.iter().enumerate().rev() {
                        index = i;
                        if *factor < current_factor {
                            break;
                        }
                    }
                    view.image_render.set_best_fit(false);
                    index
                } else {
                    if self.current_scale_index == 0 {
                        return;
                    }
                    self.current_scale_index - 1
                };

                self.current_scale_index = index;

                let scale_factor = IMAGE_SCALE_FACTORS[index];
                if view.image_render.scale_factor() != scale_factor {
                    view.image_render.set_scale_factor(scale_factor);
                }
            }
        }
    }

    fn zoom_normal(&mut self, view: &ViewerWindowView) {
        match self.display_mode {
            DisplayMode::Text | DisplayMode::FixedWidth | DisplayMode::Hexdump => {
                view.text_render
                    .set_scale_factor(TEXT_SCALE_FACTORS[DEFAULT_TEXT_SCALE_INDEX]);
            }
            DisplayMode::Image => {
                view.image_render.set_best_fit(false);
                self.current_scale_index = DEFAULT_IMAGE_SCALE_INDEX;
                view.image_render.set_scale_factor(1.0);
            }
        }
    }

    fn start_search(
        &mut self,
        sender: &ComponentSender<Self>,
        view: &ViewerWindowView,
        forward: bool,
    ) {
        if self.display_mode == DisplayMode::Image || self.current_search.is_some() {
            return;
        }

        let Some(settings) = view.searchbar.settings() else {
            // No search active, just show and focus search bar
            view.searchbar.show(true);
            return;
        };

        view.searchbar.show(false);
        view.searchbar.add_to_history();

        let input_mode = view.text_render.input_mode();
        let end_offset = input_mode.max_offset();
        let start_offset = if view.searchbar.showing_not_found() {
            // Wrap the search around after “Not found”
            if forward { 0 } else { end_offset }
        } else if let Some((start_marker, end_marker)) = view.text_render.marker() {
            // Start at the marked text if there is any
            if forward {
                input_mode.next_char_offset(start_marker)
            } else {
                input_mode.previous_char_offset(end_marker)
            }
        } else {
            // Default to starting at the edge of the current screen
            if forward {
                view.text_render.current_offset()
            } else {
                view.text_render.end_offset()
            }
        };

        let (pattern, match_case) = match settings {
            SearchSettings::Text {
                pattern,
                match_case,
            } => {
                let encoding = view
                    .action_group
                    .action_state("encoding")
                    .as_ref()
                    .and_then(String::from_variant)
                    .unwrap_or_else(|| "UTF8".to_owned());
                let pattern = if encoding.eq_ignore_ascii_case("CP437") {
                    preprocess_for_cp437_conversion(&pattern)
                } else {
                    pattern.into()
                };
                let Some((slice, _)) = glib::IConv::new(&*encoding, "UTF8")
                    .and_then(|mut iconv| iconv.convert(pattern.as_bytes()).ok())
                else {
                    view.searchbar.show_encoding_error();
                    return;
                };
                (slice.to_vec(), match_case)
            }
            SearchSettings::Binary {
                pattern,
                match_case,
            } => (pattern, match_case),
        };

        let cancellable = gio::Cancellable::new();
        self.current_search = Some((cancellable.clone(), forward, pattern.len()));

        let sender = sender.clone();
        std::thread::spawn(move || {
            let searcher = Searcher::new(
                input_mode,
                start_offset,
                end_offset,
                &pattern,
                match_case,
                forward,
                cancellable,
            );

            // Only report progress every 100 ms to avoid filling up the channel.
            let start = Instant::now();
            let min_interval = Duration::from_millis(100);
            let last_report = Cell::new(None);
            let found = searcher.search(|p| {
                let elapsed = start.elapsed();
                if last_report
                    .get()
                    .is_none_or(|last_report| elapsed - last_report >= min_interval)
                {
                    sender.input(ViewerWindowInput::SearchProgress(p));
                    last_report.set(Some(elapsed));
                }
            });
            sender.input(ViewerWindowInput::SearchDone(found));
        });
    }

    fn search_done(&mut self, view: &ViewerWindowView, found: Option<u64>) {
        let Some((cancellable, forward, pattern_len)) = self.current_search.take() else {
            return;
        };
        view.searchbar.hide_progress();
        match found {
            Some(result) => {
                let input_mode = view.text_render.input_mode();
                view.text_render.set_marker(
                    input_mode.previous_char_boundary(result),
                    input_mode.next_char_boundary(if forward {
                        result + pattern_len as u64
                    } else {
                        result - pattern_len as u64
                    }),
                );
                view.text_render.ensure_offset_visible(result);
            }
            None => {
                if !cancellable.is_cancelled() {
                    view.searchbar.show_not_found();
                }
            }
        }
    }

    async fn choose_font(view: &ViewerWindowView) {
        let dialog = gtk::FontDialog::builder().modal(true).build();
        dialog.set_filter(Some(&gtk::CustomFilter::new(|font| {
            if let Some(family) = font.downcast_ref::<pango::FontFamily>() {
                family.is_monospace()
            } else if let Some(face) = font.downcast_ref::<pango::FontFace>() {
                face.family().is_monospace()
            } else {
                eprintln!(
                    "Font filter received unexpected object type {}",
                    font.type_()
                );
                true
            }
        })));

        let options = ViewerOptions::instance();
        let font = pango::FontDescription::from_string(&options.monospaced_font.get());
        match dialog
            .choose_font_future(Some(&view.window), Some(&font))
            .await
        {
            Ok(font) => {
                handle_write_result(options.monospaced_font.set(&*font.to_str()));
            }
            Err(error) => {
                if !error.matches(gtk::DialogError::Dismissed) {
                    eprintln!("Failed choosing font: {error}");
                }
            }
        }
    }
}

fn create_menu(display_mode: DisplayMode) -> gio::Menu {
    let mut menu = gio::Menu::new()
        .submenu(
            gettext("_File"),
            gio::Menu::new().item(gettext("_Close"), "viewer.close"),
        )
        .submenu(
            gettext("_View"),
            gio::Menu::new()
                .item_param(gettext("_Text"), "viewer.display-mode", DisplayMode::Text)
                .item_param(
                    gettext("_Fixed Width"),
                    "viewer.display-mode",
                    DisplayMode::FixedWidth,
                )
                .item_param(
                    gettext("_Hexadecimal"),
                    "viewer.display-mode",
                    DisplayMode::Hexdump,
                )
                .item_param(gettext("_Image"), "viewer.display-mode", DisplayMode::Image)
                .section(
                    gio::Menu::new()
                        .item(gettext("_Zoom In"), "viewer.zoom-in")
                        .item(gettext("_Zoom Out"), "viewer.zoom-out")
                        .item(gettext("_Normal Size"), "viewer.normal-size")
                        .item(gettext("_Best Fit"), "viewer.best-fit"),
                ),
        );

    if matches!(
        display_mode,
        DisplayMode::Text | DisplayMode::FixedWidth | DisplayMode::Hexdump
    ) {
        menu = menu.submenu(
            gettext("_Text"),
            gio::Menu::new()
                .item(
                    gettext("_Copy Text Selection"),
                    "viewer.copy-text-selection",
                )
                .item(gettext("_Select All"), "viewer.select-all")
                .item(gettext("Find…"), "viewer.find")
                .item(gettext("Find Next"), "viewer.find-next")
                .item(gettext("Find Previous"), "viewer.find-previous")
                .section(gio::Menu::new().item(gettext("_Wrap lines"), "viewer.wrap-lines"))
                .submenu(
                    gettext("_Encoding"),
                    gio::Menu::new()
                        .item(gettext("_UTF-8"), "viewer.encoding('UTF8')")
                        .item(gettext("English (US-_ASCII)"), "viewer.encoding('ASCII')")
                        .item(gettext("Terminal (CP437)"), "viewer.encoding('CP437')")
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
                ),
        );
    }

    menu.submenu(
        gettext("_Settings"),
        gio::Menu::new()
            .item(gettext("_Font…"), "viewer.choose-font")
            .submenu(
                gettext("Fixed _Width Mode"),
                gio::Menu::new()
                    .item_param(gettext("_20 chars/line"), "viewer.chars-per-line", 20u32)
                    .item_param(gettext("_40 chars/line"), "viewer.chars-per-line", 40u32)
                    .item_param(gettext("_80 chars/line"), "viewer.chars-per-line", 80u32)
                    .item_param(gettext("_120 chars/line"), "viewer.chars-per-line", 120u32)
                    .item_param(gettext("1_60 chars/line"), "viewer.chars-per-line", 160u32),
            )
            .section(
                gio::Menu::new()
                    .item(gettext("Show Metadata _Tags"), "viewer.metadata-visible")
                    .item(gettext("_Hexadecimal Offset"), "viewer.hexadecimal-offset"),
            ),
    )
    .submenu(
        gettext("_Help"),
        gio::Menu::new()
            .item(gettext("Quick _Help"), "viewer.quick-help")
            .item(gettext("_Keyboard Shortcuts"), "viewer.keyboard-shortcuts"),
    )
}

fn handle_write_result(result: WriteResult) {
    if let Err(error) = result {
        eprintln!("Failed saving option: {error}");
    }
}

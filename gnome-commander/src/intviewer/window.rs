// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    image_render::{ImageOperation, ImageRender},
    input_modes::preprocess_for_cp437_conversion,
    search_bar::{SearchBar, SearchBarError, SearchBarInput, SearchBarOutput, SearchSettings},
    searcher::Searcher,
    text_render::{TextRender, TextRenderDisplayMode},
};
use crate::{
    file::{File, FileOps},
    file_metainfo_view::FileMetainfoView,
    options::{ViewerOptions, types::WriteResult, utils::remember_window_state},
    tags::{FileMetadataService, file_metadata::FileMetadata},
    utils::{display_help, u32_enum},
};
use component_framework::{
    action_list,
    helpers::{ActionGroup, ActionListOutput, MenuSection, Submenu},
    prelude::*,
};
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
        match content_type.split_once('/')?.0 {
            "text" => Some(Self::Text),
            "image" => Some(Self::Image),
            "audio" | "video" => Some(Self::FixedWidth),
            "application" => {
                if gio::content_type_is_a(&content_type, "text/plain") {
                    Some(Self::Text)
                } else {
                    Some(Self::FixedWidth)
                }
            }
            _ => None,
        }
    }
}

action_list! {
    enum ViewerActions {
        "viewer.copy-text-selection" as CopySelection,
        "viewer.select-all" as SelectAll,
        "viewer.close" as Close,
        "viewer.zoom-in" as ZoomIn,
        "viewer.zoom-out" as ZoomOut,
        "viewer.normal-size" as ZoomNormal,
        "viewer.best-fit" as ZoomBestFit,
        "viewer.find" as Find,
        "viewer.find-next" as FindNext,
        "viewer.find-previous" as FindPrevious,
        "viewer.display-mode" as DisplayMode(DisplayMode) = DisplayMode,
        "viewer.wrap-lines" as ToggleWrapMode = bool,
        "viewer.encoding" as Encoding(String) = String,
        "viewer.imageop" as ImageOp(ImageOperation),
        "viewer.choose-font" as ChooseFont,
        "viewer.chars-per-line" as CharsPerLine(u32) = u32,
        "viewer.hexadecimal-offset" as ToggleHexOffset = bool,
        "viewer.metadata-visible" as ToggleMetadata = bool,
        "viewer.quick-help" as QuickHelp,
        "viewer.keyboard-shortcuts" as KeyboardShortcuts,
    }
}

#[derive(Debug)]
pub struct ViewerWindowView {
    window: gtk::Window,
    menubar: gtk::PopoverMenuBar,
    stack: gtk::Stack,
    text_render: TextRender,
    image_render: ImageRender,
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
        let menu = with!(gio::Menu {
            ViewerActions::Output::CopySelection.menuitem(gettext("_Copy Selection"));
            ViewerActions::Output::SelectAll.menuitem(gettext("_Select All"));
        });
        Self::show_context_menu(&self.text_render, menu, x, y);
    }

    pub fn image_viewer_context_menu(&self, x: f64, y: f64) {
        let menu = with!(gio::Menu {
            ViewerActions::Output::ImageOp(ImageOperation::RotateClockwise)
                .menuitem(gettext("_Rotate Clockwise"));
            ViewerActions::Output::ImageOp(ImageOperation::RotateCounterclockwise)
                .menuitem(gettext("Rotate Counter Clockwis_e"));
            ViewerActions::Output::ImageOp(ImageOperation::RotateUpsideDown)
                .menuitem(gettext("Rotate 180°"));
            ViewerActions::Output::ImageOp(ImageOperation::FlipVertical)
                .menuitem(gettext("Flip _Vertical"));
            ViewerActions::Output::ImageOp(ImageOperation::FlipHorizontal)
                .menuitem(gettext("Flip _Horizontal"));
        });
        Self::show_context_menu(&self.image_render, menu, x, y);
    }
}

#[derive(Debug)]
pub enum ViewerWindowInput {
    SetMetadata(FileMetadata),
    FocusContent,
    TextStatusUpdate,
    TextViewerContextMenu(i32, f64, f64),
    ImageStatusUpdate,
    ImageViewerContextMenu(i32, f64, f64),
    SearchBar(SearchBarOutput),
    SearchProgress(u32),
    SearchDone(Option<u64>),
    Action(ViewerActions::Output),

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
    action_group: ComponentController<ActionGroup<ViewerActions::List>>,
    searchbar: ComponentController<SearchBar>,
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
            menubar: gtk::PopoverMenuBar::from_model(gio::MenuModel::NONE),
            stack: Default::default(),
            text_render: Default::default(),
            image_render: Default::default(),
            status_label: Default::default(),
            metadata_view: Default::default(),
        };

        with!(&view.window {
            .set_icon_name(Some("gnome-commander-internal-viewer"));
            .set_title(self.file.to_str());

            .insert_action_group(
                ViewerActions::prefix(),
                Some(self.action_group.attach(sender, Self::Input::Action))
            );

            .add_controller(with!(gtk::ShortcutController {
                ViewerActions::Output::Close.shortcut("<Control>Q");
                ViewerActions::Output::Close.shortcut("Escape");
                ViewerActions::Output::DisplayMode(DisplayMode::Text).shortcut("1");
                ViewerActions::Output::DisplayMode(DisplayMode::FixedWidth).shortcut("2");
                ViewerActions::Output::DisplayMode(DisplayMode::Hexdump).shortcut("3");
                ViewerActions::Output::DisplayMode(DisplayMode::Image).shortcut("4");
                ViewerActions::Output::ZoomIn.shortcut("<Control>equal");
                ViewerActions::Output::ZoomIn.shortcut("<Control>KP_Add");
                ViewerActions::Output::ZoomIn.shortcut("<Control>plus");
                ViewerActions::Output::ZoomOut.shortcut("<Control>KP_Subtract");
                ViewerActions::Output::ZoomOut.shortcut("<Control>minus");
                ViewerActions::Output::ZoomNormal.shortcut("<Control>0");
                ViewerActions::Output::ZoomBestFit.shortcut("<Control>period");
                ViewerActions::Output::CopySelection.shortcut("<Control>C");
                ViewerActions::Output::SelectAll.shortcut("<Control>A");
                ViewerActions::Output::Find.shortcut("<Control>F");
                ViewerActions::Output::FindNext.shortcut("F3");
                ViewerActions::Output::FindPrevious.shortcut("<Shift>F3");
                ViewerActions::Output::ToggleWrapMode.shortcut("<Control>W");
                ViewerActions::Output::Encoding("UTF8".to_owned()).shortcut("U");
                ViewerActions::Output::Encoding("ASCII".to_owned()).shortcut("A");
                ViewerActions::Output::Encoding("CP437".to_owned()).shortcut("Q");
                ViewerActions::Output::ImageOp(ImageOperation::RotateClockwise)
                    .shortcut("<Control>R");
                ViewerActions::Output::ImageOp(ImageOperation::RotateUpsideDown)
                    .shortcut("<Control><Shift>R");
                ViewerActions::Output::CharsPerLine(20).shortcut("<Control>2");
                ViewerActions::Output::CharsPerLine(40).shortcut("<Control>4");
                ViewerActions::Output::CharsPerLine(80).shortcut("<Control>8");
                ViewerActions::Output::ToggleMetadata.shortcut("T");
                ViewerActions::Output::ToggleHexOffset.shortcut("<Control>D");
                ViewerActions::Output::QuickHelp.shortcut("F1");
            }));

            // Make sure we signal that we are done even if close action isn't used.
            .connect_unmap(forward!(sender.output(())));

            .connect_focus_widget_notify({
                let sender = sender.clone();
                move |window| {
                    if gtk::prelude::GtkWindowExt::focus(window)
                        .is_none_or(|widget| widget.widget_name() == "GtkPopoverMenuBarItem")
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

                &view.menubar;

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
                                .connect_pressed(forward!(
                                    |_, n_press, x, y| sender.input(
                                        Self::Input::TextViewerContextMenu(n_press, x, y)
                                    )
                                ));
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
                                .connect_pressed(forward!(
                                    |_, n_press, x, y| sender.input(
                                        Self::Input::ImageViewerContextMenu(n_press, x, y)
                                    )
                                ));
                            }));
                        }
                    }), Some("image"));
                }

                self.searchbar.attach(sender, Self::Input::SearchBar);

                gtk::Box {
                    .set_orientation(gtk::Orientation::Horizontal);
                    .add_css_class("spacing");
                    .add_css_class("offset");

                    &view.status_label;
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
        self.update_tab_size(&view);
        self.update_wrap_mode(&view);
        self.update_encoding(&view);
        self.update_monospaced_font(&view);
        self.update_chars_per_line(&view);
        self.update_metadata(&view);
        self.update_hex_offset(&view);

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
            Self::Input::TextViewerContextMenu(n_press, x, y) => {
                if n_press == 1 {
                    view.text_viewer_context_menu(x, y)
                }
            }
            Self::Input::ImageStatusUpdate => view.image_status_update(),
            Self::Input::ImageViewerContextMenu(n_press, x, y) => {
                if n_press == 1 {
                    view.image_viewer_context_menu(x, y)
                }
            }
            Self::Input::SearchBar(message) => match message {
                SearchBarOutput::StartSearch(forward, settings) => {
                    self.start_search(sender, view, forward, settings)
                }
                SearchBarOutput::StopSearch => {
                    if let Some((cancellable, _, _)) = self.current_search.as_ref() {
                        cancellable.cancel();
                    }
                }
                SearchBarOutput::Closed => sender.input(Self::Input::FocusContent),
            },
            Self::Input::SearchProgress(progress) => {
                self.searchbar.send(SearchBarInput::Progress(progress))
            }
            Self::Input::SearchDone(found) => self.search_done(view, found),
            Self::Input::Action(action) => match action {
                ViewerActions::Output::CopySelection => {
                    if self.display_mode != DisplayMode::Image {
                        view.text_render.copy_selection();
                    }
                }
                ViewerActions::Output::SelectAll => {
                    if self.display_mode != DisplayMode::Image {
                        view.text_render.set_marker(0, view.text_render.size());
                    }
                }
                ViewerActions::Output::Close => sender.output(()),
                ViewerActions::Output::ZoomIn => self.zoom_in(view),
                ViewerActions::Output::ZoomOut => self.zoom_out(view),
                ViewerActions::Output::ZoomBestFit => {
                    if self.display_mode == DisplayMode::Image {
                        view.image_render.set_best_fit(true);
                    }
                }
                ViewerActions::Output::ZoomNormal => self.zoom_normal(view),
                ViewerActions::Output::Find => {
                    if self.display_mode != DisplayMode::Image {
                        self.searchbar
                            .send(SearchBarInput::Show(view.text_render.selected_text()));
                    }
                }
                ViewerActions::Output::FindNext | ViewerActions::Output::FindPrevious => {
                    if self.display_mode == DisplayMode::Image || self.current_search.is_some() {
                        return;
                    }

                    self.searchbar.send(SearchBarInput::StartSearch(matches!(
                        action,
                        ViewerActions::Output::FindNext
                    )));
                }
                ViewerActions::Output::DisplayMode(mode) => {
                    self.display_mode = mode;
                    self.update_display_mode(view);
                }
                ViewerActions::Output::ToggleWrapMode => {
                    let options = ViewerOptions::instance();
                    handle_write_result(options.wrap_mode.set(!options.wrap_mode.get()));
                }
                ViewerActions::Output::Encoding(encoding) => {
                    let options = ViewerOptions::instance();
                    handle_write_result(options.encoding.set(encoding));
                }
                ViewerActions::Output::ImageOp(operation) => {
                    if self.display_mode == DisplayMode::Image {
                        view.image_render.operation(operation);
                        view.image_render.queue_draw();
                    }
                }
                ViewerActions::Output::ChooseFont => Self::choose_font(view).await,
                ViewerActions::Output::CharsPerLine(num_chars) => {
                    let options = ViewerOptions::instance();
                    handle_write_result(options.binary_bytes_per_line.set(num_chars));
                }
                ViewerActions::Output::ToggleHexOffset => {
                    let options = ViewerOptions::instance();
                    handle_write_result(
                        options
                            .display_hex_offset
                            .set(!options.display_hex_offset.get()),
                    );
                }
                ViewerActions::Output::ToggleMetadata => {
                    let options = ViewerOptions::instance();
                    handle_write_result(
                        options
                            .metadata_visible
                            .set(!options.metadata_visible.get()),
                    );
                }
                ViewerActions::Output::QuickHelp => {
                    let window = view.window.clone();
                    glib::spawn_future_local(async move {
                        display_help(&window, Some("gnome-commander-internal-viewer")).await;
                    });
                }
                ViewerActions::Output::KeyboardShortcuts => {
                    let window = view.window.clone();
                    glib::spawn_future_local(async move {
                        display_help(&window, Some("gnome-commander-internal-viewer-keyboard"))
                            .await;
                    });
                }
            },

            Self::Input::UpdateTabSize => self.update_tab_size(view),
            Self::Input::UpdateWrapMode => self.update_wrap_mode(view),
            Self::Input::UpdateEncoding => self.update_encoding(view),
            Self::Input::UpdateMonospacedFont => self.update_monospaced_font(view),
            Self::Input::UpdateCharsPerLine => self.update_chars_per_line(view),
            Self::Input::UpdateMetadata => self.update_metadata(view),
            Self::Input::UpdateHexOffset => self.update_hex_offset(view),
        }
    }

    async fn handle_subcomponents(&mut self) {
        use futures::FutureExt;
        futures::select!(
            _ = self.action_group.handle_incoming().fuse() => {}
            _ = self.searchbar.handle_incoming().fuse() => {}
        )
    }
}

impl ViewerWindow {
    fn new(file: &File, path: &Path) -> Self {
        Self {
            action_group: Default::default(),
            searchbar: Default::default(),
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
        viewer.send(ViewerWindowInput::FocusContent);

        futures::select!(
            _ = viewer.receive().fuse() => {
                viewer.root().close();
                return;
            },
            metadata = file_metadata_service.extract_metadata(file).fuse() => {
                viewer.send(ViewerWindowInput::SetMetadata(metadata));
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
                self.searchbar.send(SearchBarInput::Close);
            }
        }

        view.menubar
            .set_menu_model(Some(&create_menu(self.display_mode)));

        self.action_group
            .change_action_state(ViewerActions::State::DisplayMode(self.display_mode));
        self.action_group.enable_action(
            ViewerActions::List::ZoomBestFit,
            self.display_mode == DisplayMode::Image,
        );
    }

    pub fn update_tab_size(&self, view: &ViewerWindowView) {
        view.text_render
            .set_tab_size(ViewerOptions::instance().tab_size.get());
    }

    pub fn update_wrap_mode(&self, view: &ViewerWindowView) {
        let value = ViewerOptions::instance().wrap_mode.get();
        view.text_render.set_wrap_mode(value);
        self.action_group
            .change_action_state(ViewerActions::State::ToggleWrapMode(value))
    }

    pub fn update_encoding(&self, view: &ViewerWindowView) {
        let value = ViewerOptions::instance().encoding.get();
        view.text_render.set_encoding(value.as_str());
        self.action_group
            .change_action_state(ViewerActions::State::Encoding(value))
    }

    pub fn update_monospaced_font(&self, view: &ViewerWindowView) {
        view.text_render
            .set_monospaced_font(ViewerOptions::instance().monospaced_font.get());
    }

    pub fn update_chars_per_line(&self, view: &ViewerWindowView) {
        let value = ViewerOptions::instance().binary_bytes_per_line.get();
        view.text_render.set_fixed_limit(value);
        self.action_group
            .change_action_state(ViewerActions::State::CharsPerLine(value))
    }

    pub fn update_metadata(&self, view: &ViewerWindowView) {
        let value = ViewerOptions::instance().metadata_visible.get();
        view.metadata_view.set_visible(value);
        self.action_group
            .change_action_state(ViewerActions::State::ToggleMetadata(value))
    }

    pub fn update_hex_offset(&self, view: &ViewerWindowView) {
        let value = ViewerOptions::instance().display_hex_offset.get();
        view.text_render.set_hexadecimal_offset(value);
        self.action_group
            .change_action_state(ViewerActions::State::ToggleHexOffset(value))
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
        settings: SearchSettings,
    ) {
        let input_mode = view.text_render.input_mode();
        let end_offset = input_mode.max_offset();
        let start_offset = if self.searchbar.displayed_error() == SearchBarError::NotFound {
            // Wrap the search around after “Not found”
            if forward { 0 } else { end_offset }
        } else if let Some((start_marker, end_marker)) = view.text_render.marker()
            && view.text_render.is_range_visible(start_marker, end_marker)
        {
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
                let encoding = ViewerOptions::instance().encoding.get();
                let pattern = if encoding.eq_ignore_ascii_case("CP437") {
                    preprocess_for_cp437_conversion(&pattern)
                } else {
                    pattern.into()
                };
                let Some((slice, _)) = glib::IConv::new(&*encoding, "UTF8")
                    .and_then(|mut iconv| iconv.convert(pattern.as_bytes()).ok())
                else {
                    self.searchbar
                        .send(SearchBarInput::ShowError(SearchBarError::EncodingError));
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
        self.searchbar.send(SearchBarInput::HideProgress);
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
                    self.searchbar
                        .send(SearchBarInput::ShowError(SearchBarError::NotFound));
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
    with!(gio::Menu {
        Submenu::new(gettext("_File")) {
            ViewerActions::Output::Close.menuitem(gettext("_Close"));
        }
        Submenu::new(gettext("_View")) {
            ViewerActions::Output::DisplayMode(DisplayMode::Text).menuitem(gettext("_Text"));
            ViewerActions::Output::DisplayMode(DisplayMode::FixedWidth)
                .menuitem(gettext("_Fixed Width"));
            ViewerActions::Output::DisplayMode(DisplayMode::Hexdump)
                .menuitem(gettext("_Hexadecimal"));
            ViewerActions::Output::DisplayMode(DisplayMode::Image).menuitem(gettext("_Image"));
            MenuSection {
                ViewerActions::Output::ZoomIn.menuitem(gettext("_Zoom In"));
                ViewerActions::Output::ZoomOut.menuitem(gettext("_Zoom Out"));
                ViewerActions::Output::ZoomNormal.menuitem(gettext("_Normal Size"));
                ViewerActions::Output::ZoomBestFit.menuitem(gettext("_Best Fit"));
            }
        }
        if matches!(
            display_mode,
            DisplayMode::Text | DisplayMode::FixedWidth | DisplayMode::Hexdump
        ) {
            Submenu::new(gettext("_Text")) {
                ViewerActions::Output::CopySelection.menuitem(gettext("_Copy Text Selection"));
                ViewerActions::Output::SelectAll.menuitem(gettext("_Select All"));
                ViewerActions::Output::Find.menuitem(gettext("Find…"));
                ViewerActions::Output::FindNext.menuitem(gettext("Find Next"));
                ViewerActions::Output::FindPrevious.menuitem(gettext("Find Previous"));
                MenuSection {
                    ViewerActions::Output::ToggleWrapMode.menuitem(gettext("_Wrap lines"));
                }
                Submenu::new(gettext("_Encoding")) {
                    ViewerActions::Output::Encoding("UTF8".to_owned()).menuitem(gettext("_UTF-8"));
                    ViewerActions::Output::Encoding("ASCII".to_owned())
                        .menuitem(gettext("English (US-_ASCII)"));
                    ViewerActions::Output::Encoding("CP437".to_owned())
                        .menuitem(gettext("Terminal (CP437)"));
                    ViewerActions::Output::Encoding("ISO-8859-6".to_owned())
                        .menuitem(gettext("Arabic (ISO-8859-6)"));
                    ViewerActions::Output::Encoding("ARABIC".to_owned())
                        .menuitem(gettext("Arabic (Windows, CP1256)"));
                    ViewerActions::Output::Encoding("CP864".to_owned())
                        .menuitem(gettext("Arabic (Dos, CP864)"));
                    ViewerActions::Output::Encoding("ISO-8859-4".to_owned())
                        .menuitem(gettext("Baltic (ISO-8859-4)"));
                    ViewerActions::Output::Encoding("ISO-8859-2".to_owned())
                        .menuitem(gettext("Central European (ISO-8859-2)"));
                    ViewerActions::Output::Encoding("CP1250".to_owned())
                        .menuitem(gettext("Central European (CP1250)"));
                    ViewerActions::Output::Encoding("ISO-8859-5".to_owned())
                        .menuitem(gettext("Cyrillic (ISO-8859-5)"));
                    ViewerActions::Output::Encoding("CP1251".to_owned())
                        .menuitem(gettext("Cyrillic (CP1251)"));
                    ViewerActions::Output::Encoding("ISO-8859-7".to_owned())
                        .menuitem(gettext("Greek (ISO-8859-7)"));
                    ViewerActions::Output::Encoding("CP1253".to_owned())
                        .menuitem(gettext("Greek (CP1253)"));
                    ViewerActions::Output::Encoding("HEBREW".to_owned())
                        .menuitem(gettext("Hebrew (Windows, CP1255)"));
                    ViewerActions::Output::Encoding("CP862".to_owned())
                        .menuitem(gettext("Hebrew (Dos, CP862)"));
                    ViewerActions::Output::Encoding("ISO-8859-8".to_owned())
                        .menuitem(gettext("Hebrew (ISO-8859-8)"));
                    ViewerActions::Output::Encoding("ISO-8859-15".to_owned())
                        .menuitem(gettext("Latin 9 (ISO-8859-15)"));
                    ViewerActions::Output::Encoding("ISO-8859-3".to_owned())
                        .menuitem(gettext("Maltese (ISO-8859-3)"));
                    ViewerActions::Output::Encoding("ISO-8859-9".to_owned())
                        .menuitem(gettext("Turkish (ISO-8859-9)"));
                    ViewerActions::Output::Encoding("CP1254".to_owned())
                        .menuitem(gettext("Turkish (CP1254)"));
                    ViewerActions::Output::Encoding("CP1252".to_owned())
                        .menuitem(gettext("Western (CP1252)"));
                    ViewerActions::Output::Encoding("ISO-8859-1".to_owned())
                        .menuitem(gettext("Western (ISO-8859-1)"));
                }
            }
        } else {
            Submenu::new(gettext("_Image")) {
                ViewerActions::Output::ImageOp(ImageOperation::RotateClockwise)
                    .menuitem(gettext("_Rotate Clockwise"));
                ViewerActions::Output::ImageOp(ImageOperation::RotateCounterclockwise)
                    .menuitem(gettext("Rotate Counter Clockwis_e"));
                ViewerActions::Output::ImageOp(ImageOperation::RotateUpsideDown)
                    .menuitem(gettext("Rotate 180°"));
                ViewerActions::Output::ImageOp(ImageOperation::FlipVertical)
                    .menuitem(gettext("Flip _Vertical"));
                ViewerActions::Output::ImageOp(ImageOperation::FlipHorizontal)
                    .menuitem(gettext("Flip _Horizontal"));
            }
        }

        Submenu::new(gettext("_Settings")) {
            ViewerActions::Output::ChooseFont.menuitem(gettext("_Font…"));
            Submenu::new(gettext("Fixed _Width Mode")) {
                ViewerActions::Output::CharsPerLine(20).menuitem(gettext("_20 chars/line"));
                ViewerActions::Output::CharsPerLine(40).menuitem(gettext("_40 chars/line"));
                ViewerActions::Output::CharsPerLine(80).menuitem(gettext("_80 chars/line"));
                ViewerActions::Output::CharsPerLine(120).menuitem(gettext("_120 chars/line"));
                ViewerActions::Output::CharsPerLine(160).menuitem(gettext("1_60 chars/line"));
            }
            MenuSection {
                ViewerActions::Output::ToggleMetadata.menuitem(gettext("Show Metadata _Tags"));
                ViewerActions::Output::ToggleHexOffset.menuitem(gettext("_Hexadecimal Offset"));
            }
        }

        Submenu::new(gettext("_Help")) {
            ViewerActions::Output::QuickHelp.menuitem(gettext("Quick _Help"));
            ViewerActions::Output::KeyboardShortcuts.menuitem(gettext("_Keyboard Shortcuts"));
        }
    })
}

fn handle_write_result(result: WriteResult) {
    if let Err(error) = result {
        eprintln!("Failed saving option: {error}");
    }
}

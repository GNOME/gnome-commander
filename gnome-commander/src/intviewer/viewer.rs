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
    file_metainfo_view::FileMetainfoView,
    options::{ViewerOptions, types::WriteResult},
    tags::file_metadata::FileMetadata,
    utils::u32_enum,
};
use component_framework::{action_list, helpers::ActionListOutput, prelude::*};
use gettextrs::{gettext, ngettext};
use gtk::{gdk, gio, glib, prelude::*};
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
    pub fn guess(content_type: Option<&str>) -> Option<Self> {
        let content_type = content_type?.to_lowercase();
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
    pub enum ViewerActions {
        "viewer.copy-text-selection" as CopySelection,
        "viewer.select-all" as SelectAll,
        "viewer.zoom-in" as ZoomIn,
        "viewer.zoom-out" as ZoomOut,
        "viewer.normal-size" as ZoomNormal,
        "viewer.best-fit" as ZoomBestFit,
        "viewer.metadata-visible" as ToggleMetadataVisible = bool,
        "viewer.find" as Find,
        "viewer.find-next" as FindNext,
        "viewer.find-previous" as FindPrevious,
        "viewer.display-mode" as DisplayMode(DisplayMode) = DisplayMode,
        "viewer.wrap-lines" as ToggleWrapMode = bool,
        "viewer.encoding" as Encoding(String) = String,
        "viewer.imageop" as ImageOp(ImageOperation),
    }
}

#[derive(Debug, Default)]
pub struct ViewerView {
    root: gtk::Box,
    stack: gtk::Stack,
    text_render: TextRender,
    image_render: ImageRender,
    status_label: gtk::Label,
    metadata_view: FileMetainfoView,
}

impl ViewerView {
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
pub enum ViewerInput {
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

    UpdateMonospacedFont,
    UpdateCharsPerLine,
    UpdateTabSize,
    UpdateHexOffset,
}

#[derive(Debug)]
pub enum ViewerOutput {
    DisplayMode(DisplayMode),
    MetadataVisible(bool),
    WrapMode(bool),
    Encoding(String),
}

#[derive(Debug)]
pub struct Viewer {
    id: usize,
    searchbar: ComponentController<SearchBar>,
    option_handlers: Vec<glib::SignalHandlerId>,
    file: PathBuf,
    display_mode: DisplayMode,
    metadata_visible: bool,
    wrap_mode: bool,
    encoding: String,
    current_scale_index: usize,
    current_search: Option<(gio::Cancellable, bool, usize)>,
}

impl Drop for Viewer {
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

impl Component for Viewer {
    type View = ViewerView;
    type Root = gtk::Box;
    type Input = ViewerInput;
    type Output = ViewerOutput;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let view = Self::View::default();

        with!(&view.root {
            .set_orientation(gtk::Orientation::Vertical);
            .add_css_class("spacing");

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
        });

        let options = ViewerOptions::instance();

        with!(&mut self.option_handlers {
            .push(options.monospaced_font.connect_changed(
                forward!(sender.input(Self::Input::UpdateMonospacedFont))
            ));
            .push(options.binary_bytes_per_line.connect_changed(
                forward!(sender.input(Self::Input::UpdateCharsPerLine))
            ));
            .push(options.tab_size.connect_changed(
                forward!(sender.input(Self::Input::UpdateTabSize))
            ));
            .push(options.display_hex_offset.connect_changed(
                forward!(sender.input(Self::Input::UpdateHexOffset))
            ));
        });

        self.update_display_mode(sender, &view);
        self.update_metadata_visible(sender, &view);
        self.update_wrap_mode(sender, &view);
        self.update_encoding(sender, &view);
        self.update_monospaced_font(&view);
        self.update_chars_per_line(&view);
        self.update_tab_size(&view);
        self.update_hex_offset(&view);

        view
    }

    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
        &view.root
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
                ViewerActions::Output::ZoomIn => self.zoom_in(view),
                ViewerActions::Output::ZoomOut => self.zoom_out(view),
                ViewerActions::Output::ZoomBestFit => {
                    if self.display_mode == DisplayMode::Image {
                        view.image_render.set_best_fit(true);
                    }
                }
                ViewerActions::Output::ZoomNormal => self.zoom_normal(view),
                ViewerActions::Output::ToggleMetadataVisible => {
                    self.metadata_visible = !self.metadata_visible;
                    handle_write_result(
                        ViewerOptions::instance()
                            .metadata_visible
                            .set(self.metadata_visible),
                    );
                    self.update_metadata_visible(sender, view);
                }
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
                    self.update_display_mode(sender, view);
                }
                ViewerActions::Output::ToggleWrapMode => {
                    self.wrap_mode = !self.wrap_mode;
                    handle_write_result(ViewerOptions::instance().wrap_mode.set(self.wrap_mode));
                    self.update_wrap_mode(sender, view);
                }
                ViewerActions::Output::Encoding(encoding) => {
                    self.encoding = encoding;
                    handle_write_result(ViewerOptions::instance().encoding.set(&self.encoding));
                    self.update_encoding(sender, view);
                }
                ViewerActions::Output::ImageOp(operation) => {
                    if self.display_mode == DisplayMode::Image {
                        view.image_render.operation(operation);
                        view.image_render.queue_draw();
                    }
                }
            },

            Self::Input::UpdateMonospacedFont => self.update_monospaced_font(view),
            Self::Input::UpdateCharsPerLine => self.update_chars_per_line(view),
            Self::Input::UpdateTabSize => self.update_tab_size(view),
            Self::Input::UpdateHexOffset => self.update_hex_offset(view),
        }
    }

    async fn handle_subcomponents(&mut self) {
        self.searchbar.handle_incoming().await
    }
}

impl Viewer {
    pub fn new(id: usize, path: &Path, content_type: Option<&str>) -> Self {
        let options = ViewerOptions::instance();
        Self {
            id,
            searchbar: Default::default(),
            option_handlers: Vec::new(),
            file: path.to_path_buf(),
            current_scale_index: DEFAULT_IMAGE_SCALE_INDEX,
            display_mode: DisplayMode::guess(content_type).unwrap_or_default(),
            metadata_visible: options.metadata_visible.get(),
            wrap_mode: options.wrap_mode.get(),
            encoding: options.encoding.get(),
            current_search: None,
        }
    }

    pub fn id(&self) -> usize {
        self.id
    }

    pub fn title(&self) -> Option<&str> {
        self.file.to_str()
    }

    pub fn short_title(&self) -> &str {
        self.file
            .file_name()
            .and_then(|name| name.to_str())
            .unwrap_or_default()
    }

    pub fn display_mode(&self) -> DisplayMode {
        self.display_mode
    }

    pub fn metadata_visible(&self) -> bool {
        self.metadata_visible
    }

    pub fn wrap_mode(&self) -> bool {
        self.wrap_mode
    }

    pub fn encoding(&self) -> &str {
        &self.encoding
    }

    fn update_display_mode(&self, sender: &ComponentSender<Self>, view: &ViewerView) {
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

        sender.output(ViewerOutput::DisplayMode(self.display_mode));
    }

    pub fn update_metadata_visible(&self, sender: &ComponentSender<Self>, view: &ViewerView) {
        view.metadata_view.set_visible(self.metadata_visible);
        sender.output(ViewerOutput::MetadataVisible(self.metadata_visible));
    }

    pub fn update_wrap_mode(&self, sender: &ComponentSender<Self>, view: &ViewerView) {
        view.text_render.set_wrap_mode(self.wrap_mode);
        sender.output(ViewerOutput::WrapMode(self.wrap_mode));
    }

    pub fn update_encoding(&self, sender: &ComponentSender<Self>, view: &ViewerView) {
        view.text_render.set_encoding(self.encoding.as_str());
        sender.output(ViewerOutput::Encoding(self.encoding.clone()));
    }

    pub fn update_monospaced_font(&self, view: &ViewerView) {
        view.text_render
            .set_monospaced_font(ViewerOptions::instance().monospaced_font.get());
    }

    pub fn update_chars_per_line(&self, view: &ViewerView) {
        let value = ViewerOptions::instance().binary_bytes_per_line.get();
        view.text_render.set_fixed_limit(value);
    }

    pub fn update_tab_size(&self, view: &ViewerView) {
        view.text_render
            .set_tab_size(ViewerOptions::instance().tab_size.get());
    }

    pub fn update_hex_offset(&self, view: &ViewerView) {
        let value = ViewerOptions::instance().display_hex_offset.get();
        view.text_render.set_hexadecimal_offset(value);
    }

    fn zoom_in(&mut self, view: &ViewerView) {
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

    fn zoom_out(&mut self, view: &ViewerView) {
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

    fn zoom_normal(&mut self, view: &ViewerView) {
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
        view: &ViewerView,
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
                    sender.input(ViewerInput::SearchProgress(p));
                    last_report.set(Some(elapsed));
                }
            });
            sender.input(ViewerInput::SearchDone(found));
        });
    }

    fn search_done(&mut self, view: &ViewerView, found: Option<u64>) {
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
}

fn handle_write_result(result: WriteResult) {
    if let Err(error) = result {
        eprintln!("Failed saving option: {error}");
    }
}

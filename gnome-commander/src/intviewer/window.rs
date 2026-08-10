// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    image_render::ImageOperation,
    viewer::{DisplayMode, Viewer, ViewerActions, ViewerInput, ViewerOutput},
};
use crate::{
    file::{File, FileOps},
    options::{ViewerOptions, types::WriteResult, utils::remember_window_state},
    tags::{FileMetadataService, file_metadata::FileMetadata},
    utils::display_help,
};
use component_framework::{
    action_list,
    helpers::{ActionGroup, ActionListOutput, MenuSection, Shortcuts, Submenu},
    prelude::*,
};
use gettextrs::gettext;
use gtk::{gio, pango, prelude::*};
use std::{
    cell::RefCell,
    path::PathBuf,
    sync::atomic::{AtomicUsize, Ordering},
};

action_list! {
    pub enum ViewerWindowActions {
        #[label = gettext("_Close")]
        #[shortcut = "<Control>W"]
        #[shortcut = "Escape"]
        "viewer-window.close" as Close,

        #[label = gettext("_Quit")]
        #[shortcut = "<Control>Q"]
        "viewer-window.quit" as Quit,

        #[shortcut = "<Control>ISO_Left_Tab"]
        #[shortcut = "<Control>Tab"]
        "viewer-window.next-tab" as NextTab,

        #[shortcut = "<Shift><Control>ISO_Left_Tab"]
        #[shortcut = "<Shift><Control>Tab"]
        "viewer-window.previous-tab" as PreviousTab,

        #[label = gettext("_Font…")]
        "viewer-window.choose-font" as ChooseFont,

        #[shortcut("<Control>2", 20)]
        #[shortcut("<Control>4", 40)]
        #[shortcut("<Control>8", 80)]
        "viewer-window.chars-per-line" as CharsPerLine(u32) = u32,

        #[label = gettext("_Hexadecimal Offset")]
        #[shortcut = "<Control>D"]
        "viewer-window.hexadecimal-offset" as ToggleHexOffset = bool,

        #[label = gettext("Quick _Help")]
        #[shortcut = "F1"]
        "viewer-window.quick-help" as QuickHelp,

        #[label = gettext("_Keyboard Shortcuts")]
        "viewer-window.keyboard-shortcuts" as KeyboardShortcuts,
    }
}

#[derive(Debug)]
pub struct ViewerWindowView {
    window: gtk::Window,
    menubar: gtk::PopoverMenuBar,
    notebook: gtk::Notebook,
}

#[derive(Debug)]
pub enum ViewerWindowInput {
    UpdateCurrent,
    AddFile(usize, PathBuf, Option<String>),
    FileClosed(usize),
    FileReordered(usize),
    FocusContent,
    Close(usize),
    SetMetadata(usize, FileMetadata),
    Viewer(usize, ViewerOutput),
    ViewerAction(ViewerActions::Output),
    WindowAction(ViewerWindowActions::Output),

    UpdateCharsPerLine,
    UpdateHexOffset,
}

#[derive(Debug)]
pub struct ViewerWindow {
    viewer_action_group: ComponentController<ActionGroup<ViewerActions::List>>,
    window_action_group: ComponentController<ActionGroup<ViewerWindowActions::List>>,
    viewer_shortcuts: ComponentController<Shortcuts<ViewerActions::List>>,
    window_shortcuts: ComponentController<Shortcuts<ViewerWindowActions::List>>,
    viewers: Vec<ComponentController<Viewer>>,
    option_handlers: Vec<glib::SignalHandlerId>,
}

impl Drop for ViewerWindow {
    fn drop(&mut self) {
        let options = ViewerOptions::instance();
        for handler in self.option_handlers.drain(..) {
            options.encoding.disconnect(handler);
        }
    }
}

impl Component for ViewerWindow {
    type View = ViewerWindowView;
    type Root = gtk::Window;
    type Input = ViewerWindowInput;
    type Output = ();

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let view = ViewerWindowView {
            window: Default::default(),
            menubar: gtk::PopoverMenuBar::from_model(gio::MenuModel::NONE),
            notebook: Default::default(),
        };

        with!(&view.window {
            .set_icon_name(Some("gnome-commander-internal-viewer"));

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

            .insert_action_group(
                ViewerWindowActions::prefix(),
                Some(self.window_action_group.attach(sender, Self::Input::WindowAction))
            );

            &self.window_shortcuts;

            gtk::Box {
                .set_orientation(gtk::Orientation::Vertical);
                .add_css_class("spacing");

                &view.menubar {
                    .insert_action_group(
                        ViewerActions::prefix(),
                        Some(self.viewer_action_group.attach(sender, Self::Input::ViewerAction))
                    );

                    &self.viewer_shortcuts;
                }

                &view.notebook {
                    .set_vexpand(true);
                    .set_scrollable(true);

                    .connect_switch_page(
                        forward!(|_, _, _| sender.input(Self::Input::UpdateCurrent))
                    );
                    .connect_page_removed(
                        forward!(|_, _, pos| sender.input(Self::Input::FileClosed(pos as usize)))
                    );
                    .connect_page_reordered(
                        forward!(|_, _, pos| sender.input(Self::Input::FileReordered(pos as usize)))
                    );
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
            .push(options.binary_bytes_per_line.connect_changed(
                forward!(sender.input(Self::Input::UpdateCharsPerLine))
            ));
            .push(options.display_hex_offset.connect_changed(
                forward!(sender.input(Self::Input::UpdateHexOffset))
            ));
        });

        self.update_chars_per_line();
        self.update_hex_offset();

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
            Self::Input::UpdateCurrent => {
                let viewer = self.current_viewer(view);
                view.window.set_title(viewer.title());
                for message in [
                    ViewerOutput::DisplayMode(viewer.display_mode()),
                    ViewerOutput::MetadataVisible(viewer.metadata_visible()),
                    ViewerOutput::WrapMode(viewer.wrap_mode()),
                    ViewerOutput::Encoding(viewer.encoding().to_owned()),
                ] {
                    self.handle_viewer_message(message, true, view);
                }
                viewer.send(ViewerInput::FocusContent);
            }
            Self::Input::AddFile(id, path, content_type) => {
                let viewer = Viewer::new(id, &path, content_type.as_deref()).build();
                let index = view.notebook.append_page(
                    viewer.attach(sender, move |message| Self::Input::Viewer(id, message)),
                    Some(&with!(gtk::Box {
                        .set_orientation(gtk::Orientation::Horizontal);

                        .add_controller(with!(gtk::GestureClick {
                            .set_button(2);
                            .connect_pressed(forward!(
                                |_, _, _, _| sender.input(Self::Input::Close(id))
                            ));
                        }));

                        gtk::Label {
                            .set_label(viewer.short_title());
                            .set_width_chars(40);
                            .set_ellipsize(pango::EllipsizeMode::Middle);
                            .set_tooltip_text(Some(viewer.short_title()));
                        }

                        gtk::Button {
                            .add_css_class("flat");
                            .set_icon_name("window-close");
                            .connect_clicked(forward!(sender.input(Self::Input::Close(id))));
                        }
                    })),
                );
                view.notebook.set_tab_reorderable(viewer.root(), true);
                view.notebook.set_current_page(Some(index));
                self.viewers.push(viewer);
                sender.input(Self::Input::UpdateCurrent);
                view.window.present();
            }
            Self::Input::FileClosed(pos) => {
                self.viewers.remove(pos);
                if self.viewers.is_empty() {
                    sender.output(());
                } else {
                    sender.input(Self::Input::UpdateCurrent);
                }
            }
            Self::Input::FileReordered(pos) => {
                let widget = view.notebook.nth_page(Some(pos as u32));
                if let Some(old_pos) = self
                    .viewers
                    .iter()
                    .position(|viewer| Some(viewer.root().upcast_ref()) == widget.as_ref())
                {
                    let viewer = self.viewers.remove(old_pos);
                    self.viewers.insert(pos, viewer);
                } else {
                    eprintln!("Something is wrong, could not find viewer tab that was reordered");
                }
            }
            Self::Input::FocusContent => self.current_viewer(view).send(ViewerInput::FocusContent),
            Self::Input::Close(id) => {
                if let Some(pos) = self.viewers.iter().position(|viewer| viewer.id() == id) {
                    view.notebook.remove_page(u32::try_from(pos).ok());
                }
            }
            Self::Input::SetMetadata(id, metadata) => {
                for viewer in &self.viewers {
                    if viewer.id() == id {
                        viewer.send(ViewerInput::SetMetadata(metadata));
                        break;
                    }
                }
            }
            Self::Input::Viewer(id, message) => {
                self.handle_viewer_message(message, id == self.current_viewer(view).id(), view)
            }
            Self::Input::ViewerAction(action) => {
                self.current_viewer(view).send(ViewerInput::Action(action))
            }
            Self::Input::WindowAction(action) => match action {
                ViewerWindowActions::Output::Close => {
                    view.notebook.remove_page(view.notebook.current_page());
                }
                ViewerWindowActions::Output::Quit => sender.output(()),
                ViewerWindowActions::Output::NextTab => {
                    view.notebook.emit_change_current_page(1);
                }
                ViewerWindowActions::Output::PreviousTab => {
                    view.notebook.emit_change_current_page(-1);
                }
                ViewerWindowActions::Output::ChooseFont => Self::choose_font(view).await,
                ViewerWindowActions::Output::CharsPerLine(num_chars) => {
                    let options = ViewerOptions::instance();
                    handle_write_result(options.binary_bytes_per_line.set(num_chars));
                }
                ViewerWindowActions::Output::ToggleHexOffset => {
                    let options = ViewerOptions::instance();
                    handle_write_result(
                        options
                            .display_hex_offset
                            .set(!options.display_hex_offset.get()),
                    );
                }
                ViewerWindowActions::Output::QuickHelp => {
                    let window = view.window.clone();
                    glib::spawn_future_local(async move {
                        display_help(&window, Some("gnome-commander-internal-viewer")).await;
                    });
                }
                ViewerWindowActions::Output::KeyboardShortcuts => {
                    let window = view.window.clone();
                    glib::spawn_future_local(async move {
                        display_help(&window, Some("gnome-commander-internal-viewer-keyboard"))
                            .await;
                    });
                }
            },
            Self::Input::UpdateCharsPerLine => self.update_chars_per_line(),
            Self::Input::UpdateHexOffset => self.update_hex_offset(),
        }
    }

    async fn handle_subcomponents(&mut self) {
        use futures::FutureExt;
        futures::select!(
            _ = self.viewer_action_group.handle_incoming().fuse() => {}
            _ = self.window_action_group.handle_incoming().fuse() => {}
            _ = self.viewer_shortcuts.handle_incoming().fuse() => {}
            _ = self.window_shortcuts.handle_incoming().fuse() => {}
            _ = Self::handle_subcomponent_list(&mut self.viewers).fuse() => {}
        )
    }
}

impl ViewerWindow {
    pub async fn file_view(file: &File) {
        use futures::FutureExt;

        thread_local! {
            static INSTANCE: RefCell<Option<ComponentSender<ViewerWindow>>> = Default::default();
        }
        static VIEWER_ID: AtomicUsize = AtomicUsize::new(0);

        let Some(path) = file.local_path() else {
            eprintln!("Cannot view file: no local path");
            return;
        };

        let id = VIEWER_ID.fetch_add(1, Ordering::Relaxed);
        let content_type = file.content_type().as_ref().map(glib::GString::to_string);
        if let Some(sender) = INSTANCE.with_borrow(|sender| sender.as_ref().cloned()) {
            sender.input(ViewerWindowInput::AddFile(id, path, content_type));
            sender.input(ViewerWindowInput::SetMetadata(
                id,
                FileMetadataService::default().extract_metadata(file).await,
            ));
        } else {
            let mut this = Self {
                viewer_action_group: Default::default(),
                window_action_group: Default::default(),
                viewer_shortcuts: Shortcuts::inactive([]).build(),
                window_shortcuts: Shortcuts::new([]).build(),
                viewers: Vec::new(),
                option_handlers: Vec::new(),
            }
            .build();
            this.send(ViewerWindowInput::AddFile(id, path, content_type));

            let sender = this.sender().clone();
            INSTANCE.with_borrow_mut(|instance| *instance = Some(sender));

            let file_metadata_service = FileMetadataService::default();
            futures::select!(
                _ = this.receive().fuse() => {
                    this.root().close();
                    INSTANCE.with_borrow_mut(|instance| *instance = None);
                    return;
                },
                metadata = file_metadata_service.extract_metadata(file).fuse() => {
                    this.send(ViewerWindowInput::SetMetadata(id, metadata));
                }
            );

            let _ = this.receive().await;
            this.root().close();
            INSTANCE.with_borrow_mut(|instance| *instance = None);
        }
    }

    fn current_viewer(&self, view: &ViewerWindowView) -> &ComponentController<Viewer> {
        &self.viewers[view.notebook.current_page().unwrap_or_default() as usize]
    }

    fn handle_viewer_message(&self, message: ViewerOutput, current: bool, view: &ViewerWindowView) {
        match message {
            ViewerOutput::DisplayMode(mode) => {
                if !current {
                    return;
                }
                view.menubar.set_menu_model(Some(&create_menu(mode)));

                self.viewer_action_group
                    .change_action_state(ViewerActions::State::DisplayMode(mode));
                self.viewer_action_group
                    .enable_action(ViewerActions::List::ZoomBestFit, mode == DisplayMode::Image);
            }
            ViewerOutput::MetadataVisible(metadata) => {
                if !current {
                    return;
                }
                self.viewer_action_group
                    .change_action_state(ViewerActions::State::ToggleMetadataVisible(metadata));
            }
            ViewerOutput::WrapMode(wrap) => {
                if !current {
                    return;
                }
                self.viewer_action_group
                    .change_action_state(ViewerActions::State::ToggleWrapMode(wrap));
            }
            ViewerOutput::Encoding(encoding) => {
                if !current {
                    return;
                }
                self.viewer_action_group
                    .change_action_state(ViewerActions::State::Encoding(encoding));
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

    fn update_chars_per_line(&self) {
        self.window_action_group
            .change_action_state(ViewerWindowActions::State::CharsPerLine(
                ViewerOptions::instance().binary_bytes_per_line.get(),
            ));
    }

    fn update_hex_offset(&self) {
        self.window_action_group
            .change_action_state(ViewerWindowActions::State::ToggleHexOffset(
                ViewerOptions::instance().display_hex_offset.get(),
            ));
    }
}

fn create_menu(display_mode: DisplayMode) -> gio::Menu {
    with!(gio::Menu {
        Submenu::new(gettext("_File")) {
            ViewerWindowActions::Output::Close.menuitem();
            ViewerWindowActions::Output::Quit.menuitem();
        }
        Submenu::new(gettext("_View")) {
            ViewerActions::Output::DisplayMode(DisplayMode::Text)
                .menuitem_with_label(gettext("_Text"));
            ViewerActions::Output::DisplayMode(DisplayMode::FixedWidth)
                .menuitem_with_label(gettext("_Fixed Width"));
            ViewerActions::Output::DisplayMode(DisplayMode::Hexdump)
                .menuitem_with_label(gettext("_Hexadecimal"));
            ViewerActions::Output::DisplayMode(DisplayMode::Image)
                .menuitem_with_label(gettext("_Image"));
            MenuSection {
                ViewerActions::Output::ZoomIn.menuitem();
                ViewerActions::Output::ZoomOut.menuitem();
                ViewerActions::Output::ZoomNormal.menuitem();
                ViewerActions::Output::ZoomBestFit.menuitem();
            }
            MenuSection {
                ViewerActions::Output::ToggleMetadataVisible.menuitem();
            }
        }
        if matches!(
            display_mode,
            DisplayMode::Text | DisplayMode::FixedWidth | DisplayMode::Hexdump
        ) {
            Submenu::new(gettext("_Text")) {
                ViewerActions::Output::CopySelection.menuitem();
                ViewerActions::Output::SelectAll.menuitem();
                ViewerActions::Output::Find.menuitem();
                ViewerActions::Output::FindNext.menuitem();
                ViewerActions::Output::FindPrevious.menuitem();
                MenuSection {
                    ViewerActions::Output::ToggleWrapMode.menuitem();
                }
                Submenu::new(gettext("_Encoding")) {
                    ViewerActions::Output::Encoding("UTF8".to_owned())
                        .menuitem_with_label(gettext("_UTF-8"));
                    ViewerActions::Output::Encoding("ASCII".to_owned())
                        .menuitem_with_label(gettext("English (US-_ASCII)"));
                    ViewerActions::Output::Encoding("CP437".to_owned())
                        .menuitem_with_label(gettext("Terminal (CP437)"));
                    ViewerActions::Output::Encoding("ISO-8859-6".to_owned())
                        .menuitem_with_label(gettext("Arabic (ISO-8859-6)"));
                    ViewerActions::Output::Encoding("ARABIC".to_owned())
                        .menuitem_with_label(gettext("Arabic (Windows, CP1256)"));
                    ViewerActions::Output::Encoding("CP864".to_owned())
                        .menuitem_with_label(gettext("Arabic (Dos, CP864)"));
                    ViewerActions::Output::Encoding("ISO-8859-4".to_owned())
                        .menuitem_with_label(gettext("Baltic (ISO-8859-4)"));
                    ViewerActions::Output::Encoding("ISO-8859-2".to_owned())
                        .menuitem_with_label(gettext("Central European (ISO-8859-2)"));
                    ViewerActions::Output::Encoding("CP1250".to_owned())
                        .menuitem_with_label(gettext("Central European (CP1250)"));
                    ViewerActions::Output::Encoding("ISO-8859-5".to_owned())
                        .menuitem_with_label(gettext("Cyrillic (ISO-8859-5)"));
                    ViewerActions::Output::Encoding("CP1251".to_owned())
                        .menuitem_with_label(gettext("Cyrillic (CP1251)"));
                    ViewerActions::Output::Encoding("ISO-8859-7".to_owned())
                        .menuitem_with_label(gettext("Greek (ISO-8859-7)"));
                    ViewerActions::Output::Encoding("CP1253".to_owned())
                        .menuitem_with_label(gettext("Greek (CP1253)"));
                    ViewerActions::Output::Encoding("HEBREW".to_owned())
                        .menuitem_with_label(gettext("Hebrew (Windows, CP1255)"));
                    ViewerActions::Output::Encoding("CP862".to_owned())
                        .menuitem_with_label(gettext("Hebrew (Dos, CP862)"));
                    ViewerActions::Output::Encoding("ISO-8859-8".to_owned())
                        .menuitem_with_label(gettext("Hebrew (ISO-8859-8)"));
                    ViewerActions::Output::Encoding("ISO-8859-15".to_owned())
                        .menuitem_with_label(gettext("Latin 9 (ISO-8859-15)"));
                    ViewerActions::Output::Encoding("ISO-8859-3".to_owned())
                        .menuitem_with_label(gettext("Maltese (ISO-8859-3)"));
                    ViewerActions::Output::Encoding("ISO-8859-9".to_owned())
                        .menuitem_with_label(gettext("Turkish (ISO-8859-9)"));
                    ViewerActions::Output::Encoding("CP1254".to_owned())
                        .menuitem_with_label(gettext("Turkish (CP1254)"));
                    ViewerActions::Output::Encoding("CP1252".to_owned())
                        .menuitem_with_label(gettext("Western (CP1252)"));
                    ViewerActions::Output::Encoding("ISO-8859-1".to_owned())
                        .menuitem_with_label(gettext("Western (ISO-8859-1)"));
                }
            }
        } else {
            Submenu::new(gettext("_Image")) {
                ViewerActions::Output::ImageOp(ImageOperation::RotateClockwise)
                    .menuitem_with_label(gettext("_Rotate Clockwise"));
                ViewerActions::Output::ImageOp(ImageOperation::RotateCounterclockwise)
                    .menuitem_with_label(gettext("Rotate Counter Clockwis_e"));
                ViewerActions::Output::ImageOp(ImageOperation::RotateUpsideDown)
                    .menuitem_with_label(gettext("Rotate 180°"));
                ViewerActions::Output::ImageOp(ImageOperation::FlipVertical)
                    .menuitem_with_label(gettext("Flip _Vertical"));
                ViewerActions::Output::ImageOp(ImageOperation::FlipHorizontal)
                    .menuitem_with_label(gettext("Flip _Horizontal"));
            }
        }

        Submenu::new(gettext("_Settings")) {
            ViewerWindowActions::Output::ChooseFont.menuitem();
            Submenu::new(gettext("Fixed _Width Mode")) {
                ViewerWindowActions::Output::CharsPerLine(20)
                    .menuitem_with_label(gettext("_20 chars/line"));
                ViewerWindowActions::Output::CharsPerLine(40)
                    .menuitem_with_label(gettext("_40 chars/line"));
                ViewerWindowActions::Output::CharsPerLine(80)
                    .menuitem_with_label(gettext("_80 chars/line"));
                ViewerWindowActions::Output::CharsPerLine(120)
                    .menuitem_with_label(gettext("_120 chars/line"));
                ViewerWindowActions::Output::CharsPerLine(160)
                    .menuitem_with_label(gettext("1_60 chars/line"));
            }
            MenuSection {
                ViewerWindowActions::Output::ToggleHexOffset
                    .menuitem();
            }
        }

        Submenu::new(gettext("_Help")) {
            ViewerWindowActions::Output::QuickHelp.menuitem();
            ViewerWindowActions::Output::KeyboardShortcuts.menuitem();
        }
    })
}

fn handle_write_result(result: WriteResult) {
    if let Err(error) = result {
        eprintln!("Failed saving option: {error}");
    }
}

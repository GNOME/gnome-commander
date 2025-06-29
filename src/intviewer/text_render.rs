/*
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

use super::{data_presentation::DataPresentation, file_ops::FileOps};
use crate::{
    intviewer::{
        data_presentation::{ffi::GVDataPresentation, DataPresentationMode},
        file_ops::ffi::ViewerFileOps,
        input_modes::{ffi::GVInputModesData, InputMode},
    },
    utils::Max,
};
use gtk::{
    gdk,
    glib::{
        self,
        ffi::GType,
        translate::{from_glib_borrow, Borrowed, IntoGlib, ToGlibPtr},
    },
    graphene,
    pango::{self, ffi::PangoFontDescription},
    prelude::*,
    subclass::prelude::*,
};
use std::{num::NonZeroU32, path::Path, rc::Rc};

pub mod ffi {
    use super::*;
    use gtk::{ffi::GtkSnapshot, glib::ffi::gboolean};

    pub type TextRender = <super::TextRender as glib::object::ObjectType>::GlibType;

    extern "C" {
        pub fn text_render_init(w: *mut TextRender);
        pub fn text_render_finalize(w: *mut TextRender);

        pub fn text_render_get_current_offset(w: *mut TextRender) -> u64;
        pub fn text_render_get_column(w: *mut TextRender) -> i32;

        pub fn text_render_filter_undisplayable_chars(w: *mut TextRender);
        pub fn text_render_update_adjustments_limits(w: *mut TextRender);

        pub fn text_mode_display_line(
            w: *mut TextRender,
            snapshot: *mut GtkSnapshot,
            column: i32,
            start_of_line: u64,
            end_of_line: u64,
            marker_start: u64,
            marker_end: u64,
        );
        pub fn binary_mode_display_line(
            w: *mut TextRender,
            snapshot: *mut GtkSnapshot,
            column: i32,
            start_of_line: u64,
            end_of_line: u64,
            marker_start: u64,
            marker_end: u64,
        );
        pub fn hex_mode_display_line(
            w: *mut TextRender,
            snapshot: *mut GtkSnapshot,
            column: i32,
            start_of_line: u64,
            end_of_line: u64,
            marker_start: u64,
            marker_end: u64,
        );

        pub fn text_mode_pixel_to_offset(
            w: *mut TextRender,
            x: i32,
            y: i32,
            start_marker: gboolean,
        ) -> u64;
        pub fn hex_mode_pixel_to_offset(
            w: *mut TextRender,
            x: i32,
            y: i32,
            start_marker: gboolean,
        ) -> u64;

        pub fn text_mode_copy_to_clipboard(w: *mut TextRender, start_offset: u64, end_offset: u64);
        pub fn hex_mode_copy_to_clipboard(w: *mut TextRender, start_offset: u64, end_offset: u64);
    }
}

const HEXDUMP_FIXED_LIMIT: u32 = 16;

mod imp {
    use super::*;
    use crate::intviewer::data_presentation::DataPresentationMode;
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::TextRender)]
    pub struct TextRender {
        #[property(get, set = Self::set_hadjustment, override_interface = gtk::Scrollable)]
        hadjustment: RefCell<Option<gtk::Adjustment>>,
        hadjustment_handler_ids: RefCell<Vec<glib::SignalHandlerId>>,

        #[property(get, set = Self::set_vadjustment, override_interface = gtk::Scrollable)]
        vadjustment: RefCell<Option<gtk::Adjustment>>,
        vadjustment_handler_ids: RefCell<Vec<glib::SignalHandlerId>>,

        #[property(get, set = Self::set_hscroll_policy, override_interface = gtk::Scrollable)]
        hscroll_policy: Cell<gtk::ScrollablePolicy>,
        #[property(get, set = Self::set_vscroll_policy, override_interface = gtk::Scrollable)]
        vscroll_policy: Cell<gtk::ScrollablePolicy>,

        #[property(get, set = Self::set_display_mode, builder(TextRenderDisplayMode::default()))]
        display_mode: Cell<TextRenderDisplayMode>,

        pub fixed_font_name: String,
        #[property(get, set = Self::set_font_size)]
        font_size: Cell<u32>,
        #[property(get, set = Self::set_tab_size)]
        tab_size: Cell<u32>,
        #[property(get, set = Self::set_wrap_mode)]
        wrap_mode: Cell<bool>,
        #[property(get, set = Self::set_encoding)]
        encoding: RefCell<String>,
        #[property(get, set = Self::set_fixed_limit)]
        fixed_limit: Cell<u32>,
        #[property(get, set = Self::set_hexadecimal_offset)]
        hexadecimal_offset: Cell<bool>,

        pub file_ops: RefCell<Option<Rc<FileOps>>>,
        pub input_mode: RefCell<Option<Rc<InputMode>>>,
        pub data_presentation: RefCell<Option<Rc<DataPresentation>>>,

        #[property(get, set)]
        chars_per_line: Cell<u32>,
        #[property(get, set)]
        max_column: Cell<u32>,
        pub last_displayed_offset: Cell<u64>,
        #[property(get, set)]
        pub lines_displayed: Cell<i32>,
        pub marker_start: Cell<u64>,
        pub marker_end: Cell<u64>,
        /// The button pressed to start a selection
        button: Cell<Option<u32>>,

        pub char_width: Cell<i32>,
        pub char_height: Cell<i32>,
        pub font_desc: RefCell<Option<pango::FontDescription>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for TextRender {
        const NAME: &'static str = "GnomeCmdTextRender";
        type Type = super::TextRender;
        type ParentType = gtk::Widget;
        type Interfaces = (gtk::Scrollable,);

        fn new() -> Self {
            Self {
                hadjustment: Default::default(),
                hadjustment_handler_ids: Default::default(),

                vadjustment: Default::default(),
                vadjustment_handler_ids: Default::default(),

                hscroll_policy: Cell::new(gtk::ScrollablePolicy::Minimum),
                vscroll_policy: Cell::new(gtk::ScrollablePolicy::Minimum),

                display_mode: Cell::default(),
                fixed_font_name: String::from("Monospace"),
                font_size: Cell::new(12),
                tab_size: Cell::new(8),
                wrap_mode: Default::default(),
                encoding: RefCell::new(String::from("ASCII")),
                fixed_limit: Cell::new(80),
                hexadecimal_offset: Default::default(),

                file_ops: Default::default(),
                input_mode: Default::default(),
                data_presentation: Default::default(),

                chars_per_line: Default::default(),
                max_column: Default::default(),
                last_displayed_offset: Default::default(),
                lines_displayed: Default::default(),
                marker_start: Default::default(),
                marker_end: Default::default(),
                button: Default::default(),

                char_width: Default::default(),
                char_height: Default::default(),
                font_desc: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for TextRender {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();

            this.set_focusable(true);

            unsafe { ffi::text_render_init(this.to_glib_none().0) }

            this.setup_current_font();

            let scroll_controller = gtk::EventControllerScroll::builder()
                .flags(gtk::EventControllerScrollFlags::VERTICAL)
                .build();
            scroll_controller.connect_scroll(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, _dx, dy| {
                    imp.scroll(dy);
                    glib::Propagation::Stop
                }
            ));
            this.add_controller(scroll_controller);

            let button_gesture = gtk::GestureClick::new();
            button_gesture.connect_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |gesture, n_press, x, y| imp.button_press(
                    gesture.current_button(),
                    n_press,
                    x,
                    y
                )
            ));
            button_gesture.connect_released(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |gesture, _n_press, x, y| imp.button_release(gesture.current_button(), x, y)
            ));
            this.add_controller(button_gesture);

            let motion_controller = gtk::EventControllerMotion::new();
            motion_controller.connect_motion(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, x, y| imp.motion_notify(x, y)
            ));
            this.add_controller(motion_controller);

            let key_controller = gtk::EventControllerKey::new();
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, _| imp.key_pressed(key)
            ));
            this.add_controller(key_controller);
        }

        fn dispose(&self) {
            unsafe { ffi::text_render_finalize(self.obj().to_glib_none().0) }
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![glib::subclass::Signal::builder("text-status-changed").build()]
            })
        }
    }

    impl WidgetImpl for TextRender {
        fn size_allocate(&self, width: i32, height: i32, baseline: i32) {
            self.parent_size_allocate(width, height, baseline);

            let char_width = self.char_width.get();
            let char_height = self.char_height.get();

            if let Some(dp) = self.obj().data_presentation() {
                if let Some(chars) =
                    (width > 0 && char_width > 0).then(|| (width / char_width) as u32)
                {
                    self.chars_per_line.set(chars);
                    dp.set_wrap_limit(chars);
                    self.obj().queue_draw();
                }
            }

            self.lines_displayed.set(if char_height > 0 {
                height / char_height
            } else {
                10
            });
        }

        fn measure(&self, orientation: gtk::Orientation, _for_size: i32) -> (i32, i32, i32, i32) {
            const DEFAULT_WIDTH: i32 = 100;
            const DEFAULT_HEIGHT: i32 = 200;
            if orientation == gtk::Orientation::Horizontal {
                (DEFAULT_WIDTH, DEFAULT_WIDTH, -1, -1)
            } else {
                (DEFAULT_HEIGHT, DEFAULT_HEIGHT, -1, -1)
            }
        }

        fn snapshot(&self, snapshot: &gtk::Snapshot) {
            let Some(dp) = self.obj().data_presentation() else {
                return;
            };

            let display_mode = self.obj().display_mode();

            let char_height = self.char_height.get();
            let width = self.obj().width();
            let height = self.obj().height();

            snapshot.push_clip(&graphene::Rect::new(0.0, 0.0, width as f32, height as f32));

            let column = self.obj().column();

            let mut offset = self.obj().current_offset();
            let mut y = 0;

            let mut marker_start = self.marker_start.get();
            let mut marker_end = self.marker_end.get();
            if marker_start > marker_end {
                std::mem::swap(&mut marker_start, &mut marker_end);
            }

            loop {
                let eol_offset = dp.end_of_line_offset(offset);
                if eol_offset == offset {
                    break;
                }

                snapshot.save();
                snapshot.translate(&graphene::Point::new(0.0, y as f32));
                match display_mode {
                    TextRenderDisplayMode::Text => unsafe {
                        ffi::text_mode_display_line(
                            self.obj().to_glib_none().0,
                            snapshot.to_glib_none().0,
                            column,
                            offset,
                            eol_offset,
                            marker_start,
                            marker_end,
                        )
                    },
                    TextRenderDisplayMode::Binary => unsafe {
                        ffi::binary_mode_display_line(
                            self.obj().to_glib_none().0,
                            snapshot.to_glib_none().0,
                            column,
                            offset,
                            eol_offset,
                            marker_start,
                            marker_end,
                        )
                    },
                    TextRenderDisplayMode::Hexdump => unsafe {
                        ffi::hex_mode_display_line(
                            self.obj().to_glib_none().0,
                            snapshot.to_glib_none().0,
                            column,
                            offset,
                            eol_offset,
                            marker_start,
                            marker_end,
                        )
                    },
                }
                snapshot.restore();

                offset = eol_offset;

                y += char_height;
                if y >= height {
                    break;
                }
            }

            snapshot.pop();

            self.last_displayed_offset.set(offset);
        }
    }

    impl ScrollableImpl for TextRender {}

    impl TextRender {
        fn set_hadjustment(&self, adjustment: Option<gtk::Adjustment>) {
            if let Some(adjustment) = self.obj().hadjustment() {
                for handler_id in self.hadjustment_handler_ids.take() {
                    adjustment.disconnect(handler_id);
                }
            }

            let adjustment = adjustment.unwrap_or_default();
            self.hadjustment.replace(Some(adjustment.clone()));

            adjustment.set_lower(0.0);
            adjustment.set_upper(
                if self
                    .obj()
                    .data_presentation()
                    .map_or(false, |dp| dp.mode() == DataPresentationMode::NoWrap)
                {
                    // TODO: find our the real horz limit
                    self.max_column.get() as f64
                } else {
                    0.0
                },
            );
            adjustment.set_step_increment(1.0);
            adjustment.set_page_increment(5.0);
            adjustment.set_page_size(self.chars_per_line.get() as f64);

            self.hadjustment_handler_ids
                .borrow_mut()
                .push(adjustment.connect_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.hadjustment_update()
                )));
            self.hadjustment_handler_ids
                .borrow_mut()
                .push(adjustment.connect_value_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.hadjustment_update()
                )));

            self.hadjustment_update();
        }

        fn hadjustment_update(&self) {
            self.obj().emit_text_status_changed();
            self.obj().queue_draw();
        }

        fn set_vadjustment(&self, adjustment: Option<gtk::Adjustment>) {
            if let Some(adjustment) = self.obj().vadjustment() {
                for handler_id in self.vadjustment_handler_ids.take() {
                    adjustment.disconnect(handler_id);
                }
            }

            let adjustment = adjustment.unwrap_or_default();
            self.vadjustment.replace(Some(adjustment.clone()));

            adjustment.set_lower(0.0);
            adjustment.set_upper(
                self.obj()
                    .file_ops()
                    .and_then(|f| f.max_offset().checked_sub(1))
                    .unwrap_or_default() as f64,
            );
            adjustment.set_step_increment(1.0);
            adjustment.set_page_increment(10.0);
            adjustment.set_page_size(10.0);

            self.vadjustment_handler_ids
                .borrow_mut()
                .push(adjustment.connect_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.vadjustment_update()
                )));
            self.vadjustment_handler_ids
                .borrow_mut()
                .push(adjustment.connect_value_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.vadjustment_update()
                )));

            self.vadjustment_update();
        }

        fn vadjustment_update(&self) {
            if let Some(vadjustment) = self.obj().vadjustment() {
                if let Some(dp) = self.obj().data_presentation() {
                    let value = vadjustment.value() as u64;
                    let new_value = dp.align_offset_to_line_start(value);
                    if new_value != value {
                        vadjustment.set_value(new_value as f64);
                    }
                }
            }

            self.obj().emit_text_status_changed();
            self.obj().queue_draw();
        }

        fn set_hscroll_policy(&self, policy: gtk::ScrollablePolicy) {
            self.hscroll_policy.set(policy);
            self.obj().queue_resize();
        }

        fn set_vscroll_policy(&self, policy: gtk::ScrollablePolicy) {
            self.vscroll_policy.set(policy);
            self.obj().queue_resize();
        }

        fn set_display_mode(&self, mode: TextRenderDisplayMode) {
            self.display_mode.set(mode);

            let Some(dp) = self.obj().data_presentation() else {
                return;
            };
            match mode {
                TextRenderDisplayMode::Text => {
                    dp.set_mode(if self.wrap_mode.get() {
                        DataPresentationMode::Wrap
                    } else {
                        DataPresentationMode::NoWrap
                    });
                }
                TextRenderDisplayMode::Binary => {
                    // Binary display mode doesn't support UTF8
                    // TODO: switch back to the previous encoding, not just ASCII
                    //        input_mode.set_mode("ASCII");
                    dp.set_fixed_count(self.fixed_limit.get());
                    dp.set_mode(DataPresentationMode::BinaryFixed);
                }
                TextRenderDisplayMode::Hexdump => {
                    // HEX display mode doesn't support UTF8
                    // TODO: switch back to the previous encoding, not just ASCII
                    //        input_mode.set_mode("ASCII");
                    dp.set_fixed_count(HEXDUMP_FIXED_LIMIT);
                    dp.set_mode(DataPresentationMode::BinaryFixed);
                }
            }
            // self.obj().setup_current_font();
            if let Some(hadjustment) = self.obj().hadjustment() {
                hadjustment.set_value(0.0);
            }
            if let Some(vadjustment) = self.obj().vadjustment() {
                vadjustment
                    .set_value(dp.align_offset_to_line_start(self.obj().current_offset()) as f64);
            }

            self.obj().queue_draw();
        }

        fn set_font_size(&self, font_size: u32) {
            self.font_size.set(font_size.max(4));
            self.obj().setup_current_font();
            self.obj().queue_draw();
        }

        fn set_tab_size(&self, tab_size: u32) {
            if tab_size > 0 {
                self.tab_size.set(tab_size);
                if let Some(dp) = self.obj().data_presentation() {
                    dp.set_tab_size(tab_size);
                }
                self.obj().queue_draw();
            }
        }

        fn set_wrap_mode(&self, wrap_mode: bool) {
            self.wrap_mode.set(wrap_mode);
            if self.obj().display_mode() == TextRenderDisplayMode::Text {
                if let Some(hadjustment) = self.obj().hadjustment() {
                    hadjustment.set_value(0.0);
                }
                if let Some(dp) = self.obj().data_presentation() {
                    dp.set_mode(if wrap_mode {
                        DataPresentationMode::Wrap
                    } else {
                        DataPresentationMode::NoWrap
                    });
                }
                unsafe {
                    ffi::text_render_update_adjustments_limits(self.obj().to_glib_none().0);
                }
            }
            self.obj().queue_draw();
        }

        fn set_encoding(&self, encoding: String) {
            match self.obj().display_mode() {
                TextRenderDisplayMode::Binary | TextRenderDisplayMode::Hexdump
                    if encoding.eq_ignore_ascii_case("UTF8") =>
                {
                    // Ugly hack: UTF-8 is not acceptable encoding in Binary/Hexdump modes
                    eprintln!("Can't set UTF8 encoding when in Binary or HexDump display mode");
                }
                _ => {
                    self.encoding.replace(encoding.clone());
                    if let Some(input_mode) = self.obj().input_mode() {
                        input_mode.set_mode(&encoding);
                        unsafe {
                            ffi::text_render_filter_undisplayable_chars(
                                self.obj().to_glib_none().0,
                            );
                        }
                    }
                    self.obj().queue_draw();
                }
            }
        }

        fn set_fixed_limit(&self, fixed_limit: u32) {
            self.fixed_limit.set(fixed_limit);
            if let Some(dp) = self.obj().data_presentation() {
                // always 16 bytes in hex dump
                let fixed_limit = match self.obj().display_mode() {
                    TextRenderDisplayMode::Text | TextRenderDisplayMode::Binary => fixed_limit,
                    TextRenderDisplayMode::Hexdump => HEXDUMP_FIXED_LIMIT,
                };
                dp.set_fixed_count(fixed_limit);
            }

            self.obj().queue_draw();
        }

        fn set_hexadecimal_offset(&self, hexadecimal_offset: bool) {
            self.hexadecimal_offset.set(hexadecimal_offset);
            self.obj().queue_draw();
        }

        fn scroll(&self, dy: f64) {
            let Some(vadjustment) = self.obj().vadjustment() else {
                return;
            };
            let Some(dp) = self.obj().data_presentation() else {
                return;
            };

            let mut current_offset = self.obj().current_offset();
            current_offset = dp.scroll_lines(current_offset, (4.0 * dy) as i32);
            vadjustment.set_value(current_offset as f64);

            self.obj().emit_text_status_changed();
            self.obj().queue_draw();
        }

        fn button_press(&self, button: u32, n_press: i32, x: f64, y: f64) {
            if n_press == 1 && self.button.get().is_none() {
                self.button.set(Some(button));
                self.marker_start.set(match self.obj().display_mode() {
                    TextRenderDisplayMode::Text | TextRenderDisplayMode::Binary => unsafe {
                        ffi::text_mode_pixel_to_offset(
                            self.obj().to_glib_none().0,
                            x as i32,
                            y as i32,
                            1,
                        )
                    },
                    TextRenderDisplayMode::Hexdump => unsafe {
                        ffi::hex_mode_pixel_to_offset(
                            self.obj().to_glib_none().0,
                            x as i32,
                            y as i32,
                            1,
                        )
                    },
                });
            }
        }

        fn button_release(&self, button: u32, x: f64, y: f64) {
            if self.button.get() == Some(button) {
                self.button.set(None);
                self.marker_end.set(match self.obj().display_mode() {
                    TextRenderDisplayMode::Text | TextRenderDisplayMode::Binary => unsafe {
                        ffi::text_mode_pixel_to_offset(
                            self.obj().to_glib_none().0,
                            x as i32,
                            y as i32,
                            0,
                        )
                    },
                    TextRenderDisplayMode::Hexdump => unsafe {
                        ffi::hex_mode_pixel_to_offset(
                            self.obj().to_glib_none().0,
                            x as i32,
                            y as i32,
                            0,
                        )
                    },
                });
                self.obj().queue_draw();
            }
        }

        fn motion_notify(&self, x: f64, y: f64) {
            if self.button.get().is_some() {
                let new_marker = match self.obj().display_mode() {
                    TextRenderDisplayMode::Text | TextRenderDisplayMode::Binary => unsafe {
                        ffi::text_mode_pixel_to_offset(
                            self.obj().to_glib_none().0,
                            x as i32,
                            y as i32,
                            0,
                        )
                    },
                    TextRenderDisplayMode::Hexdump => unsafe {
                        ffi::hex_mode_pixel_to_offset(
                            self.obj().to_glib_none().0,
                            x as i32,
                            y as i32,
                            0,
                        )
                    },
                };
                if new_marker != self.marker_end.get() {
                    self.marker_end.set(new_marker);
                    self.obj().queue_draw();
                }
            }
        }

        fn key_pressed(&self, key: gdk::Key) -> glib::Propagation {
            let Some(fops) = self.obj().file_ops() else {
                return glib::Propagation::Proceed;
            };
            let Some(dp) = self.obj().data_presentation() else {
                return glib::Propagation::Proceed;
            };
            let Some(hadjustment) = self.obj().hadjustment() else {
                return glib::Propagation::Proceed;
            };
            let Some(vadjustment) = self.obj().vadjustment() else {
                return glib::Propagation::Proceed;
            };

            let column = self.obj().column();
            let current_offset = self.obj().current_offset();

            match key {
                gdk::Key::Up => vadjustment.set_value(dp.scroll_lines(current_offset, -1) as f64),
                gdk::Key::Down => vadjustment.set_value(dp.scroll_lines(current_offset, 1) as f64),
                gdk::Key::Page_Up => vadjustment.set_value(
                    dp.scroll_lines(current_offset, -1 * (self.lines_displayed.get() - 1)) as f64,
                ),
                gdk::Key::Page_Down => vadjustment.set_value(
                    dp.scroll_lines(current_offset, self.lines_displayed.get() - 1) as f64,
                ),
                gdk::Key::Left => {
                    if !self.wrap_mode.get() && column > 0 {
                        hadjustment.set_value((column - 1) as f64);
                    }
                }
                gdk::Key::Right => {
                    if !self.wrap_mode.get() {
                        hadjustment.set_value((column + 1) as f64);
                    }
                }
                gdk::Key::Home => vadjustment.set_value(0.0),
                gdk::Key::End => vadjustment
                    .set_value(dp.align_offset_to_line_start(fops.max_offset() - 1) as f64),
                _ => return glib::Propagation::Proceed,
            }

            self.obj().emit_text_status_changed();
            self.obj().queue_draw();

            glib::Propagation::Stop
        }
    }
}

glib::wrapper! {
    pub struct TextRender(ObjectSubclass<imp::TextRender>)
        @extends gtk::Widget,
        @implements gtk::Scrollable;
}

impl TextRender {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn ensure_offset_visible(&self, mut offset: u64) {
        if offset >= self.current_offset() && offset < self.imp().last_displayed_offset.get() {
            return;
        }
        let Some(vadjustment) = self.vadjustment() else {
            return;
        };
        let Some(dp) = self.data_presentation() else {
            return;
        };

        offset = dp.align_offset_to_line_start(offset);
        offset = dp.scroll_lines(offset, -self.imp().lines_displayed.get() / 2);

        vadjustment.set_value(offset as f64);
        self.queue_draw();
        self.emit_text_status_changed();
    }

    pub fn set_marker(&self, start: u64, end: u64) {
        self.imp().marker_start.set(start);
        self.imp().marker_end.set(end);
        self.queue_draw();
    }

    pub fn current_offset(&self) -> u64 {
        unsafe { ffi::text_render_get_current_offset(self.to_glib_none().0) }
    }

    pub fn size(&self) -> u64 {
        self.file_ops().map(|f| f.max_offset()).unwrap_or_default()
    }

    pub fn column(&self) -> i32 {
        unsafe { ffi::text_render_get_column(self.to_glib_none().0) }
    }

    pub fn input_mode(&self) -> Option<Rc<InputMode>> {
        self.imp().input_mode.borrow().clone()
    }

    pub fn file_ops(&self) -> Option<Rc<FileOps>> {
        self.imp().file_ops.borrow().clone()
    }

    pub fn data_presentation(&self) -> Option<Rc<DataPresentation>> {
        self.imp().data_presentation.borrow().clone()
    }

    pub fn load_file(&self, filename: &Path) {
        self.imp().input_mode.replace(None);
        self.imp().file_ops.replace(None);
        self.imp().data_presentation.replace(None);

        let file_ops = Rc::new(FileOps::new());
        if !file_ops.open(filename) {
            eprintln!("Failed to load file {}", filename.display());
            return;
        }

        if let Some(hadjustment) = self.hadjustment() {
            hadjustment.set_value(0.0);
        }
        if let Some(vadjustment) = self.vadjustment() {
            vadjustment.set_value(0.0);
        }
        self.set_max_column(0);

        // Setup the input mode translations
        let input_mode = Rc::new(InputMode::new());
        input_mode.init(file_ops.clone());
        input_mode.set_mode(&self.encoding());

        // Setup the data presentation mode
        let data_presentation = Rc::new(DataPresentation::new());
        data_presentation.init(&input_mode, file_ops.max_offset());

        data_presentation.set_wrap_limit(50);
        data_presentation.set_fixed_count(self.fixed_limit());
        data_presentation.set_tab_size(self.tab_size());
        data_presentation.set_mode(if self.wrap_mode() {
            DataPresentationMode::Wrap
        } else {
            DataPresentationMode::NoWrap
        });

        self.set_display_mode(TextRenderDisplayMode::Text);

        self.imp().input_mode.replace(Some(input_mode));
        self.imp().file_ops.replace(Some(file_ops));
        self.imp()
            .data_presentation
            .replace(Some(data_presentation));

        unsafe {
            ffi::text_render_update_adjustments_limits(self.to_glib_none().0);
        }
    }

    pub fn notify_status_changed(&self) {
        self.emit_text_status_changed();
    }

    pub fn copy_selection(&self) {
        let mut marker_start = self.imp().marker_start.get();
        let mut marker_end = self.imp().marker_end.get();
        if marker_start == marker_end {
            return;
        }
        if marker_start > marker_end {
            std::mem::swap(&mut marker_start, &mut marker_end);
        }
        match self.display_mode() {
            TextRenderDisplayMode::Text | TextRenderDisplayMode::Binary => unsafe {
                ffi::text_mode_copy_to_clipboard(self.to_glib_none().0, marker_start, marker_end)
            },
            TextRenderDisplayMode::Hexdump => unsafe {
                ffi::hex_mode_copy_to_clipboard(self.to_glib_none().0, marker_start, marker_end)
            },
        }
    }

    fn setup_current_font(&self) {
        self.setup_font(
            &self.imp().fixed_font_name,
            NonZeroU32::new(self.font_size()).unwrap(),
        );
    }

    fn setup_font(&self, fontname: &str, fontsize: NonZeroU32) {
        let new_desc = pango::FontDescription::from_string(&format!("{fontname} {fontsize}"));
        let (char_width, char_height) = get_max_char_width_and_height(self.upcast_ref(), &new_desc);
        self.imp().char_width.set(char_width);
        self.imp().char_height.set(char_height);
        self.imp().font_desc.replace(Some(new_desc));
    }

    fn emit_text_status_changed(&self) {
        self.emit_by_name::<()>("text-status-changed", &[]);
    }

    pub fn connect_text_status_changed<F: Fn(&Self) + 'static>(
        &self,
        f: F,
    ) -> glib::SignalHandlerId {
        unsafe extern "C" fn pressed_trampoline<F: Fn(&TextRender) + 'static>(
            this: *mut ffi::TextRender,
            f: glib::ffi::gpointer,
        ) {
            let f: &F = &*(f as *const F);
            f(&from_glib_borrow(this))
        }
        unsafe {
            let f: Box<F> = Box::new(f);
            glib::signal::connect_raw(
                self.as_ptr() as *mut _,
                c"text-status-changed".as_ptr() as *const _,
                Some(std::mem::transmute::<*const (), unsafe extern "C" fn()>(
                    pressed_trampoline::<F> as *const (),
                )),
                Box::into_raw(f),
            )
        }
    }
}

#[no_mangle]
pub extern "C" fn text_render_get_type() -> GType {
    TextRender::static_type().into_glib()
}

#[no_mangle]
pub extern "C" fn text_render_get_font_description(
    w: *mut ffi::TextRender,
) -> *mut PangoFontDescription {
    let w: Borrowed<TextRender> = unsafe { from_glib_borrow(w) };
    let font_desc = w.imp().font_desc.borrow();
    font_desc.as_ref().map_or_else(
        || std::ptr::null_mut(),
        |font_desc| font_desc.to_glib_none().0,
    )
}

#[no_mangle]
pub extern "C" fn text_render_get_char_width(w: *mut ffi::TextRender) -> i32 {
    let w: Borrowed<TextRender> = unsafe { from_glib_borrow(w) };
    w.imp().char_width.get()
}

#[no_mangle]
pub extern "C" fn text_render_get_char_height(w: *mut ffi::TextRender) -> i32 {
    let w: Borrowed<TextRender> = unsafe { from_glib_borrow(w) };
    w.imp().char_height.get()
}

fn get_max_char_width_and_height(
    widget: &gtk::Widget,
    font_desc: &pango::FontDescription,
) -> (i32, i32) {
    let layout = widget.create_pango_layout(None);
    layout.set_font_description(Some(font_desc));

    let (max_width, max_height): (Max<i32>, Max<i32>) = (0..256)
        .filter_map(char::from_u32)
        .filter(|c| *c == ' ' || c.is_ascii_graphic())
        .map(|ch| {
            layout.set_text(&ch.to_string());
            let (_ink_rect, logical_rect) = layout.pixel_extents();
            (logical_rect.width(), logical_rect.height())
        })
        .unzip();

    (
        max_width.take().unwrap_or_default(),
        max_height.take().unwrap_or_default(),
    )
}

#[no_mangle]
pub extern "C" fn text_render_get_file_ops(w: *mut ffi::TextRender) -> *mut ViewerFileOps {
    let w: Borrowed<TextRender> = unsafe { from_glib_borrow(w) };
    w.file_ops().map(|f| f.0).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn text_render_get_input_mode_data(
    w: *mut ffi::TextRender,
) -> *mut GVInputModesData {
    let w: Borrowed<TextRender> = unsafe { from_glib_borrow(w) };
    w.input_mode().map(|f| f.0).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn text_render_get_data_presentation(
    w: *mut ffi::TextRender,
) -> *mut GVDataPresentation {
    let w: Borrowed<TextRender> = unsafe { from_glib_borrow(w) };
    w.data_presentation()
        .map(|f| f.0)
        .unwrap_or(std::ptr::null_mut())
}

#[derive(Clone, Copy, Default, PartialEq, Eq, Debug, glib::Enum)]
#[enum_type(name = "GnomeCmdTextRenderDisplayMode")]
#[repr(C)]
pub enum TextRenderDisplayMode {
    #[default]
    Text = 0,
    Binary,
    Hexdump,
}

#[cfg(test)]
mod test {
    use super::*;
    use rusty_fork::rusty_fork_test;

    const FILENAME: &str = "./TODO";

    rusty_fork_test! {
        #[test]
        fn display_mode() {
            gtk::init().unwrap();
            let text_render = TextRender::new();
            text_render.load_file(&Path::new(FILENAME));
            for mode in [
                TextRenderDisplayMode::Text,
                TextRenderDisplayMode::Binary,
                TextRenderDisplayMode::Hexdump,
            ] {
                text_render.set_display_mode(mode);
                assert_eq!(text_render.display_mode(), mode);
            }
        }

        #[test]
        fn tab_size() {
            gtk::init().unwrap();
            let text_render = TextRender::new();
            text_render.load_file(&Path::new(FILENAME));
            for tab_size in 1..=10 {
                text_render.set_tab_size(tab_size);
                assert_eq!(text_render.tab_size(), tab_size);
            }
        }

        #[test]
        fn wrap_mode() {
            gtk::init().unwrap();
            let text_render = TextRender::new();
            text_render.load_file(&Path::new(FILENAME));
            for mode in [false, true] {
                text_render.set_wrap_mode(mode);
                assert_eq!(text_render.wrap_mode(), mode);
            }
        }

        #[test]
        fn fixed_limit() {
            gtk::init().unwrap();
            let text_render = TextRender::new();
            text_render.load_file(&Path::new(FILENAME));
            for limit in 1..=10 {
                text_render.set_fixed_limit(limit);
                assert_eq!(text_render.fixed_limit(), limit);
            }
        }

        #[test]
        fn encoding() {
            gtk::init().unwrap();
            let text_render = TextRender::new();
            text_render.load_file(&Path::new(FILENAME));
            for encoding in ["ASCII", "UTF8", "CP437", "CP1251"] {
                text_render.set_encoding(encoding);
                assert_eq!(text_render.encoding(), encoding);
            }
        }
    }
}

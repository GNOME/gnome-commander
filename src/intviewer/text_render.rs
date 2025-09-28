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

use super::data_presentation::DataPresentation;
use crate::{
    intviewer::{
        data_presentation::DataPresentationMode, file_input_source::FileInputSource,
        input_modes::InputMode,
    },
    utils::Max,
};
use gtk::{gdk, glib, graphene, pango, prelude::*, subclass::prelude::*};
use std::{num::NonZeroU32, path::Path, rc::Rc, sync::Arc};

const HEXDUMP_FIXED_LIMIT: u32 = 16;

mod imp {
    use super::*;
    use crate::{
        intviewer::data_presentation::{DataPresentationMode, next_tab_position},
        utils::MinMax,
    };
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

        pub input_mode: RefCell<Option<Arc<InputMode>>>,
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
        hexmode_marker_on_hexdump: Cell<bool>,

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

                input_mode: Default::default(),
                data_presentation: Default::default(),

                chars_per_line: Default::default(),
                max_column: Default::default(),
                last_displayed_offset: Default::default(),
                lines_displayed: Default::default(),
                marker_start: Default::default(),
                marker_end: Default::default(),
                button: Default::default(),
                hexmode_marker_on_hexdump: Default::default(),

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

            let display_options = DisplayOptions::default();
            let display_options_alternate = DisplayOptions::alternate_marker();
            loop {
                let eol_offset = dp.end_of_line_offset(offset);
                if eol_offset == offset {
                    break;
                }

                snapshot.save();
                snapshot.translate(&graphene::Point::new(
                    -(self.char_width.get() * column) as f32,
                    y as f32,
                ));
                match display_mode {
                    TextRenderDisplayMode::Text => self.text_mode_display_line(
                        snapshot,
                        offset,
                        eol_offset,
                        marker_start,
                        marker_end,
                        &display_options,
                    ),
                    TextRenderDisplayMode::Binary => self.binary_mode_display_line(
                        snapshot,
                        offset,
                        eol_offset,
                        marker_start,
                        marker_end,
                        &display_options,
                    ),
                    TextRenderDisplayMode::Hexdump => self.hex_mode_display_line(
                        snapshot,
                        offset,
                        eol_offset,
                        marker_start,
                        marker_end,
                        &display_options,
                        &display_options_alternate,
                    ),
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
            self.update_hadjustment_limits(&adjustment);

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
            self.update_vadjustment_limits(&adjustment);

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

        pub fn update_adjustments_limits(&self) {
            if let Some(vadjustment) = self.obj().vadjustment() {
                self.update_vadjustment_limits(&vadjustment);
            }
            if let Some(hadjustment) = self.obj().hadjustment() {
                self.update_hadjustment_limits(&hadjustment);
            }
        }

        fn update_hadjustment_limits(&self, adjustment: &gtk::Adjustment) {
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
        }

        fn update_vadjustment_limits(&self, adjustment: &gtk::Adjustment) {
            adjustment.set_lower(0.0);
            adjustment.set_upper(
                self.obj()
                    .input_mode()
                    .map(|f| f.max_offset().saturating_sub(1))
                    .unwrap_or_default() as f64,
            );
            adjustment.set_step_increment(1.0);
            adjustment.set_page_increment(10.0);
            adjustment.set_page_size(10.0);
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
                self.update_adjustments_limits();
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
                        self.filter_undisplayable_chars();
                    }
                    self.obj().queue_draw();
                }
            }
        }

        fn filter_undisplayable_chars(&self) {
            let Some(input_mode) = self.obj().input_mode() else {
                return;
            };

            let layout = self.obj().create_pango_layout(None);
            layout.set_font_description(self.font_desc.borrow().as_ref());
            for byte in 0..=255 {
                let displayable = input_mode
                    .byte_to_char(byte)
                    .filter(|ch| *ch != '\0')
                    .map(|ch| {
                        layout.set_text(&ch.to_string());
                        let (_ink_rect, logical_rect) = layout.pixel_extents();
                        // Pango displays something
                        logical_rect.width() > 0
                    })
                    .unwrap_or_default();

                if !displayable {
                    input_mode.update_utf8_translation(byte, '.');
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
                    TextRenderDisplayMode::Text | TextRenderDisplayMode::Binary => {
                        self.text_mode_pixel_to_offset(x, y, true)
                    }
                    TextRenderDisplayMode::Hexdump => self.hex_mode_pixel_to_offset(x, y, true),
                });
            }
        }

        fn button_release(&self, button: u32, x: f64, y: f64) {
            if self.button.get() == Some(button) {
                self.button.set(None);
                self.marker_end.set(match self.obj().display_mode() {
                    TextRenderDisplayMode::Text | TextRenderDisplayMode::Binary => {
                        self.text_mode_pixel_to_offset(x, y, false)
                    }
                    TextRenderDisplayMode::Hexdump => self.hex_mode_pixel_to_offset(x, y, false),
                });
                self.obj().queue_draw();
            }
        }

        fn motion_notify(&self, x: f64, y: f64) {
            if self.button.get().is_some() {
                let new_marker = match self.obj().display_mode() {
                    TextRenderDisplayMode::Text | TextRenderDisplayMode::Binary => {
                        self.text_mode_pixel_to_offset(x, y, false)
                    }
                    TextRenderDisplayMode::Hexdump => self.hex_mode_pixel_to_offset(x, y, false),
                };
                if new_marker != self.marker_end.get() {
                    self.marker_end.set(new_marker);
                    self.obj().queue_draw();
                }
            }
        }

        fn key_pressed(&self, key: gdk::Key) -> glib::Propagation {
            let Some(input_mode) = self.obj().input_mode() else {
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
                    .set_value(dp.align_offset_to_line_start(input_mode.max_offset() - 1) as f64),
                _ => return glib::Propagation::Proceed,
            }

            self.obj().emit_text_status_changed();
            self.obj().queue_draw();

            glib::Propagation::Stop
        }

        pub fn text_mode_pixel_to_offset(&self, x: f64, y: f64, start_marker: bool) -> u64 {
            let Some(dp) = self.obj().data_presentation() else {
                return 0;
            };
            let Some(input_mode) = self.obj().input_mode() else {
                return 0;
            };

            let char_width = self.char_width.get();
            let char_height = self.char_height.get();
            let tab_size = self.tab_size.get();

            let current_offset: u64 = self.obj().current_offset();

            if x < 0.0 || y < 0.0 || char_height <= 0 || char_width <= 0 {
                return current_offset;
            }

            let line = (y / char_height as f64) as i32;
            let column = (x as i32 / char_width + self.obj().column()) as u32;

            let line_offset = dp.scroll_lines(current_offset, line);
            let next_line_offset = dp.scroll_lines(line_offset, 1);

            let offset = text_mode_line_iter(&input_mode, line_offset, next_line_offset, tab_size)
                .find_map(move |(offset, c, _)| {
                    if start_marker {
                        (c >= column).then_some(offset)
                    } else {
                        (c > column).then_some(offset)
                    }
                })
                .unwrap_or(next_line_offset);

            offset
        }

        pub fn hex_mode_pixel_to_offset(&self, x: f64, y: f64, start_marker: bool) -> u64 {
            let Some(dp) = self.obj().data_presentation() else {
                return 0;
            };
            let Some(input_mode) = self.obj().input_mode() else {
                return 0;
            };

            let char_width = self.char_width.get();
            let char_height = self.char_height.get();

            let current_offset: u64 = self.obj().current_offset();

            if x < 0.0 || y < 0.0 || char_height <= 0 || char_width <= 0 {
                return current_offset;
            }

            let line = (y / char_height as f64) as i32;
            let column = (x as i32 / char_width + self.obj().column()) as u32;

            let line_offset = dp.scroll_lines(current_offset, line);
            let next_line_offset = dp.scroll_lines(line_offset, 1);

            let (hex_offset, bin_offset) = hex_mode_column_layout();

            if column < hex_offset {
                return line_offset;
            }

            let mut byte_offset = if start_marker {
                if column < bin_offset {
                    // the user selected the hex dump portion
                    self.hexmode_marker_on_hexdump.set(true);
                    (column - hex_offset + 2) / 3
                } else {
                    // the user selected the ascii portion
                    self.hexmode_marker_on_hexdump.set(false);
                    column - bin_offset
                }
            } else {
                if self.hexmode_marker_on_hexdump.get() {
                    if column < bin_offset {
                        // the user selected the hex dump portion
                        (column - hex_offset + 2) / 3
                    } else {
                        HEXDUMP_FIXED_LIMIT
                    }
                } else {
                    if column < bin_offset {
                        0
                    } else {
                        column - bin_offset
                    }
                }
            };

            let mut offset = line_offset;
            while byte_offset > 0 && offset < next_line_offset {
                offset = input_mode.next_char_offset(offset);
                byte_offset -= 1;
            }

            offset
        }

        fn display_line(
            &self,
            snapshot: &gtk::Snapshot,
            chars: &[(u64, u32, char)],
            marker_start: u64,
            marker_end: u64,
            display_options: &DisplayOptions,
        ) {
            let layout = self.obj().create_pango_layout(None);
            layout.set_font_description(self.font_desc.borrow().as_ref());

            let marked: MinMax<u32> = chars
                .iter()
                .filter(|(offset, _, _)| *offset >= marker_start && *offset < marker_end)
                .map(|(_, column, _)| *column)
                .collect();

            if let Some((start, end)) = marked.take() {
                snapshot.save();
                snapshot.translate(&graphene::Point::new(
                    (self.char_width.get() * (start as i32)) as f32,
                    0_f32,
                ));
                snapshot.append_color(
                    &display_options.marker_background_color,
                    &graphene::Rect::new(
                        0.0,
                        0.0,
                        (self.char_width.get() * (end + 1 - start) as i32) as f32,
                        self.char_height.get() as f32,
                    ),
                );
                snapshot.restore();
            }

            for (current, column, character) in chars {
                layout.set_text(&character.to_string());
                snapshot.save();
                snapshot.translate(&graphene::Point::new(
                    (self.char_width.get() * (*column as i32)) as f32,
                    0_f32,
                ));
                if *current >= marker_start && *current < marker_end {
                    snapshot.append_layout(&layout, &display_options.marker_text_color);
                } else {
                    snapshot.append_layout(&layout, &display_options.text_color);
                }
                snapshot.restore();
            }

            let max_column = chars
                .iter()
                .map(|(_, column, _)| *column)
                .max()
                .unwrap_or_default();
            if max_column > self.max_column.get() {
                self.max_column.set(max_column);
                self.update_adjustments_limits();
            }
        }

        fn text_mode_chars(&self, start_of_line: u64, end_of_line: u64) -> Vec<(u64, u32, char)> {
            let Some(input_mode) = self.obj().input_mode() else {
                return Vec::new();
            };
            text_mode_line_iter(&input_mode, start_of_line, end_of_line, self.tab_size.get())
                .filter_map(|(offset, column, character)| match character {
                    Some('\r') | Some('\n') => None,
                    Some(ch) if ch == '\0' || ch.is_whitespace() => Some((offset, column, ' ')),
                    Some(ch) => Some((offset, column, ch)),
                    None => Some((offset, column, '\u{FFFD}')),
                })
                .collect::<Vec<_>>()
        }

        fn text_mode_display_line(
            &self,
            snapshot: &gtk::Snapshot,
            start_of_line: u64,
            end_of_line: u64,
            marker_start: u64,
            marker_end: u64,
            display_options: &DisplayOptions,
        ) {
            let chars = self.text_mode_chars(start_of_line, end_of_line);
            self.display_line(snapshot, &chars, marker_start, marker_end, display_options);
        }

        pub fn text_mode_copy_to_clipboard(&self, start_offset: u64, end_offset: u64) {
            let Some(input_mode) = self.obj().input_mode() else {
                return;
            };
            let text: String = input_mode
                .offsets(start_offset, end_offset)
                .filter_map(|offset| input_mode.character(offset))
                .collect();
            self.obj().clipboard().set_text(&text);
        }

        fn binary_mode_chars(&self, start_of_line: u64, end_of_line: u64) -> Vec<(u64, u32, char)> {
            let Some(input_mode) = self.obj().input_mode() else {
                return Vec::new();
            };
            binary_mode_line_iter(&input_mode, start_of_line, end_of_line)
                .filter_map(|(offset, column, character)| {
                    Some((
                        offset,
                        column,
                        character.map_or('.', |c| if c.is_control() { '.' } else { c }),
                    ))
                })
                .collect()
        }

        fn binary_mode_display_line(
            &self,
            snapshot: &gtk::Snapshot,
            start_of_line: u64,
            end_of_line: u64,
            marker_start: u64,
            marker_end: u64,
            display_options: &DisplayOptions,
        ) {
            let chars = self.binary_mode_chars(start_of_line, end_of_line);
            self.display_line(snapshot, &chars, marker_start, marker_end, display_options);
        }

        fn hex_mode_display_line(
            &self,
            snapshot: &gtk::Snapshot,
            start_of_line: u64,
            end_of_line: u64,
            marker_start: u64,
            marker_end: u64,
            display_options: &DisplayOptions,
            display_options_alternate: &DisplayOptions,
        ) {
            let Some(input_mode) = self.obj().input_mode() else {
                return;
            };

            let layout = self.obj().create_pango_layout(None);
            layout.set_font_description(self.font_desc.borrow().as_ref());
            layout.set_text(&if self.hexadecimal_offset.get() {
                format!("{:08X}", start_of_line)
            } else {
                format!("{:09}", start_of_line)
            });
            snapshot.append_layout(&layout, &display_options.text_color);

            let (display_options, display_options_alternate) =
                if self.hexmode_marker_on_hexdump.get() {
                    (display_options, display_options_alternate)
                } else {
                    (display_options_alternate, display_options)
                };

            let hex_chars = hex_mode_line_iter(&input_mode, start_of_line, end_of_line)
                .flat_map(|(offset, column, byte)| {
                    match byte {
                        Some(b) => format!("{:02X}", b)
                            .chars()
                            .enumerate()
                            .map(|(i, c)| (offset, column * 3 + i as u32, c))
                            .collect(),
                        None => {
                            vec![]
                        }
                    }
                    .into_iter()
                })
                .collect::<Vec<_>>();

            let bin_chars = hex_mode_line_iter(&input_mode, start_of_line, end_of_line)
                .filter_map(|(offset, column, byte)| {
                    Some((
                        offset,
                        column,
                        byte.filter(|b| *b != 0)
                            .and_then(|b| char::from_u32(b as u32))
                            .unwrap_or('.'),
                    ))
                })
                .collect::<Vec<_>>();

            let (hex_offset, bin_offset) = hex_mode_column_layout();

            snapshot.save();
            snapshot.translate(&graphene::Point::new(
                (self.char_width.get() * hex_offset as i32) as f32,
                0_f32,
            ));
            self.display_line(
                snapshot,
                &hex_chars,
                marker_start,
                marker_end,
                display_options,
            );
            snapshot.restore();

            snapshot.save();
            snapshot.translate(&graphene::Point::new(
                (self.char_width.get() * bin_offset as i32) as f32,
                0_f32,
            ));
            self.display_line(
                snapshot,
                &bin_chars,
                marker_start,
                marker_end,
                display_options_alternate,
            );
            snapshot.restore();
        }

        pub fn hex_mode_copy_to_clipboard(&self, start_offset: u64, end_offset: u64) {
            if self.hexmode_marker_on_hexdump.get() {
                let Some(input_mode) = self.obj().input_mode() else {
                    return;
                };
                let text = input_mode
                    .offsets(start_offset, end_offset)
                    .filter_map(|offset| input_mode.raw_byte(offset))
                    .map(|b| format!("{:02X}", b))
                    .collect::<Vec<_>>();
                self.obj().clipboard().set_text(&text.join(" "));
            } else {
                self.text_mode_copy_to_clipboard(start_offset, end_offset);
            }
        }
    }

    fn text_mode_line_iter(
        input_mode: &InputMode,
        start: u64,
        end: u64,
        tab_size: u32,
    ) -> impl Iterator<Item = (u64, u32, Option<char>)> + use<'_> {
        input_mode
            .offsets(start, end)
            .scan(0, move |column, offset| {
                let current = *column;
                let character = input_mode.character(offset);
                if character == Some('\t') {
                    *column = next_tab_position(*column, tab_size);
                } else {
                    *column += 1;
                }
                Some((offset, current, character))
            })
    }

    fn binary_mode_line_iter(
        input_mode: &InputMode,
        start: u64,
        end: u64,
    ) -> impl Iterator<Item = (u64, u32, Option<char>)> + use<'_> {
        input_mode
            .offsets(start, end)
            .scan(0, move |column, offset| {
                let current = *column;
                let character = input_mode.character(offset);
                *column += 1;
                Some((offset, current, character))
            })
    }

    fn hex_mode_line_iter(
        input_mode: &InputMode,
        start: u64,
        end: u64,
    ) -> impl Iterator<Item = (u64, u32, Option<u8>)> + use<'_> {
        input_mode
            .byte_offsets(start, end)
            .scan(0, move |column, offset| {
                let current = *column;
                let character = input_mode.raw_byte(offset);
                *column += 1;
                Some((offset, current, character))
            })
    }

    const fn hex_mode_column_layout() -> (u32, u32) {
        (10, 10 + ((HEXDUMP_FIXED_LIMIT * 3) - 1) + 2)
    }

    struct DisplayOptions {
        text_color: gdk::RGBA,
        marker_text_color: gdk::RGBA,
        marker_background_color: gdk::RGBA,
    }

    impl Default for DisplayOptions {
        fn default() -> Self {
            Self {
                text_color: gdk::RGBA::BLACK,
                marker_text_color: gdk::RGBA::WHITE,
                marker_background_color: gdk::RGBA::BLUE,
            }
        }
    }

    impl DisplayOptions {
        fn alternate_marker() -> Self {
            Self {
                text_color: gdk::RGBA::BLACK,
                marker_text_color: gdk::RGBA::BLACK,
                marker_background_color: gdk::RGBA::new(0_f32, 1_f32, 1_f32, 1_f32),
            }
        }
    }
}

glib::wrapper! {
    pub struct TextRender(ObjectSubclass<imp::TextRender>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::Scrollable;
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
        self.vadjustment()
            .map(|a| a.value() as u64)
            .unwrap_or_default()
    }

    pub fn size(&self) -> u64 {
        self.input_mode()
            .map(|f| f.max_offset())
            .unwrap_or_default()
    }

    pub fn column(&self) -> i32 {
        self.hadjustment()
            .map(|a| a.value() as i32)
            .unwrap_or_default()
    }

    pub fn input_mode(&self) -> Option<Arc<InputMode>> {
        self.imp().input_mode.borrow().clone()
    }

    pub fn data_presentation(&self) -> Option<Rc<DataPresentation>> {
        self.imp().data_presentation.borrow().clone()
    }

    pub fn load_file(&self, filename: &Path) {
        self.imp().input_mode.replace(None);
        self.imp().data_presentation.replace(None);

        let source = match FileInputSource::open(filename) {
            Ok(source) => source,
            Err(error) => {
                eprintln!("Failed to load file {}: {}", filename.display(), error);
                return;
            }
        };

        if let Some(hadjustment) = self.hadjustment() {
            hadjustment.set_value(0.0);
        }
        if let Some(vadjustment) = self.vadjustment() {
            vadjustment.set_value(0.0);
        }
        self.set_max_column(0);

        // Setup the input mode translations
        let input_mode = Arc::new(InputMode::new(source));
        input_mode.set_mode(&self.encoding());

        // Setup the data presentation mode
        let data_presentation = Rc::new(DataPresentation::new(&input_mode));
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
        self.imp()
            .data_presentation
            .replace(Some(data_presentation));

        self.imp().update_adjustments_limits();
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
            TextRenderDisplayMode::Text | TextRenderDisplayMode::Binary => self
                .imp()
                .text_mode_copy_to_clipboard(marker_start, marker_end),
            TextRenderDisplayMode::Hexdump => self
                .imp()
                .hex_mode_copy_to_clipboard(marker_start, marker_end),
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
        self.connect_closure(
            "text-status-changed",
            false,
            glib::closure_local!(move |this| (f)(this)),
        )
    }
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
    use crate::intviewer::data_presentation::next_tab_position;
    use rusty_fork::rusty_fork_test;

    const FILENAME: &str = "./TODO";

    #[test]
    fn test_scan() {
        let c = "ab\tcd"
            .chars()
            .scan(0, move |column, character| {
                let current = *column;
                if character == '\t' {
                    *column = next_tab_position(*column, 4);
                } else {
                    *column += 1;
                }
                Some((current, character))
            })
            .collect::<Vec<_>>();

        assert_eq!(c, vec![(0, 'a'), (1, 'b'), (2, '\t'), (4, 'c'), (5, 'd')]);
    }

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

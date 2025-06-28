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
    intviewer::input_modes::{ffi::GVInputModesData, InputMode},
    utils::Max,
};
use gtk::{
    glib::{
        self,
        ffi::GType,
        translate::{from_glib_borrow, from_glib_none, Borrowed, IntoGlib, ToGlibPtr},
    },
    graphene,
    pango::{self, ffi::PangoFontDescription},
    prelude::*,
    subclass::prelude::*,
};
use std::{ffi::c_char, num::NonZeroU32, path::Path, sync::LazyLock};

pub mod ffi {
    use super::*;
    use crate::intviewer::{
        data_presentation::ffi::GVDataPresentation, file_ops::ffi::ViewerFileOps,
    };
    use gtk::ffi::GtkSnapshot;
    use std::ffi::c_char;

    pub type TextRender = <super::TextRender as glib::object::ObjectType>::GlibType;

    extern "C" {
        pub fn text_render_init(w: *mut TextRender);
        pub fn text_render_finalize(w: *mut TextRender);

        pub fn text_render_set_marker(w: *mut TextRender, start: u64, end: u64);

        pub fn text_render_get_current_offset(w: *mut TextRender) -> u64;
        pub fn text_render_get_size(w: *mut TextRender) -> u64;
        pub fn text_render_get_column(w: *mut TextRender) -> i32;

        pub fn text_render_get_file_ops(w: *mut TextRender) -> *mut ViewerFileOps;
        pub fn text_render_get_input_mode_data(w: *mut TextRender) -> *mut GVInputModesData;
        pub fn text_render_get_data_presentation(w: *mut TextRender) -> *mut GVDataPresentation;

        pub fn text_render_load_file(w: *mut TextRender, filename: *const c_char);

        pub fn text_render_notify_status_changed(w: *mut TextRender);

        pub fn text_render_copy_selection(w: *mut TextRender);

        pub fn text_render_get_display_mode(w: *mut TextRender) -> super::TextRenderDisplayMode;
        pub fn text_render_set_display_mode(w: *mut TextRender, mode: super::TextRenderDisplayMode);

        pub fn text_render_filter_undisplayable_chars(w: *mut TextRender);
        pub fn text_render_update_adjustments_limits(w: *mut TextRender);

        pub fn text_mode_display_line(
            w: *mut TextRender,
            snapshot: *mut GtkSnapshot,
            column: i32,
            start_of_line: u64,
            end_of_line: u64,
        );
        pub fn binary_mode_display_line(
            w: *mut TextRender,
            snapshot: *mut GtkSnapshot,
            column: i32,
            start_of_line: u64,
            end_of_line: u64,
        );
        pub fn hex_mode_display_line(
            w: *mut TextRender,
            snapshot: *mut GtkSnapshot,
            column: i32,
            start_of_line: u64,
            end_of_line: u64,
        );
    }
}

const HEXDUMP_FIXED_LIMIT: u32 = 16;

mod imp {
    use crate::intviewer::data_presentation::DataPresentationMode;
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    use super::*;

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

        #[property(get, set)]
        chars_per_line: Cell<u32>,
        #[property(get, set)]
        max_column: Cell<u32>,
        pub last_displayed_offset: Cell<u64>,
        #[property(get, set)]
        pub lines_displayed: Cell<i32>,
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

                fixed_font_name: String::from("Monospace"),
                font_size: Cell::new(12),
                tab_size: Cell::new(8),
                wrap_mode: Default::default(),
                encoding: RefCell::new(String::from("ASCII")),
                fixed_limit: Cell::new(80),
                hexadecimal_offset: Default::default(),

                chars_per_line: Default::default(),
                max_column: Default::default(),
                last_displayed_offset: Default::default(),
                lines_displayed: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for TextRender {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();

            this.set_can_focus(true);

            unsafe { ffi::text_render_init(this.to_glib_none().0) }

            this.setup_current_font();
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

            let char_width = self.obj().private().char_width;
            let char_height = self.obj().private().char_height;

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

            let char_height = self.obj().private().char_height;
            let width = self.obj().width();
            let height = self.obj().height();

            snapshot.push_clip(&graphene::Rect::new(0.0, 0.0, width as f32, height as f32));

            let column = self.obj().column();

            let mut offset = self.obj().current_offset();
            let mut y = 0;

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
                        )
                    },
                    TextRenderDisplayMode::Binary => unsafe {
                        ffi::binary_mode_display_line(
                            self.obj().to_glib_none().0,
                            snapshot.to_glib_none().0,
                            column,
                            offset,
                            eol_offset,
                        )
                    },
                    TextRenderDisplayMode::Hexdump => unsafe {
                        ffi::hex_mode_display_line(
                            self.obj().to_glib_none().0,
                            snapshot.to_glib_none().0,
                            column,
                            offset,
                            eol_offset,
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
    }
}

glib::wrapper! {
    pub struct TextRender(ObjectSubclass<imp::TextRender>)
        @extends gtk::Widget,
        @implements gtk::Scrollable;
}

#[derive(Default)]
struct TextRenderPrivate {
    char_width: i32,
    char_height: i32,
    font_desc: Option<pango::FontDescription>,
}

impl TextRender {
    fn private(&self) -> &mut TextRenderPrivate {
        static QUARK: LazyLock<glib::Quark> =
            LazyLock::new(|| glib::Quark::from_str("text-render-private"));
        unsafe {
            if let Some(mut private) = self.qdata::<TextRenderPrivate>(*QUARK) {
                private.as_mut()
            } else {
                self.set_qdata(*QUARK, TextRenderPrivate::default());
                self.qdata::<TextRenderPrivate>(*QUARK).unwrap().as_mut()
            }
        }
    }

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
        unsafe { ffi::text_render_set_marker(self.to_glib_none().0, start, end) }
    }

    pub fn current_offset(&self) -> u64 {
        unsafe { ffi::text_render_get_current_offset(self.to_glib_none().0) }
    }

    pub fn size(&self) -> u64 {
        unsafe { ffi::text_render_get_size(self.to_glib_none().0) }
    }

    pub fn column(&self) -> i32 {
        unsafe { ffi::text_render_get_column(self.to_glib_none().0) }
    }

    pub fn input_mode(&self) -> Option<InputMode> {
        let ptr = unsafe { ffi::text_render_get_input_mode_data(self.to_glib_none().0) };
        if ptr.is_null() {
            None
        } else {
            Some(InputMode::borrow(ptr))
        }
    }

    pub fn file_ops(&self) -> Option<FileOps> {
        let ptr = unsafe { ffi::text_render_get_file_ops(self.to_glib_none().0) };
        if ptr.is_null() {
            None
        } else {
            Some(FileOps::borrow(ptr))
        }
    }

    pub fn data_presentation(&self) -> Option<DataPresentation> {
        let ptr = unsafe { ffi::text_render_get_data_presentation(self.to_glib_none().0) };
        if ptr.is_null() {
            None
        } else {
            Some(DataPresentation::borrow(ptr))
        }
    }

    pub fn load_file(&self, filename: &Path) {
        unsafe { ffi::text_render_load_file(self.to_glib_none().0, filename.to_glib_none().0) }
    }

    pub fn notify_status_changed(&self) {
        unsafe { ffi::text_render_notify_status_changed(self.to_glib_none().0) }
    }

    pub fn copy_selection(&self) {
        unsafe { ffi::text_render_copy_selection(self.to_glib_none().0) }
    }

    pub fn display_mode(&self) -> TextRenderDisplayMode {
        unsafe { ffi::text_render_get_display_mode(self.to_glib_none().0) }
    }

    pub fn set_display_mode(&self, mode: TextRenderDisplayMode) {
        unsafe { ffi::text_render_set_display_mode(self.to_glib_none().0, mode) }
    }

    fn setup_current_font(&self) {
        self.setup_font(
            &self.imp().fixed_font_name,
            NonZeroU32::new(self.font_size()).unwrap(),
        );
    }

    fn setup_font(&self, fontname: &str, fontsize: NonZeroU32) {
        let private = self.private();
        let new_desc = pango::FontDescription::from_string(&format!("{fontname} {fontsize}"));
        (private.char_width, private.char_height) =
            get_max_char_width_and_height(self.upcast_ref(), &new_desc);
        private.font_desc = Some(new_desc);
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
pub extern "C" fn text_render_setup_font(
    w: *mut ffi::TextRender,
    fontname: *const c_char,
    fontsize: u32,
) {
    let w: Borrowed<TextRender> = unsafe { from_glib_borrow(w) };
    let fontname: String = unsafe { from_glib_none(fontname) };
    let Ok(fontsize) = fontsize.try_into() else {
        eprintln!("Invalid font size: {fontsize}");
        return;
    };
    w.setup_font(&fontname, fontsize);
}

#[no_mangle]
pub extern "C" fn text_render_get_font_description(
    w: *mut ffi::TextRender,
) -> *mut PangoFontDescription {
    let w: Borrowed<TextRender> = unsafe { from_glib_borrow(w) };
    let private = w.private();
    if let Some(ref font_desc) = private.font_desc {
        font_desc.to_glib_none().0
    } else {
        std::ptr::null_mut()
    }
}

#[no_mangle]
pub extern "C" fn text_render_get_char_width(w: *mut ffi::TextRender) -> i32 {
    let w: Borrowed<TextRender> = unsafe { from_glib_borrow(w) };
    w.private().char_width
}

#[no_mangle]
pub extern "C" fn text_render_get_char_height(w: *mut ffi::TextRender) -> i32 {
    let w: Borrowed<TextRender> = unsafe { from_glib_borrow(w) };
    w.private().char_height
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

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
#[repr(C)]
pub enum TextRenderDisplayMode {
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

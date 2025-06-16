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
use crate::{intviewer::input_modes::ffi::GVInputModesData, utils::Max};
use gtk::{
    glib::{
        self,
        translate::{from_glib_borrow, from_glib_none, Borrowed, ToGlibPtr},
    },
    pango::{self, ffi::PangoFontDescription},
    prelude::*,
};
use std::{ffi::c_char, num::NonZeroU32, path::Path, sync::LazyLock};

pub mod ffi {
    use super::*;
    use crate::intviewer::{
        data_presentation::ffi::GVDataPresentation, file_ops::ffi::ViewerFileOps,
    };
    use glib::ffi::GType;
    use std::ffi::c_char;

    #[repr(C)]
    pub struct TextRender {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn text_render_get_type() -> GType;

        pub fn text_render_ensure_offset_visible(w: *mut TextRender, offset: u64);
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

        pub fn text_render_get_fixed_limit(w: *mut TextRender) -> i32;
        pub fn text_render_set_fixed_limit(w: *mut TextRender, fixed_limit: i32);
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct TextRenderClass {
        pub parent_class: gtk::ffi::GtkWidgetClass,
    }
}

glib::wrapper! {
    pub struct TextRender(Object<ffi::TextRender, ffi::TextRenderClass>)
        @extends gtk::Widget;

    match fn {
        type_ => || ffi::text_render_get_type(),
    }
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

    pub fn ensure_offset_visible(&self, offset: u64) {
        unsafe { ffi::text_render_ensure_offset_visible(self.to_glib_none().0, offset) }
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

    pub fn wrap_mode(&self) -> bool {
        self.property("wrap-mode")
    }

    pub fn set_wrap_mode(&self, wrap: bool) {
        self.set_property("wrap-mode", wrap)
    }

    pub fn font_size(&self) -> u32 {
        self.property("font-size")
    }

    pub fn set_font_size(&self, font_size: u32) {
        self.set_property("font-size", font_size);
    }

    pub fn tab_size(&self) -> u32 {
        self.property("tab-size")
    }

    pub fn set_tab_size(&self, tab_size: u32) {
        self.set_property("tab-size", tab_size);
    }

    pub fn encoding(&self) -> String {
        self.property("encoding")
    }

    pub fn set_encoding(&self, encoding: &str) {
        self.set_property("encoding", encoding);
    }

    pub fn input_mode_data(&self) -> *mut GVInputModesData {
        unsafe { ffi::text_render_get_input_mode_data(self.to_glib_none().0) }
    }

    pub fn file_ops(&self) -> Option<FileOps> {
        let ptr = unsafe { ffi::text_render_get_file_ops(self.to_glib_none().0) };
        if ptr.is_null() {
            None
        } else {
            Some(FileOps(ptr))
        }
    }

    pub fn data_presentation(&self) -> Option<DataPresentation> {
        let ptr = unsafe { ffi::text_render_get_data_presentation(self.to_glib_none().0) };
        if ptr.is_null() {
            None
        } else {
            Some(DataPresentation(ptr))
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

    pub fn fixed_limit(&self) -> i32 {
        unsafe { ffi::text_render_get_fixed_limit(self.to_glib_none().0) }
    }

    pub fn set_fixed_limit(&self, fixed_limit: i32) {
        unsafe { ffi::text_render_set_fixed_limit(self.to_glib_none().0, fixed_limit) }
    }

    fn setup_font(&self, fontname: &str, fontsize: NonZeroU32) {
        let private = self.private();
        let new_desc = pango::FontDescription::from_string(&format!("{fontname} {fontsize}"));
        (private.char_width, private.char_height) = get_max_char_width_and_height(self.upcast_ref(), &new_desc);
        private.font_desc = Some(new_desc);
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

fn get_max_char_width_and_height(widget: &gtk::Widget, font_desc: &pango::FontDescription) -> (i32, i32) {
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

/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
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

#![allow(dead_code)]

use gtk::glib::{
    self,
    ffi::gpointer,
    translate::{ToGlibPtr, from_glib_full},
};
use std::path::Path;

pub mod ffi {
    use glib::ffi::{GError, GQuark, gboolean, gpointer};
    use std::ffi::{c_char, c_uint};

    pub type GModuleFlags = c_uint;
    pub const G_MODULE_BIND_LAZY: GModuleFlags = 1;
    pub const G_MODULE_BIND_LOCAL: GModuleFlags = 2;
    pub const G_MODULE_BIND_MASK: GModuleFlags = 3;

    #[repr(C)]
    #[derive(Debug, Copy, Clone)]
    pub struct GModule {
        _unused: [u8; 0],
    }

    pub type GModuleError = c_uint;
    pub const G_MODULE_ERROR_FAILED: GModuleError = 0;
    pub const G_MODULE_ERROR_CHECK_FAILED: GModuleError = 1;

    unsafe extern "C" {
        pub fn g_module_supported() -> gboolean;

        pub fn g_module_open_full(
            file_name: *const c_char,
            flags: GModuleFlags,
            error: *mut *mut GError,
        ) -> *mut GModule;

        pub fn g_module_close(module: *mut GModule) -> gboolean;

        pub fn g_module_make_resident(module: *mut GModule);

        pub fn g_module_error_quark() -> GQuark;

        pub fn g_module_symbol(
            module: *mut GModule,
            symbol_name: *const c_char,
            symbol: *mut gpointer,
        ) -> gboolean;

        pub fn g_module_name(module: *mut GModule) -> *const c_char;
    }
}

#[repr(C)]
pub struct GModule(*mut ffi::GModule);

impl GModule {
    pub fn open(file_name: &Path, flags: GModuleFlags) -> Result<Self, glib::Error> {
        let mut f: ffi::GModuleFlags = 0;
        if flags.lazy {
            f |= ffi::G_MODULE_BIND_LAZY;
        }
        if flags.local {
            f |= ffi::G_MODULE_BIND_LOCAL;
        }

        let mut error = std::ptr::null_mut();
        let module = unsafe { ffi::g_module_open_full(file_name.to_glib_none().0, f, &mut error) };

        if module.is_null() {
            return Err(unsafe { from_glib_full(error) });
        }

        Ok(Self(module))
    }

    pub fn symbol(&self, symbol_name: &str) -> Option<gpointer> {
        let mut symbol = std::ptr::null_mut();
        let result =
            unsafe { ffi::g_module_symbol(self.0, symbol_name.to_glib_none().0, &mut symbol) != 0 };
        if result { Some(symbol) } else { None }
    }

    pub fn leak(mut self) -> *mut ffi::GModule {
        std::mem::replace(&mut self.0, std::ptr::null_mut())
    }
}

impl Drop for GModule {
    fn drop(&mut self) {
        if !self.0.is_null() {
            let status = unsafe { ffi::g_module_close(self.0) };
            if status == 0 {
                eprintln!("Failed to close a GModule.");
            }
        }
    }
}

#[derive(Clone, Copy)]
pub struct GModuleFlags {
    pub lazy: bool,
    pub local: bool,
}

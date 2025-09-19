/*
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
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

use super::file_descriptor::FileDescriptor;
use gtk::glib::{self, prelude::*, translate::*};

pub mod ffi {
    use crate::libgcmd::file_descriptor::ffi::GnomeCmdFileDescriptor;
    use gtk::glib::ffi::{GList, GType};

    #[repr(C)]
    pub struct GnomeCmdState {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    unsafe extern "C" {
        pub fn gnome_cmd_state_get_type() -> GType;

        pub fn gnome_cmd_state_get_active_dir(
            state: *mut GnomeCmdState,
        ) -> *mut GnomeCmdFileDescriptor;
        pub fn gnome_cmd_state_set_active_dir(
            state: *mut GnomeCmdState,
            active_dir: *mut GnomeCmdFileDescriptor,
        );
        pub fn gnome_cmd_state_get_inactive_dir(
            state: *mut GnomeCmdState,
        ) -> *mut GnomeCmdFileDescriptor;
        pub fn gnome_cmd_state_set_inactive_dir(
            state: *mut GnomeCmdState,
            inactive_dir: *mut GnomeCmdFileDescriptor,
        );
        pub fn gnome_cmd_state_get_active_dir_files(state: *mut GnomeCmdState) -> *const GList;
        pub fn gnome_cmd_state_set_active_dir_files(
            state: *mut GnomeCmdState,
            active_dir_files: *mut GList,
        );
        pub fn gnome_cmd_state_get_inactive_dir_files(state: *mut GnomeCmdState) -> *const GList;
        pub fn gnome_cmd_state_set_inactive_dir_files(
            state: *mut GnomeCmdState,
            inactive_dir_files: *mut GList,
        );
        pub fn gnome_cmd_state_get_active_dir_selected_files(
            state: *mut GnomeCmdState,
        ) -> *const GList;
        pub fn gnome_cmd_state_set_active_dir_selected_files(
            state: *mut GnomeCmdState,
            active_dir_selected_files: *mut GList,
        );
        pub fn gnome_cmd_state_get_inactive_dir_selected_files(
            state: *mut GnomeCmdState,
        ) -> *const GList;
        pub fn gnome_cmd_state_set_inactive_dir_selected_files(
            state: *mut GnomeCmdState,
            inactive_dir_selected_files: *mut GList,
        );
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdStateClass {
        pub parent_class: glib::gobject_ffi::GObjectClass,
    }
}

glib::wrapper! {
    pub struct State(Object<ffi::GnomeCmdState, ffi::GnomeCmdStateClass>);

    match fn {
        type_ => || ffi::gnome_cmd_state_get_type(),
    }
}

impl State {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }
}

pub trait StateExt: IsA<State> + 'static {
    fn active_dir(&self) -> Option<FileDescriptor> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_state_get_active_dir(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn set_active_dir(&self, active_dir: Option<&FileDescriptor>) {
        unsafe {
            ffi::gnome_cmd_state_set_active_dir(
                self.as_ref().to_glib_none().0,
                active_dir.to_glib_none().0,
            )
        }
    }
    fn inactive_dir(&self) -> Option<FileDescriptor> {
        unsafe {
            from_glib_none(ffi::gnome_cmd_state_get_inactive_dir(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn set_inactive_dir(&self, inactive_dir: Option<&FileDescriptor>) {
        unsafe {
            ffi::gnome_cmd_state_set_inactive_dir(
                self.as_ref().to_glib_none().0,
                inactive_dir.to_glib_none().0,
            )
        }
    }
    fn active_dir_files(&self) -> glib::List<FileDescriptor> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_state_get_active_dir_files(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn set_active_dir_files<F: IsA<FileDescriptor> + glib::translate::TransparentPtrType>(
        &self,
        active_dir_files: &glib::List<F>,
    ) {
        unsafe {
            ffi::gnome_cmd_state_set_active_dir_files(
                self.as_ref().to_glib_none().0,
                active_dir_files.to_glib_none().0,
            )
        }
    }
    fn inactive_dir_files(&self) -> glib::List<FileDescriptor> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_state_get_inactive_dir_files(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn set_inactive_dir_files<F: IsA<FileDescriptor> + glib::translate::TransparentPtrType>(
        &self,
        inactive_dir_files: &glib::List<F>,
    ) {
        unsafe {
            ffi::gnome_cmd_state_set_inactive_dir_files(
                self.as_ref().to_glib_none().0,
                inactive_dir_files.to_glib_none().0,
            )
        }
    }
    fn active_dir_selected_files(&self) -> glib::List<FileDescriptor> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_state_get_active_dir_selected_files(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn set_active_dir_selected_files<
        F: IsA<FileDescriptor> + glib::translate::TransparentPtrType,
    >(
        &self,
        active_dir_selected_files: &glib::List<F>,
    ) {
        unsafe {
            ffi::gnome_cmd_state_set_active_dir_selected_files(
                self.as_ref().to_glib_none().0,
                active_dir_selected_files.to_glib_none().0,
            )
        }
    }
    fn inactive_dir_selected_files(&self) -> glib::List<FileDescriptor> {
        unsafe {
            glib::List::from_glib_none(ffi::gnome_cmd_state_get_inactive_dir_selected_files(
                self.as_ref().to_glib_none().0,
            ))
        }
    }
    fn set_inactive_dir_selected_files<
        F: IsA<FileDescriptor> + glib::translate::TransparentPtrType,
    >(
        &self,
        inactive_dir_selected_files: &glib::List<F>,
    ) {
        unsafe {
            ffi::gnome_cmd_state_set_inactive_dir_selected_files(
                self.as_ref().to_glib_none().0,
                inactive_dir_selected_files.to_glib_none().0,
            )
        }
    }
}

impl<O: IsA<State>> StateExt for O {}

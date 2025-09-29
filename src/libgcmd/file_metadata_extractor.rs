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

use super::file_descriptor::{FileDescriptor, ffi::GnomeCmdFileDescriptor};
use crate::tags::tags::{GnomeCmdTag, GnomeCmdTagClass};
use gtk::glib::{
    self,
    ffi::{GStrv, gpointer},
    prelude::*,
    subclass::{object::ObjectImpl, prelude::*},
    translate::*,
};
use std::ffi::c_char;

pub mod ffi {
    use super::*;
    use crate::libgcmd::file_descriptor::ffi::GnomeCmdFileDescriptor;
    use gtk::glib::{
        ffi::{GStrv, GType, gpointer},
        gobject_ffi,
    };

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GnomeCmdFileMetadataExtractorIface {
        pub g_iface: gobject_ffi::GTypeInterface,

        pub supported_tags:
            Option<unsafe extern "C" fn(*mut GnomeCmdFileMetadataExtractor) -> GStrv>,
        pub summary_tags: Option<unsafe extern "C" fn(*mut GnomeCmdFileMetadataExtractor) -> GStrv>,
        pub class_name: Option<
            unsafe extern "C" fn(
                *mut GnomeCmdFileMetadataExtractor,
                class: *const c_char,
            ) -> *mut c_char,
        >,
        pub tag_name: Option<
            unsafe extern "C" fn(
                *mut GnomeCmdFileMetadataExtractor,
                tag: *const c_char,
            ) -> *mut c_char,
        >,
        pub tag_description: Option<
            unsafe extern "C" fn(
                *mut GnomeCmdFileMetadataExtractor,
                tag: *const c_char,
            ) -> *mut c_char,
        >,
        pub extract_metadata: Option<
            unsafe extern "C" fn(
                *mut GnomeCmdFileMetadataExtractor,
                f: *mut GnomeCmdFileDescriptor,
                add: GnomeCmdFileMetadataExtractorAddTag,
                user_data: gpointer,
            ),
        >,
    }

    #[repr(C)]
    pub struct GnomeCmdFileMetadataExtractor {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    pub type GnomeCmdFileMetadataExtractorAddTag =
        Option<unsafe extern "C" fn(tag: *const c_char, value: *const c_char, user_data: gpointer)>;

    unsafe extern "C" {
        pub fn gnome_cmd_file_metadata_extractor_get_type() -> GType;

        pub fn gnome_cmd_file_metadata_extractor_supported_tags(
            fme: *mut GnomeCmdFileMetadataExtractor,
        ) -> GStrv;
        pub fn gnome_cmd_file_metadata_extractor_summary_tags(
            fme: *mut GnomeCmdFileMetadataExtractor,
        ) -> GStrv;
        pub fn gnome_cmd_file_metadata_extractor_class_name(
            fme: *mut GnomeCmdFileMetadataExtractor,
            class: *const c_char,
        ) -> *mut c_char;
        pub fn gnome_cmd_file_metadata_extractor_tag_name(
            fme: *mut GnomeCmdFileMetadataExtractor,
            tag: *const c_char,
        ) -> *mut c_char;
        pub fn gnome_cmd_file_metadata_extractor_tag_description(
            fme: *mut GnomeCmdFileMetadataExtractor,
            tag: *const c_char,
        ) -> *mut c_char;
        pub fn gnome_cmd_file_metadata_extractor_extract_metadata(
            fme: *mut GnomeCmdFileMetadataExtractor,
            fd: *const GnomeCmdFileDescriptor,
            add: GnomeCmdFileMetadataExtractorAddTag,
            user_data: gpointer,
        );
    }
}

glib::wrapper! {
    pub struct FileMetadataExtractor(Interface<ffi::GnomeCmdFileMetadataExtractor, ffi::GnomeCmdFileMetadataExtractorIface>);

    match fn {
        type_ => || ffi::gnome_cmd_file_metadata_extractor_get_type(),
    }
}

pub trait FileMetadataExtractorImpl:
    ObjectImpl + ObjectSubclass<Type: IsA<FileMetadataExtractor>>
{
    fn supported_tags(&self) -> Vec<GnomeCmdTag> {
        self.parent_supported_tags()
    }

    fn summary_tags(&self) -> Vec<GnomeCmdTag> {
        self.parent_summary_tags()
    }

    fn class_name(&self, tag: &GnomeCmdTagClass) -> Option<String> {
        self.parent_class_name(tag)
    }

    fn tag_name(&self, tag: &GnomeCmdTag) -> Option<String> {
        self.parent_tag_name(tag)
    }

    fn tag_description(&self, tag: &GnomeCmdTag) -> Option<String> {
        self.parent_tag_description(tag)
    }

    fn extract_metadata<F: FnMut(GnomeCmdTag, Option<&str>)>(
        &self,
        fd: &impl IsA<FileDescriptor>,
        add: F,
    ) {
        self.parent_extract_metadata(fd, add)
    }
}

pub trait FileMetadataExtractorImplExt: FileMetadataExtractorImpl {
    fn parent_supported_tags(&self) -> Vec<GnomeCmdTag> {
        unsafe {
            let type_data = Self::type_data();
            let parent_iface = type_data
                .as_ref()
                .parent_interface::<FileMetadataExtractor>()
                as *const ffi::GnomeCmdFileMetadataExtractorIface;

            let func = (*parent_iface)
                .supported_tags
                .expect("no parent \"supported_tags\" implementation");
            let ret = func(
                self.obj()
                    .unsafe_cast_ref::<FileMetadataExtractor>()
                    .to_glib_none()
                    .0,
            );
            glib::StrV::from_glib_full(ret)
                .into_iter()
                .map(|t| GnomeCmdTag(t.to_string().into()))
                .collect()
        }
    }

    fn parent_summary_tags(&self) -> Vec<GnomeCmdTag> {
        unsafe {
            let type_data = Self::type_data();
            let parent_iface = type_data
                .as_ref()
                .parent_interface::<FileMetadataExtractor>()
                as *const ffi::GnomeCmdFileMetadataExtractorIface;

            let func = (*parent_iface)
                .summary_tags
                .expect("no parent \"summary_tags\" implementation");
            let ret = func(
                self.obj()
                    .unsafe_cast_ref::<FileMetadataExtractor>()
                    .to_glib_none()
                    .0,
            );
            glib::StrV::from_glib_full(ret)
                .into_iter()
                .map(|t| GnomeCmdTag(t.to_string().into()))
                .collect()
        }
    }

    fn parent_class_name(&self, class: &GnomeCmdTagClass) -> Option<String> {
        unsafe {
            let type_data = Self::type_data();
            let parent_iface = type_data
                .as_ref()
                .parent_interface::<FileMetadataExtractor>()
                as *const ffi::GnomeCmdFileMetadataExtractorIface;

            let func = (*parent_iface)
                .class_name
                .expect("no parent \"class_name\" implementation");
            let ret = func(
                self.obj()
                    .unsafe_cast_ref::<FileMetadataExtractor>()
                    .to_glib_none()
                    .0,
                class.0.to_glib_none().0,
            );
            from_glib_full(ret)
        }
    }

    fn parent_tag_name(&self, tag: &GnomeCmdTag) -> Option<String> {
        unsafe {
            let type_data = Self::type_data();
            let parent_iface = type_data
                .as_ref()
                .parent_interface::<FileMetadataExtractor>()
                as *const ffi::GnomeCmdFileMetadataExtractorIface;

            let func = (*parent_iface)
                .tag_name
                .expect("no parent \"tag_name\" implementation");
            let ret = func(
                self.obj()
                    .unsafe_cast_ref::<FileMetadataExtractor>()
                    .to_glib_none()
                    .0,
                tag.0.to_glib_none().0,
            );
            from_glib_full(ret)
        }
    }

    fn parent_tag_description(&self, tag: &GnomeCmdTag) -> Option<String> {
        unsafe {
            let type_data = Self::type_data();
            let parent_iface = type_data
                .as_ref()
                .parent_interface::<FileMetadataExtractor>()
                as *const ffi::GnomeCmdFileMetadataExtractorIface;

            let func = (*parent_iface)
                .tag_description
                .expect("no parent \"tag_description\" implementation");
            let ret = func(
                self.obj()
                    .unsafe_cast_ref::<FileMetadataExtractor>()
                    .to_glib_none()
                    .0,
                tag.0.to_glib_none().0,
            );
            from_glib_full(ret)
        }
    }

    fn parent_extract_metadata<F: FnMut(GnomeCmdTag, Option<&str>)>(
        &self,
        fd: &impl IsA<FileDescriptor>,
        add: F,
    ) {
        unsafe {
            let type_data = Self::type_data();
            let parent_iface = type_data
                .as_ref()
                .parent_interface::<FileMetadataExtractor>()
                as *const ffi::GnomeCmdFileMetadataExtractorIface;

            unsafe extern "C" fn add_trampoline<F: FnMut(GnomeCmdTag, Option<&str>)>(
                tag: *const c_char,
                value: *const c_char,
                user_data: gpointer,
            ) {
                let tag: String = unsafe { from_glib_none(tag) };
                let value: Option<String> = unsafe { from_glib_none(value) };
                let add: &mut F = unsafe { &mut *(user_data as *mut F) };
                add(GnomeCmdTag(tag.into()), value.as_deref());
            }

            let func = (*parent_iface)
                .extract_metadata
                .expect("no parent \"extract_metadata\" implementation");
            func(
                self.obj()
                    .unsafe_cast_ref::<FileMetadataExtractor>()
                    .to_glib_none()
                    .0,
                fd.as_ref().to_glib_none().0,
                std::mem::transmute(add_trampoline::<F> as gpointer),
                Box::into_raw(Box::new(add)) as gpointer,
            );
        }
    }
}

impl<T: FileMetadataExtractorImpl> FileMetadataExtractorImplExt for T {}

unsafe impl<T: FileMetadataExtractorImpl> IsImplementable<T> for FileMetadataExtractor {
    fn interface_init(iface: &mut glib::Interface<Self>) {
        let iface = iface.as_mut();
        iface.supported_tags = Some(file_metadata_extractor_supported_tags::<T>);
        iface.summary_tags = Some(file_metadata_extractor_summary_tags::<T>);
        iface.class_name = Some(file_metadata_extractor_class_name::<T>);
        iface.tag_name = Some(file_metadata_extractor_tag_name::<T>);
        iface.tag_description = Some(file_metadata_extractor_tag_description::<T>);
        iface.extract_metadata = Some(file_metadata_extractor_extract_metadata::<T>);
    }
}

unsafe extern "C" fn file_metadata_extractor_supported_tags<T: FileMetadataExtractorImpl>(
    fme: *mut ffi::GnomeCmdFileMetadataExtractor,
) -> GStrv {
    let instance = unsafe { &*(fme as *mut T::Instance) };
    let imp = instance.imp();
    let tags: glib::StrV = imp
        .supported_tags()
        .into_iter()
        .map(|t| t.0.into())
        .collect();
    tags.to_glib_full()
}

unsafe extern "C" fn file_metadata_extractor_summary_tags<T: FileMetadataExtractorImpl>(
    fme: *mut ffi::GnomeCmdFileMetadataExtractor,
) -> GStrv {
    let instance = unsafe { &*(fme as *mut T::Instance) };
    let imp = instance.imp();
    let tags: glib::StrV = imp.summary_tags().into_iter().map(|t| t.0.into()).collect();
    tags.to_glib_full()
}

unsafe extern "C" fn file_metadata_extractor_class_name<T: FileMetadataExtractorImpl>(
    fme: *mut ffi::GnomeCmdFileMetadataExtractor,
    tag: *const c_char,
) -> *mut c_char {
    let instance = unsafe { &*(fme as *mut T::Instance) };
    let tag = GnomeCmdTagClass(unsafe { String::from_glib_none(tag) }.into());
    let imp = instance.imp();
    imp.class_name(&tag).to_glib_full()
}

unsafe extern "C" fn file_metadata_extractor_tag_name<T: FileMetadataExtractorImpl>(
    fme: *mut ffi::GnomeCmdFileMetadataExtractor,
    tag: *const c_char,
) -> *mut c_char {
    let instance = unsafe { &*(fme as *mut T::Instance) };
    let tag = GnomeCmdTag(unsafe { String::from_glib_none(tag) }.into());
    let imp = instance.imp();
    imp.tag_name(&tag).to_glib_full()
}

unsafe extern "C" fn file_metadata_extractor_tag_description<T: FileMetadataExtractorImpl>(
    fme: *mut ffi::GnomeCmdFileMetadataExtractor,
    tag: *const c_char,
) -> *mut c_char {
    let instance = unsafe { &*(fme as *mut T::Instance) };
    let tag = GnomeCmdTag(unsafe { String::from_glib_none(tag) }.into());
    let imp = instance.imp();
    imp.tag_description(&tag).to_glib_full()
}

unsafe extern "C" fn file_metadata_extractor_extract_metadata<T: FileMetadataExtractorImpl>(
    fme: *mut ffi::GnomeCmdFileMetadataExtractor,
    fd: *mut GnomeCmdFileDescriptor,
    add: ffi::GnomeCmdFileMetadataExtractorAddTag,
    user_data: gpointer,
) {
    let instance = unsafe { &*(fme as *mut T::Instance) };
    let fd: Borrowed<FileDescriptor> = unsafe { from_glib_borrow(fd) };
    let imp = instance.imp();
    imp.extract_metadata(&*fd, |tag, value| {
        if let Some(add) = add {
            unsafe {
                (add)(tag.0.to_glib_none().0, value.to_glib_none().0, user_data);
            }
        }
    });
}

pub trait FileMetadataExtractorExt: IsA<FileMetadataExtractor> + 'static {
    fn supported_tags(&self) -> Vec<GnomeCmdTag> {
        unsafe {
            glib::StrV::from_glib_full(ffi::gnome_cmd_file_metadata_extractor_supported_tags(
                self.as_ref().to_glib_none().0,
            ))
        }
        .into_iter()
        .map(|t| GnomeCmdTag(t.to_string().into()))
        .collect()
    }

    fn summary_tags(&self) -> Vec<GnomeCmdTag> {
        unsafe {
            glib::StrV::from_glib_full(ffi::gnome_cmd_file_metadata_extractor_summary_tags(
                self.as_ref().to_glib_none().0,
            ))
        }
        .into_iter()
        .map(|t| GnomeCmdTag(t.to_string().into()))
        .collect()
    }

    fn class_name(&self, class: &GnomeCmdTagClass) -> Option<String> {
        unsafe {
            from_glib_full(ffi::gnome_cmd_file_metadata_extractor_class_name(
                self.as_ref().to_glib_none().0,
                class.0.to_glib_none().0,
            ))
        }
    }

    fn tag_name(&self, tag: &GnomeCmdTag) -> Option<String> {
        unsafe {
            from_glib_full(ffi::gnome_cmd_file_metadata_extractor_tag_name(
                self.as_ref().to_glib_none().0,
                tag.0.to_glib_none().0,
            ))
        }
    }

    fn tag_description(&self, tag: &GnomeCmdTag) -> Option<String> {
        unsafe {
            from_glib_full(ffi::gnome_cmd_file_metadata_extractor_tag_description(
                self.as_ref().to_glib_none().0,
                tag.0.to_glib_none().0,
            ))
        }
    }

    fn extract_metadata<F: FnMut(GnomeCmdTag, Option<&str>)>(
        &self,
        fd: &impl IsA<FileDescriptor>,
        add: F,
    ) {
        unsafe extern "C" fn add_trampoline<F: FnMut(GnomeCmdTag, Option<&str>)>(
            tag: *const c_char,
            value: *const c_char,
            user_data: gpointer,
        ) {
            let tag: String = unsafe { from_glib_none(tag) };
            let value: Option<String> = unsafe { from_glib_none(value) };
            let add: &mut F = unsafe { &mut *(user_data as *mut F) };
            add(GnomeCmdTag(tag.into()), value.as_deref());
        }

        unsafe {
            ffi::gnome_cmd_file_metadata_extractor_extract_metadata(
                self.as_ref().to_glib_none().0,
                fd.as_ref().to_glib_none().0,
                std::mem::transmute(add_trampoline::<F> as gpointer),
                Box::into_raw(Box::new(add)) as gpointer,
            );
        }
    }
}

impl<O: IsA<FileMetadataExtractor>> FileMetadataExtractorExt for O {}

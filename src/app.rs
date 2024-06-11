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

use crate::{
    data::ProgramsOptionsRead,
    file::File,
    libgcmd::file_base::FileBaseExt,
    spawn::{parse_command_template, spawn_async_command},
    utils::{make_run_in_terminal_command, ErrorMessage},
};
use gettextrs::gettext;
use gtk::{
    gio,
    glib::{self, translate::*},
    prelude::*,
};
use std::{
    borrow::Cow,
    ffi::{c_char, c_void, CString, OsString},
    path::PathBuf,
    ptr,
};

#[repr(i32)]
#[derive(Default, Clone, Copy, Debug, glib::Variant)]
pub enum AppTarget {
    #[default]
    AllFiles = 0,
    AllDirs,
    AllDirsAndFiles,
    SomeFiles,
}

pub trait AppExt {
    fn name(&self) -> String;
    fn icon(&self) -> Option<gio::Icon>;
}

#[derive(Clone, Debug)]
pub struct RegularApp {
    pub app_info: gio::AppInfo,
}

impl AppExt for RegularApp {
    #[inline]
    fn name(&self) -> String {
        self.app_info.name().into()
    }

    #[inline]
    fn icon(&self) -> Option<gio::Icon> {
        self.app_info.icon()
    }
}

impl RegularApp {
    pub fn launch(&self, files: &glib::List<File>) -> Result<(), ErrorMessage> {
        let files: Vec<_> = files.into_iter().map(|f| f.file()).collect();
        self.app_info
            .launch(&files, gio::AppLaunchContext::NONE)
            .map_err(|error| {
                ErrorMessage::with_error(
                    gettext!("Launch of {} failed.", self.app_info.name()),
                    &error,
                )
            })
    }
}

impl glib::variant::StaticVariantType for RegularApp {
    fn static_variant_type() -> Cow<'static, glib::VariantTy> {
        Cow::Borrowed(glib::VariantTy::STRING)
    }
}

impl glib::variant::ToVariant for RegularApp {
    fn to_variant(&self) -> glib::Variant {
        self.app_info
            .id()
            .map(|app_id| app_id.to_variant())
            .unwrap_or_else(|| "".to_variant())
    }
}

impl From<RegularApp> for glib::Variant {
    fn from(value: RegularApp) -> glib::Variant {
        glib::variant::ToVariant::to_variant(&value)
    }
}

impl glib::variant::FromVariant for RegularApp {
    fn from_variant(variant: &glib::Variant) -> Option<Self> {
        let app_id = variant.str()?;
        let app_info = gio::AppInfo::all()
            .into_iter()
            .find(|a| a.id().as_deref() == Some(app_id))?;
        Some(RegularApp { app_info })
    }
}

#[derive(Clone, Debug, glib::Variant)]
pub struct UserDefinedApp {
    pub name: String,
    pub command_template: String,
    pub icon_path: Option<String>,
    pub target: AppTarget,
    pub pattern_string: String,
    pub handles_uris: bool,
    pub handles_multiple: bool,
    pub requires_terminal: bool,
}

impl AppExt for UserDefinedApp {
    fn name(&self) -> String {
        self.name.clone()
    }

    fn icon(&self) -> Option<gio::Icon> {
        Some(gio::FileIcon::new(&gio::File::for_path(self.icon_path.as_ref()?)).upcast())
    }
}

impl UserDefinedApp {
    pub fn pattern_list(&self) -> Vec<&str> {
        self.pattern_string.split(';').collect()
    }

    pub fn build_command_line(
        &self,
        files: &glib::List<File>,
        options: &dyn ProgramsOptionsRead,
    ) -> Option<OsString> {
        let mut commandline = parse_command_template(files, &self.command_template)?;
        if self.requires_terminal {
            commandline = make_run_in_terminal_command(&commandline, options);
        }
        Some(commandline)
    }

    pub fn launch(
        &self,
        files: &glib::List<File>,
        options: &dyn ProgramsOptionsRead,
    ) -> Result<(), ErrorMessage> {
        let working_directory: Option<PathBuf> = files
            .front()
            .ok_or_else(|| ErrorMessage {
                message: gettext!("Cannot launch an app {}. No files were given.", self.name),
                secondary_text: None,
            })?
            .get_real_path()
            .parent()
            .map(|d| d.to_path_buf());

        let command = self
            .build_command_line(files, options)
            .ok_or_else(|| ErrorMessage {
                message: gettext("Cannot build a command line."),
                secondary_text: None,
            })?;

        spawn_async_command(working_directory.as_deref(), &command)
            .map_err(|e| e.into_message())?;
        Ok(())
    }
}

#[derive(Clone, Debug, glib::Variant)]
pub enum App {
    Regular(RegularApp),
    UserDefined(UserDefinedApp),
}

impl App {
    unsafe fn into_raw(self) -> *mut App {
        let app = Box::new(self);
        Box::leak(app) as *mut App
    }

    pub fn name(&self) -> String {
        match self {
            App::Regular(app) => app.name(),
            App::UserDefined(app) => app.name(),
        }
    }

    pub fn icon(&self) -> Option<gio::Icon> {
        match self {
            App::Regular(app) => app.icon(),
            App::UserDefined(app) => app.icon(),
        }
    }

    pub fn target(&self) -> AppTarget {
        match self {
            App::Regular(_) => AppTarget::AllFiles,
            App::UserDefined(app) => app.target,
        }
    }

    pub fn pattern_string(&self) -> &str {
        match self {
            App::Regular(..) => "",
            App::UserDefined(app) => &app.pattern_string,
        }
    }

    pub fn pattern_list(&self) -> Vec<&str> {
        match self {
            App::Regular(..) => Vec::new(),
            App::UserDefined(app) => app.pattern_list(),
        }
    }

    pub fn handles_uris(&self) -> bool {
        match self {
            App::Regular(app) => app.app_info.supports_uris(),
            App::UserDefined(app) => app.handles_uris,
        }
    }

    pub fn handles_multiple(&self) -> bool {
        match self {
            App::Regular(..) => true,
            App::UserDefined(app) => app.handles_multiple,
        }
    }

    pub fn requires_terminal(&self) -> bool {
        match self {
            App::Regular(..) => false,
            App::UserDefined(app) => app.requires_terminal,
        }
    }

    pub fn launch(
        &self,
        files: &glib::List<File>,
        options: &dyn ProgramsOptionsRead,
    ) -> Result<(), ErrorMessage> {
        match self {
            App::Regular(app) => app.launch(files),
            App::UserDefined(app) => app.launch(files, options),
        }
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_app_new_with_values(
    name: *const c_char,
    cmd: *const c_char,
    icon_path: *const c_char,
    target: AppTarget,
    pattern_string: *const c_char,
    handles_uris: glib::ffi::gboolean,
    handles_multiple: glib::ffi::gboolean,
    requires_terminal: glib::ffi::gboolean,
    app_info_ptr: *mut gio::ffi::GAppInfo,
) -> *mut App {
    unsafe {
        let app_info: Option<gio::AppInfo> = from_glib_none(app_info_ptr);
        let app = if let Some(app_info) = app_info {
            App::Regular(RegularApp { app_info })
        } else {
            App::UserDefined(UserDefinedApp {
                name: from_glib_none(name),
                command_template: from_glib_none(cmd),
                icon_path: from_glib_none(icon_path),
                target,
                pattern_string: from_glib_none(pattern_string),
                handles_uris: handles_uris != 0,
                handles_multiple: handles_multiple != 0,
                requires_terminal: requires_terminal != 0,
            })
        };
        app.into_raw()
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_app_new_from_app_info(
    app_info_ptr: *mut gio::ffi::GAppInfo,
) -> *mut App {
    unsafe {
        let app_info: Option<gio::AppInfo> = from_glib_none(app_info_ptr);

        if let Some(app_info) = app_info {
            App::Regular(RegularApp { app_info }).into_raw()
        } else {
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn gnome_cmd_app_free(ptr: *mut App) {
    let app: Box<App> = unsafe { Box::from_raw(ptr) };
    drop(app);
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_set_name(app: *mut App, name: *const c_char) {
    if app.is_null() || name.is_null() {
        return;
    }
    match &mut *app {
        App::Regular(_) => {}
        App::UserDefined(user_app) => user_app.name = from_glib_none(name),
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_set_command(app: *mut App, cmd: *const c_char) {
    if app.is_null() || cmd.is_null() {
        return;
    }
    match &mut *app {
        App::Regular(_) => {}
        App::UserDefined(user_app) => user_app.command_template = from_glib_none(cmd),
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_set_icon_path(app: *mut App, icon_path: *const c_char) {
    if app.is_null() || icon_path.is_null() {
        return;
    }
    match &mut *app {
        App::Regular(_) => {}
        App::UserDefined(user_app) => user_app.icon_path = from_glib_none(icon_path),
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_set_target(app: *mut App, target: AppTarget) {
    if app.is_null() {
        return;
    }
    match &mut *app {
        App::Regular(_) => {}
        App::UserDefined(user_app) => user_app.target = target,
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_set_pattern_string(
    app: *mut App,
    pattern_string: *const c_char,
) {
    if app.is_null() || pattern_string.is_null() {
        return;
    }
    match &mut *app {
        App::Regular(_) => {}
        App::UserDefined(user_app) => user_app.pattern_string = from_glib_none(pattern_string),
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_set_handles_uris(
    app: *mut App,
    handles_uris: glib::ffi::gboolean,
) {
    match &mut *app {
        App::Regular(_) => {}
        App::UserDefined(user_app) => user_app.handles_uris = handles_uris != 0,
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_set_handles_multiple(
    app: *mut App,
    handles_multiple: glib::ffi::gboolean,
) {
    match &mut *app {
        App::Regular(_) => {}
        App::UserDefined(user_app) => user_app.handles_multiple = handles_multiple != 0,
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_set_requires_terminal(
    app: *mut App,
    requires_terminal: glib::ffi::gboolean,
) {
    match &mut *app {
        App::Regular(_) => {}
        App::UserDefined(user_app) => user_app.requires_terminal = requires_terminal != 0,
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_name(app: *mut App) -> *mut c_char {
    if app.is_null() {
        return ptr::null_mut();
    }
    (&*app).name().to_glib_full()
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_command(app: *mut App) -> *mut c_char {
    if app.is_null() {
        return ptr::null_mut();
    }
    match &*app {
        App::Regular(app) => app.app_info.commandline().to_glib_full(),
        App::UserDefined(app) => app.command_template.to_glib_full(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_icon(app: *mut App) -> *mut gio::ffi::GIcon {
    if app.is_null() {
        return ptr::null_mut();
    }
    (&*app).icon().to_glib_full()
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_icon_path(app: *mut App) -> *mut c_char {
    if app.is_null() {
        return ptr::null_mut();
    }
    match &mut *app {
        App::Regular(_) => ptr::null_mut(),
        App::UserDefined(user_app) => user_app.icon_path.to_glib_full(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_target(app: *mut App) -> AppTarget {
    (&*app).target()
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_pattern_string(app: *mut App) -> *mut c_char {
    (&*app).pattern_string().to_glib_full()
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_pattern_list(app: *mut App) -> *mut glib::ffi::GList {
    if app.is_null() {
        return ptr::null_mut();
    }
    let mut list = ptr::null_mut();
    for p in (&*app).pattern_list() {
        list = glib::ffi::g_list_append(list, CString::new(p).unwrap().into_raw() as *mut c_void);
    }
    list
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_handles_uris(app: *mut App) -> glib::ffi::gboolean {
    (&*app).handles_uris() as glib::ffi::gboolean
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_handles_multiple(app: *mut App) -> glib::ffi::gboolean {
    (&*app).handles_multiple() as glib::ffi::gboolean
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_requires_terminal(app: *mut App) -> glib::ffi::gboolean {
    (&*app).requires_terminal() as glib::ffi::gboolean
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_get_app_info(app: *mut App) -> *const gio::ffi::GAppInfo {
    match &*app {
        App::Regular(app) => app.app_info.to_glib_none().0,
        App::UserDefined(_) => ptr::null(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_save_to_variant(app: *mut App) -> *mut glib::ffi::GVariant {
    (&*app).to_variant().into_glib_ptr()
}

#[no_mangle]
pub unsafe extern "C" fn gnome_cmd_app_load_from_variant(
    variant: *mut glib::ffi::GVariant,
) -> *mut App {
    let variant = glib::Variant::from_glib_full(variant);
    match App::from_variant(&variant) {
        Some(app) => app.into_raw(),
        None => ptr::null_mut(),
    }
}

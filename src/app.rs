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
    debug::debug,
    file::File,
    options::{
        options::{GeneralOptions, ProgramsOptions},
        types::WriteResult,
    },
    spawn::{parse_command_template, spawn_async_command},
    utils::{ErrorMessage, make_run_in_terminal_command},
};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*};
use std::{borrow::Cow, ffi::OsString, path::PathBuf};

#[repr(i32)]
#[derive(Default, Clone, Copy, PartialEq, Eq, Debug, glib::Variant, strum::FromRepr)]
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
        debug!('g', "Launching {:?}", self.app_info.commandline());
        self.app_info
            .launch(&files, gio::AppLaunchContext::NONE)
            .map_err(|error| {
                ErrorMessage::with_error(
                    gettext("Launch of {} failed.").replace("{}", &self.app_info.name()),
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
    pub icon_path: Option<PathBuf>,
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
        options: &ProgramsOptions,
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
        options: &ProgramsOptions,
    ) -> Result<(), ErrorMessage> {
        let working_directory: Option<PathBuf> = files
            .front()
            .ok_or_else(|| ErrorMessage {
                message: gettext("Cannot launch an app {}. No files were given.")
                    .replace("{}", &self.name),
                secondary_text: None,
            })?
            .get_dirname();

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
    pub fn name(&self) -> String {
        match self {
            App::Regular(app) => app.name(),
            App::UserDefined(app) => app.name(),
        }
    }

    pub fn command(&self) -> Option<String> {
        match self {
            App::Regular(app) => Some(app.app_info.commandline()?.to_str()?.to_owned()),
            App::UserDefined(app) => Some(app.command_template.clone()),
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
        options: &ProgramsOptions,
    ) -> Result<(), ErrorMessage> {
        match self {
            App::Regular(app) => app.launch(files),
            App::UserDefined(app) => app.launch(files, options),
        }
    }
}

#[derive(glib::Variant)]
pub struct FavoriteAppVariant {
    pub name: String,
    pub command_template: String,
    pub icon_path: String,
    pub pattern_string: String,
    pub target: u32,
    pub handles_uris: bool,
    pub handles_multiple: bool,
    pub requires_terminal: bool,
}

pub fn save_favorite_apps(apps: &[UserDefinedApp], options: &GeneralOptions) -> WriteResult {
    let variants = apps
        .iter()
        .map(|app| FavoriteAppVariant {
            name: app.name.to_owned(),
            command_template: app.command_template.to_owned(),
            icon_path: app
                .icon_path
                .as_ref()
                .and_then(|p| p.to_str().map(ToString::to_string))
                .unwrap_or_default(),
            pattern_string: app.pattern_string.to_owned(),
            target: app.target as u32,
            handles_uris: app.handles_uris,
            handles_multiple: app.handles_multiple,
            requires_terminal: app.requires_terminal,
        })
        .collect::<Vec<_>>();
    options.favorite_apps.set(variants)
}

pub fn load_favorite_apps(options: &GeneralOptions) -> Vec<UserDefinedApp> {
    options
        .favorite_apps
        .get()
        .into_iter()
        .map(|app| UserDefinedApp {
            name: app.name,
            command_template: app.command_template,
            icon_path: Some(app.icon_path)
                .filter(|p| !p.is_empty())
                .map(|p| PathBuf::from(p)),
            pattern_string: app.pattern_string,
            target: app
                .target
                .try_into()
                .ok()
                .and_then(AppTarget::from_repr)
                .unwrap_or_default(),
            handles_uris: app.handles_uris,
            handles_multiple: app.handles_multiple,
            requires_terminal: app.requires_terminal,
        })
        .collect()
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_variant_type() {
        assert_eq!(&*FavoriteAppVariant::static_variant_type(), "(ssssubbb)");
    }
}

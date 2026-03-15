// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    debug::debug,
    file::{File, FileOps},
    options::ProgramsOptions,
    utils::{ErrorMessage, make_run_in_terminal_command},
};
use gettextrs::gettext;
use gtk::{gdk, gio, gio::prelude::*, glib, prelude::DisplayExt};
use std::{
    cell::OnceCell,
    ffi::{OsStr, OsString},
    io::BufRead,
    path::{Path, PathBuf},
};

/// Parses a command template and substitutes a certain
/// symbol with path or URI names. Leading blanks are removed.
///
/// Possible symbols are:
///
/// - `%f` - file name (or list for multiple selections)
/// - `%F` - quoted filename (or list for multiple selections)
/// - `%p` - full file system path (or list for multiple selections)
/// - `%P` - quoted full file system path (or list for multiple selections)
/// - `%s` - synonym for %P (for compatibility with previous versions of gcmd)
/// - `%u` - fully qualified URI for the file (or list for multiple selections)
/// - `%d` - full path to the directory containing file
/// - `%D` - quoted full path to the directory containg file
/// - `%%` - percent sign
pub fn parse_command_template(files: &[impl FileOps], command_template: &str) -> Option<OsString> {
    let filename: OnceCell<Vec<String>> = OnceCell::new();
    let file_path: OnceCell<Vec<PathBuf>> = OnceCell::new();
    let uri: OnceCell<Vec<String>> = OnceCell::new();
    let mut cmd = OsString::new();

    let first_file = files.first()?;
    let directory = first_file.file().parent()?;
    let dir_path: OsString = if first_file.is_local() {
        directory.path()?.into_os_string()
    } else {
        OsString::from(directory.uri())
    };

    // loop over chars of command-string set by user
    let mut percent: bool = false;
    for s in command_template.trim_start().chars() {
        if !percent {
            match s {
                '%' => percent = true,
                _ => cmd.push(s.to_string()),
            }
        } else {
            match s {
                'f' | 'F' => {
                    let raw = s == 'f';
                    let names = filename.get_or_init(|| files.iter().map(|f| f.name()).collect());
                    for (i, name) in names.iter().enumerate() {
                        if i > 0 {
                            cmd.push(" ");
                        }
                        if raw {
                            cmd.push(name);
                        } else {
                            cmd.push(glib::shell_quote(name));
                        }
                    }
                }
                'p' | 'P' | 's' => {
                    let raw = s == 'p';
                    let paths = file_path
                        .get_or_init(|| files.iter().filter_map(|f| f.local_path()).collect());
                    for (i, path) in paths.iter().enumerate() {
                        if i > 0 {
                            cmd.push(" ");
                        }
                        if raw {
                            cmd.push(path);
                        } else {
                            cmd.push(glib::shell_quote(path));
                        }
                    }
                }
                'u' => {
                    let uris = uri.get_or_init(|| files.iter().map(|f| f.uri()).collect());
                    for (i, uri) in uris.iter().enumerate() {
                        if i > 0 {
                            cmd.push(" ");
                        }
                        cmd.push(glib::shell_quote(uri));
                    }
                }
                'd' => cmd.push(&dir_path),
                'D' => cmd.push(glib::shell_quote(&dir_path)),
                '%' => cmd.push(s.to_string()),
                _ => {
                    cmd.push("%");
                    cmd.push(s.to_string());
                }
            }
            percent = false;
        }
    }

    if percent {
        cmd.push("%");
    }

    Some(cmd).filter(|c| !c.is_empty())
}

pub enum SpawnError {
    InvalidTemplate,
    InvalidCommand(glib::Error),
    Failure(std::io::Error),
}

impl SpawnError {
    pub fn into_message(self) -> ErrorMessage {
        match self {
            Self::InvalidTemplate => ErrorMessage {
                message: gettext("No valid command given."),
                secondary_text: Some(gettext("Bad command template.")),
            },
            Self::InvalidCommand(error) => {
                ErrorMessage::with_error(gettext("No valid command given."), &error)
            }
            Self::Failure(error) => {
                ErrorMessage::with_error(gettext("Unable to execute command."), &error)
            }
        }
    }
}

pub fn spawn_async_command(
    working_directory: Option<&Path>,
    command: &OsStr,
) -> Result<(), SpawnError> {
    let argv = glib::shell_parse_argv(command).map_err(SpawnError::InvalidCommand)?;

    debug!('g', "running: {}", command.to_string_lossy());

    let mut cmd = std::process::Command::new(&argv[0]);
    cmd.args(&argv[1..]);
    if let Some(d) = working_directory {
        cmd.current_dir(d);
    }

    // Make sure the application can gain focus on Wayland
    if let Some(sn_id) = gdk::Display::default().and_then(|display| {
        display
            .app_launch_context()
            // These parameters are ignored for Wayland, on X11 this will
            // always return None without an AppInfo - which is what we want.
            .startup_notify_id(gio::AppInfo::NONE, &[])
    }) {
        cmd.env("XDG_ACTIVATION_TOKEN", &sn_id);
    }

    cmd.spawn().map_err(SpawnError::Failure)?;

    Ok(())
}

pub fn spawn_async(
    working_directory: Option<&Path>,
    files: &[impl FileOps],
    command_template: &str,
) -> Result<(), SpawnError> {
    let Some(cmd) = parse_command_template(files, command_template) else {
        return Err(SpawnError::InvalidTemplate);
    };
    spawn_async_command(working_directory, &cmd)
}

pub fn run_command_indir(
    working_directory: Option<&Path>,
    command: &OsStr,
    in_terminal: bool,
    options: &ProgramsOptions,
) -> Result<(), SpawnError> {
    let actual_command = if in_terminal {
        make_run_in_terminal_command(command, options)
    } else {
        command.to_owned()
    };
    spawn_async_command(working_directory, &actual_command)
}

fn app_get_linked_libs(app_path: &Path) -> std::io::Result<Vec<String>> {
    Ok(std::process::Command::new("ldd")
        .arg(app_path)
        .output()?
        .stdout
        .lines()
        .map_while(Result::ok)
        .filter_map(|line| line.split_once(' ').map(|p| p.0.trim().to_owned()))
        .collect())
}

pub fn app_needs_terminal(file: &File) -> bool {
    let is_executable = file
        .content_type()
        .map(|c| c == "application/x-executable" || c == "application/x-executable-binary")
        .unwrap_or_default();

    if is_executable && let Some(app_path) = file.local_path() {
        match app_get_linked_libs(&app_path) {
            Ok(libs) => !libs.into_iter().any(|lib| lib.starts_with("libX11")),
            Err(error) => {
                eprintln!(
                    "Failed to detect libraries used by an application {}: {}",
                    app_path.display(),
                    error
                );
                true
            }
        }
    } else {
        true
    }
}

/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
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

use crate::{config::PREFIX, data::ProgramsOptionsRead, file::File};
use gettextrs::{gettext, ngettext};
use gtk::{gdk, glib, prelude::*};
use std::{
    ffi::{OsStr, OsString},
    process::Command,
    sync::OnceLock,
    time::{Duration, SystemTime, UNIX_EPOCH},
};

pub const GNOME_CMD_PERM_USER_READ: u32 = 256; //r--------
pub const GNOME_CMD_PERM_USER_WRITE: u32 = 128; //-w-------
pub const GNOME_CMD_PERM_USER_EXEC: u32 = 64; //--x------
pub const GNOME_CMD_PERM_GROUP_READ: u32 = 32; //---r-----
pub const GNOME_CMD_PERM_GROUP_WRITE: u32 = 16; //----w----
pub const GNOME_CMD_PERM_GROUP_EXEC: u32 = 8; //-----x---
pub const GNOME_CMD_PERM_OTHER_READ: u32 = 4; //------r--
pub const GNOME_CMD_PERM_OTHER_WRITE: u32 = 2; //-------w-
pub const GNOME_CMD_PERM_OTHER_EXEC: u32 = 1; //--------x
pub const GNOME_CMD_PERM_USER_ALL: u32 = 448; //rwx------
pub const GNOME_CMD_PERM_GROUP_ALL: u32 = 56; //---rwx---
pub const GNOME_CMD_PERM_OTHER_ALL: u32 = 7; //------rwx

pub fn temp_directory() -> &'static tempfile::TempDir {
    static TEMP_DIRECTORY: OnceLock<tempfile::TempDir> = OnceLock::new();
    TEMP_DIRECTORY
        .get_or_init(|| tempfile::tempdir().expect("Cannot create a temporary directory."))
}

pub fn temp_file(f: &File) -> Result<File, ErrorMessage> {
    let name = f.get_name();
    let name_parts = name.rsplit_once('.');

    let temp_file = tempfile::Builder::new()
        .prefix(name_parts.map(|p| p.0).unwrap_or("tmp"))
        .suffix(name_parts.map(|p| p.0).unwrap_or(".tmp"))
        .tempfile_in(temp_directory().path())
        .map_err(|error| ErrorMessage {
            message: gettext("Cannot create a temporary file."),
            secondary_text: Some(error.to_string()),
        })?;

    let path = temp_file.into_temp_path();
    File::new_from_path(&path).map_err(|error| {
        ErrorMessage::with_error(
            gettext!("Cannot create a file for a path {}.", path.display()),
            &error,
        )
    })
}

fn substitute_command_argument(command_template: &str, arg: &OsStr) -> OsString {
    let mut cmd = OsString::new();
    match command_template.split_once("%s") {
        Some((before, after)) => {
            cmd.push(before);
            cmd.push(arg);
            cmd.push(after);
        }
        None => {
            cmd.push(command_template);
            cmd.push(" ");
            cmd.push(arg);
        }
    }
    cmd
}

pub fn make_run_in_terminal_command(
    command: &OsStr,
    options: &dyn ProgramsOptionsRead,
) -> OsString {
    let arg = if options.use_gcmd_block() {
        glib::shell_quote(substitute_command_argument(
            &format!("bash -c \"%s; {PREFIX}/bin/gcmd-block\""),
            command,
        ))
    } else {
        glib::shell_quote(command)
    };
    substitute_command_argument(&options.terminal_exec_cmd(), &arg)
}

pub fn sudo_command() -> Result<Command, ErrorMessage> {
    fn find_sudo_program(cmd: &str, args: &[&str]) -> Option<std::process::Command> {
        let path = glib::find_program_in_path(cmd)?;
        let mut command = Command::new(path);
        for arg in args {
            command.arg(arg);
        }
        Some(command)
    }

    find_sudo_program("gksudo", &[])
        .or_else(|| find_sudo_program("xdg-su", &[]))
        .or_else(|| find_sudo_program("gksu", &[]))
        .or_else(|| find_sudo_program("gnomesu", &["-c"]))
        .or_else(|| find_sudo_program("kdesu", &[]))
        .or_else(|| find_sudo_program("beesu", &[]))
        .or_else(|| find_sudo_program("pkexec", &[]))
        .ok_or_else(|| ErrorMessage {
            message: gettext("gksudo, xdg-su, gksu, gnomesu, kdesu, beesu or pkexec is not found."),
            secondary_text: None,
        })
}

pub async fn run_simple_dialog(
    parent: &gtk::Window,
    ignore_close_box: bool,
    msg_type: gtk::MessageType,
    text: &str,
    title: &str,
    def_response: Option<u16>,
    buttons: &[&str],
) -> gtk::ResponseType {
    let dialog = gtk::MessageDialog::builder()
        .transient_for(parent)
        .modal(true)
        .message_type(msg_type)
        .text(text)
        .use_markup(true)
        .title(title)
        .deletable(!ignore_close_box)
        .build();
    for (i, button) in buttons.into_iter().enumerate() {
        dialog.add_button(button, gtk::ResponseType::Other(i as u16));
    }

    if let Some(response_id) = def_response {
        dialog.set_default_response(gtk::ResponseType::Other(response_id));
    }

    if ignore_close_box {
        dialog.connect_delete_event(|_, _| glib::Propagation::Stop);
    } else {
        dialog.connect_key_press_event(|dialog, event| {
            if event.keyval() == gdk::keys::constants::Escape {
                dialog.response(gtk::ResponseType::None);
                glib::Propagation::Stop
            } else {
                glib::Propagation::Proceed
            }
        });
    }

    let result = dialog.run_future().await;
    dialog.hide();

    result
}

pub fn close_dialog_with_escape_key(dialog: &gtk::Dialog) {
    let key_controller = gtk::EventControllerKey::new(dialog);
    key_controller.connect_key_pressed(
        glib::clone!(@weak dialog => @default-return false, move |_, key, _, _| {
            if key == *gdk::keys::constants::Escape {
                dialog.response(gtk::ResponseType::Cancel);
                true
            } else {
                false
            }
        }),
    );
}

pub async fn prompt_message(
    parent: &gtk::Window,
    message_type: gtk::MessageType,
    buttons: gtk::ButtonsType,
    message: &str,
    secondary_text: Option<&str>,
) -> gtk::ResponseType {
    let dlg = gtk::MessageDialog::builder()
        .transient_for(parent)
        .destroy_with_parent(true)
        .message_type(message_type)
        .buttons(buttons)
        .text(message)
        .build();
    dlg.set_secondary_text(secondary_text);
    let result = dlg.run_future().await;
    dlg.close();
    result
}

fn create_error_dialog(parent: &gtk::Window, message: &str) -> gtk::MessageDialog {
    gtk::MessageDialog::builder()
        .transient_for(parent)
        .destroy_with_parent(true)
        .message_type(gtk::MessageType::Error)
        .buttons(gtk::ButtonsType::Ok)
        .text(message)
        .build()
}

pub struct ErrorMessage {
    pub message: String,
    pub secondary_text: Option<String>,
}

impl ErrorMessage {
    pub fn new<M: Into<String>, S: Into<String>>(message: M, secondary_text: Option<S>) -> Self {
        Self {
            message: message.into(),
            secondary_text: secondary_text.map(|s| s.into()),
        }
    }

    pub fn with_error(message: impl Into<String>, error: &dyn std::error::Error) -> Self {
        Self {
            message: message.into(),
            secondary_text: Some(error.to_string()),
        }
    }

    pub async fn show(&self, parent: &gtk::Window) {
        let dlg = create_error_dialog(parent, &self.message);
        dlg.set_secondary_text(self.secondary_text.as_deref());
        dlg.present();
        dlg.run_future().await;
        dlg.close();
    }
}

pub fn show_error_message(parent: &gtk::Window, message: &ErrorMessage) {
    show_message(parent, &message.message, message.secondary_text.as_deref());
}

pub fn show_message(parent: &gtk::Window, message: &str, secondary_text: Option<&str>) {
    let dlg = create_error_dialog(parent, message);
    dlg.set_secondary_text(secondary_text);
    dlg.connect_response(|dlg, _response| dlg.close());
    dlg.present();
}

pub fn show_error(parent: &gtk::Window, message: &str, error: &dyn std::error::Error) {
    show_message(parent, message, Some(&error.to_string()));
}

pub async fn show_error_message_future(
    parent: &gtk::Window,
    message: &str,
    secondary_text: Option<&str>,
) {
    let dlg = create_error_dialog(parent, message);
    dlg.set_secondary_text(secondary_text);
    dlg.present();
    dlg.run_future().await;
    dlg.close();
}

pub fn display_help(parent_window: &gtk::Window, link_id: Option<&str>) {
    let mut help_uri = format!("help:{}", crate::config::PACKAGE);
    if let Some(link_id) = link_id {
        help_uri.push('/');
        help_uri.push_str(link_id);
    }

    if let Err(error) =
        gtk::show_uri_on_window(Some(parent_window), &help_uri, gtk::current_event_time())
    {
        show_error(
            parent_window,
            &gettext("There was an error displaying help."),
            &error,
        );
    }
}

pub fn toggle_file_name_selection(entry: &gtk::Entry) {
    let text: Vec<char> = entry.text().chars().collect();

    let base = text
        .iter()
        .enumerate()
        .rev()
        .find_map(|(index, ch)| (*ch == '/').then_some(index + 1))
        .unwrap_or_default();

    if let Some((mut beg, end)) = entry.selection_bounds() {
        let text_len = text.len() as i32;

        let ext = text
            .iter()
            .enumerate()
            .skip(base)
            .rev()
            .find_map(|(index, ch)| (*ch == '.').then_some(index as i32))
            .unwrap_or(-1);
        let base = base as i32;

        if beg == 0 && end == text_len {
            entry.select_region(base, ext);
        } else {
            if beg != base {
                beg = if beg > base { base } else { 0 };
            } else {
                if end != ext || end == text_len {
                    beg = 0;
                }
            }

            entry.select_region(beg, -1);
        }
    } else {
        entry.select_region(base as i32, -1);
    }
}

pub fn get_modifiers_state(window: &gtk::Window) -> Option<gdk::ModifierType> {
    let gdk_window = window.window()?;
    let display = gdk_window.display();
    let pointer = display.default_seat()?.pointer()?;
    let (_w, _x, _y, modifiers) = gdk_window.device_position(&pointer);
    Some(modifiers)
}

pub async fn pending() {
    glib::timeout_future(Duration::from_millis(1)).await;
}

#[repr(C)]
#[derive(Clone, Copy)]
pub enum SizeDisplayMode {
    Plain = 0,
    Locale,
    Grouped,
    Powered,
}

pub fn size_to_string(size: u64, mode: SizeDisplayMode) -> String {
    let size_u32 = u32::try_from(size).unwrap_or(u32::MAX);
    match mode {
        SizeDisplayMode::Powered => {
            const PIBI: u64 = 1024 * 1024 * 1024 * 1024 * 1024;
            const TIBI: u64 = 1024 * 1024 * 1024 * 1024;
            const GIBI: u64 = 1024 * 1024 * 1024;
            const MIBI: u64 = 1024 * 1024;
            const KIBI: u64 = 1024;

            if size >= PIBI {
                ngettext!(
                    "{} PiB",
                    "{} PiB",
                    size_u32,
                    format!("{:.1}", size as f64 / PIBI as f64)
                )
            } else if size >= TIBI {
                ngettext!(
                    "{} TiB",
                    "{} TiB",
                    size_u32,
                    format!("{:.1}", size as f64 / TIBI as f64)
                )
            } else if size >= GIBI {
                ngettext!(
                    "{} GiB",
                    "{} GiB",
                    size_u32,
                    format!("{:.1}", size as f64 / GIBI as f64)
                )
            } else if size >= MIBI {
                ngettext!(
                    "{} MiB",
                    "{} MiB",
                    size_u32,
                    format!("{:.1}", size as f64 / MIBI as f64)
                )
            } else if size >= KIBI {
                ngettext!(
                    "{} kiB",
                    "{} kiB",
                    size_u32,
                    format!("{:.1}", size as f64 / KIBI as f64)
                )
            } else {
                ngettext!("{} byte", "{} bytes", size_u32, size)
            }
        }
        SizeDisplayMode::Grouped => {
            let mut digits: Vec<char> = size.to_string().chars().collect();
            let mut i = digits.len() as isize - 3;
            while i > 0 {
                digits.insert(i as usize, ' ');
                i -= 3;
            }
            let value: String = digits.into_iter().collect();
            ngettext!("{} byte", "{} bytes", size_u32, value)
        }
        SizeDisplayMode::Locale => size.to_string(), // TODO
        SizeDisplayMode::Plain => ngettext!("{} byte", "{} bytes", size_u32, size),
    }
}

pub fn nice_size(size: u64, mode: SizeDisplayMode) -> String {
    match mode {
        SizeDisplayMode::Powered if size >= 1024 => {
            format!(
                "{} ({})",
                size_to_string(size, mode),
                nice_size(size, SizeDisplayMode::Grouped)
            )
        }
        _ => size_to_string(size, mode),
    }
}

pub fn time_to_string(
    time: SystemTime,
    format: &str,
) -> Result<String, Box<dyn std::error::Error>> {
    // TODO: fix for dates before EPOCH
    let local = glib::DateTime::from_unix_utc(time.duration_since(UNIX_EPOCH)?.as_secs() as i64)?
        .to_local()?;
    let string = local.format(format).or_else(|_| local.format("%c"))?;
    Ok(string.to_string())
}

pub fn bold(text: &str) -> String {
    format!(
        "<span weight=\"bold\">{}</span>",
        glib::markup_escape_text(text)
    )
}

pub trait Gtk3to4BoxCompat {
    fn append(&self, child: &impl IsA<gtk::Widget>);
}

impl Gtk3to4BoxCompat for gtk::Box {
    fn append(&self, child: &impl IsA<gtk::Widget>) {
        self.add(child);
    }
}

pub trait Gdk3to4RectangleCompat {
    fn contains_point(&self, x: i32, y: i32) -> bool;
}

impl Gdk3to4RectangleCompat for gdk::Rectangle {
    fn contains_point(&self, x: i32, y: i32) -> bool {
        x >= self.x()
            && x < self.x() + self.width()
            && y >= self.y()
            && y < self.y() + self.height()
    }
}

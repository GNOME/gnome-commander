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

mod advanced_rename;
mod app;
mod application;
mod chmod_component;
mod chown_component;
mod command_line;
mod config;
mod connection;
mod connection_bar;
mod connection_button;
mod debug;
mod dialogs;
mod dir;
mod directory_indicator;
mod dirlist;
mod file;
mod file_edit;
mod file_list;
mod file_metainfo_view;
mod file_selector;
mod file_view;
mod filter;
mod hintbox;
mod history_entry;
mod i18n;
mod imageloader;
mod intviewer;
mod layout;
mod libgcmd;
mod main_win;
mod notebook_ext;
mod open_connection;
mod open_file;
mod options;
mod paned_ext;
mod path;
mod plugin_manager;
mod pwd;
mod search;
mod select_directory_button;
mod select_icon_button;
mod shortcuts;
mod spawn;
mod tab_label;
mod tags;
mod transfer;
mod types;
mod user_actions;
mod utils;
mod weak_map;
mod weak_ref;
mod weak_set;

mod gmodule;

use application::Application;
use gtk::{glib, prelude::*};
use std::error::Error;
use std::path::Path;
use std::process::Termination;

use crate::config::{DATADIR, PACKAGE};

fn main() -> Result<impl Termination, Box<dyn Error>> {
    if let Some(mismatch) = glib::check_version(2, 66, 0) {
        eprintln!("GLib version mismatch: {mismatch}");
        std::process::exit(1);
    }
    if let Some(mismatch) = gtk::check_version(4, 12, 0) {
        eprintln!("GTK version mismatch: {mismatch}");
        std::process::exit(1);
    }

    gettextrs::setlocale(gettextrs::LocaleCategory::LcAll, "");
    gettextrs::bindtextdomain(PACKAGE, Path::new(DATADIR).join("locale"))?;
    gettextrs::bind_textdomain_codeset(PACKAGE, "UTF-8")?;
    gettextrs::textdomain(PACKAGE)?;

    gtk::init()?;

    let app = Application::default();
    let exis_code = app.run();
    Ok(exis_code)
}

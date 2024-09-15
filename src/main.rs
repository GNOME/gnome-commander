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

mod advrename_profile_component;
mod app;
mod application;
mod config;
mod connection;
mod data;
mod dialogs;
mod dir;
mod dirlist;
mod file;
mod file_list;
mod file_list_actions;
mod file_selector;
mod file_view;
mod i18n;
mod intviewer;
mod libgcmd;
mod main_win;
mod notebook_ext;
mod open_file;
mod spawn;
mod transfer;
mod types;
mod user_actions;
mod utils;

use application::GnomeCmdApplication;
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
    if let Some(mismatch) = gtk::check_version(3, 24, 0) {
        eprintln!("GTK version mismatch: {mismatch}");
        std::process::exit(1);
    }

    gettextrs::setlocale(gettextrs::LocaleCategory::LcAll, "");
    gettextrs::bindtextdomain(PACKAGE, Path::new(DATADIR).join("locale"))?;
    gettextrs::bind_textdomain_codeset(PACKAGE, "UTF-8")?;
    gettextrs::textdomain(PACKAGE)?;

    gtk::init()?;

    let app = GnomeCmdApplication::new();
    let exis_code = app.run();
    Ok(exis_code)
}

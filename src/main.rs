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

mod config;

use gtk::{self, glib};
use std::error::Error;
use std::process::Termination;

extern "C" {
    fn c_main(argc: libc::c_int, argv: *const *const libc::c_char) -> libc::c_int;
}

fn main() -> Result<impl Termination, Box<dyn Error>> {
    if let Some(mismatch) = glib::check_version(2, 66, 0) {
        eprintln!("GLib version mismatch: {mismatch}");
        std::process::exit(1);
    }
    if let Some(mismatch) = gtk::check_version(3, 24, 0) {
        eprintln!("GTK version mismatch: {mismatch}");
        std::process::exit(1);
    }

    let retcode = unsafe { c_main(0, std::ptr::null()) };
    Ok(std::process::ExitCode::from(retcode as u8))
}

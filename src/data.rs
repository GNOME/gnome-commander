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

use gtk::{gio, prelude::*};

pub struct ProgramsOptions(pub gio::Settings);

pub trait ProgramsOptionsRead {
    fn dont_download(&self) -> bool;
    fn use_internal_viewer(&self) -> bool;
    fn viewer_cmd(&self) -> String;
    fn editor_cmd(&self) -> String;
    fn differ_cmd(&self) -> String;
    fn use_internal_search(&self) -> bool;
    fn search_cmd(&self) -> String;
    fn sendto_cmd(&self) -> String;
    fn terminal_cmd(&self) -> String;
    fn terminal_exec_cmd(&self) -> String;
    fn use_gcmd_block(&self) -> bool;
}

impl ProgramsOptions {
    pub fn new() -> Self {
        Self(gio::Settings::new(
            "org.gnome.gnome-commander.preferences.programs",
        ))
    }
}

impl ProgramsOptionsRead for ProgramsOptions {
    fn dont_download(&self) -> bool {
        self.0.boolean("dont-download")
    }

    fn use_internal_viewer(&self) -> bool {
        self.0.boolean("use-internal-viewer")
    }

    fn viewer_cmd(&self) -> String {
        self.0.string("viewer-cmd").into()
    }

    fn editor_cmd(&self) -> String {
        self.0.string("editor-cmd").into()
    }

    fn differ_cmd(&self) -> String {
        self.0.string("differ-cmd").into()
    }

    fn use_internal_search(&self) -> bool {
        self.0.boolean("use-internal-search")
    }

    fn search_cmd(&self) -> String {
        self.0.string("search-cmd").into()
    }

    fn sendto_cmd(&self) -> String {
        self.0.string("sendto-cmd").into()
    }

    fn terminal_cmd(&self) -> String {
        self.0.string("terminal-cmd").into()
    }

    fn terminal_exec_cmd(&self) -> String {
        self.0.string("terminal-exec-cmd").into()
    }

    fn use_gcmd_block(&self) -> bool {
        self.0.boolean("use-gcmd-block")
    }
}

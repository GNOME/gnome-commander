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

use super::{apps_widget::FavoriteApps, common::create_category};
use crate::options::{
    options::{GeneralOptions, ProgramsOptions},
    types::WriteResult,
};
use gettextrs::gettext;
use gtk::prelude::*;

pub struct ProgramsTab {
    scrolled_window: gtk::ScrolledWindow,
    honor_expect_uris: gtk::CheckButton,
    viewer: gtk::Entry,
    use_internal_viewer: gtk::CheckButton,
    editor: gtk::Entry,
    differ: gtk::Entry,
    search: gtk::Entry,
    use_internal_search: gtk::CheckButton,
    send_to: gtk::Entry,
    termopen: gtk::Entry,
    fav_apps: FavoriteApps,
    termexec: gtk::Entry,
    use_gcmd_block: gtk::CheckButton,
}

impl ProgramsTab {
    pub fn new() -> Self {
        let vbox = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .margin_top(6)
            .margin_bottom(6)
            .margin_start(6)
            .margin_end(6)
            .hexpand(true)
            .vexpand(true)
            .build();

        let scrolled_window = gtk::ScrolledWindow::builder()
            .hscrollbar_policy(gtk::PolicyType::Never)
            .vscrollbar_policy(gtk::PolicyType::Automatic)
            .hexpand(true)
            .vexpand(true)
            .child(&vbox)
            .build();

        let honor_expect_uris = gtk::CheckButton::builder()
            .label(gettext(
                "Always download remote files before opening in external programs",
            ))
            .build();
        vbox.append(&create_category(
            &gettext("MIME applications"),
            &honor_expect_uris,
        ));

        let grid = gtk::Grid::builder()
            .column_spacing(12)
            .row_spacing(6)
            .build();
        vbox.append(&create_category(&gettext("Standard programs"), &grid));

        let label = gtk::Label::builder().label(gettext("Viewer:")).build();
        grid.attach(&label, 0, 0, 1, 1);
        let viewer = gtk::Entry::builder().hexpand(true).build();
        grid.attach(&viewer, 1, 0, 1, 1);
        let use_internal_viewer = gtk::CheckButton::builder()
            .label(gettext("Use Internal Viewer"))
            .build();
        grid.attach(&use_internal_viewer, 1, 1, 1, 1);

        let label = gtk::Label::builder().label(gettext("Editor:")).build();
        grid.attach(&label, 0, 2, 1, 1);
        let editor = gtk::Entry::builder().hexpand(true).build();
        grid.attach(&editor, 1, 2, 1, 1);

        let label = gtk::Label::builder().label(gettext("Differ:")).build();
        grid.attach(&label, 0, 3, 1, 1);
        let differ = gtk::Entry::builder().hexpand(true).build();
        grid.attach(&differ, 1, 3, 1, 1);

        let label = gtk::Label::builder().label(gettext("Search:")).build();
        grid.attach(&label, 0, 4, 1, 1);
        let search = gtk::Entry::builder().hexpand(true).build();
        grid.attach(&search, 1, 4, 1, 1);
        let use_internal_search = gtk::CheckButton::builder()
            .label(gettext("Use Internal Search"))
            .build();
        grid.attach(&use_internal_search, 1, 5, 1, 1);

        let label = gtk::Label::builder().label(gettext("Send files:")).build();
        grid.attach(&label, 0, 6, 1, 1);
        let send_to = gtk::Entry::builder().hexpand(true).build();
        grid.attach(&send_to, 1, 6, 1, 1);

        let label = gtk::Label::builder().label(gettext("Terminal:")).build();
        grid.attach(&label, 0, 7, 1, 1);
        let termopen = gtk::Entry::builder().hexpand(true).build();
        grid.attach(&termopen, 1, 7, 1, 1);

        let fav_apps = FavoriteApps::new();
        vbox.append(&create_category(
            &gettext("Other favourite apps"),
            &fav_apps.widget(),
        ));

        let grid = gtk::Grid::builder()
            .column_spacing(12)
            .row_spacing(6)
            .build();
        vbox.append(&create_category(&gettext("Global app options"), &grid));

        let label = gtk::Label::builder()
            .label(gettext("Terminal command for apps in the list above:"))
            .build();
        grid.attach(&label, 0, 0, 1, 1);
        let termexec = gtk::Entry::builder().hexpand(true).build();
        grid.attach(&termexec, 1, 0, 1, 1);
        let use_gcmd_block = gtk::CheckButton::builder()
            .label(gettext("Leave terminal window open"))
            .build();
        grid.attach(&use_gcmd_block, 0, 2, 1, 1);

        Self {
            scrolled_window,
            honor_expect_uris,
            viewer,
            use_internal_viewer,
            editor,
            differ,
            search,
            use_internal_search,
            send_to,
            termopen,
            fav_apps,
            termexec,
            use_gcmd_block,
        }
    }

    pub fn widget(&self) -> gtk::Widget {
        self.scrolled_window.clone().upcast()
    }

    pub fn read(&self, general_options: &GeneralOptions, programs_options: &ProgramsOptions) {
        self.honor_expect_uris
            .set_active(!programs_options.dont_download.get());
        self.viewer.set_text(&programs_options.viewer_cmd.get());
        self.use_internal_viewer
            .set_active(programs_options.use_internal_viewer.get());
        self.editor.set_text(&programs_options.editor_cmd.get());
        self.differ.set_text(&programs_options.differ_cmd.get());
        self.search.set_text(&programs_options.search_cmd.get());
        self.use_internal_search
            .set_active(programs_options.use_internal_search.get());
        self.send_to.set_text(&programs_options.sendto_cmd.get());
        self.termopen.set_text(&programs_options.terminal_cmd.get());
        self.fav_apps.read(general_options);
        self.termexec
            .set_text(&programs_options.terminal_exec_cmd.get());
        self.use_gcmd_block
            .set_active(programs_options.use_gcmd_block.get());
    }

    pub fn write(
        &self,
        general_options: &GeneralOptions,
        programs_options: &ProgramsOptions,
    ) -> WriteResult {
        programs_options
            .dont_download
            .set(!self.honor_expect_uris.is_active())?;
        programs_options.viewer_cmd.set(self.viewer.text())?;
        programs_options
            .use_internal_viewer
            .set(self.use_internal_viewer.is_active())?;
        programs_options.editor_cmd.set(self.editor.text())?;
        programs_options.differ_cmd.set(self.differ.text())?;
        programs_options.search_cmd.set(self.search.text())?;
        programs_options
            .use_internal_search
            .set(self.use_internal_search.is_active())?;
        programs_options.sendto_cmd.set(self.send_to.text())?;
        programs_options.terminal_cmd.set(self.termopen.text())?;
        self.fav_apps.write(general_options)?;
        programs_options
            .terminal_exec_cmd
            .set(self.termexec.text())?;
        programs_options
            .use_gcmd_block
            .set(self.use_gcmd_block.is_active())?;
        Ok(())
    }
}

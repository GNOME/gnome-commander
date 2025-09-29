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

use super::common::create_category;
use crate::{
    dialogs::options::common::{radio_group_get_value, radio_group_set_value},
    options::{options::GeneralOptions, types::WriteResult},
    types::{LeftMouseButtonMode, MiddleMouseButtonMode, QuickSearchShortcut},
};
use gettextrs::gettext;
use gtk::prelude::*;

pub struct GeneralTab {
    scrolled_window: gtk::ScrolledWindow,
    lmb_single_click: gtk::CheckButton,
    lmb_double_click: gtk::CheckButton,
    lmb_unselects: gtk::CheckButton,
    mmb_cd_up: gtk::CheckButton,
    mmb_new_tab: gtk::CheckButton,
    delete_to_trash: gtk::CheckButton,
    select_dirs: gtk::CheckButton,
    case_sensitive: gtk::CheckButton,
    symbolic_links_as_regular_files: gtk::CheckButton,
    quick_search_ctrl_alt: gtk::CheckButton,
    quick_search_alt: gtk::CheckButton,
    quick_search_just_char: gtk::CheckButton,
    quick_search_exact_match_begin: gtk::CheckButton,
    quick_search_exact_match_end: gtk::CheckButton,
    search_window_is_minimizable: gtk::CheckButton,
    single_instance: gtk::CheckButton,
    save_dirs: gtk::CheckButton,
    save_tabs: gtk::CheckButton,
    save_dir_history: gtk::CheckButton,
    save_cmdline_history: gtk::CheckButton,
    save_search_history: gtk::CheckButton,
}

impl GeneralTab {
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

        let left_mouse_button = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        vbox.append(&create_category(
            &gettext("Left mouse button"),
            &left_mouse_button,
        ));
        let lmb_single_click = gtk::CheckButton::builder()
            .label(gettext("Single click to open items"))
            .build();
        left_mouse_button.append(&lmb_single_click);
        let lmb_double_click = gtk::CheckButton::builder()
            .label(gettext("Double click to open items"))
            .group(&lmb_single_click)
            .build();
        left_mouse_button.append(&lmb_double_click);
        let lmb_unselects = gtk::CheckButton::builder()
            .label(gettext("Single click unselects files"))
            .build();
        left_mouse_button.append(&lmb_unselects);

        let middle_mouse_button = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        vbox.append(&create_category(
            &gettext("Middle mouse button"),
            &middle_mouse_button,
        ));
        let mmb_cd_up = gtk::CheckButton::builder()
            .label(gettext("Up one directory"))
            .build();
        middle_mouse_button.append(&mmb_cd_up);
        let mmb_new_tab = gtk::CheckButton::builder()
            .label(gettext("Opens new tab"))
            .group(&mmb_cd_up)
            .build();
        middle_mouse_button.append(&mmb_new_tab);

        let deletion = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        vbox.append(&create_category(&gettext("Deletion"), &deletion));
        let delete_to_trash = gtk::CheckButton::builder()
            .label(gettext("Move to trash"))
            .build();
        deletion.append(&delete_to_trash);

        let selection = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        vbox.append(&create_category(&gettext("Selection"), &selection));
        let select_dirs = gtk::CheckButton::builder()
            .label(gettext("Select directories"))
            .build();
        selection.append(&select_dirs);

        let sorting = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        vbox.append(&create_category(&gettext("Sorting"), &sorting));
        let case_sensitive = gtk::CheckButton::builder()
            .label(gettext("Case sensitive"))
            .build();
        sorting.append(&case_sensitive);
        let symbolic_links_as_regular_files = gtk::CheckButton::builder()
            .label(gettext("Treat symbolic links as regular files"))
            .build();
        sorting.append(&symbolic_links_as_regular_files);

        let quick_search = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        vbox.append(&create_category(&gettext("Quick search"), &quick_search));
        let quick_search_ctrl_alt = gtk::CheckButton::builder()
            .label(gettext("CTRL+ALT+letters"))
            .build();
        quick_search.append(&quick_search_ctrl_alt);
        let quick_search_alt = gtk::CheckButton::builder()
            .label(gettext("ALT+letters (menu access with F12)"))
            .group(&quick_search_ctrl_alt)
            .build();
        quick_search.append(&quick_search_alt);
        let quick_search_just_char = gtk::CheckButton::builder()
            .label(gettext(
                "Just letters (command line access with CTRL+ALT+C)",
            ))
            .group(&quick_search_ctrl_alt)
            .build();
        quick_search.append(&quick_search_just_char);
        let quick_search_exact_match_begin = gtk::CheckButton::builder()
            .label(gettext("Match beginning of the file name"))
            .group(&quick_search_ctrl_alt)
            .build();
        quick_search.append(&quick_search_exact_match_begin);
        let quick_search_exact_match_end = gtk::CheckButton::builder()
            .label(gettext("Match end of the file name"))
            .group(&quick_search_ctrl_alt)
            .build();
        quick_search.append(&quick_search_exact_match_end);

        let search_window = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        vbox.append(&create_category(&gettext("Search Window"), &search_window));
        let search_window_is_minimizable = gtk::CheckButton::builder()
            .label(gettext(
                "Search window is minimizable\n(Needs program restart if altered)",
            ))
            .build();
        search_window.append(&search_window_is_minimizable);

        let mutiple_instances = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        vbox.append(&create_category(
            &gettext("Multiple instances"),
            &mutiple_instances,
        ));
        let single_instance = gtk::CheckButton::builder()
            .label(gettext("Don’t start a new instance"))
            .build();
        mutiple_instances.append(&single_instance);

        let save_on_exit = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .build();
        vbox.append(&create_category(&gettext("Save on exit"), &save_on_exit));
        let save_dirs = gtk::CheckButton::builder()
            .label(gettext("Directories"))
            .build();
        save_on_exit.append(&save_dirs);
        let save_tabs = gtk::CheckButton::builder().label(gettext("Tabs")).build();
        save_on_exit.append(&save_tabs);
        let save_dir_history = gtk::CheckButton::builder()
            .label(gettext("Directory history"))
            .build();
        save_on_exit.append(&save_dir_history);
        let save_cmdline_history = gtk::CheckButton::builder()
            .label(gettext("Commandline history"))
            .build();
        save_on_exit.append(&save_cmdline_history);
        let save_search_history = gtk::CheckButton::builder()
            .label(gettext("Search history"))
            .build();
        save_on_exit.append(&save_search_history);
        save_tabs
            .bind_property("active", &save_dirs, "sensitive")
            .invert_boolean()
            .sync_create()
            .build();

        Self {
            scrolled_window,
            lmb_single_click,
            lmb_double_click,
            lmb_unselects,
            mmb_cd_up,
            mmb_new_tab,
            delete_to_trash,
            select_dirs,
            case_sensitive,
            symbolic_links_as_regular_files,
            quick_search_ctrl_alt,
            quick_search_alt,
            quick_search_just_char,
            quick_search_exact_match_begin,
            quick_search_exact_match_end,
            search_window_is_minimizable,
            single_instance,
            save_dirs,
            save_tabs,
            save_dir_history,
            save_cmdline_history,
            save_search_history,
        }
    }

    pub fn widget(&self) -> gtk::Widget {
        self.scrolled_window.clone().upcast()
    }

    pub fn read(&self, options: &GeneralOptions) {
        radio_group_set_value(
            [
                (
                    &self.lmb_single_click,
                    LeftMouseButtonMode::OpensWithSingleClick,
                ),
                (
                    &self.lmb_double_click,
                    LeftMouseButtonMode::OpensWithDoubleClick,
                ),
            ],
            options.left_mouse_button_mode.get(),
        );
        self.lmb_unselects
            .set_active(options.left_mouse_button_unselects.get());
        radio_group_set_value(
            [
                (&self.mmb_cd_up, MiddleMouseButtonMode::GoesUpDir),
                (&self.mmb_new_tab, MiddleMouseButtonMode::OpensNewTab),
            ],
            options.middle_mouse_button_mode.get(),
        );
        self.delete_to_trash.set_active(options.use_trash.get());
        self.select_dirs.set_active(options.select_dirs.get());
        self.case_sensitive.set_active(options.case_sensitive.get());
        self.symbolic_links_as_regular_files
            .set_active(options.symbolic_links_as_regular_files.get());
        radio_group_set_value(
            [
                (&self.quick_search_ctrl_alt, QuickSearchShortcut::CtrlAlt),
                (&self.quick_search_alt, QuickSearchShortcut::Alt),
                (
                    &self.quick_search_just_char,
                    QuickSearchShortcut::JustACharacter,
                ),
            ],
            options.quick_search_shortcut.get(),
        );
        self.quick_search_exact_match_begin
            .set_active(options.quick_search_exact_match_begin.get());
        self.quick_search_exact_match_end
            .set_active(options.quick_search_exact_match_end.get());
        self.search_window_is_minimizable
            .set_active(!options.search_window_is_transient.get());
        self.single_instance
            .set_active(!options.allow_multiple_instances.get());
        self.save_dirs.set_active(options.save_dirs_on_exit.get());
        self.save_tabs.set_active(options.save_tabs_on_exit.get());
        self.save_dir_history
            .set_active(options.save_directory_history_on_exit.get());
        self.save_cmdline_history
            .set_active(options.save_command_line_history_on_exit.get());
        self.save_search_history
            .set_active(options.save_search_history.get());
    }

    pub fn write(&self, options: &GeneralOptions) -> WriteResult {
        options.left_mouse_button_mode.set(radio_group_get_value([
            (
                &self.lmb_single_click,
                LeftMouseButtonMode::OpensWithSingleClick,
            ),
            (
                &self.lmb_double_click,
                LeftMouseButtonMode::OpensWithDoubleClick,
            ),
        ]))?;
        options
            .left_mouse_button_unselects
            .set(self.lmb_unselects.is_active())?;
        options
            .middle_mouse_button_mode
            .set(radio_group_get_value([
                (&self.mmb_cd_up, MiddleMouseButtonMode::GoesUpDir),
                (&self.mmb_new_tab, MiddleMouseButtonMode::OpensNewTab),
            ]))?;
        options.use_trash.set(self.delete_to_trash.is_active())?;
        options.select_dirs.set(self.select_dirs.is_active())?;
        options
            .case_sensitive
            .set(self.case_sensitive.is_active())?;
        options
            .symbolic_links_as_regular_files
            .set(self.symbolic_links_as_regular_files.is_active())?;
        options.quick_search_shortcut.set(radio_group_get_value([
            (&self.quick_search_ctrl_alt, QuickSearchShortcut::CtrlAlt),
            (&self.quick_search_alt, QuickSearchShortcut::Alt),
            (
                &self.quick_search_just_char,
                QuickSearchShortcut::JustACharacter,
            ),
        ]))?;
        options
            .quick_search_exact_match_begin
            .set(self.quick_search_exact_match_begin.is_active())?;
        options
            .quick_search_exact_match_end
            .set(self.quick_search_exact_match_end.is_active())?;
        options
            .search_window_is_transient
            .set(!self.search_window_is_minimizable.is_active())?;
        options
            .allow_multiple_instances
            .set(!self.single_instance.is_active())?;
        options.save_dirs_on_exit.set(self.save_dirs.is_active())?;
        options.save_tabs_on_exit.set(self.save_tabs.is_active())?;
        options
            .save_directory_history_on_exit
            .set(self.save_dir_history.is_active())?;
        options
            .save_command_line_history_on_exit
            .set(self.save_cmdline_history.is_active())?;
        options
            .save_search_history
            .set(self.save_search_history.is_active())?;
        Ok(())
    }
}

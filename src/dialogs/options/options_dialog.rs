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

use super::{
    confirmation_tab::CondifrmationTab, devices_tab::DevicesTab, filters_tab::FiltersTab,
    format_tab::FormatTab, general_tab::GeneralTab, layout_tab::LayoutTab,
    programs_tab::ProgramsTab, tabs_tab::TabsTab,
};
use crate::{
    options::{
        options::{ColorOptions, ConfirmOptions, FiltersOptions, GeneralOptions, ProgramsOptions},
        utils::remember_window_size,
    },
    utils::{SenderExt, dialog_button_box, display_help},
};
use gettextrs::gettext;
use gtk::{glib, prelude::*};
use std::sync::Mutex;

pub async fn show_options_dialog(parent_window: &impl IsA<gtk::Window>) -> bool {
    let dialog = gtk::Window::builder()
        .title(gettext("Options"))
        .transient_for(parent_window)
        .modal(true)
        .build();

    let content_area = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .margin_top(10)
        .margin_bottom(10)
        .margin_start(10)
        .margin_end(10)
        .spacing(6)
        .build();
    dialog.set_child(Some(&content_area));

    let general_options = GeneralOptions::new();
    let color_options = ColorOptions::new();
    let confirmation_options = ConfirmOptions::new();
    let filters_options = FiltersOptions::new();
    let programs_options = ProgramsOptions::new();

    remember_window_size(
        &dialog,
        &general_options.options_window_width,
        &general_options.options_window_height,
    );

    let notebook = gtk::Notebook::builder().hexpand(true).vexpand(true).build();
    content_area.append(&notebook);

    let general_tab = GeneralTab::new();
    general_tab.read(&general_options);
    let format_tab = FormatTab::new();
    format_tab.read(&general_options);
    let layout_tab = LayoutTab::new();
    layout_tab.read(&general_options, &color_options);
    let tabs_tab = TabsTab::new();
    tabs_tab.read(&general_options);
    let confirmation_tab = CondifrmationTab::new();
    confirmation_tab.read(&confirmation_options);
    let filters_tab = FiltersTab::new();
    filters_tab.read(&filters_options);
    let programs_tab = ProgramsTab::new();
    programs_tab.read(&general_options, &programs_options);
    let devices_tab = DevicesTab::new();
    devices_tab.read(&general_options);

    notebook.append_page(
        &general_tab.widget(),
        Some(&gtk::Label::builder().label(gettext("General")).build()),
    );
    notebook.append_page(
        &format_tab.widget(),
        Some(&gtk::Label::builder().label(gettext("Format")).build()),
    );
    notebook.append_page(
        &layout_tab.widget(),
        Some(&gtk::Label::builder().label(gettext("Layout")).build()),
    );
    notebook.append_page(
        &tabs_tab.widget(),
        Some(&gtk::Label::builder().label(gettext("Tabs")).build()),
    );
    notebook.append_page(
        &confirmation_tab.widget(),
        Some(&gtk::Label::builder().label(gettext("Confirmation")).build()),
    );
    notebook.append_page(
        &filters_tab.widget(),
        Some(&gtk::Label::builder().label(gettext("Filters")).build()),
    );
    notebook.append_page(
        &programs_tab.widget(),
        Some(&gtk::Label::builder().label(gettext("Programs")).build()),
    );
    notebook.append_page(
        &devices_tab.widget(),
        Some(&gtk::Label::builder().label(gettext("Devices")).build()),
    );

    let help_button = gtk::Button::builder()
        .label(gettext("_Help"))
        .use_underline(true)
        .build();
    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .build();

    content_area.append(&dialog_button_box(
        &[&help_button],
        &[&cancel_button, &ok_button],
    ));
    dialog.set_default_widget(Some(&ok_button));

    help_button.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        #[weak]
        notebook,
        move |_| {
            let link_id = match notebook.current_page() {
                Some(0) => Some("gnome-commander-prefs-general"),
                Some(1) => Some("gnome-commander-prefs-format"),
                Some(2) => Some("gnome-commander-prefs-layout"),
                Some(3) => Some("gnome-commander-prefs-tabs"),
                Some(4) => Some("gnome-commander-prefs-confirmation"),
                Some(5) => Some("gnome-commander-prefs-filters"),
                Some(6) => Some("gnome-commander-prefs-programs"),
                Some(7) => Some("gnome-commander-prefs-devices"),
                _ => None,
            };
            glib::spawn_future_local(async move {
                display_help(dialog.upcast_ref(), link_id).await;
            });
        }
    ));

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    dialog.connect_close_request(glib::clone!(
        #[strong]
        sender,
        move |_| {
            sender.toss(false);
            glib::Propagation::Proceed
        }
    ));
    cancel_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(false)
    ));
    ok_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(true)
    ));

    static LAST_ACTIVE_TAB: Mutex<u32> = Mutex::new(0);

    // open the tab which was active when closing the options notebook last time
    notebook.set_current_page(LAST_ACTIVE_TAB.lock().map(|g| *g).ok());

    dialog.present();

    let result = receiver.recv().await == Ok(true);

    if result {
        if let Err(error) = general_tab.write(&general_options) {
            eprintln!("{error}");
        }
        if let Err(error) = format_tab.write(&general_options) {
            eprintln!("{error}");
        }
        if let Err(error) = layout_tab.write(&general_options, &color_options) {
            eprintln!("{error}");
        }
        if let Err(error) = tabs_tab.write(&general_options) {
            eprintln!("{error}");
        }
        if let Err(error) = confirmation_tab.write(&confirmation_options) {
            eprintln!("{error}");
        }
        if let Err(error) = filters_tab.write(&filters_options) {
            eprintln!("{error}");
        }
        if let Err(error) = programs_tab.write(&general_options, &programs_options) {
            eprintln!("{error}");
        }
        if let Err(error) = devices_tab.write(&general_options) {
            eprintln!("{error}");
        }
        if let Ok(mut last_active_tab) = LAST_ACTIVE_TAB.lock() {
            *last_active_tab = notebook.current_page().unwrap_or_default();
        }
    }

    dialog.close();

    result
}

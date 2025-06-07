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

use super::{devices_tab::DevicesTab, tabs_tab::TabsTab};
use crate::{
    data::GeneralOptions,
    main_win::ffi::GnomeCmdMainWin,
    utils::{dialog_button_box, display_help, SenderExt},
};
use gettextrs::gettext;
use gtk::{
    ffi::{GtkWidget, GtkWindow},
    gio,
    glib::{
        self,
        translate::{from_glib_none, ToGlibPtr},
    },
    prelude::*,
};
use std::{ffi::c_void, sync::Mutex};

extern "C" {
    fn create_general_tab(dialog: *mut GtkWindow, cfg: *mut c_void) -> *mut GtkWidget;
    fn create_format_tab(dialog: *mut GtkWindow, cfg: *mut c_void) -> *mut GtkWidget;
    fn create_layout_tab(dialog: *mut GtkWindow, cfg: *mut c_void) -> *mut GtkWidget;
    fn create_confirmation_tab(dialog: *mut GtkWindow, cfg: *mut c_void) -> *mut GtkWidget;
    fn create_filter_tab(dialog: *mut GtkWindow, cfg: *mut c_void) -> *mut GtkWidget;
    fn create_programs_tab(dialog: *mut GtkWindow, cfg: *mut c_void) -> *mut GtkWidget;

    fn store_general_options(dialog: *mut GtkWindow, cfg: *mut c_void);
    fn store_format_options(dialog: *mut GtkWindow, cfg: *mut c_void);
    fn store_layout_options(dialog: *mut GtkWindow, cfg: *mut c_void);
    fn store_confirmation_options(dialog: *mut GtkWindow, cfg: *mut c_void);
    fn store_filter_options(dialog: *mut GtkWindow, cfg: *mut c_void);
    fn store_programs_options(dialog: *mut GtkWindow, cfg: *mut c_void);

    fn gnome_cmd_data_options() -> *mut c_void;
    fn gnome_cmd_data_save(mw: *mut GnomeCmdMainWin);
}

pub async fn show_options_dialog(parent_window: &impl IsA<gtk::Window>) -> bool {
    let cfg = unsafe { gnome_cmd_data_options() };

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
    remember_window_size(&dialog, &general_options.0);

    let notebook = gtk::Notebook::builder().hexpand(true).vexpand(true).build();
    content_area.append(&notebook);

    let general_tab: gtk::Widget =
        unsafe { from_glib_none(create_general_tab(dialog.to_glib_none().0, cfg)) };
    let format_tab: gtk::Widget =
        unsafe { from_glib_none(create_format_tab(dialog.to_glib_none().0, cfg)) };
    let layout_tab: gtk::Widget =
        unsafe { from_glib_none(create_layout_tab(dialog.to_glib_none().0, cfg)) };
    let tabs_tab = TabsTab::new();
    tabs_tab.read(&general_options);
    let confirmation_tab: gtk::Widget =
        unsafe { from_glib_none(create_confirmation_tab(dialog.to_glib_none().0, cfg)) };
    let filter_tab: gtk::Widget =
        unsafe { from_glib_none(create_filter_tab(dialog.to_glib_none().0, cfg)) };
    let programs_tab: gtk::Widget =
        unsafe { from_glib_none(create_programs_tab(dialog.to_glib_none().0, cfg)) };
    let devices_tab = DevicesTab::new();
    devices_tab.read(&general_options);

    notebook.append_page(
        &general_tab,
        Some(&gtk::Label::builder().label(gettext("General")).build()),
    );
    notebook.append_page(
        &format_tab,
        Some(&gtk::Label::builder().label(gettext("Format")).build()),
    );
    notebook.append_page(
        &layout_tab,
        Some(&gtk::Label::builder().label(gettext("Layout")).build()),
    );
    notebook.append_page(
        &tabs_tab.widget(),
        Some(&gtk::Label::builder().label(gettext("Tabs")).build()),
    );
    notebook.append_page(
        &confirmation_tab,
        Some(&gtk::Label::builder().label(gettext("Confirmation")).build()),
    );
    notebook.append_page(
        &filter_tab,
        Some(&gtk::Label::builder().label(gettext("Filters")).build()),
    );
    notebook.append_page(
        &programs_tab,
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
        unsafe {
            store_general_options(dialog.to_glib_none().0, cfg);
            store_format_options(dialog.to_glib_none().0, cfg);
            store_layout_options(dialog.to_glib_none().0, cfg);
            if let Err(error) = tabs_tab.write(&general_options) {
                eprintln!("{error}");
            }
            store_confirmation_options(dialog.to_glib_none().0, cfg);
            store_filter_options(dialog.to_glib_none().0, cfg);
            store_programs_options(dialog.to_glib_none().0, cfg);
            if let Err(error) = devices_tab.write(&general_options) {
                eprintln!("{error}");
            }
            gnome_cmd_data_save(std::ptr::null_mut());
            if let Ok(mut last_active_tab) = LAST_ACTIVE_TAB.lock() {
                *last_active_tab = notebook.current_page().unwrap_or_default();
            }
        }
    }

    dialog.close();

    result
}

fn remember_window_size(dialog: &gtk::Window, settings: &gio::Settings) {
    settings
        .bind("opts-dialog-width", &*dialog, "default-width")
        .mapping(|v, _| {
            let width: i32 = v.get::<u32>()?.try_into().ok()?;
            Some(width.to_value())
        })
        .set_mapping(|v, _| {
            let width: u32 = v.get::<i32>().ok()?.try_into().ok()?;
            Some(width.to_variant())
        })
        .build();

    settings
        .bind("opts-dialog-height", &*dialog, "default-height")
        .mapping(|v, _| {
            let height: i32 = v.get::<u32>()?.try_into().ok()?;
            Some(height.to_value())
        })
        .set_mapping(|v, _| {
            let height: u32 = v.get::<i32>().ok()?.try_into().ok()?;
            Some(height.to_variant())
        })
        .build();
}

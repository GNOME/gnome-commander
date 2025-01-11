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

use super::{
    search_progress_dialog::SearchProgressDialog,
    searcher::{SearchProgress, Searcher},
    text_render::TextRender,
};
use crate::{
    file::File,
    utils::{extract_menu_shortcuts, pending, MenuBuilderExt},
};
use gettextrs::gettext;
use gtk::{
    ffi::GtkPopoverMenuBar,
    gio,
    glib::{self, ffi::gboolean, translate::*},
    prelude::*,
};
use std::{ffi::c_char, ptr};

pub mod ffi {
    use crate::{
        file::ffi::GnomeCmdFile,
        intviewer::{searcher::ffi::GViewerSearcher, text_render::ffi::TextRender},
    };
    use gtk::{ffi::GtkWindowClass, glib::ffi::GType};
    use std::ffi::c_void;

    #[repr(C)]
    pub struct GViewerWindow {
        _data: [u8; 0],
        _marker: std::marker::PhantomData<(*mut u8, std::marker::PhantomPinned)>,
    }

    extern "C" {
        pub fn gviewer_window_get_type() -> GType;

        pub fn gviewer_window_file_view(
            f: *mut GnomeCmdFile,
            initial_settings: /* GViewerWindowSettings */ *mut c_void,
        ) -> *mut GViewerWindow;

        pub fn gviewer_window_get_searcher(w: *mut GViewerWindow) -> *mut GViewerSearcher;
        pub fn gviewer_window_get_text_render(w: *mut GViewerWindow) -> *mut TextRender;
    }

    #[derive(Copy, Clone)]
    #[repr(C)]
    pub struct GViewerWindowClass {
        pub parent_class: GtkWindowClass,
    }
}

glib::wrapper! {
    pub struct ViewerWindow(Object<ffi::GViewerWindow, ffi::GViewerWindowClass>)
        @extends gtk::Window, gtk::Widget;

    match fn {
        type_ => || ffi::gviewer_window_get_type(),
    }
}

impl ViewerWindow {
    pub fn file_view(file: &File) -> Self {
        unsafe {
            from_glib_none(ffi::gviewer_window_file_view(
                file.to_glib_none().0,
                ptr::null_mut(),
            ))
        }
    }

    fn searcher(&self) -> Searcher {
        unsafe { from_glib_none(ffi::gviewer_window_get_searcher(self.to_glib_none().0)) }
    }

    fn text_render(&self) -> Option<TextRender> {
        unsafe { from_glib_none(ffi::gviewer_window_get_text_render(self.to_glib_none().0)) }
    }
}

async fn say_not_found(parent: &gtk::Window, search_pattern: &str) {
    let _answer = gtk::AlertDialog::builder()
        .modal(true)
        .message(gettext("Pattern “{}” was not found").replace("{}", search_pattern))
        .buttons([gettext("_OK")])
        .cancel_button(0)
        .default_button(0)
        .build()
        .choose_future(Some(parent))
        .await;
}

async fn start_search(
    window: &ViewerWindow,
    search_pattern: &str,
    search_pattern_len: i32,
    forward: bool,
) {
    let searcher = window.searcher();

    let (sender, receiver) = async_channel::bounded::<SearchProgress>(1);

    let thread = std::thread::spawn(glib::clone!(
        #[strong]
        searcher,
        move || {
            let sender1 = sender.clone();
            let found = searcher.search(forward, move |p| {
                sender1.send_blocking(SearchProgress::Progress(p)).unwrap()
            });
            sender.send_blocking(SearchProgress::Done(found)).unwrap();
        }
    ));

    let progress_dlg = SearchProgressDialog::new(window.upcast_ref(), search_pattern);
    progress_dlg.connect_stop(glib::clone!(
        #[weak]
        searcher,
        move || searcher.abort()
    ));

    progress_dlg.present();
    let found = loop {
        let message = receiver.recv().await.unwrap();
        match message {
            SearchProgress::Progress(progress) => progress_dlg.set_progress(progress),
            SearchProgress::Done(found) => break found,
        }
    };
    thread.join().unwrap();
    progress_dlg.close();

    pending().await;

    match found {
        Some(result) => {
            let text_render = window.text_render().unwrap();
            text_render.set_marker(
                result,
                if forward {
                    result + search_pattern_len as u64
                } else {
                    result - search_pattern_len as u64
                },
            );
            text_render.ensure_offset_visible(result);
        }
        None => {
            say_not_found(window.upcast_ref(), search_pattern).await;
        }
    }
}

#[no_mangle]
pub extern "C" fn start_search_r(
    window_ptr: *mut ffi::GViewerWindow,
    search_pattern_ptr: *const c_char,
    search_pattern_len: i32,
    forward: gboolean,
) {
    let window: ViewerWindow = unsafe { from_glib_none(window_ptr) };
    let search_pattern: String = unsafe { from_glib_none(search_pattern_ptr) };
    let forward: bool = forward != 0;

    glib::spawn_future_local(async move {
        start_search(&window, &search_pattern, search_pattern_len, forward).await
    });
}

const ROTATE_90_STOCKID: &str = "gnome-commander-rotate-90";
const ROTATE_270_STOCKID: &str = "gnome-commander-rotate-270";
const ROTATE_180_STOCKID: &str = "gnome-commander-rotate-180";
const FLIP_VERTICAL_STOCKID: &str = "gnome-commander-flip-vertical";
const FLIP_HORIZONTAL_STOCKID: &str = "gnome-commander-flip-horizontal";

fn create_menu(window: &ViewerWindow) -> gtk::PopoverMenuBar {
    let menu = gio::Menu::new()
        .submenu(
            gettext("_File"),
            gio::Menu::new().item_accel(gettext("_Close"), "viewer.close", "Escape"),
        )
        .submenu(
            gettext("_View"),
            gio::Menu::new()
                .item_accel(gettext("_Text"), "viewer.view-mode(0)", "1")
                .item_accel(gettext("_Binary"), "viewer.view-mode(1)", "2")
                .item_accel(gettext("_Hexadecimal"), "viewer.view-mode(2)", "3")
                .item_accel(gettext("_Image"), "viewer.view-mode(3)", "4")
                .section(
                    gio::Menu::new()
                        .item_accel_and_icon(
                            gettext("_Zoom In"),
                            "viewer.zoom-in",
                            "<Control>plus",
                            "zoom-in",
                        )
                        .item_accel_and_icon(
                            gettext("_Zoom Out"),
                            "viewer.zoom-out",
                            "<Control>minus",
                            "zoom-out",
                        )
                        .item_accel_and_icon(
                            gettext("_Normal Size"),
                            "viewer.normal-size",
                            "<Control>0",
                            "zoom-original",
                        )
                        .item_accel_and_icon(
                            gettext("_Best Fit"),
                            "viewer.best-fit",
                            "<Control>period",
                            "zoom-fit-best",
                        ),
                ),
        )
        .submenu(
            gettext("_Text"),
            gio::Menu::new()
                .item_accel(
                    gettext("_Copy Text Selection"),
                    "viewer.copy-text-selection",
                    "<Control>C",
                )
                .item_accel(gettext("Find…"), "viewer.find", "<Control>F")
                .item_accel(gettext("Find Next"), "viewer.find-next", "F3")
                .item_accel(
                    gettext("Find Previous"),
                    "viewer.find-previous",
                    "<Shift>F3",
                )
                .section(gio::Menu::new().item_accel(
                    gettext("_Wrap lines"),
                    "viewer.wrap-lines",
                    "<Control>W",
                ))
                .submenu(
                    gettext("_Encoding"),
                    gio::Menu::new()
                        .item_accel(gettext("_UTF-8"), "viewer.encoding(0)", "U")
                        .item_accel(gettext("English (US-_ASCII)"), "viewer.encoding(1)", "A")
                        .item_accel(gettext("Terminal (CP437)"), "viewer.encoding(2)", "Q")
                        .item(gettext("Arabic (ISO-8859-6)"), "viewer.encoding(3)")
                        .item(gettext("Arabic (Windows, CP1256)"), "viewer.encoding(4)")
                        .item(gettext("Arabic (Dos, CP864)"), "viewer.encoding(5)")
                        .item(gettext("Baltic (ISO-8859-4)"), "viewer.encoding(6)")
                        .item(
                            gettext("Central European (ISO-8859-2)"),
                            "viewer.encoding(7)",
                        )
                        .item(gettext("Central European (CP1250)"), "viewer.encoding(8)")
                        .item(gettext("Cyrillic (ISO-8859-5)"), "viewer.encoding(9)")
                        .item(gettext("Cyrillic (CP1251)"), "viewer.encoding(10)")
                        .item(gettext("Greek (ISO-8859-7)"), "viewer.encoding(11)")
                        .item(gettext("Greek (CP1253)"), "viewer.encoding(12)")
                        .item(gettext("Hebrew (Windows, CP1255)"), "viewer.encoding(13)")
                        .item(gettext("Hebrew (Dos, CP862)"), "viewer.encoding(14)")
                        .item(gettext("Hebrew (ISO-8859-8)"), "viewer.encoding(15)")
                        .item(gettext("Latin 9 (ISO-8859-15)"), "viewer.encoding(16)")
                        .item(gettext("Maltese (ISO-8859-3)"), "viewer.encoding(17)")
                        .item(gettext("Turkish (ISO-8859-9)"), "viewer.encoding(18)")
                        .item(gettext("Turkish (CP1254)"), "viewer.encoding(19)")
                        .item(gettext("Western (CP1252)"), "viewer.encoding(20)")
                        .item(gettext("Western (ISO-8859-1)"), "viewer.encoding(21)"),
                ),
        )
        .submenu(
            gettext("_Image"),
            gio::Menu::new()
                .item_accel_and_icon(
                    gettext("Rotate Clockwise"),
                    "viewer.imageop(0)",
                    "<Control>R",
                    ROTATE_90_STOCKID,
                )
                .item_icon(
                    gettext("Rotate Counter Clockwis_e"),
                    "viewer.imageop(1)",
                    ROTATE_270_STOCKID,
                )
                .item_accel_and_icon(
                    gettext("Rotate 180\u{00B0}"),
                    "viewer.imageop(2)",
                    "<Control><Shift>R",
                    ROTATE_180_STOCKID,
                )
                .item_icon(
                    gettext("Flip _Vertical"),
                    "viewer.imageop(3)",
                    FLIP_VERTICAL_STOCKID,
                )
                .item_icon(
                    gettext("Flip _Horizontal"),
                    "viewer.imageop(4)",
                    FLIP_HORIZONTAL_STOCKID,
                ),
        )
        .submenu(
            gettext("_Settings"),
            gio::Menu::new()
                .submenu(
                    gettext("_Binary Mode"),
                    gio::Menu::new()
                        .item_accel(
                            gettext("_20 chars/line"),
                            "viewer.chars-per-line(20)",
                            "<Control>2",
                        )
                        .item_accel(
                            gettext("_40 chars/line"),
                            "viewer.chars-per-line(40)",
                            "<Control>4",
                        )
                        .item_accel(
                            gettext("_80 chars/line"),
                            "viewer.chars-per-line(80)",
                            "<Control>8",
                        ),
                )
                .section(
                    gio::Menu::new()
                        .item_accel(
                            gettext("Show Metadata _Tags"),
                            "viewer.show-metadata-tags",
                            "T",
                        )
                        .item_accel(
                            gettext("_Hexadecimal Offset"),
                            "viewer.hexadecimal-offset",
                            "<Control>D",
                        ),
                )
                .item_accel(
                    gettext("_Save Current Settings"),
                    "viewer.save-current-settings",
                    "<Control>S",
                ),
        )
        .submenu(
            gettext("_Help"),
            gio::Menu::new()
                .item_accel(gettext("Quick _Help"), "viewer.quick-help", "F1")
                .item(gettext("_Keyboard Shortcuts"), "viewer.keyboard-shortcuts"),
        );

    let shortcuts = extract_menu_shortcuts(menu.upcast_ref());
    let shortcuts_controller = gtk::ShortcutController::for_model(&shortcuts);
    window.add_controller(shortcuts_controller);

    gtk::PopoverMenuBar::from_model(Some(&menu))
}

#[no_mangle]
pub extern "C" fn gviewer_window_create_menus(
    window: *mut ffi::GViewerWindow,
) -> *mut GtkPopoverMenuBar {
    let window: ViewerWindow = unsafe { from_glib_none(window) };
    create_menu(&window).to_glib_full()
}

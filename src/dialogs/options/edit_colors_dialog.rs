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

use crate::{layout::color_themes::ColorTheme, utils::SenderExt};
use gettextrs::gettext;
use gtk::prelude::*;

pub async fn edit_colors(parent_window: &gtk::Window, theme: ColorTheme) -> Option<ColorTheme> {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .modal(true)
        .title(gettext("Edit Colors…"))
        .resizable(false)
        .build();

    let grid = gtk::Grid::builder()
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .row_spacing(6)
        .column_spacing(12)
        .build();
    dialog.set_child(Some(&grid));

    let color_dialog = gtk::ColorDialog::builder().modal(true).build();

    let default_fg = gtk::ColorDialogButton::builder()
        .halign(gtk::Align::Center)
        .rgba(&theme.norm_fg)
        .dialog(&color_dialog)
        .build();
    grid.attach(&default_fg, 1, 1, 1, 1);

    let default_bg = gtk::ColorDialogButton::builder()
        .halign(gtk::Align::Center)
        .rgba(&theme.norm_bg)
        .dialog(&color_dialog)
        .build();
    grid.attach(&default_bg, 2, 1, 1, 1);

    let alternate_fg = gtk::ColorDialogButton::builder()
        .halign(gtk::Align::Center)
        .rgba(&theme.alt_fg)
        .dialog(&color_dialog)
        .build();
    grid.attach(&alternate_fg, 1, 2, 1, 1);

    let alternate_bg = gtk::ColorDialogButton::builder()
        .halign(gtk::Align::Center)
        .rgba(&theme.alt_bg)
        .dialog(&color_dialog)
        .build();
    grid.attach(&alternate_bg, 2, 2, 1, 1);

    let selected_fg = gtk::ColorDialogButton::builder()
        .halign(gtk::Align::Center)
        .rgba(&theme.sel_fg)
        .dialog(&color_dialog)
        .build();
    grid.attach(&selected_fg, 1, 3, 1, 1);

    let selected_bg = gtk::ColorDialogButton::builder()
        .halign(gtk::Align::Center)
        .rgba(&theme.sel_bg)
        .dialog(&color_dialog)
        .build();
    grid.attach(&selected_bg, 2, 3, 1, 1);

    let cursor_fg = gtk::ColorDialogButton::builder()
        .halign(gtk::Align::Center)
        .rgba(&theme.curs_fg)
        .dialog(&color_dialog)
        .build();
    grid.attach(&cursor_fg, 1, 4, 1, 1);

    let cursor_bg = gtk::ColorDialogButton::builder()
        .halign(gtk::Align::Center)
        .rgba(&theme.curs_bg)
        .dialog(&color_dialog)
        .build();
    grid.attach(&cursor_bg, 2, 4, 1, 1);

    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Foreground"))
            .hexpand(true)
            .halign(gtk::Align::Center)
            .build(),
        1,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Background"))
            .hexpand(true)
            .halign(gtk::Align::Center)
            .build(),
        2,
        0,
        1,
        1,
    );

    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Default:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        1,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Alternate:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        2,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Selected file:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        3,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Cursor:"))
            .halign(gtk::Align::Start)
            .build(),
        0,
        4,
        1,
        1,
    );

    let close_button = gtk::Button::builder()
        .label(gettext("_Close"))
        .use_underline(true)
        .hexpand(true)
        .halign(gtk::Align::Center)
        .build();
    grid.attach(&close_button, 0, 5, 3, 1);

    let (sender, receiver) = async_channel::bounded::<()>(1);
    close_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| sender.toss(())
    ));
    dialog.connect_close_request(glib::clone!(
        #[strong]
        sender,
        move |_| {
            sender.toss(());
            glib::Propagation::Proceed
        }
    ));

    dialog.present();
    let ok = receiver.recv().await == Ok(());

    let theme = ok.then(|| ColorTheme {
        norm_fg: default_fg.rgba(),
        norm_bg: default_bg.rgba(),
        alt_fg: alternate_fg.rgba(),
        alt_bg: alternate_bg.rgba(),
        sel_fg: selected_fg.rgba(),
        sel_bg: selected_bg.rgba(),
        curs_fg: cursor_fg.rgba(),
        curs_bg: cursor_bg.rgba(),
    });

    dialog.close();

    theme
}

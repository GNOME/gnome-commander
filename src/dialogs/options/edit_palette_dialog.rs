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

use crate::{
    layout::{
        ls_colors::{LsPalletteColor, LsPallettePlane},
        ls_colors_palette::LsColorsPalette,
    },
    utils::{SenderExt, dialog_button_box},
};
use gettextrs::gettext;
use gtk::prelude::*;
use std::collections::HashMap;
use strum::{EnumCount, VariantArray};

pub async fn edit_palette(
    parent_window: &gtk::Window,
    palette: LsColorsPalette,
) -> Option<LsColorsPalette> {
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .modal(true)
        .title(gettext("Edit LS_COLORS Palette"))
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

    let mut buttons: HashMap<(LsPallettePlane, LsPalletteColor), gtk::ColorDialogButton> =
        HashMap::new();
    for plane in LsPallettePlane::VARIANTS {
        for palette_color in LsPalletteColor::VARIANTS {
            let btn = gtk::ColorDialogButton::builder()
                .halign(gtk::Align::Center)
                .rgba(palette.color(*plane, *palette_color))
                .dialog(&color_dialog)
                .build();
            grid.attach(&btn, *palette_color as i32 + 1, *plane as i32 + 1, 1, 1);
            buttons.insert((*plane, *palette_color), btn);
        }
    }

    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Foreground"))
            .hexpand(true)
            .halign(gtk::Align::Start)
            .build(),
        0,
        1,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Background"))
            .hexpand(true)
            .halign(gtk::Align::Start)
            .build(),
        0,
        2,
        1,
        1,
    );

    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Black"))
            .halign(gtk::Align::Center)
            .build(),
        1,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Red"))
            .halign(gtk::Align::Center)
            .build(),
        2,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Green"))
            .halign(gtk::Align::Center)
            .build(),
        3,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Yellow"))
            .halign(gtk::Align::Center)
            .build(),
        4,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Blue"))
            .halign(gtk::Align::Center)
            .build(),
        5,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Magenta"))
            .halign(gtk::Align::Center)
            .build(),
        6,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("Cyan"))
            .halign(gtk::Align::Center)
            .build(),
        7,
        0,
        1,
        1,
    );
    grid.attach(
        &gtk::Label::builder()
            .label(gettext("White"))
            .halign(gtk::Align::Center)
            .build(),
        8,
        0,
        1,
        1,
    );

    let reset_button = gtk::Button::builder()
        .label(gettext("_Reset"))
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

    grid.attach(
        &dialog_button_box(&[&reset_button], &[&cancel_button, &ok_button]),
        0,
        LsPallettePlane::COUNT as i32 + 1,
        LsPalletteColor::COUNT as i32 + 1,
        1,
    );

    reset_button.connect_clicked(glib::clone!(
        #[strong]
        buttons,
        move |_| {
            let palette = LsColorsPalette::default();
            for plane in LsPallettePlane::VARIANTS {
                for palette_color in LsPalletteColor::VARIANTS {
                    buttons[&(*plane, *palette_color)]
                        .set_rgba(palette.color(*plane, *palette_color));
                }
            }
        }
    ));

    let (sender, receiver) = async_channel::bounded::<bool>(1);
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
    dialog.connect_close_request(glib::clone!(
        #[strong]
        sender,
        move |_| {
            sender.toss(false);
            glib::Propagation::Proceed
        }
    ));

    dialog.present();
    let result = receiver.recv().await;

    let palette = (result == Ok(true)).then(|| {
        let mut palette = LsColorsPalette::default();
        for plane in LsPallettePlane::VARIANTS {
            for palette_color in LsPalletteColor::VARIANTS {
                palette.set_color(
                    *plane,
                    *palette_color,
                    buttons[&(*plane, *palette_color)].rgba(),
                );
            }
        }
        palette
    });

    dialog.close();

    palette
}

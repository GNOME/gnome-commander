// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    layout::{
        ls_colors::{LsPalletteColor, LsPallettePlane},
        ls_colors_palette::LsColorsPalette,
    },
    utils::{SenderExt, WindowExt, dialog_button_box},
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
    dialog.add_css_class("dialog");

    let grid = gtk::Grid::builder().build();
    dialog.set_child(Some(&grid));

    let color_dialog = gtk::ColorDialog::builder().modal(true).build();

    let foreground_label = gtk::Label::builder()
        .label(gettext("Foreground"))
        .hexpand(true)
        .halign(gtk::Align::Start)
        .build();
    grid.attach(&foreground_label, 0, 1, 1, 1);

    let background_label = gtk::Label::builder()
        .label(gettext("Background"))
        .hexpand(true)
        .halign(gtk::Align::Start)
        .build();
    grid.attach(&background_label, 0, 2, 1, 1);

    let color_labels = [
        gtk::Label::builder()
            .label(gettext("Black"))
            .halign(gtk::Align::Center)
            .build(),
        gtk::Label::builder()
            .label(gettext("Red"))
            .halign(gtk::Align::Center)
            .build(),
        gtk::Label::builder()
            .label(gettext("Green"))
            .halign(gtk::Align::Center)
            .build(),
        gtk::Label::builder()
            .label(gettext("Yellow"))
            .halign(gtk::Align::Center)
            .build(),
        gtk::Label::builder()
            .label(gettext("Blue"))
            .halign(gtk::Align::Center)
            .build(),
        gtk::Label::builder()
            .label(gettext("Magenta"))
            .halign(gtk::Align::Center)
            .build(),
        gtk::Label::builder()
            .label(gettext("Cyan"))
            .halign(gtk::Align::Center)
            .build(),
        gtk::Label::builder()
            .label(gettext("White"))
            .halign(gtk::Align::Center)
            .build(),
    ];

    for (i, label) in color_labels.iter().enumerate() {
        grid.attach(label, i as i32 + 1, 0, 1, 1);
    }

    let mut buttons: HashMap<(LsPallettePlane, LsPalletteColor), gtk::ColorDialogButton> =
        HashMap::new();
    for plane in LsPallettePlane::VARIANTS {
        for palette_color in LsPalletteColor::VARIANTS {
            let btn = gtk::ColorDialogButton::builder()
                .halign(gtk::Align::Center)
                .rgba(palette.color(*plane, *palette_color))
                .dialog(&color_dialog)
                .build();
            if let Some(button) = btn.first_child() {
                button.update_relation(&[gtk::accessible::Relation::DescribedBy(&[
                    color_labels[*palette_color as usize].upcast_ref(),
                    if *plane == LsPallettePlane::Foreground {
                        foreground_label.upcast_ref()
                    } else {
                        background_label.upcast_ref()
                    },
                ])]);
            }
            grid.attach(&btn, *palette_color as i32 + 1, *plane as i32 + 1, 1, 1);
            buttons.insert((*plane, *palette_color), btn);
        }
    }

    let reset_button = gtk::Button::builder()
        .label(gettext("_Reset"))
        .use_underline(true)
        .build();
    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .build();
    let ok_button = gtk::Button::builder()
        .label(gettext("_Save"))
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
    dialog.set_default_widget(Some(&ok_button));
    dialog.set_cancel_widget(&cancel_button);

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

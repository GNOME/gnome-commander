// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use gtk::{pango, prelude::*};

pub fn hintbox(hint: &str) -> gtk::Widget {
    let bx = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(6)
        .hexpand(false)
        .build();

    bx.append(&gtk::Image::from_icon_name("gnome-commander-info-outline"));

    let attrs = pango::AttrList::new();
    attrs.insert(pango::AttrInt::new_style(pango::Style::Italic));

    let label = gtk::Label::builder()
        .label(hint)
        .wrap(true)
        .wrap_mode(pango::WrapMode::Word)
        .max_width_chars(1)
        .justify(gtk::Justification::Fill)
        .xalign(0.0)
        .yalign(0.5)
        .hexpand(true)
        .halign(gtk::Align::Fill)
        .attributes(&attrs)
        .build();
    bx.append(&label);

    bx.upcast()
}

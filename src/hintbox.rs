/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
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

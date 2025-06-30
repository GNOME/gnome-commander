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

use crate::utils::attributes_bold;
use gtk::prelude::*;

pub fn create_category(title: &str, content: &impl IsA<gtk::Widget>) -> gtk::Widget {
    let frame = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .build();

    frame.append(
        &gtk::Label::builder()
            .label(title)
            .attributes(&attributes_bold())
            .halign(gtk::Align::Start)
            .build(),
    );

    let content = content.as_ref();
    content.set_margin_top(3);
    content.set_margin_bottom(3);
    content.set_margin_start(18);
    content.set_margin_end(18);

    frame.append(content);

    frame.upcast()
}

pub fn radio_group_set_value<const N: usize, T: Copy + PartialEq>(
    mapping: [(&gtk::CheckButton, T); N],
    value: T,
) {
    for (button, option) in mapping {
        if option == value {
            button.set_active(true);
            break;
        }
    }
}

pub fn radio_group_get_value<const N: usize, T: Copy + Default>(
    mapping: [(&gtk::CheckButton, T); N],
) -> T {
    for (button, option) in mapping {
        if button.is_active() {
            return option;
        }
    }
    T::default()
}

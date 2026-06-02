// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use gtk::prelude::*;

pub fn create_category(title: &str, content: &impl IsA<gtk::Widget>) -> gtk::Widget {
    let frame = gtk::Frame::builder()
        .label(title)
        .child(content)
        .css_classes(["flat"])
        .build();

    let content = content.as_ref();
    content.set_margin_top(3);
    content.set_margin_bottom(3);
    content.set_margin_start(18);
    content.set_margin_end(18);

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

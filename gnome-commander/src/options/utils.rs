// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::types::U32Option;
use gtk::prelude::*;

pub fn remember_window_size(
    window: &impl AsRef<gtk::Window>,
    width_option: &U32Option,
    height_option: &U32Option,
) {
    if let Ok(width) = width_option.get().try_into() {
        window.as_ref().set_default_width(width);
    }
    if let Ok(height) = height_option.get().try_into() {
        window.as_ref().set_default_height(height);
    }

    let width_option = width_option.clone();
    let height_option = height_option.clone();
    window.as_ref().connect_destroy(move |window| {
        if let Ok(width) = u32::try_from(window.default_width()) {
            let _ = width_option.set(width);
        }
        if let Ok(height) = u32::try_from(window.default_height()) {
            let _ = height_option.set(height);
        }
    });
}

pub fn remember_window_state(
    window: &impl AsRef<gtk::Window>,
    width_option: &U32Option,
    height_option: &U32Option,
    state_option: &U32Option,
) {
    remember_window_size(window, width_option, height_option);
    window.as_ref().set_maximized(state_option.get() == 4);

    let state_option = state_option.clone();
    window.as_ref().connect_destroy(move |window| {
        let _ = state_option.set(if window.is_maximized() { 4_u32 } else { 0 });
    });
}

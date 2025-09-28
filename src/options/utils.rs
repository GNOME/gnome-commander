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

use super::types::AppOption;
use gtk::{gio, prelude::*};

pub fn remember_window_size(
    window: &impl AsRef<gtk::Window>,
    width_option: &AppOption<u32>,
    height_option: &AppOption<u32>,
) {
    fn map_u32_as_i32<'a>(binding: gio::BindingBuilder<'a>) -> gio::BindingBuilder<'a> {
        binding
            .mapping(|v, _| {
                let height: i32 = v.get::<u32>()?.try_into().ok()?;
                Some(height.to_value())
            })
            .set_mapping(|v, _| {
                let height: u32 = v.get::<i32>().ok()?.try_into().ok()?;
                Some(height.to_variant())
            })
    }

    map_u32_as_i32(width_option.bind(window.as_ref(), "default-width")).build();
    map_u32_as_i32(height_option.bind(window.as_ref(), "default-height")).build();
}

pub fn remember_window_state(
    window: &impl AsRef<gtk::Window>,
    width_option: &AppOption<u32>,
    height_option: &AppOption<u32>,
    state_option: &AppOption<u32>,
) {
    remember_window_size(window, width_option, height_option);
    state_option
        .bind(window.as_ref(), "maximized")
        .mapping(|v, _| {
            let state = v.get::<u32>()?;
            let maximized: bool = state == 4;
            Some(maximized.to_value())
        })
        .set_mapping(|v, _| {
            let maximized = v.get::<bool>().ok()?;
            let state: u32 = if maximized { 4_u32 } else { 0_u32 };
            Some(state.to_variant())
        })
        .build();
}

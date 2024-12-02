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
    shortcuts::{Call, Shortcut},
    user_actions::USER_ACTIONS,
};

#[derive(Clone)]
pub struct ShortcutAction {
    pub shortcut: Shortcut,
    pub call: Call,
}

pub fn action_description(action_name: &str) -> String {
    for user_action in &*USER_ACTIONS {
        if user_action.action_name == action_name {
            return user_action.description.clone();
        }
    }
    action_name.to_owned()
}

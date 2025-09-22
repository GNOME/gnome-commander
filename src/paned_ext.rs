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

use gtk::{graphene, prelude::*};

pub trait GnomeCmdPanedExt {
    fn handle_rect(&self) -> Option<graphene::Rect>;
}

impl GnomeCmdPanedExt for gtk::Paned {
    fn handle_rect(&self) -> Option<graphene::Rect> {
        let start = self
            .start_child()
            .and_then(|c| c.compute_bounds(self))?
            .bottom_right();

        let end = self
            .end_child()
            .and_then(|c| c.compute_bounds(self))?
            .top_left();

        let x1 = f32::min(start.x(), end.x());
        let x2 = f32::max(start.x(), end.x());
        let y1 = f32::min(start.y(), end.y());
        let y2 = f32::max(start.y(), end.y());

        Some(graphene::Rect::new(x1, y1, x2 - x1, y2 - y1))
    }
}

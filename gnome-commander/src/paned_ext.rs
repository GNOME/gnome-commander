// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

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

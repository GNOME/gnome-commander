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

use crate::utils::Gdk3to4RectangleCompat;
use gtk::prelude::*;
use std::i32;

pub enum TabClick {
    Tab(u32),
    Area,
}

pub trait GnomeCmdNotebookExt {
    /// Computes the allocation of a header area.
    fn header_allocation(&self) -> Option<gtk::Allocation>;

    /// Find index of a tab by screen coordinates
    fn find_tab_num_at_pos(&self, screen_x: i32, screen_y: i32) -> Option<TabClick>;
}

impl GnomeCmdNotebookExt for gtk::Notebook {
    fn header_allocation(&self) -> Option<gtk::Allocation> {
        let n = self.n_pages();
        if n == 0 {
            return None;
        }

        let tab_pos = self.tab_pos();
        // GtkWidget *the_page;

        let mut x1 = i32::MAX;
        let mut y1 = i32::MAX;
        let mut x2 = 0;
        let mut y2 = 0;
        for page_num in 0..n {
            let the_page = self.nth_page(Some(page_num))?;
            let tab = self.tab_label(&the_page)?;
            if !tab.is_mapped() {
                continue;
            }

            let tab_allocation = tab.allocation();
            let Some((x, y)) = tab.translate_coordinates(self, 0, 0) else {
                continue;
            };

            x1 = i32::min(x1, x);
            y1 = i32::min(y1, y);
            x2 = i32::max(x2, x + tab_allocation.width());
            y2 = i32::max(y2, y + tab_allocation.height());
        }

        let mut x = x1;
        let mut y = y1;
        let mut w = x2 - x1;
        let mut h = y2 - y1;

        let notebook_allocation = self.allocation();

        match tab_pos {
            gtk::PositionType::Top | gtk::PositionType::Bottom => {
                x = 0;
                w = notebook_allocation.width();
            }
            gtk::PositionType::Left | gtk::PositionType::Right => {
                y = 0;
                h = notebook_allocation.height();
            }
            _ => {}
        }

        Some(gtk::Allocation::new(x, y, w, h))
    }

    fn find_tab_num_at_pos(&self, screen_x: i32, screen_y: i32) -> Option<TabClick> {
        let n = self.n_pages();
        if n == 0 {
            return None;
        }

        for page_num in 0..n {
            let the_page = self.nth_page(Some(page_num))?;
            let tab = self.tab_label(&the_page)?;

            let tab_allocation = tab.allocation();
            let Some((x, y)) = tab.translate_coordinates(self, 0, 0) else {
                continue;
            };
            let tab_allocation =
                gtk::Allocation::new(x, y, tab_allocation.width(), tab_allocation.height());

            if tab_allocation.contains_point(screen_x, screen_y) {
                return Some(TabClick::Tab(page_num));
            }
        }

        if let Some(head_allocation) = self.header_allocation() {
            if head_allocation.contains_point(screen_x, screen_y) {
                return Some(TabClick::Area);
            }
        }

        None
    }
}

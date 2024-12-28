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

use gtk::{graphene, prelude::*};

pub enum TabClick {
    Tab(u32),
    Area,
}

pub trait GnomeCmdNotebookExt {
    /// Find index of a tab by screen coordinates
    fn find_tab_num_at_pos(&self, pos: &graphene::Point) -> Option<TabClick>;
}

impl GnomeCmdNotebookExt for gtk::Notebook {
    fn find_tab_num_at_pos(&self, pos: &graphene::Point) -> Option<TabClick> {
        let n = self.n_pages();

        let tab_rects: Vec<_> = (0..n)
            .filter_map(|page_num| Some((page_num, self.nth_page(Some(page_num))?)))
            .filter_map(|(page_num, page)| Some((page_num, self.tab_label(&page)?)))
            .filter(|(_page_num, tab)| tab.is_mapped())
            .filter_map(|(page_num, tab)| Some((page_num, tab.compute_bounds(self)?)))
            .collect();

        for (page_num, tab_rect) in &tab_rects {
            if tab_rect.contains_point(pos) {
                return Some(TabClick::Tab(*page_num));
            }
        }

        let all_tabs_rect = tab_rects
            .into_iter()
            .map(|(_, r)| r)
            .reduce(|r1, r2| r1.union(&r2))?;

        let header_rect = match self.tab_pos() {
            gtk::PositionType::Top | gtk::PositionType::Bottom => graphene::Rect::new(
                0.0,
                all_tabs_rect.y(),
                self.width() as f32,
                all_tabs_rect.height(),
            ),
            gtk::PositionType::Left | gtk::PositionType::Right => graphene::Rect::new(
                all_tabs_rect.x(),
                0.0,
                all_tabs_rect.width(),
                self.height() as f32,
            ),
            _ => all_tabs_rect,
        };
        if header_rect.contains_point(pos) {
            return Some(TabClick::Area);
        }
        None
    }
}

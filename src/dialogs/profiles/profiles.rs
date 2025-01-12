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

pub trait ProfileManager {
    fn len(&self) -> usize;

    fn profile_name(&self, profile_index: usize) -> String;
    fn set_profile_name(&self, profile_index: usize, name: &str);

    fn profile_description(&self, profile_index: usize) -> String;

    fn reset_profile(&self, profile_index: usize);
    fn duplicate_profile(&self, profile_index: usize) -> usize;
    fn pick(&self, profile_indexes: &[usize]);

    fn create_component(
        &self,
        profile_index: usize,
        labels_size_group: &gtk::SizeGroup,
    ) -> gtk::Widget; // implies component.update();
    fn update_component(&self, profile_index: usize, component: &gtk::Widget);
    fn copy_component(&self, profile_index: usize, component: &gtk::Widget);
}

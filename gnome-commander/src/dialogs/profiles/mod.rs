// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub mod edit_profile_dialog;
pub mod manage_profiles_dialog;

pub trait ProfileManager {
    #[allow(dead_code)]
    fn is_empty(&self) -> bool {
        self.len() == 0
    }
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

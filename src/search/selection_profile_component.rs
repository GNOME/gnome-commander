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

use gtk::{
    glib::{
        self,
        ffi::GType,
        translate::{IntoGlib, ToGlibPtr},
    },
    prelude::*,
    subclass::prelude::*,
};

mod imp {
    use super::*;

    #[derive(Default)]
    pub struct SelectionProfileComponent {}

    #[glib::object_subclass]
    impl ObjectSubclass for SelectionProfileComponent {
        const NAME: &'static str = "GnomeCmdSelectionProfileComponent";
        type Type = super::SelectionProfileComponent;
        type ParentType = gtk::Widget;
    }

    impl ObjectImpl for SelectionProfileComponent {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_layout_manager(Some(gtk::BinLayout::new()));

            unsafe {
                ffi::gnome_cmd_selection_profile_component_init(obj.to_glib_none().0);
            }
        }

        fn dispose(&self) {
            unsafe {
                ffi::gnome_cmd_selection_profile_component_dispose(self.obj().to_glib_none().0);
            }
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for SelectionProfileComponent {}
}

mod ffi {
    pub type GnomeCmdSelectionProfileComponent =
        <super::SelectionProfileComponent as glib::object::ObjectType>::GlibType;

    extern "C" {
        pub fn gnome_cmd_selection_profile_component_init(
            component: *mut GnomeCmdSelectionProfileComponent,
        );
        pub fn gnome_cmd_selection_profile_component_dispose(
            component: *mut GnomeCmdSelectionProfileComponent,
        );
    }
}

glib::wrapper! {
    pub struct SelectionProfileComponent(ObjectSubclass<imp::SelectionProfileComponent>)
        @extends gtk::Widget;
}

#[no_mangle]
pub extern "C" fn gnome_cmd_selection_profile_component_get_type() -> GType {
    SelectionProfileComponent::static_type().into_glib()
}

// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::helpers::{Grid, GridRow};
use gtk::prelude::*;

/// A helper trait used by the [with! macro](crate::with), provides a standard API to add a child to
/// some Gtk widgets. This trait is used when no explicit method is specified in `with!`.
pub trait ContainerExt<T> {
    fn container_set_child(&self, child: T);
}

macro_rules! container_ext_impl {
    ($container_type:ty, $method:ident, $child_type:ty) => {
        impl<T: AsRef<$child_type>> ContainerExt<T> for $container_type {
            fn container_set_child(&self, child: T) {
                child.as_ref().unparent();
                self.$method(child.as_ref().into());
            }
        }

        impl<T: AsRef<$child_type>> ContainerExt<T> for &$container_type {
            fn container_set_child(&self, child: T) {
                child.as_ref().unparent();
                self.$method(child.as_ref().into());
            }
        }
    };
}

container_ext_impl!(gtk::ApplicationWindow, set_child, gtk::Widget);
container_ext_impl!(gtk::Box, append, gtk::Widget);
container_ext_impl!(gtk::Button, set_child, gtk::Widget);
container_ext_impl!(gtk::CheckButton, set_child, gtk::Widget);
container_ext_impl!(gtk::Expander, set_child, gtk::Widget);
container_ext_impl!(gtk::Frame, set_child, gtk::Widget);
container_ext_impl!(gtk::LinkButton, set_child, gtk::Widget);
container_ext_impl!(gtk::ListBox, append, gtk::Widget);
container_ext_impl!(gtk::ListBoxRow, set_child, gtk::Widget);
container_ext_impl!(gtk::MenuButton, set_child, gtk::Widget);
container_ext_impl!(gtk::Popover, set_child, gtk::Widget);
container_ext_impl!(gtk::Revealer, set_child, gtk::Widget);
container_ext_impl!(gtk::ScrolledWindow, set_child, gtk::Widget);
container_ext_impl!(gtk::SearchBar, set_child, gtk::Widget);
container_ext_impl!(gtk::Stack, add_child, gtk::Widget);
container_ext_impl!(gtk::StackSidebar, set_stack, gtk::Stack);
container_ext_impl!(gtk::StackSwitcher, set_stack, gtk::Stack);
container_ext_impl!(gtk::ToggleButton, set_child, gtk::Widget);
container_ext_impl!(gtk::TreeExpander, set_child, gtk::Widget);
container_ext_impl!(gtk::Window, set_child, gtk::Widget);
container_ext_impl!(gtk::WindowHandle, set_child, gtk::Widget);

container_ext_impl!(Grid, append_row, GridRow);
container_ext_impl!(GridRow, append, gtk::Widget);

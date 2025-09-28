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

use crate::connection::connection::Connection;
use gtk::{gdk, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{connection::connection::ConnectionExt, utils::get_modifiers_state};
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::ConnectionButton)]
    pub struct ConnectionButton {
        #[property(get, set = Self::set_connection, nullable)]
        connection: RefCell<Option<Connection>>,

        #[property(get, set = Self::set_only_icon)]
        only_icon: Cell<bool>,

        pub button: gtk::Button,
        pub image: gtk::Image,
        pub label: gtk::Label,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ConnectionButton {
        const NAME: &'static str = "GnomeCmdConnectionButton";
        type Type = super::ConnectionButton;
        type ParentType = gtk::Widget;
    }

    #[glib::derived_properties]
    impl ObjectImpl for ConnectionButton {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_layout_manager(Some(gtk::BinLayout::new()));

            self.button.set_has_frame(false);
            self.button.set_can_focus(false);
            self.button.set_parent(&*obj);

            let hbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(1)
                .build();
            self.button.set_child(Some(&hbox));

            hbox.append(&self.image);
            hbox.append(&self.label);

            let click_controller = gtk::GestureClick::new();
            click_controller.set_button(0);
            click_controller.connect_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |gesture, n_press, _, _| {
                    if n_press == 1 {
                        imp.clicked(gesture.current_button());
                    }
                }
            ));
            self.button.add_controller(click_controller);
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    glib::subclass::Signal::builder("clicked")
                        .param_types([Connection::static_type(), bool::static_type()])
                        .build(),
                ]
            })
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for ConnectionButton {}

    impl ConnectionButton {
        fn set_connection(&self, connection: Option<&Connection>) {
            self.connection.replace(connection.cloned());
            self.update();
        }

        fn set_only_icon(&self, only_icon: bool) {
            self.only_icon.set(only_icon);
            self.update();
        }

        fn update(&self) {
            match self.connection.borrow().as_ref() {
                Some(connection) => {
                    let icon = connection.go_icon();
                    if let Some(ref icon) = icon {
                        self.image.set_from_gicon(icon);
                    } else {
                        self.image.clear();
                    }
                    if !self.only_icon.get() || icon.is_none() {
                        self.label
                            .set_label(&connection.alias().unwrap_or_default());
                    } else {
                        self.label.set_label("");
                    }
                    self.button
                        .set_tooltip_text(connection.go_text().as_deref());
                }
                None => {
                    self.image.clear();
                    self.label.set_label("");
                    self.button.set_tooltip_text(None);
                }
            }
        }

        fn clicked(&self, button: u32) {
            let Some(connection) = self.obj().connection() else {
                return;
            };
            let new_tab = match button {
                1 => self.is_control_pressed(),
                2 => true,
                _ => return,
            };
            self.obj()
                .emit_by_name::<()>("clicked", &[&connection, &new_tab]);
        }

        fn is_control_pressed(&self) -> bool {
            self.obj()
                .root()
                .and_downcast::<gtk::Window>()
                .and_then(|window| get_modifiers_state(&window))
                .map_or(false, |state| state == gdk::ModifierType::CONTROL_MASK)
        }
    }
}

glib::wrapper! {
    pub struct ConnectionButton(ObjectSubclass<imp::ConnectionButton>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl Default for ConnectionButton {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ConnectionButton {
    pub fn connect_clicked<F>(&self, f: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &Connection, bool) + 'static,
    {
        self.connect_closure(
            "clicked",
            false,
            glib::closure_local!(|this, connection, new_tab| { (f)(this, connection, new_tab) }),
        )
    }
}

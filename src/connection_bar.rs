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

use crate::connection::{connection::Connection, list::ConnectionList};
use gtk::{glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{
        connection::{
            connection::{Connection, ConnectionExt},
            device::ConnectionDevice,
            list::ConnectionList,
            smb::ConnectionSmb,
        },
        connection_button::ConnectionButton,
        options::options::GeneralOptions,
    };
    use std::{cell::OnceCell, rc::Rc, sync::OnceLock};

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::ConnectionBar)]
    pub struct ConnectionBar {
        #[property(get, construct_only)]
        pub connection_list: OnceCell<ConnectionList>,
        pub list_view: gtk::ListView,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ConnectionBar {
        const NAME: &'static str = "GnomeCmdConnectionBar";
        type Type = super::ConnectionBar;
        type ParentType = gtk::Widget;
    }

    #[glib::derived_properties]
    impl ObjectImpl for ConnectionBar {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_layout_manager(Some(gtk::BinLayout::new()));

            gtk::ScrolledWindow::builder()
                .hscrollbar_policy(gtk::PolicyType::Automatic)
                .vscrollbar_policy(gtk::PolicyType::Never)
                .child(&self.list_view)
                .build()
                .set_parent(&*obj);

            let model = gtk::FilterListModel::new(
                Some(obj.connection_list().all()),
                Some(gtk::CustomFilter::new(|item| {
                    item.downcast_ref::<Connection>().map_or(false, |con| {
                        con.is_open()
                            || con.downcast_ref::<ConnectionDevice>().is_some()
                            || con.downcast_ref::<ConnectionSmb>().is_some()
                    })
                })),
            );

            let options = GeneralOptions::new();

            let selection_model = gtk::NoSelection::new(Some(model));
            self.list_view.set_orientation(gtk::Orientation::Horizontal);
            self.list_view.set_model(Some(&selection_model));
            self.list_view
                .set_factory(Some(&button_factory(obj.upcast_ref(), Rc::new(options))));
            self.list_view.add_css_class("gcmd-toolbar");
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

    impl WidgetImpl for ConnectionBar {}

    fn button_factory(emitter: &glib::Object, options: Rc<GeneralOptions>) -> gtk::ListItemFactory {
        let factory = gtk::SignalListItemFactory::new();
        factory.connect_setup(glib::clone!(
            #[weak]
            emitter,
            #[strong]
            options,
            move |_, item| {
                let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
                    return;
                };

                let button = ConnectionButton::default();
                options.device_only_icon.bind(&button, "only-icon").build();
                button.connect_clicked(glib::clone!(
                    #[weak]
                    emitter,
                    move |_, connection, new_tab| {
                        emitter.emit_by_name::<()>("clicked", &[connection, &new_tab]);
                    }
                ));

                list_item.set_child(Some(&button));
            }
        ));
        factory.connect_bind(move |_, item| {
            let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
                return;
            };
            let Some(button) = list_item.child().and_downcast::<ConnectionButton>() else {
                return;
            };
            button.set_connection(list_item.item().and_downcast::<Connection>());
        });
        factory.connect_unbind(|_, item| {
            let Some(list_item) = item.downcast_ref::<gtk::ListItem>() else {
                return;
            };
            let Some(button) = list_item.child().and_downcast::<ConnectionButton>() else {
                return;
            };
            button.set_connection(None::<Connection>);
        });
        factory.upcast()
    }
}

glib::wrapper! {
    pub struct ConnectionBar(ObjectSubclass<imp::ConnectionBar>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl ConnectionBar {
    pub fn new(connection_list: &ConnectionList) -> Self {
        glib::Object::builder()
            .property("connection-list", connection_list)
            .build()
    }

    pub fn connect_clicked<F: Fn(&Self, &Connection, bool) + 'static>(
        &self,
        f: F,
    ) -> glib::SignalHandlerId {
        self.connect_closure(
            "clicked",
            false,
            glib::closure_local!(move |this, con, new_tab| { (f)(this, con, new_tab) }),
        )
    }
}

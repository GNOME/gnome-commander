/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024-2024 Andrey Kutejko <andy128k@gmail.com>
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

use crate::pwd::{SystemGroup, SystemUser, gid_t, uid_t};
use gtk::{gio, glib, prelude::*, subclass::prelude::*};
use std::ffi::CString;

mod imp {
    use super::*;
    use crate::{
        pwd::{system_groups, system_users, uid},
        utils::grid_attach,
    };
    use gettextrs::gettext;

    pub struct ChownComponent {
        pub user: gtk::DropDown,
        pub group: gtk::DropDown,
        pub users_model: gio::ListStore,
        pub groups_model: gio::ListStore,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ChownComponent {
        const NAME: &'static str = "GnomeCmdChownComponent";
        type Type = super::ChownComponent;
        type ParentType = gtk::Widget;

        fn new() -> Self {
            let users_model = gio::ListStore::new::<glib::BoxedAnyObject>();
            let groups_model = gio::ListStore::new::<glib::BoxedAnyObject>();
            Self {
                user: gtk::DropDown::builder()
                    .hexpand(true)
                    .model(&users_model)
                    .expression(gtk::ClosureExpression::new::<String>(
                        &[] as &[gtk::Expression],
                        glib::closure!(|item: &glib::Object| {
                            item.downcast_ref::<glib::BoxedAnyObject>()
                                .map(|b| {
                                    b.borrow::<SystemUser>().name.to_string_lossy().to_string()
                                })
                                .unwrap_or_default()
                        }),
                    ))
                    .build(),
                group: gtk::DropDown::builder()
                    .hexpand(true)
                    .model(&groups_model)
                    .expression(gtk::ClosureExpression::new::<String>(
                        &[] as &[gtk::Expression],
                        glib::closure!(|item: &glib::Object| {
                            item.downcast_ref::<glib::BoxedAnyObject>()
                                .map(|b| {
                                    b.borrow::<SystemGroup>().name.to_string_lossy().to_string()
                                })
                                .unwrap_or_default()
                        }),
                    ))
                    .build(),
                users_model,
                groups_model,
            }
        }
    }

    impl ObjectImpl for ChownComponent {
        fn constructed(&self) {
            self.parent_constructed();
            let this = self.obj();

            let layout = gtk::GridLayout::builder()
                .column_spacing(12)
                .row_spacing(6)
                .build();
            this.set_layout_manager(Some(layout.clone()));

            grid_attach(
                &*this,
                &gtk::Label::builder()
                    .label(gettext("Owner:"))
                    .halign(gtk::Align::Start)
                    .build(),
                0,
                0,
                1,
                1,
            );
            grid_attach(&*this, &self.user, 1, 0, 1, 1);
            grid_attach(
                &*this,
                &gtk::Label::builder()
                    .label(gettext("Group:"))
                    .halign(gtk::Align::Start)
                    .build(),
                0,
                1,
                1,
                1,
            );
            grid_attach(&*this, &self.group, 1, 1, 1, 1);

            self.load_users_and_groups();
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for ChownComponent {}

    impl ChownComponent {
        fn load_users_and_groups(&self) {
            let mut users = system_users();
            users.sort_by_key(|u| u.name.clone());

            let mut groups = system_groups();
            groups.sort_by_key(|g| g.name.clone());

            let uid = uid();
            let user = users.iter().find(|p| p.id == uid).cloned();

            for user in users {
                self.users_model.append(&glib::BoxedAnyObject::new(user));
            }
            // disable user drop down if user is not root
            self.user.set_sensitive(uid == 0);

            if uid != 0 {
                if let Some(user) = user {
                    for group in groups {
                        if group.id == user.gid || group.members.contains(&user.name) {
                            self.groups_model.append(&glib::BoxedAnyObject::new(group));
                        }
                    }
                }
            } else {
                for group in groups {
                    self.groups_model.append(&glib::BoxedAnyObject::new(group));
                }
            }
        }
    }
}

glib::wrapper! {
    pub struct ChownComponent(ObjectSubclass<imp::ChownComponent>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl Default for ChownComponent {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ChownComponent {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn ownership(&self) -> (uid_t, gid_t) {
        let owner = self
            .imp()
            .user
            .selected_item()
            .and_downcast::<glib::BoxedAnyObject>()
            .map(|b| b.borrow::<SystemUser>().id)
            .unwrap_or_default();
        let group = self
            .imp()
            .group
            .selected_item()
            .and_downcast::<glib::BoxedAnyObject>()
            .map(|b| b.borrow::<SystemGroup>().id)
            .unwrap_or_default();
        (owner, group)
    }

    pub fn set_ownership(&self, owner: uid_t, group: gid_t) {
        if let Some(owner_position) = self
            .imp()
            .users_model
            .iter::<glib::BoxedAnyObject>()
            .flatten()
            .position(|b| b.borrow::<SystemUser>().id == owner)
            .and_then(|p| p.try_into().ok())
        {
            self.imp().user.set_selected(owner_position);
        } else {
            self.imp()
                .users_model
                .append(&glib::BoxedAnyObject::new(SystemUser {
                    id: owner,
                    name: CString::new(owner.to_string()).unwrap(),
                    gid: 0,
                }));
            let owner_position = self.imp().users_model.n_items().saturating_sub(1);
            self.imp().user.set_selected(owner_position);
        }

        if let Some(group_position) = self
            .imp()
            .groups_model
            .iter::<glib::BoxedAnyObject>()
            .flatten()
            .position(|b| b.borrow::<SystemGroup>().id == group)
            .and_then(|p| p.try_into().ok())
        {
            self.imp().group.set_selected(group_position);
        } else {
            self.imp()
                .groups_model
                .append(&glib::BoxedAnyObject::new(SystemGroup {
                    id: owner,
                    name: CString::new(group.to_string()).unwrap(),
                    members: Vec::new(),
                }));
            let group_position = self.imp().users_model.n_items().saturating_sub(1);
            self.imp().group.set_selected(group_position);
        }
    }
}

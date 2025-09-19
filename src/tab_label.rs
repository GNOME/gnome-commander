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

use crate::config::PIXMAPS_DIR;
use gtk::{glib, pango, prelude::*, subclass::prelude::*};
use std::path::Path;

mod imp {
    use super::*;
    use std::cell::{Cell, RefCell};

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::TabLabel)]
    pub struct TabLabel {
        tab_label_pin: gtk::Image,
        tab_label_text: gtk::Label,

        #[property(get, set = Self::set_label)]
        label: RefCell<String>,

        #[property(get, set = Self::set_locked)]
        locked: Cell<bool>,

        #[property(get, set = Self::set_indicator, builder(TabLockIndicator::default()))]
        indicator: Cell<TabLockIndicator>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for TabLabel {
        const NAME: &'static str = "GnomeCmdTabLabel";
        type Type = super::TabLabel;
        type ParentType = gtk::Widget;

        fn new() -> Self {
            Self {
                tab_label_pin: gtk::Image::from_file(Path::new(PIXMAPS_DIR).join("pin.png")),
                tab_label_text: gtk::Label::builder().hexpand(true).build(),
                label: Default::default(),
                locked: Default::default(),
                indicator: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for TabLabel {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();
            this.set_layout_manager(Some(
                gtk::BoxLayout::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .build(),
            ));

            self.tab_label_pin.set_parent(&*this);
            self.tab_label_text.set_parent(&*this);
        }
    }

    impl WidgetImpl for TabLabel {}

    impl TabLabel {
        fn set_label(&self, label: String) {
            self.label.replace(label);
            self.update();
        }

        fn set_locked(&self, locked: bool) {
            self.locked.set(locked);
            self.update();
        }

        fn set_indicator(&self, indicator: TabLockIndicator) {
            self.indicator.set(indicator);
            self.update();
        }

        fn update(&self) {
            let locked = self.locked.get();
            let label = self.label.borrow();
            match self.indicator.get() {
                TabLockIndicator::Icon => {
                    self.tab_label_pin.set_visible(locked);
                    self.tab_label_text.set_text(&*label);
                    self.tab_label_text.set_attributes(None);
                }
                TabLockIndicator::Asterisk => {
                    self.tab_label_pin.set_visible(false);
                    self.tab_label_text.set_text(&format!(
                        "{}{}",
                        if locked { "* " } else { "" },
                        label
                    ));
                    self.tab_label_text.set_attributes(None);
                }
                TabLockIndicator::StyledText => {
                    self.tab_label_pin.set_visible(false);
                    self.tab_label_text.set_text(&*label);
                    self.tab_label_text.set_attributes(
                        if locked {
                            let attrs = pango::AttrList::new();
                            attrs.insert(pango::AttrColor::new_foreground(0, 0, 0xFFFF));
                            Some(attrs)
                        } else {
                            None
                        }
                        .as_ref(),
                    );
                }
            }
        }
    }
}

glib::wrapper! {
    pub struct TabLabel(ObjectSubclass<imp::TabLabel>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl Default for TabLabel {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

#[derive(Clone, Copy, Default, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdTabLockIndicator")]
pub enum TabLockIndicator {
    #[default]
    Icon = 0,
    Asterisk,
    StyledText,
}

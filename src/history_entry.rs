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

use gtk::{gdk, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use std::cell::Cell;

    pub struct HistoryEntry {
        pub entry: gtk::Entry,
        pub selection: gtk::SingleSelection,
        pub popover: gtk::Popover,
        pub history: gtk::StringList,
        pub max_history_size: Cell<usize>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for HistoryEntry {
        const NAME: &'static str = "GnomeCmdHistoryEntry";
        type Type = super::HistoryEntry;
        type ParentType = gtk::Widget;

        fn new() -> Self {
            let history = gtk::StringList::default();
            Self {
                entry: gtk::Entry::builder()
                    .hexpand(true)
                    .editable(true)
                    .can_focus(true)
                    .width_request(60)
                    .build(),
                selection: gtk::SingleSelection::new(Some(history.clone())),
                popover: gtk::Popover::builder()
                    .position(gtk::PositionType::Bottom)
                    .has_arrow(false)
                    .build(),
                history,
                max_history_size: Cell::new(16),
            }
        }
    }

    impl ObjectImpl for HistoryEntry {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_layout_manager(Some(
                gtk::BoxLayout::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .spacing(4)
                    .build(),
            ));

            self.entry.set_parent(&*obj);

            self.entry
                .set_secondary_icon_name(Some("gnome-commander-down"));
            self.entry.connect_icon_press(glib::clone!(
                #[weak]
                obj,
                move |_, _| obj.show_history()
            ));

            // let history_button = gtk::Button::builder()
            //     .icon_name("gnome-commander-down")
            //     .can_focus(false)
            //     // .has_frame(false)
            //     .build();
            // history_button.set_parent(&*obj);
            // history_button.connect_clicked(glib::clone!(
            //     #[weak]
            //     obj,
            //     move |_| obj.show_history()
            // ));

            let list =
                gtk::ListView::new(Some(self.selection.clone()), Some(history_item_factory()));
            list.connect_activate(glib::clone!(
                #[weak]
                obj,
                move |_, position| {
                    if let Some(str) = obj.imp().history.string(position) {
                        obj.imp().entry.set_text(&str);
                        obj.imp().popover.popdown();
                        obj.grab_focus();
                    }
                }
            ));
            self.popover.set_child(Some(&list));
            self.popover.set_parent(&*obj);

            let key_controller = gtk::EventControllerKey::new();
            key_controller.set_propagation_phase(gtk::PropagationPhase::Capture);
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, state| imp.on_key_pressed(key, state)
            ));
            self.entry.add_controller(key_controller);
        }

        fn dispose(&self) {
            self.popover.unparent();
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for HistoryEntry {
        fn grab_focus(&self) -> bool {
            let result = self.entry.grab_focus();
            self.entry.set_position(-1);
            result
        }
    }

    impl HistoryEntry {
        fn on_key_pressed(&self, key: gdk::Key, state: gdk::ModifierType) -> glib::Propagation {
            match (state, key) {
                (gdk::ModifierType::CONTROL_MASK, gdk::Key::Down) => {
                    self.obj().show_history();
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
        }

        pub fn truncate(&self) {
            let max_history_size = self.max_history_size.get() as u32;
            let len = self.history.n_items();
            if len > max_history_size {
                self.history
                    .splice(max_history_size, len - max_history_size, &[]);
            }
        }
    }

    fn history_item_factory() -> gtk::ListItemFactory {
        let factory = gtk::SignalListItemFactory::new();
        factory.connect_setup(|_, obj| {
            if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
                list_item.set_child(Some(
                    &gtk::Label::builder().hexpand(true).xalign(0.0).build(),
                ));
            }
        });
        factory.connect_bind(|_, obj| {
            if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
                if let Some(label) = list_item.child().and_downcast::<gtk::Label>() {
                    if let Some(item) = list_item.item().and_downcast::<gtk::StringObject>() {
                        label.set_label(&item.string());
                    } else {
                        label.set_label("");
                    }
                }
            }
        });
        factory.upcast()
    }
}

glib::wrapper! {
    pub struct HistoryEntry(ObjectSubclass<imp::HistoryEntry>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl Default for HistoryEntry {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl HistoryEntry {
    pub fn entry(&self) -> gtk::Entry {
        self.imp().entry.clone()
    }

    pub fn text(&self) -> glib::GString {
        self.imp().entry.text()
    }

    pub fn set_text(&self, text: &str) {
        self.imp().entry.set_text(text);
    }

    pub fn add_to_history(&self, value: &str) {
        let history = &self.imp().history;
        for i in (0..history.n_items()).rev() {
            if history.string(i).as_deref() == Some(value) {
                history.remove(i);
            }
        }
        history.splice(0, 0, &[value]);
        self.imp().truncate();
    }

    pub fn history(&self) -> Vec<String> {
        let history = &self.imp().history;
        (0..history.n_items())
            .filter_map(|i| history.string(i))
            .map(|s| s.to_string())
            .collect()
    }

    pub fn set_history(&self, items: &[String]) {
        let items = items
            .iter()
            .take(self.imp().max_history_size.get())
            .map(|s| s.as_str())
            .collect::<Vec<_>>();
        let len = self.imp().history.n_items();
        self.imp().history.splice(0, len, &items);
    }

    pub fn set_max_history_size(&self, max_history_size: usize) {
        self.imp().max_history_size.set(max_history_size);
        self.imp().truncate();
    }

    pub fn show_history(&self) {
        self.imp().popover.set_width_request(self.width());
        self.imp().popover.popup();
        self.imp().selection.select_item(0, true);
    }

    pub fn set_position(&self, position: gtk::PositionType) {
        self.imp().popover.set_position(position);
    }
}

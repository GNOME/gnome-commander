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

use super::list::FileList;
use crate::{file::File, filter::fnmatch, options::options::GeneralOptions};
use gettextrs::gettext;
use gtk::{gdk, glib, prelude::*, subclass::prelude::*};
use std::rc::Rc;

mod imp {
    use super::*;
    use std::cell::{Cell, OnceCell, RefCell};

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::QuickSearch)]
    pub struct QuickSearch {
        #[property(get, construct_only)]
        file_list: OnceCell<FileList>,
        pub entry: gtk::Entry,
        matches: RefCell<Vec<File>>,
        position: Cell<Option<usize>>,
        last_focused_file: RefCell<Option<File>>,
        pub options: OnceCell<Rc<GeneralOptions>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for QuickSearch {
        const NAME: &'static str = "GnomeCmdQuickSearch";
        type Type = super::QuickSearch;
        type ParentType = gtk::Popover;
    }

    #[glib::derived_properties]
    impl ObjectImpl for QuickSearch {
        fn constructed(&self) {
            self.parent_constructed();

            self.obj().set_position(gtk::PositionType::Bottom);

            let bx = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(6)
                .build();

            let label = gtk::Label::builder().label(&gettext("Search")).build();
            bx.append(&label);
            bx.append(&self.entry);
            self.obj().set_child(Some(&bx));

            let key_controller = gtk::EventControllerKey::new();
            key_controller.set_propagation_phase(gtk::PropagationPhase::Capture);
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, _| imp.key_pressed(key)
            ));
            self.entry.add_controller(key_controller);

            self.entry.connect_closure(
                "changed",
                true,
                glib::closure_local!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_: &gtk::Entry| imp.entry_changed()
                ),
            );
        }
    }

    impl WidgetImpl for QuickSearch {}
    impl PopoverImpl for QuickSearch {}

    impl QuickSearch {
        fn options(&self) -> Rc<GeneralOptions> {
            self.options.get().unwrap().clone()
        }

        fn hide_popup(&self) {
            self.obj().popdown();
            self.obj().file_list().grab_focus();
        }

        fn focus_file(&self, file: &File) {
            if file.is_dotdot() {
                return;
            }

            self.last_focused_file.replace(Some(file.clone()));

            if let Some(row) = self.obj().file_list().get_row_from_file(file) {
                self.obj().file_list().focus_file_at_row(&row);
            }
        }

        fn entry_changed(&self) {
            self.set_filter(&self.entry.text());

            if let Some(file) = self.matches.borrow().first() {
                self.position.set(Some(0));
                self.focus_file(file);
            } else {
                self.position.set(None);
            }
        }

        fn key_pressed(&self, key: gdk::Key) -> glib::Propagation {
            // While in quicksearch, treat "ALT/CTRL + key" as a simple "key"
            match key {
                gdk::Key::Escape => {
                    self.hide_popup();
                    glib::Propagation::Stop
                }
                gdk::Key::Up => {
                    if let Some(mut position) =  self.position.get()
                    {
                        let len = self.matches.borrow().len();
                        position = (position + len - 1) % len;
                        self.position.set(Some(position));

                        self.focus_file(&self.matches.borrow()[position]);
                    }
                    glib::Propagation::Stop
                }
                gdk::Key::Down => {
                    if let Some(mut position) =  self.position.get()
                    {
                        let len = self.matches.borrow().len();
                        position = (position + 1) % len;
                        self.position.set(Some(position));

                        self.focus_file(&self.matches.borrow()[position]);
                    }
                    glib::Propagation::Stop
                }
                // for more convenience do ENTER action directly on the current quicksearch item
                gdk::Key::Return | gdk::Key::KP_Enter |
                // for more convenience jump direct to Fx function on the current quicksearch item
                gdk::Key::F3 |
                gdk::Key::F4 |
                gdk::Key::F5 |
                gdk::Key::F6 |
                gdk::Key::F8 => {
                    // hide popup and let an event to bubble up and be processed by a parent widget
                    self.hide_popup();
                    glib::Propagation::Proceed
                }
                _ => {
                    glib::Propagation::Proceed
                }
            }
        }

        fn set_filter(&self, text: &str) {
            let options = self.options();

            let mut pattern = text.to_owned();
            if !options.quick_search_exact_match_begin.get() && !pattern.starts_with("*") {
                pattern.insert(0, '*');
            }
            if !options.quick_search_exact_match_end.get() && !pattern.ends_with("*") {
                pattern.push('*');
            }

            let case_sensitive = options.case_sensitive.get();

            let mut matching_files: Vec<File> = self
                .obj()
                .file_list()
                .visible_files()
                .into_iter()
                .filter(|f| fnmatch(&pattern, &f.file_info().display_name(), case_sensitive))
                .collect();

            // If no file matches the new filter, focus on the last file that matched a previous filter
            if matching_files.is_empty() {
                if let Some(file) = self.last_focused_file.borrow().clone() {
                    matching_files.push(file);
                }
            }

            self.matches.replace(matching_files);
        }
    }
}

glib::wrapper! {
    pub struct QuickSearch(ObjectSubclass<imp::QuickSearch>)
        @extends gtk::Popover, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Native;
}

impl QuickSearch {
    pub fn new(file_list: &FileList) -> Self {
        let this: Self = glib::Object::builder()
            .property("file-list", file_list)
            .build();
        this.set_parent(file_list);
        this.imp()
            .options
            .set(Rc::new(GeneralOptions::new()))
            .ok()
            .unwrap();
        this
    }

    pub fn set_text(&self, text: &str) {
        self.imp().entry.set_text(text);
        self.imp().entry.set_position(-1);
    }
}

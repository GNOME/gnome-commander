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

use crate::{dir::Directory, file::File};
use gtk::{gdk, glib, pango, prelude::*, subclass::prelude::*};
use winnow::{
    Result as PResult,
    combinator::{alt, opt, preceded, separated, terminated},
    prelude::*,
    seq,
    token::{take_till, take_while},
};

mod imp {
    use super::*;
    use crate::utils::get_modifiers_state;
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::DirectoryIndicator)]
    pub struct DirectoryrIndicator {
        path_inits: RefCell<Vec<String>>,

        #[property(get, set = Self::set_directory, nullable)]
        directory: RefCell<Option<Directory>>,
        #[property(get, set = Self::set_active)]
        active: Cell<bool>,

        pub host_label: gtk::Label,
        pub path_labels: RefCell<Vec<gtk::Label>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for DirectoryrIndicator {
        const NAME: &'static str = "GnomeCmdDirectoryIndicator";
        type Type = super::DirectoryIndicator;
        type ParentType = gtk::Widget;
    }

    #[glib::derived_properties]
    impl ObjectImpl for DirectoryrIndicator {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();
            this.set_layout_manager(Some(gtk::BoxLayout::new(gtk::Orientation::Horizontal)));

            self.host_label
                .set_attributes(Some(&make_attributes(false, false)));
            self.host_label.set_halign(gtk::Align::Start);
            self.host_label.set_valign(gtk::Align::Center);
            self.host_label.set_parent(&*this);
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    glib::subclass::Signal::builder("navigate")
                        .param_types([String::static_type(), bool::static_type()])
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

    impl WidgetImpl for DirectoryrIndicator {}

    impl DirectoryrIndicator {
        pub fn set_path(&self, path: &str) {
            for child in self.path_labels.borrow().iter() {
                child.unparent();
            }

            self.path_inits.borrow_mut().clear();
            self.path_labels.borrow_mut().clear();

            if !path.is_empty() {
                let mut init = String::new();
                for path_segment in split_path(path) {
                    init.push_str(&path_segment);
                    self.path_inits.borrow_mut().push(init.clone());

                    let label = gtk::Label::builder()
                        .label(&path_segment)
                        .attributes(&make_attributes(false, self.active.get()))
                        .halign(gtk::Align::Start)
                        .valign(gtk::Align::Center)
                        .build();
                    label.set_parent(&*self.obj());
                    self.path_labels.borrow_mut().push(label);
                }
            }

            for (index, label) in self.path_labels.borrow().iter().enumerate() {
                let motion_controller = gtk::EventControllerMotion::new();
                motion_controller.connect_enter(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_, _, _| imp.on_motion(index)
                ));
                motion_controller.connect_motion(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_, _, _| imp.on_motion(index)
                ));
                motion_controller.connect_leave(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.on_leave()
                ));
                label.add_controller(motion_controller);

                let click_gesture = gtk::GestureClick::builder().button(0).build();
                click_gesture.connect_pressed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |g, n_press, _, _| imp.on_click(g.current_button(), n_press, index)
                ));
                label.add_controller(click_gesture);
            }
        }

        fn set_directory(&self, directory: Option<Directory>) {
            if let Some(dir) = directory {
                let path = dir.display_path();

                let as_file = dir.upcast_ref::<File>();

                let mut host = None;
                if !dir.is_local() {
                    // show host in dir indicator if we are not on a local connection
                    let uri = as_file.file().uri();
                    match glib::Uri::parse(&uri, glib::UriFlags::NONE) {
                        Err(error) => {
                            eprintln!("g_uri_parse error {}: {}", uri, error.message());
                        }
                        Ok(uri) => {
                            host = uri.host();
                        }
                    }
                }

                match host {
                    Some(host) => self.host_label.set_label(&format!("{host}:")),
                    None => {
                        self.host_label.set_label("");
                    }
                }

                self.set_path(&path);
            } else {
                self.host_label.set_label("");
                self.set_path("");
            }
            self.update_markup(None);
        }

        fn set_active(&self, active: bool) {
            self.active.set(active);
            self.update_markup(None);
        }

        pub fn update_markup(&self, up_to: Option<usize>) {
            self.host_label
                .set_attributes(Some(&make_attributes(false, self.active.get())));

            for (index, label) in self.path_labels.borrow().iter().enumerate() {
                label.set_attributes(Some(&make_attributes(
                    up_to.map_or(false, |c| c >= index),
                    self.active.get(),
                )));
            }
        }

        fn on_motion(&self, index: usize) {
            self.obj()
                .set_cursor(gdk::Cursor::from_name("pointer", None).as_ref());
            self.update_markup(Some(index));
        }

        fn on_leave(&self) {
            self.obj().set_cursor(None);
            self.update_markup(None);
        }

        fn on_click(&self, button: u32, n_press: i32, index: usize) {
            if n_press != 1 {
                return;
            }

            let Some(path) = self.path_inits.borrow().get(index).cloned() else {
                return;
            };

            match button {
                1 => self.emit_navigate(
                    &path,
                    self.modifiers_state() == Some(gdk::ModifierType::CONTROL_MASK),
                ),
                2 => self.emit_navigate(&path, true),
                3 => self.obj().clipboard().set_text(&path),
                _ => {}
            }
        }

        fn modifiers_state(&self) -> Option<gdk::ModifierType> {
            self.obj()
                .root()
                .and_downcast::<gtk::Window>()
                .and_then(|w| get_modifiers_state(&w))
        }

        fn emit_navigate(&self, path: &str, new_tab: bool) {
            self.obj()
                .emit_by_name::<()>("navigate", &[&path, &new_tab]);
        }
    }

    fn make_attributes(selected: bool, active: bool) -> pango::AttrList {
        let attrs = pango::AttrList::new();
        attrs.insert(pango::AttrString::new_family("monospace"));
        if selected {
            attrs.insert(pango::AttrInt::new_weight(pango::Weight::Bold));
        } else {
            attrs.insert(pango::AttrInt::new_weight(pango::Weight::Normal));
        }
        if active {
            attrs.insert(pango::AttrInt::new_underline(pango::Underline::Single));
        }
        attrs
    }
}

glib::wrapper! {
    pub struct DirectoryIndicator(ObjectSubclass<imp::DirectoryrIndicator>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl Default for DirectoryIndicator {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl DirectoryIndicator {
    pub fn connect_navigate<F>(&self, callback: F) -> glib::SignalHandlerId
    where
        F: Fn(&Self, &str, bool) + 'static,
    {
        self.connect_closure(
            "navigate",
            false,
            glib::closure_local!(move |this: &Self, path: &str, new_tab: bool| {
                (callback)(this, path, new_tab)
            }),
        )
    }
}

fn split_path(path: &str) -> Vec<String> {
    fn unix_path_segment<'s>(input: &mut &'s str) -> PResult<&'s str> {
        take_till(1.., '/').parse_next(input)
    }

    fn unix_path<'s>(input: &mut &str) -> PResult<Vec<String>> {
        seq!(
            '/',
            separated(0.., unix_path_segment, '/'),
            take_while(0.., '/')
        )
        .map(|(_, segments, _): (char, Vec<&str>, &str)| {
            let mut path = vec![String::from("/")];
            path.extend(segments.into_iter().map(|s| format!("{s}/")));
            path
        })
        .parse_next(input)
    }

    fn unc_host_and_share<'s>(input: &mut &'s str) -> PResult<&'s str> {
        seq!("\\\\", take_till(1.., '\\'), '\\', take_till(1.., '\\'))
            .take()
            .parse_next(input)
    }

    fn unc_path_segment<'s>(input: &mut &'s str) -> PResult<&'s str> {
        seq!(take_till(1.., '\\'),).take().parse_next(input)
    }

    fn unc_path_segments<'s>(input: &mut &'s str) -> PResult<Vec<&'s str>> {
        terminated(
            separated(0.., unc_path_segment, '\\'),
            take_while(0.., '\\'),
        )
        .parse_next(input)
    }

    fn unc_path<'s>(input: &mut &str) -> PResult<Vec<String>> {
        seq!(unc_host_and_share, opt(preceded('\\', unc_path_segments)))
            .map(|(host_and_share, path)| {
                let mut full_path = Vec::new();
                if let Some(path) = path {
                    full_path.push(format!("{host_and_share}\\"));
                    full_path.extend(path.into_iter().map(|s| format!("{s}\\")));
                } else {
                    full_path.push(host_and_share.to_owned());
                }
                full_path
            })
            .parse_next(input)
    }

    fn path_parser<'s>(input: &mut &str) -> PResult<Vec<String>> {
        alt((unix_path, unc_path)).parse_next(input)
    }

    match path_parser.parse(path) {
        Ok(segments) => segments,
        Err(error) => {
            eprintln!("split_path failed:\n{}", error);
            vec![path.to_owned()]
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_parse_path() {
        assert_eq!(split_path("/"), vec!["/".to_string()]);
        assert_eq!(split_path("/a/"), vec!["/".to_string(), "a/".to_string()]);
        assert_eq!(
            split_path("/abc/def/ghi"),
            vec![
                "/".to_string(),
                "abc/".to_string(),
                "def/".to_string(),
                "ghi/".to_string()
            ]
        );
        assert_eq!(
            split_path("\\\\abc\\def"),
            vec!["\\\\abc\\def".to_string(),]
        );
        assert_eq!(
            split_path("\\\\abc\\def\\"),
            vec!["\\\\abc\\def\\".to_string(),]
        );
        assert_eq!(
            split_path("\\\\abc\\def\\ghi"),
            vec!["\\\\abc\\def\\".to_string(), "ghi\\".to_string()]
        );
        assert_eq!(
            split_path("\\\\abc\\def\\ghi\\jkl"),
            vec![
                "\\\\abc\\def\\".to_string(),
                "ghi\\".to_string(),
                "jkl\\".to_string()
            ]
        );
        assert_eq!(split_path("ab/c"), vec!["ab/c".to_string()]);
        assert_eq!(split_path("\\\\abc"), vec!["\\\\abc".to_string()]);
    }
}

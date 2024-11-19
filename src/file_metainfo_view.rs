/*
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
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

use crate::{file::File, utils::bold};
use gettextrs::gettext;
use gtk::{
    gio,
    glib::{self, ffi::GType, translate::IntoGlib},
    pango,
    prelude::*,
    subclass::prelude::*,
};
use imp::TagNode;

mod imp {
    use super::*;
    use crate::{
        libgcmd::file_base::FileBaseExt,
        tags::tags::{gcmd_tags_bulk_load, GnomeCmdTag},
    };
    use std::{
        cell::{Ref, RefCell},
        sync::OnceLock,
    };

    pub struct TagNode {
        pub tag: GnomeCmdTag,
        pub value: String,
    }

    #[derive(Default)]
    pub struct FileMetainfoView {
        pub file: RefCell<Option<File>>,
        pub view: gtk::ColumnView,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileMetainfoView {
        const NAME: &'static str = "GnomeCmdFileMetainfoView";
        type Type = super::FileMetainfoView;
        type ParentType = gtk::Widget;
    }

    impl ObjectImpl for FileMetainfoView {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_layout_manager(Some(gtk::BinLayout::new()));

            self.view.set_header_factory(Some(&header_factory()));
            self.view.append_column(
                &gtk::ColumnViewColumn::builder()
                    .title(gettext("Name"))
                    .factory(&name_factory())
                    .resizable(true)
                    .build(),
            );
            self.view.append_column(
                &gtk::ColumnViewColumn::builder()
                    .title(gettext("Value"))
                    .factory(&value_factory())
                    .resizable(true)
                    .build(),
            );
            self.view.append_column(
                &gtk::ColumnViewColumn::builder()
                    .title(gettext("Description"))
                    .factory(&description_factory())
                    .resizable(true)
                    .build(),
            );

            let scrolledwindow = gtk::ScrolledWindow::builder().child(&self.view).build();
            scrolledwindow.set_parent(&*obj);
        }

        fn properties() -> &'static [glib::ParamSpec] {
            static PROPERTIES: OnceLock<Vec<glib::ParamSpec>> = OnceLock::new();
            PROPERTIES.get_or_init(|| vec![glib::ParamSpecObject::builder::<File>("file").build()])
        }

        fn property(&self, _id: usize, _pspec: &glib::ParamSpec) -> glib::Value {
            self.file.borrow().to_value()
        }

        fn set_property(&self, _id: usize, value: &glib::Value, _pspec: &glib::ParamSpec) {
            *self.file.borrow_mut() = value.get().ok();
            self.update_model();
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for FileMetainfoView {}

    impl FileMetainfoView {
        fn update_model(&self) {
            self.view.set_model(None::<&gtk::SelectionModel>);

            let Ok(file) = Ref::filter_map(self.file.borrow(), |f| f.as_ref()) else {
                return;
            };

            if file.file_info().file_type() == gio::FileType::Special {
                return;
            }

            let metadata = Ref::map(file, |f| gcmd_tags_bulk_load(f));

            let model: gio::ListStore = metadata
                .tags
                .iter()
                .map(|(_tag_class, tags)| {
                    tags.iter()
                        .map(|(tag, values)| {
                            glib::BoxedAnyObject::new(TagNode {
                                tag: *tag,
                                value: values.join(", "),
                            })
                        })
                        .collect::<gio::ListStore>()
                })
                .collect();

            let model = gtk::FlattenListModel::new(Some(model));

            let selection_model = gtk::NoSelection::new(Some(model));

            self.view.set_model(Some(&selection_model));
        }
    }
}

glib::wrapper! {
    pub struct FileMetainfoView(ObjectSubclass<imp::FileMetainfoView>)
        @extends gtk::Widget;
}

#[no_mangle]
pub extern "C" fn gnome_cmd_file_metainfo_view_get_type() -> GType {
    FileMetainfoView::static_type().into_glib()
}

fn header_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        let Some(item) = item.downcast_ref::<gtk::ListHeader>() else {
            return;
        };
        item.set_child(Some(
            &gtk::Label::builder()
                .xalign(0.0)
                .margin_start(6)
                .margin_end(6)
                .margin_top(12)
                .margin_bottom(6)
                .build(),
        ));
    });
    factory.connect_bind(|_, item| {
        let Some(item) = item.downcast_ref::<gtk::ListHeader>() else {
            return;
        };
        let Some(label) = item.child().and_downcast::<gtk::Label>() else {
            return;
        };
        label.set_text("");
        let Some(obj) = item.item().and_downcast::<glib::BoxedAnyObject>() else {
            return;
        };
        let Ok(node) = obj.try_borrow::<TagNode>() else {
            return;
        };
        label.set_markup(&bold(&node.tag.class_name()));
    });
    factory.upcast()
}

fn name_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        item.set_child(Some(&gtk::Label::builder().xalign(0.0).build()));
    });
    factory.connect_bind(|_, item| {
        let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(label) = item.child().and_downcast::<gtk::Label>() else {
            return;
        };
        label.set_text("");
        let Some(obj) = item.item().and_downcast::<glib::BoxedAnyObject>() else {
            return;
        };
        let Ok(node) = obj.try_borrow::<TagNode>() else {
            return;
        };
        label.set_text(&node.tag.title());
    });
    factory.upcast()
}

fn value_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        item.set_child(Some(&gtk::Label::builder().xalign(0.0).build()));
    });
    factory.connect_bind(|_, item| {
        let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(label) = item.child().and_downcast::<gtk::Label>() else {
            return;
        };
        label.set_text("");
        let Some(obj) = item.item().and_downcast::<glib::BoxedAnyObject>() else {
            return;
        };
        let Ok(node) = obj.try_borrow::<TagNode>() else {
            return;
        };
        label.set_text(&node.value);
    });
    factory.upcast()
}

fn description_factory() -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        item.set_child(Some(
            &gtk::Label::builder()
                .xalign(0.0)
                .ellipsize(pango::EllipsizeMode::End)
                .build(),
        ));
    });
    factory.connect_bind(|_, item| {
        let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        let Some(label) = item.child().and_downcast::<gtk::Label>() else {
            return;
        };
        label.set_text("");
        let Some(obj) = item.item().and_downcast::<glib::BoxedAnyObject>() else {
            return;
        };
        let Ok(node) = obj.try_borrow::<TagNode>() else {
            return;
        };
        label.set_text(&node.tag.description());
        label.set_tooltip_text(Some(&node.tag.description()));
    });
    factory.upcast()
}

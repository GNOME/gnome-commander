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

use crate::{file::File, tags::tags::FileMetadataService, utils::bold};
use gettextrs::gettext;
use gtk::{gio, glib, pango, prelude::*, subclass::prelude::*};
use imp::TagNode;

mod imp {
    use super::*;
    use crate::tags::tags::{FileMetadataService, GnomeCmdTag};
    use std::cell::{OnceCell, RefCell};

    pub struct TagNode {
        pub tag: GnomeCmdTag,
        pub value: String,
    }

    #[derive(Default, glib::Properties)]
    #[properties(wrapper_type = super::FileMetainfoView)]
    pub struct FileMetainfoView {
        #[property(get, set = Self::set_file, nullable)]
        pub file: RefCell<Option<File>>,
        #[property(get, construct_only)]
        pub file_metadata_service: OnceCell<FileMetadataService>,
        pub view: gtk::ColumnView,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FileMetainfoView {
        const NAME: &'static str = "GnomeCmdFileMetainfoView";
        type Type = super::FileMetainfoView;
        type ParentType = gtk::Widget;
    }

    #[glib::derived_properties]
    impl ObjectImpl for FileMetainfoView {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.set_layout_manager(Some(gtk::BinLayout::new()));

            let file_metadata_service = self.obj().file_metadata_service();

            self.view
                .set_header_factory(Some(&header_factory(file_metadata_service.clone())));
            self.view.append_column(
                &gtk::ColumnViewColumn::builder()
                    .title(gettext("Name"))
                    .factory(&name_factory(file_metadata_service.clone()))
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
                    .factory(&description_factory(file_metadata_service))
                    .resizable(true)
                    .build(),
            );

            let scrolledwindow = gtk::ScrolledWindow::builder().child(&self.view).build();
            scrolledwindow.set_parent(&*obj);
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for FileMetainfoView {}

    impl FileMetainfoView {
        fn set_file(&self, file: Option<File>) {
            self.file.replace(file);
            self.update_model();
        }

        fn update_model(&self) {
            self.view.set_model(None::<&gtk::SelectionModel>);

            let Some(file) = self.obj().file() else {
                return;
            };

            if file.file_info().file_type() == gio::FileType::Special {
                return;
            }

            let metadata = self.obj().file_metadata_service().extract_metadata(&file);

            let model: gio::ListStore = metadata
                .tags
                .iter()
                .map(|(_tag_class, tags)| {
                    tags.iter()
                        .map(|(tag, values)| {
                            glib::BoxedAnyObject::new(TagNode {
                                tag: tag.clone(),
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
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl FileMetainfoView {
    pub fn new(file_metadata_service: &FileMetadataService) -> Self {
        glib::Object::builder()
            .property("file-metadata-service", file_metadata_service)
            .build()
    }
}

fn header_factory(file_metadata_service: FileMetadataService) -> gtk::ListItemFactory {
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
    factory.connect_bind(move |_, item| {
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
        let name = node
            .tag
            .class()
            .map(|c| file_metadata_service.class_name(&c))
            .unwrap_or_default();
        label.set_markup(&bold(&name));
    });
    factory.upcast()
}

fn name_factory(file_metadata_service: FileMetadataService) -> gtk::ListItemFactory {
    let factory = gtk::SignalListItemFactory::new();
    factory.connect_setup(|_, item| {
        let Some(item) = item.downcast_ref::<gtk::ListItem>() else {
            return;
        };
        item.set_child(Some(&gtk::Label::builder().xalign(0.0).build()));
    });
    factory.connect_bind(move |_, item| {
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
        label.set_text(
            file_metadata_service
                .tag_name(&node.tag)
                .as_deref()
                .unwrap_or_default(),
        );
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

fn description_factory(file_metadata_service: FileMetadataService) -> gtk::ListItemFactory {
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
    factory.connect_bind(move |_, item| {
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
        let description = file_metadata_service
            .tag_name(&node.tag)
            .unwrap_or_default();
        label.set_text(&description);
        label.set_tooltip_text(Some(&description));
    });
    factory.upcast()
}

// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    tags::{Tag, file_metadata::FileMetadata},
    utils::bold,
};
use gettextrs::gettext;
use gtk::{gio, glib, pango, prelude::*, subclass::prelude::*};

pub struct TagNode {
    pub tag: Box<dyn Tag>,
    pub value: String,
}

mod imp {
    use super::*;

    #[derive(Default)]
    pub struct FileMetainfoView {
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

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }
    }

    impl WidgetImpl for FileMetainfoView {}
}

glib::wrapper! {
    pub struct FileMetainfoView(ObjectSubclass<imp::FileMetainfoView>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl FileMetainfoView {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn set_metadata(&self, metadata: &mut FileMetadata) {
        self.imp().view.set_model(None::<&gtk::SelectionModel>);

        let model: gio::ListStore = metadata
            .groups()
            .map(|tags| {
                tags.iter()
                    .map(|(tag, value)| {
                        glib::BoxedAnyObject::new(TagNode {
                            tag: tag.as_ref().clone(),
                            // GLib doesn't like NUL characters in strings
                            value: value.replace('\0', "").to_string(),
                        })
                    })
                    .collect::<gio::ListStore>()
            })
            .collect();

        let model = gtk::FlattenListModel::new(Some(model));

        let selection_model = gtk::NoSelection::new(Some(model));

        self.imp().view.set_model(Some(&selection_model));
    }
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
        label.set_markup(&bold(&node.tag.class()));
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
        label.set_text(&node.tag.name());
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
        let description = node.tag.description();
        label.set_text(&description);
        label.set_tooltip_text(Some(&description));
    });
    factory.upcast()
}

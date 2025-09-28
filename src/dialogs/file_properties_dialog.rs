/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
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

use crate::{file::File, tags::tags::FileMetadataService, utils::SenderExt};
use gettextrs::gettext;
use gtk::{gio, glib, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{
        chmod_component::ChmodComponent,
        chown_component::ChownComponent,
        connection::{connection::ConnectionExt, device::ConnectionDevice},
        dir::Directory,
        file_metainfo_view::FileMetainfoView,
        tags::{file_metadata::FileMetadata, tags::FileMetadataService},
        types::SizeDisplayMode,
        utils::{
            ErrorMessage, attributes_bold, dialog_button_box, display_help, handle_escape_key,
            nice_size, time_to_string,
        },
    };
    use futures::StreamExt;
    use std::{
        cell::{OnceCell, RefCell},
        sync::OnceLock,
    };

    pub const SIGNAL_DIALOG_RESPONSE: &str = "dialog-response";

    #[derive(glib::Properties, Default)]
    #[properties(wrapper_type = super::FilePropertiesDialog)]
    pub struct FilePropertiesDialog {
        notebook: gtk::Notebook,
        filename_label: gtk::Label,
        filename_entry: gtk::Entry,
        chown_component: ChownComponent,
        chmod_component: ChmodComponent,
        #[property(get, construct_only)]
        file: OnceCell<File>,
        file_metadata: RefCell<Option<FileMetadata>>,
        #[property(get, construct_only)]
        file_metadata_service: OnceCell<FileMetadataService>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for FilePropertiesDialog {
        const NAME: &'static str = "GnomeCmdFilePropertiesDialog";
        type Type = super::FilePropertiesDialog;
        type ParentType = gtk::Window;

        fn new() -> Self {
            let filename_entry = gtk::Entry::builder()
                .hexpand(true)
                .activates_default(true)
                .build();
            let filename_label = gtk::Label::builder()
                .mnemonic_widget(&filename_entry)
                .halign(gtk::Align::Start)
                .valign(gtk::Align::Center)
                .build();
            Self {
                notebook: gtk::Notebook::builder().vexpand(true).build(),
                filename_label,
                filename_entry,
                chown_component: Default::default(),
                chmod_component: Default::default(),
                file: Default::default(),
                file_metadata: Default::default(),
                file_metadata_service: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for FilePropertiesDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let dlg = self.obj();
            dlg.set_title(Some(&gettext("File Properties")));
            dlg.set_modal(true);

            let content = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .margin_top(12)
                .margin_bottom(12)
                .margin_start(12)
                .margin_end(12)
                .spacing(6)
                .build();
            dlg.set_child(Some(&content));

            content.append(&self.notebook);

            self.notebook.append_page(
                &self.properties_tab(),
                Some(&gtk::Label::new(Some(&gettext("Properties")))),
            );
            self.notebook.append_page(
                &self.permissions_tab(),
                Some(&gtk::Label::new(Some(&gettext("Permissions")))),
            );
            self.notebook.append_page(
                &self.metadata_tab(),
                Some(&gtk::Label::new(Some(&gettext("Metadata")))),
            );

            let help_button = gtk::Button::builder()
                .label(gettext("_Help"))
                .use_underline(true)
                .build();
            help_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.help_clicked().await });
                }
            ));

            let cancel_button = gtk::Button::builder()
                .label(gettext("_Cancel"))
                .use_underline(true)
                .build();
            let ok_button = gtk::Button::builder()
                .label(gettext("_OK"))
                .use_underline(true)
                .build();

            self.obj().set_default_widget(Some(&ok_button));

            cancel_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.obj().close()
            ));
            ok_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.ok_clicked().await });
                }
            ));

            content.append(&dialog_button_box(
                &[&help_button],
                &[&cancel_button, &ok_button],
            ));

            handle_escape_key(
                self.obj().upcast_ref(),
                &gtk::CallbackAction::new(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    #[upgrade_or]
                    glib::Propagation::Proceed,
                    move |_, _| {
                        imp.obj().close();
                        glib::Propagation::Proceed
                    }
                )),
            );
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    glib::subclass::Signal::builder(SIGNAL_DIALOG_RESPONSE)
                        .param_types([bool::static_type()])
                        .build(),
                ]
            })
        }
    }

    impl WidgetImpl for FilePropertiesDialog {}

    impl WindowImpl for FilePropertiesDialog {
        fn close_request(&self) -> glib::Propagation {
            self.emit_response(false);
            self.parent_close_request()
        }
    }

    impl FilePropertiesDialog {
        fn properties_tab(&self) -> gtk::Widget {
            let file = self.obj().file();
            let file_info = file.file_info();

            let tab = gtk::Grid::builder()
                .margin_top(6)
                .margin_bottom(6)
                .margin_start(6)
                .margin_end(6)
                .column_spacing(12)
                .row_spacing(6)
                .build();

            self.filename_label
                .set_label(&if file.downcast_ref::<Directory>().is_some() {
                    gettext("Directory name:")
                } else {
                    gettext("File name:")
                });
            self.filename_entry.set_text(&file.file_info().edit_name());

            tab.attach(&self.filename_label, 0, 0, 1, 1);
            tab.attach(&self.filename_entry, 1, 0, 1, 1);

            let mut y = 1;
            if let Some(symlink_target) = file_info
                .is_symlink()
                .then(|| file_info.symlink_target())
                .flatten()
            {
                attach_labels(
                    &tab,
                    gettext("Symlink target:"),
                    symlink_target.display(),
                    &mut y,
                );
            }

            tab.attach(
                &gtk::Separator::new(gtk::Orientation::Horizontal),
                0,
                y,
                2,
                1,
            );
            y += 1;

            if file.is_local() {
                if let Some(dir) = file.get_dirname().as_ref().and_then(|p| p.to_str()) {
                    attach_labels(&tab, gettext("Location:"), dir, &mut y);
                }

                let connection = file.connection();
                let uuid = connection
                    .downcast_ref::<ConnectionDevice>()
                    .and_then(|d| d.mount())
                    .and_then(|m| m.uuid())
                    .map(|u| format!(" ({})", u))
                    .unwrap_or_default();
                attach_labels(
                    &tab,
                    gettext("Volume:"),
                    format!("{}{}", connection.alias().unwrap_or_default(), uuid),
                    &mut y,
                );

                if connection.can_show_free_space() {
                    match file.free_space() {
                        Ok(free_space) => {
                            attach_labels(
                                &tab,
                                gettext("Free space:"),
                                &glib::format_size(free_space),
                                &mut y,
                            );
                        }
                        Err(error) => {
                            eprintln!("Failed to get free space: {error}");
                        }
                    }
                }
            }

            if let Some(content_type) = file.content_type() {
                attach_labels(&tab, gettext("Content type:"), &content_type, &mut y);
            }

            let file_type = file_info.file_type();
            if file_type != gio::FileType::Directory {
                attach_labels(
                    &tab,
                    gettext("Opens with:"),
                    &file
                        .app_info_for_content_type()
                        .map(|app| app.name().to_string())
                        .unwrap_or_else(|| gettext("No default application registered")),
                    &mut y,
                );
            }

            tab.attach(
                &gtk::Separator::new(gtk::Orientation::Horizontal),
                0,
                y,
                2,
                1,
            );
            y += 1;

            if let Some(dt) = file_info.modification_date_time() {
                match time_to_string(dt, "%c") {
                    Ok(str) => {
                        attach_labels(&tab, gettext("Modified:"), &str, &mut y);
                    }
                    Err(error) => eprintln!("Failed to format file modification time: {error}"),
                }
            }
            if let Some(dt) = file_info.access_date_time() {
                match time_to_string(dt, "%c") {
                    Ok(str) => {
                        attach_labels(&tab, gettext("Accessed:"), &str, &mut y);
                    }
                    Err(error) => eprintln!("Failed to format file access time: {error}"),
                }
            }

            tab.attach(
                &gtk::Separator::new(gtk::Orientation::Horizontal),
                0,
                y,
                2,
                1,
            );
            y += 1;

            let size_label = attach_labels(
                &tab,
                gettext("Size:"),
                &nice_size(file_info.size() as u64, SizeDisplayMode::Grouped),
                &mut y,
            );
            if file_type == gio::FileType::Directory {
                self.do_calc_tree_size(size_label);
            }

            if file_type != gio::FileType::Special {
                let file_metadata_service = self.obj().file_metadata_service();

                let metadata = file_metadata_service.extract_metadata(&file);

                for (title, value) in file_metadata_service.file_summary(&metadata) {
                    attach_labels(&tab, format!("{title}:"), truncate(&value, 120), &mut y);
                }

                self.file_metadata.replace(Some(metadata));
            }

            tab.upcast()
        }

        fn permissions_tab(&self) -> gtk::Widget {
            let file_info = self.obj().file().file_info();

            let tab = gtk::Grid::builder()
                .margin_top(6)
                .margin_bottom(6)
                .margin_start(6)
                .margin_end(6)
                .row_spacing(6)
                .build();

            tab.attach(
                &gtk::Label::builder()
                    .label(gettext("Owner and group"))
                    .halign(gtk::Align::Start)
                    .valign(gtk::Align::Center)
                    .attributes(&attributes_bold())
                    .build(),
                0,
                0,
                1,
                1,
            );

            self.chown_component.set_ownership(
                file_info.attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_UID),
                file_info.attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_GID),
            );
            self.chown_component.set_hexpand(true);
            tab.attach(&self.chown_component, 0, 1, 1, 1);

            tab.attach(
                &gtk::Label::builder()
                    .label(gettext("Access permissions"))
                    .halign(gtk::Align::Start)
                    .valign(gtk::Align::Center)
                    .attributes(&attributes_bold())
                    .build(),
                0,
                2,
                1,
                1,
            );

            self.chmod_component
                .set_permissions(file_info.attribute_uint32(gio::FILE_ATTRIBUTE_UNIX_MODE));
            self.chmod_component.set_hexpand(true);
            tab.attach(&self.chmod_component, 0, 3, 1, 1);

            tab.upcast()
        }

        fn metadata_tab(&self) -> gtk::Widget {
            let tab = gtk::Grid::builder().row_spacing(6).build();

            let view = FileMetainfoView::new(&self.obj().file_metadata_service());
            view.set_hexpand(true);
            view.set_vexpand(true);
            view.set_file(Some(self.obj().file()));
            tab.attach(&view, 0, 0, 2, 1);

            let copy_button = gtk::Button::builder()
                .label(gettext("Co_py"))
                .use_underline(true)
                .hexpand(false)
                .halign(gtk::Align::Start)
                .margin_start(12)
                .margin_end(12)
                .margin_bottom(12)
                .build();
            tab.attach(&copy_button, 0, 1, 1, 1);

            copy_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.copy_clicked()
            ));

            tab.upcast()
        }

        fn emit_response(&self, changed: bool) {
            self.obj()
                .emit_by_name::<()>(SIGNAL_DIALOG_RESPONSE, &[&changed]);
        }

        fn do_calc_tree_size(&self, size_label: gtk::Label) {
            let (fut, mut stream) = self
                .obj()
                .file()
                .file()
                .measure_disk_usage_future(gio::FileMeasureFlags::NONE, glib::Priority::LOW);
            glib::spawn_future_local({
                let size_label = size_label.clone();
                async move {
                    while let Some((reporting, disk_usage, _num_dirs, _num_files)) =
                        stream.next().await
                    {
                        if reporting {
                            if disk_usage != 0 {
                                size_label
                                    .set_label(&nice_size(disk_usage, SizeDisplayMode::Grouped));
                            }
                        } else {
                            break;
                        }
                    }
                }
            });
            glib::spawn_future_local(async move {
                match fut.await {
                    Ok((disk_usage, _num_dirs, _num_files)) => {
                        size_label.set_label(&nice_size(disk_usage, SizeDisplayMode::Grouped));
                    }
                    Err(error) if error.matches(gio::IOErrorEnum::Cancelled) => {}
                    Err(error) => {
                        eprintln!("measure_disk_usage error: {}", error.message());
                    }
                }
            });
        }

        fn copy_clicked(&self) {
            if let Some(metadata) = self.file_metadata.borrow().as_ref() {
                let tsv = self.obj().file_metadata_service().to_tsv(&*metadata);
                self.obj().clipboard().set_text(&tsv);
            }
        }

        async fn help_clicked(&self) {
            let link_id = match self.notebook.current_page() {
                Some(1) => "gnome-commander-permissions",
                Some(2) => "gnome-commander-advanced-rename-metadata-tags",
                _ => "gnome-commander-file-properties",
            };
            display_help(self.obj().upcast_ref(), Some(link_id)).await;
        }

        async fn ok_clicked(&self) {
            match self.apply_changes() {
                Ok(changed) => {
                    self.emit_response(changed);
                }
                Err(error) => {
                    ErrorMessage::with_error(&gettext("Failed to apply changes."), &error)
                        .show(self.obj().upcast_ref())
                        .await;
                }
            }
        }

        fn apply_changes(&self) -> Result<bool, glib::Error> {
            let file = self.obj().file();
            let mut changed = false;

            let filename = self.filename_entry.text();
            if filename != file.get_name() {
                file.rename(&filename)?;
                changed = true;
            }

            let permissions = self.chmod_component.permissions();
            if permissions != file.permissions() {
                file.chmod(permissions)?;
                changed = true;
            }

            let (uid, gid) = self.chown_component.ownership();
            if uid != file.uid() || gid != file.gid() {
                file.chown(Some(uid), gid)?;
                changed = true;
            }

            Ok(changed)
        }
    }

    fn attach_labels(
        grid: &gtk::Grid,
        name: impl ToString,
        value: impl ToString,
        y: &mut i32,
    ) -> gtk::Label {
        let name = gtk::Label::builder()
            .label(name.to_string())
            .halign(gtk::Align::Start)
            .valign(gtk::Align::Center)
            .build();
        grid.attach(&name, 0, *y, 1, 1);
        let value = gtk::Label::builder()
            .label(value.to_string())
            .halign(gtk::Align::Start)
            .valign(gtk::Align::Center)
            .build();
        grid.attach(&value, 1, *y, 1, 1);
        *y += 1;
        value
    }
}

glib::wrapper! {
    pub struct FilePropertiesDialog(ObjectSubclass<imp::FilePropertiesDialog>)
        @extends gtk::Window, gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native;
}

impl FilePropertiesDialog {
    pub fn new(
        parent_window: &gtk::Window,
        file_metadata_service: &FileMetadataService,
        file: &File,
    ) -> Self {
        glib::Object::builder()
            .property("transient-for", parent_window)
            .property("file-metadata-service", file_metadata_service)
            .property("file", file)
            .build()
    }

    pub async fn show(
        parent_window: &gtk::Window,
        file_metadata_service: &FileMetadataService,
        file: &File,
    ) -> bool {
        if file.is_dotdot() {
            return false;
        }

        let dialog = Self::new(parent_window, file_metadata_service, file);

        let (sender, receiver) = async_channel::bounded::<bool>(1);
        dialog.connect(
            imp::SIGNAL_DIALOG_RESPONSE,
            false,
            glib::clone!(move |values| {
                let changed = values
                    .get(1)
                    .and_then(|v| v.get::<bool>().ok())
                    .unwrap_or_default();
                sender.toss(changed);
                None
            }),
        );

        dialog.present();
        let changed = receiver.recv().await == Ok(true);
        dialog.close();

        changed
    }
}

fn truncate(s: &str, n: usize) -> String {
    let mut iter = s.chars();
    let mut substr: String = iter.by_ref().take(n).collect();
    if iter.next().is_some() {
        substr.push_str("...");
    }
    substr
}

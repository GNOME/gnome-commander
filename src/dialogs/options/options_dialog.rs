// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    confirmation_tab::CondifrmationTab, devices_tab::DevicesTab, filters_tab::FiltersTab,
    format_tab::FormatTab, general_tab::GeneralTab, layout_tab::LayoutTab,
    programs_tab::ProgramsTab, tabs_tab::TabsTab,
};
use crate::{
    main_win::MainWindow,
    options::{
        ColorOptions, ConfirmOptions, FiltersOptions, GeneralOptions, ProgramsOptions,
        types::WriteResult, utils::remember_window_size,
    },
    shortcuts::Area,
    user_actions::UserAction,
    utils::{SenderExt, WindowExt, dialog_button_box, display_help},
};
use gettextrs::gettext;
use gtk::{glib, prelude::*, subclass::prelude::*};
use std::sync::Mutex;

mod imp {
    use super::*;

    pub struct OptionsDialog {
        pub(super) notebook: gtk::Notebook,
        pub(super) general_tab: GeneralTab,
        pub(super) format_tab: FormatTab,
        pub(super) layout_tab: LayoutTab,
        pub(super) tabs_tab: TabsTab,
        pub(super) confirmation_tab: CondifrmationTab,
        pub(super) filters_tab: FiltersTab,
        pub(super) programs_tab: ProgramsTab,
        pub(super) devices_tab: DevicesTab,
        sender: async_channel::Sender<bool>,
        pub(super) receiver: async_channel::Receiver<bool>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for OptionsDialog {
        const NAME: &'static str = "GnomeCmdOptionsDialog";
        type Type = super::OptionsDialog;
        type ParentType = gtk::Window;

        fn class_init(klass: &mut Self::Class) {
            klass.install_action(UserAction::ViewNextTab.name(), None, |obj, _, _| {
                obj.imp().notebook.emit_change_current_page(1);
            });
            klass.install_action(UserAction::ViewPrevTab.name(), None, |obj, _, _| {
                obj.imp().notebook.emit_change_current_page(-1);
            });
        }
    }

    impl OptionsDialog {
        fn new() -> Self {
            let (sender, receiver) = async_channel::bounded(1);

            Self {
                notebook: gtk::Notebook::builder().hexpand(true).vexpand(true).build(),
                general_tab: GeneralTab::new(),
                format_tab: FormatTab::new(),
                layout_tab: LayoutTab::new(),
                tabs_tab: TabsTab::new(),
                confirmation_tab: CondifrmationTab::new(),
                filters_tab: FiltersTab::new(),
                programs_tab: ProgramsTab::new(),
                devices_tab: DevicesTab::new(),
                sender,
                receiver,
            }
        }
    }

    impl Default for OptionsDialog {
        fn default() -> Self {
            Self::new()
        }
    }

    impl ObjectImpl for OptionsDialog {
        fn constructed(&self) {
            self.parent_constructed();

            let obj = self.obj();
            obj.add_css_class("dialog");
            obj.set_title(Some(&gettext("Options")));
            obj.set_modal(true);

            let content_area = gtk::Box::builder()
                .orientation(gtk::Orientation::Vertical)
                .build();
            obj.set_child(Some(&content_area));

            let general_options = GeneralOptions::new();
            let color_options = ColorOptions::new();
            let confirmation_options = ConfirmOptions::new();
            let filters_options = FiltersOptions::new();
            let programs_options = ProgramsOptions::new();

            remember_window_size(
                &*obj,
                &general_options.options_window_width,
                &general_options.options_window_height,
            );

            content_area.append(&self.notebook);

            self.general_tab.read(&general_options);
            self.format_tab.read(&general_options);
            self.layout_tab.read(&general_options, &color_options);
            self.tabs_tab.read(&general_options);
            self.confirmation_tab.read(&confirmation_options);
            self.filters_tab.read(&filters_options);
            self.programs_tab.read(&general_options, &programs_options);
            self.devices_tab.read(&general_options);

            self.notebook.append_page(
                &self.general_tab.widget(),
                Some(&gtk::Label::builder().label(gettext("General")).build()),
            );
            self.notebook.append_page(
                &self.format_tab.widget(),
                Some(&gtk::Label::builder().label(gettext("Format")).build()),
            );
            self.notebook.append_page(
                &self.layout_tab.widget(),
                Some(&gtk::Label::builder().label(gettext("Layout")).build()),
            );
            self.notebook.append_page(
                &self.tabs_tab.widget(),
                Some(&gtk::Label::builder().label(gettext("Tabs")).build()),
            );
            self.notebook.append_page(
                &self.confirmation_tab.widget(),
                Some(&gtk::Label::builder().label(gettext("Confirmation")).build()),
            );
            self.notebook.append_page(
                &self.filters_tab.widget(),
                Some(&gtk::Label::builder().label(gettext("Filters")).build()),
            );
            self.notebook.append_page(
                &self.programs_tab.widget(),
                Some(&gtk::Label::builder().label(gettext("Programs")).build()),
            );
            self.notebook.append_page(
                &self.devices_tab.widget(),
                Some(&gtk::Label::builder().label(gettext("Devices")).build()),
            );

            self.notebook.connect_page_notify(|notebook| {
                // Make sure to focus first element of the tab
                notebook.grab_focus();
                notebook.child_focus(gtk::DirectionType::TabForward);
            });

            let help_button = gtk::Button::builder()
                .label(gettext("_Help"))
                .use_underline(true)
                .build();
            let cancel_button = gtk::Button::builder()
                .label(gettext("_Cancel"))
                .use_underline(true)
                .build();
            let ok_button = gtk::Button::builder()
                .label(gettext("_Save"))
                .use_underline(true)
                .build();

            content_area.append(&dialog_button_box(
                &[&help_button],
                &[&cancel_button, &ok_button],
            ));

            help_button.connect_clicked(glib::clone!(
                #[weak]
                obj,
                #[weak(rename_to = notebook)]
                self.notebook,
                move |_| {
                    let link_id = match notebook.current_page() {
                        Some(0) => Some("gnome-commander-prefs-general"),
                        Some(1) => Some("gnome-commander-prefs-format"),
                        Some(2) => Some("gnome-commander-prefs-layout"),
                        Some(3) => Some("gnome-commander-prefs-tabs"),
                        Some(4) => Some("gnome-commander-prefs-confirmation"),
                        Some(5) => Some("gnome-commander-prefs-filters"),
                        Some(6) => Some("gnome-commander-prefs-programs"),
                        Some(7) => Some("gnome-commander-prefs-devices"),
                        _ => None,
                    };
                    glib::spawn_future_local(async move {
                        display_help(obj.upcast_ref(), link_id).await;
                    });
                }
            ));

            cancel_button.connect_clicked(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_| sender.toss(false)
            ));
            ok_button.connect_clicked(glib::clone!(
                #[strong(rename_to = sender)]
                self.sender,
                move |_| sender.toss(true)
            ));

            obj.set_default_widget(Some(&ok_button));
            obj.set_cancel_widget(&cancel_button);
        }
    }

    impl WidgetImpl for OptionsDialog {}
    impl WindowImpl for OptionsDialog {}
}

glib::wrapper! {
    pub struct OptionsDialog(ObjectSubclass<imp::OptionsDialog>)
        @extends gtk::Widget, gtk::Window,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::ShortcutManager, gtk::Root, gtk::Native;
}

impl OptionsDialog {
    pub fn notebook(&self) -> &gtk::Notebook {
        &self.imp().notebook
    }

    pub fn receiver(&self) -> &async_channel::Receiver<bool> {
        &self.imp().receiver
    }

    pub fn save(&self) -> WriteResult {
        let general_options = GeneralOptions::new();
        let color_options = ColorOptions::new();
        let confirmation_options = ConfirmOptions::new();
        let filters_options = FiltersOptions::new();
        let programs_options = ProgramsOptions::new();

        self.imp().general_tab.write(&general_options)?;
        self.imp().format_tab.write(&general_options)?;
        self.imp()
            .layout_tab
            .write(&general_options, &color_options)?;
        self.imp().tabs_tab.write(&general_options)?;
        self.imp().confirmation_tab.write(&confirmation_options)?;
        self.imp().filters_tab.write(&filters_options)?;
        self.imp()
            .programs_tab
            .write(&general_options, &programs_options)?;
        self.imp().devices_tab.write(&general_options)?;
        Ok(())
    }
}

pub async fn show_options_dialog(parent_window: &MainWindow) -> bool {
    let dialog: OptionsDialog = glib::Object::builder().build();
    dialog.set_transient_for(Some(parent_window));

    parent_window
        .shortcuts()
        .add_controller(dialog.notebook(), Area::Panel);

    dialog.present();

    // open the tab which was active when closing the options notebook last time
    static LAST_ACTIVE_TAB: Mutex<u32> = Mutex::new(0);
    let notebook = &dialog.imp().notebook;
    notebook.set_current_page(None); // making sure our page change handler is triggered
    notebook.set_current_page(Some(LAST_ACTIVE_TAB.lock().map(|g| *g).unwrap_or_default()));

    let result = dialog.receiver().recv().await == Ok(true);
    if result && let Err(error) = dialog.save() {
        eprintln!("{error}");
    }

    if let Ok(mut last_active_tab) = LAST_ACTIVE_TAB.lock() {
        *last_active_tab = dialog.notebook().current_page().unwrap_or_default();
    }
    dialog.close();

    result
}

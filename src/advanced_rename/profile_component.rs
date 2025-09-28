/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
 * Copyright 2024-2025 Andrey Kutejko <andy128k@gmail.com>
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

use super::regex_dialog::RegexReplace;
use crate::{
    advanced_rename::profile::AdvancedRenameProfile,
    tags::tags::FileMetadataService,
    utils::{NO_BUTTONS, dialog_button_box},
};
use gettextrs::gettext;
use gtk::{gio, glib, pango, prelude::*, subclass::prelude::*};

mod imp {
    use super::*;
    use crate::{
        advanced_rename::{
            profile::{CaseConversion, TrimBlanks},
            regex_dialog::show_advrename_regex_dialog,
        },
        dialogs::order_utils::ordering_buttons,
        history_entry::HistoryEntry,
        utils::{MenuBuilderExt, attributes_bold},
    };
    use std::{
        cell::{Cell, OnceCell, RefCell},
        sync::OnceLock,
    };

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::AdvancedRenameProfileComponent)]
    pub struct AdvancedRenameProfileComponent {
        #[property(get, construct_only)]
        file_metadata_service: OnceCell<FileMetadataService>,
        #[property(get, set, nullable)]
        profile: RefCell<Option<AdvancedRenameProfile>>,

        pub template_entry: HistoryEntry,
        metadata_button: gtk::MenuButton,

        pub counter_start_spin: gtk::SpinButton,
        pub counter_step_spin: gtk::SpinButton,
        pub counter_digits_combo: gtk::DropDown,

        pub regex_model: gio::ListStore,
        regex_selection_model: gtk::SingleSelection,
        block_deleted_signal: Cell<i32>,
        regex_view: gtk::ColumnView,

        regex_add_button: gtk::Button,
        regex_edit_button: gtk::Button,
        regex_remove_button: gtk::Button,
        regex_remove_all_button: gtk::Button,

        pub case_combo: gtk::DropDown,
        pub trim_combo: gtk::DropDown,

        pub sample_file_name: RefCell<Option<String>>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for AdvancedRenameProfileComponent {
        const NAME: &'static str = "GnomeCmdAdvancedRenameProfileComponent";
        type Type = super::AdvancedRenameProfileComponent;
        type ParentType = gtk::Widget;

        fn class_init(klass: &mut Self::Class) {
            klass.install_action(
                "advrenametag.insert-text-tag",
                Some(&String::static_variant_type()),
                |obj, _, param| {
                    if let Some(text) = param.and_then(|p| p.str()) {
                        obj.imp().insert_tag(text)
                    }
                },
            );
            klass.install_action_async(
                "advrenametag.insert-range",
                Some(&String::static_variant_type()),
                |obj, _, param| async move {
                    if let Some(text) = param.as_ref().and_then(|p| p.str()) {
                        obj.imp().insert_range(text).await;
                    }
                },
            );
        }

        fn new() -> Self {
            let regex_model = gio::ListStore::new::<glib::BoxedAnyObject>();
            let regex_selection_model = gtk::SingleSelection::new(Some(regex_model.clone()));

            Self {
                file_metadata_service: OnceCell::new(),
                profile: Default::default(),

                template_entry: Default::default(),
                metadata_button: gtk::MenuButton::builder().label(gettext("Metatag")).build(),

                counter_start_spin: gtk::SpinButton::with_range(0.0, 1_000_000.0, 1.0),
                counter_step_spin: gtk::SpinButton::with_range(-1000.0, 1000.0, 1.0),
                counter_digits_combo: gtk::DropDown::builder()
                    .model(&counter_digits_model())
                    .build(),

                regex_view: gtk::ColumnView::builder()
                    .model(&regex_selection_model)
                    .build(),
                regex_model,
                regex_selection_model,
                block_deleted_signal: Default::default(),

                regex_add_button: gtk::Button::builder()
                    .label(gettext("_Add"))
                    .use_underline(true)
                    .build(),
                regex_edit_button: gtk::Button::builder()
                    .label(gettext("_Edit"))
                    .use_underline(true)
                    .build(),
                regex_remove_button: gtk::Button::builder()
                    .label(gettext("_Remove"))
                    .use_underline(true)
                    .build(),
                regex_remove_all_button: gtk::Button::builder()
                    .label(gettext("Remove A_ll"))
                    .use_underline(true)
                    .build(),

                case_combo: gtk::DropDown::builder().model(&case_modes()).build(),
                trim_combo: gtk::DropDown::builder().model(&trim_modes()).build(),

                sample_file_name: Default::default(),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for AdvancedRenameProfileComponent {
        fn constructed(&self) {
            self.parent_constructed();

            let this = self.obj();
            this.set_layout_manager(Some(
                gtk::BoxLayout::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .build(),
            ));

            let hbox = gtk::Box::builder()
                .orientation(gtk::Orientation::Horizontal)
                .spacing(18)
                .hexpand(true)
                .build();
            hbox.set_parent(&*this);

            // Template
            {
                let vbox = gtk::Box::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .spacing(6)
                    .hexpand(true)
                    .build();
                hbox.append(&vbox);

                let label = gtk::Label::builder()
                    .label(gettext("_Template"))
                    .use_underline(true)
                    .attributes(&attributes_bold())
                    .halign(gtk::Align::Start)
                    .valign(gtk::Align::Center)
                    .mnemonic_widget(&self.template_entry)
                    .build();
                vbox.append(&label);

                self.template_entry.entry().set_activates_default(true);
                vbox.append(&self.template_entry);

                let bbox = gtk::Box::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .spacing(6)
                    .build();
                let bbox_size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Horizontal);
                vbox.append(&bbox);

                let button = gtk::MenuButton::builder()
                    .label(gettext("Directory"))
                    .menu_model(&create_directory_tag_menu())
                    .build();
                bbox_size_group.add_widget(&button);
                bbox.append(&button);

                let button = gtk::MenuButton::builder()
                    .label(gettext("File"))
                    .menu_model(&create_file_tag_menu())
                    .build();
                bbox_size_group.add_widget(&button);
                bbox.append(&button);

                let button = gtk::MenuButton::builder()
                    .label(gettext("Counter"))
                    .menu_model(&create_counter_tag_menu())
                    .build();
                bbox_size_group.add_widget(&button);
                bbox.append(&button);

                let button = gtk::MenuButton::builder()
                    .label(gettext("Date"))
                    .menu_model(&create_date_tag_menu())
                    .build();
                bbox_size_group.add_widget(&button);
                bbox.append(&button);

                bbox_size_group.add_widget(&self.metadata_button);
                self.metadata_button.set_menu_model(Some(
                    &this
                        .file_metadata_service()
                        .create_menu("advrenametag.insert-text-tag"),
                ));
                if let Some(popover) = self
                    .metadata_button
                    .popover()
                    .and_downcast::<gtk::PopoverMenu>()
                {
                    popover.set_flags(gtk::PopoverMenuFlags::NESTED);
                }

                bbox.append(&self.metadata_button);
            }

            // Counter
            {
                let grid = gtk::Grid::builder()
                    .row_spacing(6)
                    .column_spacing(12)
                    .margin_start(12)
                    .build();
                hbox.append(&grid);

                let label = gtk::Label::builder()
                    .label(gettext("Counter"))
                    .use_underline(true)
                    .attributes(&attributes_bold())
                    .halign(gtk::Align::Start)
                    .valign(gtk::Align::Center)
                    .build();
                grid.attach(&label, 0, 0, 2, 1);

                grid.attach(
                    &gtk::Label::builder()
                        .label(gettext("_Start:"))
                        .use_underline(true)
                        .mnemonic_widget(&self.counter_start_spin)
                        .halign(gtk::Align::Start)
                        .valign(gtk::Align::Center)
                        .build(),
                    0,
                    1,
                    1,
                    1,
                );
                grid.attach(&self.counter_start_spin, 1, 1, 1, 1);

                grid.attach(
                    &gtk::Label::builder()
                        .label(gettext("Ste_p:"))
                        .use_underline(true)
                        .mnemonic_widget(&self.counter_step_spin)
                        .halign(gtk::Align::Start)
                        .valign(gtk::Align::Center)
                        .build(),
                    0,
                    2,
                    1,
                    1,
                );
                grid.attach(&self.counter_step_spin, 1, 2, 1, 1);

                grid.attach(
                    &gtk::Label::builder()
                        .label(gettext("Di_gits:"))
                        .use_underline(true)
                        .mnemonic_widget(&self.counter_digits_combo)
                        .halign(gtk::Align::Start)
                        .valign(gtk::Align::Center)
                        .build(),
                    0,
                    3,
                    1,
                    1,
                );
                grid.attach(&self.counter_digits_combo, 1, 3, 1, 1);
            }

            // Regex
            {
                let label = gtk::Label::builder()
                    .label(gettext("Regex replacing"))
                    .attributes(&attributes_bold())
                    .halign(gtk::Align::Start)
                    .valign(gtk::Align::Center)
                    .build();
                label.set_parent(&*this);

                let grid = gtk::Grid::builder()
                    .row_spacing(6)
                    .column_spacing(12)
                    .margin_top(6)
                    .margin_bottom(12)
                    .build();
                grid.set_parent(&*this);

                create_regex_view(&self.regex_view);

                let scrolled_window = gtk::ScrolledWindow::builder()
                    .hscrollbar_policy(gtk::PolicyType::Never)
                    .vscrollbar_policy(gtk::PolicyType::Automatic)
                    .has_frame(true)
                    .hexpand(true)
                    .vexpand(true)
                    .child(&self.regex_view)
                    .build();
                grid.attach(&scrolled_window, 0, 0, 1, 1);

                let bbox = gtk::Box::builder()
                    .orientation(gtk::Orientation::Vertical)
                    .spacing(12)
                    .build();
                grid.attach(&bbox, 1, 0, 1, 1);

                bbox.append(&self.regex_add_button);
                bbox.append(&self.regex_edit_button);
                bbox.append(&self.regex_remove_button);
                bbox.append(&self.regex_remove_all_button);

                let (up_button, down_button) = ordering_buttons(&self.regex_selection_model);
                bbox.append(&up_button);
                bbox.append(&down_button);
            }

            // Case conversion & blank trimming
            {
                let hbox = gtk::Box::builder()
                    .orientation(gtk::Orientation::Horizontal)
                    .spacing(12)
                    .margin_bottom(18)
                    .build();
                hbox.set_parent(&*this);

                let label = gtk::Label::builder()
                    .label(gettext("Case"))
                    .attributes(&attributes_bold())
                    .halign(gtk::Align::Start)
                    .valign(gtk::Align::Center)
                    .mnemonic_widget(&self.case_combo)
                    .build();
                hbox.append(&label);

                hbox.append(&self.case_combo);

                let label = gtk::Label::builder()
                    .label(gettext("Trim blanks"))
                    .attributes(&attributes_bold())
                    .halign(gtk::Align::Start)
                    .valign(gtk::Align::Center)
                    .mnemonic_widget(&self.trim_combo)
                    .build();
                hbox.append(&label);

                hbox.append(&self.trim_combo);
            }

            self.template_entry.entry().connect_changed(glib::clone!(
                #[weak]
                this,
                move |_| this.emit_by_name::<()>("template-changed", &[])
            ));

            self.counter_start_spin.connect_value_changed(glib::clone!(
                #[weak]
                this,
                move |spin| {
                    let value: u32 = spin.value_as_int().try_into().unwrap_or_default();
                    if let Some(profile) = this.profile() {
                        profile.set_counter_start(value);
                    }
                    this.emit_by_name::<()>("counter-changed", &[]);
                }
            ));
            self.counter_step_spin.connect_value_changed(glib::clone!(
                #[weak]
                this,
                move |spin| {
                    let value = spin.value_as_int();
                    if let Some(profile) = this.profile() {
                        profile.set_counter_step(value);
                    }
                    this.emit_by_name::<()>("counter-changed", &[]);
                }
            ));
            self.counter_digits_combo
                .connect_selected_notify(glib::clone!(
                    #[weak]
                    this,
                    move |dropdown| {
                        let value = dropdown.selected().clamp(0, 16);
                        if let Some(profile) = this.profile() {
                            profile.set_counter_width(value);
                        }
                        this.emit_by_name::<()>("counter-changed", &[]);
                    }
                ));

            self.regex_model.connect_items_changed(glib::clone!(
                #[weak]
                this,
                move |_, _, _, _| if this.imp().block_deleted_signal.get() == 0 {
                    this.emit_by_name::<()>("regex-changed", &[])
                }
            ));
            self.regex_view.connect_activate(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, _| {
                    glib::spawn_future_local(async move { imp.edit_regex().await });
                }
            ));
            self.regex_add_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.add_regex().await });
                }
            ));
            self.regex_edit_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| {
                    glib::spawn_future_local(async move { imp.edit_regex().await });
                }
            ));
            self.regex_remove_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.remove_regex()
            ));
            self.regex_remove_all_button.connect_clicked(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_| imp.remove_all_regexes()
            ));
            this.connect_local(
                "regex-changed",
                true,
                glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    #[upgrade_or]
                    None,
                    move |_| {
                        imp.regex_changed();
                        None
                    }
                ),
            );

            self.case_combo.connect_selected_notify(glib::clone!(
                #[weak]
                this,
                move |dropdown| {
                    if let Some(case_conversion) = CaseConversion::from_repr(dropdown.selected()) {
                        if let Some(profile) = this.profile() {
                            profile.set_case_conversion(case_conversion);
                            this.emit_by_name::<()>("regex-changed", &[]);
                        }
                    }
                }
            ));
            self.trim_combo.connect_selected_notify(glib::clone!(
                #[weak]
                this,
                move |dropdown| {
                    if let Some(trim_blanks) = TrimBlanks::from_repr(dropdown.selected()) {
                        if let Some(profile) = this.profile() {
                            profile.set_trim_blanks(trim_blanks);
                            this.emit_by_name::<()>("regex-changed", &[]);
                        }
                    }
                }
            ));
        }

        fn dispose(&self) {
            while let Some(child) = self.obj().first_child() {
                child.unparent();
            }
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![
                    glib::subclass::Signal::builder("template-changed").build(),
                    glib::subclass::Signal::builder("counter-changed").build(),
                    glib::subclass::Signal::builder("regex-changed").build(),
                ]
            })
        }
    }

    impl WidgetImpl for AdvancedRenameProfileComponent {
        fn grab_focus(&self) -> bool {
            self.template_entry.grab_focus()
        }
    }

    impl AdvancedRenameProfileComponent {
        fn insert_tag(&self, tag: &str) {
            let entry = self.template_entry.entry();
            let position0 = entry.position();
            let mut position = position0;
            entry.insert_text(tag, &mut position);
            entry.set_position(position);
            entry.grab_focus();
            entry.select_region(position0, position);
        }

        async fn insert_range(&self, tag: &str) {
            let Some(parent_window) = self.obj().root().and_downcast::<gtk::Window>() else {
                eprintln!("No window");
                return;
            };

            let filename = self
                .sample_file_name
                .borrow()
                .as_ref()
                .map(|name| if tag == "$n" { take_stem(&name) } else { name }.to_owned());

            if let Some(rtag) = get_selected_range(&parent_window, tag, filename.as_deref()).await {
                self.insert_tag(&rtag);
            }
        }

        async fn add_regex(&self) {
            let Some(parent_window) = self.obj().root().and_downcast::<gtk::Window>() else {
                eprintln!("No window");
                return;
            };
            if let Some(rx) =
                show_advrename_regex_dialog(&parent_window, &gettext("Add Rule"), None).await
            {
                self.regex_model.append(&glib::BoxedAnyObject::new(rx));
            }
        }

        async fn edit_regex(&self) {
            let Some(parent_window) = self.obj().root().and_downcast::<gtk::Window>() else {
                eprintln!("No window");
                return;
            };
            let position = self.regex_selection_model.selected();
            if position == gtk::INVALID_LIST_POSITION {
                return;
            }
            let Some(rx) = self
                .regex_model
                .item(position)
                .and_downcast::<glib::BoxedAnyObject>()
                .map(|b| b.borrow::<RegexReplace>().clone())
            else {
                return;
            };
            if let Some(rr) =
                show_advrename_regex_dialog(&parent_window, &gettext("Edit Rule"), Some(&rx)).await
            {
                self.regex_model
                    .splice(position, 1, &[glib::BoxedAnyObject::new(rr)]);
            }
        }

        fn remove_regex(&self) {
            let position = self.regex_selection_model.selected();
            if position == gtk::INVALID_LIST_POSITION {
                return;
            }
            self.regex_model.remove(position);
            self.regex_changed();
        }

        pub fn remove_all_regexes(&self) {
            self.block_deleted_signal
                .set(self.block_deleted_signal.get() + 1);

            self.regex_model.remove_all();

            self.block_deleted_signal
                .set(self.block_deleted_signal.get() - 1);

            self.regex_changed();
        }

        fn regex_changed(&self) {
            let empty = self.regex_model.n_items() == 0;
            self.regex_edit_button.set_sensitive(!empty);
            self.regex_remove_button.set_sensitive(!empty);
            self.regex_remove_all_button.set_sensitive(!empty);
        }
    }

    fn take_stem(name: &str) -> &str {
        if let Some((stem, _ext)) = name.rsplit_once('.') {
            stem
        } else {
            name
        }
    }

    fn create_directory_tag_menu() -> gio::Menu {
        gio::Menu::new()
            .item(gettext("Grandparent"), "advrenametag.insert-text-tag('$g')")
            .item(gettext("Parent"), "advrenametag.insert-text-tag('$p')")
    }

    fn create_file_tag_menu() -> gio::Menu {
        gio::Menu::new()
            .item(gettext("File name"), "advrenametag.insert-text-tag('$N')")
            .item(
                gettext("File name (range)"),
                "advrenametag.insert-range('$N')",
            )
            .item(
                gettext("File name without extension"),
                "advrenametag.insert-text-tag('$n')",
            )
            .item(
                gettext("File name without extension (range)"),
                "advrenametag.insert-range('$n')",
            )
            .item(
                gettext("File extension"),
                "advrenametag.insert-text-tag('$e')",
            )
    }

    fn create_counter_tag_menu() -> gio::Menu {
        gio::Menu::new()
            .item(gettext("Counter"), "advrenametag.insert-text-tag('$c')")
            .item(
                gettext("Counter (width)"),
                "advrenametag.insert-text-tag('$c(2)')",
            )
            .item(
                gettext("Counter (auto)"),
                "advrenametag.insert-text-tag('$c(a)')",
            )
            .item(
                gettext("Hexadecimal random number (width)"),
                "advrenametag.insert-text-tag('$x(8)')",
            )
    }

    fn create_date_tag_menu() -> gio::Menu {
        gio::Menu::new()
            .submenu(
                gettext("Date"),
                gio::Menu::new()
                    .item(gettext("<locale>"), "advrenametag.insert-text-tag('%x')")
                    .item(
                        gettext("yyyy-mm-dd"),
                        "advrenametag.insert-text-tag('%Y-%m-%d')",
                    )
                    .item(
                        gettext("yy-mm-dd"),
                        "advrenametag.insert-text-tag('%y-%m-%d')",
                    )
                    .item(
                        gettext("yy.mm.dd"),
                        "advrenametag.insert-text-tag('%y.%m.%d')",
                    )
                    .item(gettext("yymmdd"), "advrenametag.insert-text-tag('%y%m%d')")
                    .item(
                        gettext("dd.mm.yy"),
                        "advrenametag.insert-text-tag('%d.%m.%y')",
                    )
                    .item(
                        gettext("mm-dd-yy"),
                        "advrenametag.insert-text-tag('%m-%d-%y')",
                    )
                    .item(gettext("yyyy"), "advrenametag.insert-text-tag('%Y')")
                    .item(gettext("yy"), "advrenametag.insert-text-tag('%y')")
                    .item(gettext("mm"), "advrenametag.insert-text-tag('%m')")
                    .item(gettext("mmm"), "advrenametag.insert-text-tag('%b')")
                    .item(gettext("dd"), "advrenametag.insert-text-tag('%d')"),
            )
            .submenu(
                gettext("Time"),
                gio::Menu::new()
                    .item(gettext("<locale>"), "advrenametag.insert-text-tag('%X')")
                    .item(
                        gettext("HH.MM.SS"),
                        "advrenametag.insert-text-tag('%H.%M.%S')",
                    )
                    .item(
                        gettext("HH-MM-SS"),
                        "advrenametag.insert-text-tag('%H-%M-%S')",
                    )
                    .item(gettext("HHMMSS"), "advrenametag.insert-text-tag('%H%M%S')")
                    .item(gettext("HH"), "advrenametag.insert-text-tag('%H')")
                    .item(gettext("MM"), "advrenametag.insert-text-tag('%M')")
                    .item(gettext("SS"), "advrenametag.insert-text-tag('%S')"),
            )
    }

    fn counter_digits_model() -> gtk::StringList {
        let model = gtk::StringList::new(&[&gettext("auto")]);
        for c in 1..=16 {
            model.append(&c.to_string());
        }
        model
    }

    fn regex_attributes(regex_replace: &RegexReplace) -> Option<pango::AttrList> {
        if !regex_replace.is_valid() {
            let attrs = pango::AttrList::new();
            attrs.insert(pango::AttrColor::new_foreground(0xFFFF, 0, 0));
            Some(attrs)
        } else {
            None
        }
    }

    fn create_regex_view(view: &gtk::ColumnView) {
        view.append_column(
            &gtk::ColumnViewColumn::builder()
                .title(gettext("Search for"))
                .resizable(true)
                .expand(true)
                .factory(&regex_pattern_factory())
                .build(),
        );

        view.append_column(
            &gtk::ColumnViewColumn::builder()
                .title(gettext("Replace with"))
                .resizable(true)
                .expand(true)
                .factory(&regex_replacement_factory())
                .build(),
        );

        view.append_column(
            &gtk::ColumnViewColumn::builder()
                .title(gettext("Match case"))
                .resizable(true)
                .expand(false)
                .factory(&regex_match_case_factory())
                .build(),
        );
    }

    fn regex_pattern_factory() -> gtk::ListItemFactory {
        let factory = gtk::SignalListItemFactory::new();
        factory.connect_setup(|_, obj| {
            if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
                let label = gtk::Label::builder().xalign(0.0).build();
                list_item.set_child(Some(&label));
            }
        });
        factory.connect_bind(|_, obj| {
            let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() else {
                return;
            };
            let Some(label) = list_item.child().and_downcast::<gtk::Label>() else {
                return;
            };
            let item = list_item.item().and_downcast::<glib::BoxedAnyObject>();
            let regex_item = item.as_ref().map(|b| b.borrow::<RegexReplace>());

            label.set_text(regex_item.as_ref().map(|r| &*r.pattern).unwrap_or_default());
            label.set_attributes(regex_item.and_then(|r| regex_attributes(&r)).as_ref());
        });
        factory.upcast()
    }

    fn regex_replacement_factory() -> gtk::ListItemFactory {
        let factory = gtk::SignalListItemFactory::new();
        factory.connect_setup(|_, obj| {
            if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
                let label = gtk::Label::builder().xalign(0.0).build();
                list_item.set_child(Some(&label));
            }
        });
        factory.connect_bind(|_, obj| {
            let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() else {
                return;
            };
            let Some(label) = list_item.child().and_downcast::<gtk::Label>() else {
                return;
            };
            let item = list_item.item().and_downcast::<glib::BoxedAnyObject>();
            let regex_item = item.as_ref().map(|b| b.borrow::<RegexReplace>());

            label.set_text(
                regex_item
                    .as_ref()
                    .map(|r| &*r.replacement)
                    .unwrap_or_default(),
            );
            label.set_attributes(regex_item.and_then(|r| regex_attributes(&r)).as_ref());
        });
        factory.upcast()
    }

    fn regex_match_case_factory() -> gtk::ListItemFactory {
        let factory = gtk::SignalListItemFactory::new();
        factory.connect_setup(|_, obj| {
            if let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() {
                let label = gtk::Label::builder().xalign(0.0).build();
                list_item.set_child(Some(&label));
            }
        });
        factory.connect_bind(|_, obj| {
            let Some(list_item) = obj.downcast_ref::<gtk::ListItem>() else {
                return;
            };
            let Some(label) = list_item.child().and_downcast::<gtk::Label>() else {
                return;
            };
            let item = list_item.item().and_downcast::<glib::BoxedAnyObject>();
            let regex_item = item.as_ref().map(|b| b.borrow::<RegexReplace>());

            label.set_text(
                &regex_item
                    .as_ref()
                    .map(|r| {
                        if r.match_case {
                            gettext("Yes")
                        } else {
                            gettext("No")
                        }
                    })
                    .unwrap_or_default(),
            );
            label.set_attributes(regex_item.and_then(|r| regex_attributes(&r)).as_ref());
        });
        factory.upcast()
    }

    fn case_modes() -> gtk::StringList {
        gtk::StringList::new(&[
            &gettext("<unchanged>"),
            &gettext("lowercase"),
            &gettext("UPPERCASE"),
            &gettext("Sentence case"),
            &gettext("Initial Caps"),
            &gettext("tOGGLE cASE"),
        ])
    }

    fn trim_modes() -> gtk::StringList {
        gtk::StringList::new(&[
            &gettext("<none>"),
            &gettext("leading"),
            &gettext("trailing"),
            &gettext("leading and trailing"),
        ])
    }
}

glib::wrapper! {
    pub struct AdvancedRenameProfileComponent(ObjectSubclass<imp::AdvancedRenameProfileComponent>)
        @extends gtk::Widget, gtk::Box,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget;
}

impl AdvancedRenameProfileComponent {
    pub fn new(file_metadata_service: &FileMetadataService) -> Self {
        let this: Self = glib::Object::builder()
            .property("file-metadata-service", file_metadata_service)
            .build();
        this
    }

    pub fn set_template_history(&self, history: &[String]) {
        self.imp().template_entry.set_history(history);
    }

    pub fn template_entry(&self) -> String {
        self.imp().template_entry.text().into()
    }

    pub fn valid_regexes(&self) -> Vec<RegexReplace> {
        let model = &self.imp().regex_model;

        let mut result = Vec::new();
        for b in model.iter::<glib::BoxedAnyObject>().flatten() {
            let regex_replace = b.borrow::<RegexReplace>();
            if regex_replace.is_valid() {
                result.push(regex_replace.clone());
            }
        }
        result
    }

    pub fn set_sample_file_name(&self, name: Option<String>) {
        self.imp().sample_file_name.replace(name);
    }

    pub fn update(&self) {
        let Some(profile) = self.profile() else {
            return;
        };

        let template_entry = self.imp().template_entry.entry();
        let template_string = profile.template_string();
        template_entry.set_text({
            if template_string.is_empty() {
                "$N"
            } else {
                &template_string
            }
        });
        template_entry.set_position(-1);
        template_entry.select_region(-1, -1);

        self.imp()
            .counter_start_spin
            .set_value(profile.counter_start() as f64);
        self.imp()
            .counter_step_spin
            .set_value(profile.counter_step() as f64);
        self.imp()
            .counter_digits_combo
            .set_selected(profile.counter_width());

        self.imp().remove_all_regexes();
        let regex_model = &self.imp().regex_model;
        for rx in profile.patterns() {
            regex_model.append(&glib::BoxedAnyObject::new(rx));
        }

        self.imp()
            .case_combo
            .set_selected(profile.case_conversion() as u32);
        self.imp()
            .trim_combo
            .set_selected(profile.trim_blanks() as u32);

        self.emit_by_name::<()>("regex-changed", &[]);
    }

    pub fn copy(&self) {
        let Some(profile) = self.profile() else {
            return;
        };

        let template_entry = self.template_entry();
        profile.set_template_string(if template_entry.is_empty() {
            "$N"
        } else {
            &template_entry
        });

        let patterns: Vec<RegexReplace> = self
            .imp()
            .regex_model
            .iter::<glib::BoxedAnyObject>()
            .flatten()
            .map(|b| b.borrow::<RegexReplace>().clone())
            .collect();

        profile.set_patterns(patterns);
    }
}

async fn get_selected_range(
    parent_window: &gtk::Window,
    placeholder: &str,
    filename: Option<&str>,
) -> Option<String> {
    let filename = filename.unwrap_or(concat!("Lorem ipsum dolor sit amet, consectetur adipisici elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. ",
                   "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. ",
                   "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. ",
                   "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."));

    let dialog = gtk::Window::builder()
        .title(gettext("Range Selection"))
        .transient_for(parent_window)
        .modal(true)
        .resizable(false)
        .width_request(480)
        .build();

    let grid = gtk::Grid::builder()
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .row_spacing(6)
        .column_spacing(12)
        .build();
    dialog.set_child(Some(&grid));

    let entry = gtk::Entry::builder()
        .text(filename)
        .activates_default(true)
        .hexpand(true)
        .build();
    let label = gtk::Label::builder()
        .label(gettext("_Select range:"))
        .use_underline(true)
        .mnemonic_widget(&entry)
        .build();

    grid.attach(&label, 0, 0, 1, 1);
    grid.attach(&entry, 1, 0, 1, 1);

    let option = gtk::CheckButton::builder()
        .label(gettext("_Inverse selection"))
        .use_underline(true)
        .build();
    grid.attach(&option, 0, 1, 2, 1);

    let (sender, receiver) = async_channel::bounded::<bool>(1);

    let cancel_button = gtk::Button::builder()
        .label(gettext("_Cancel"))
        .use_underline(true)
        .hexpand(true)
        .halign(gtk::Align::End)
        .build();
    cancel_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| if let Err(error) = sender.send_blocking(false) {
            eprintln!("Failed to send a 'cancel' message: {error}.")
        }
    ));

    let ok_button = gtk::Button::builder()
        .label(gettext("_OK"))
        .use_underline(true)
        .build();
    ok_button.connect_clicked(glib::clone!(
        #[strong]
        sender,
        move |_| if let Err(error) = sender.send_blocking(true) {
            eprintln!("Failed to send an 'ok' message: {error}.")
        }
    ));

    grid.attach(
        &dialog_button_box(NO_BUTTONS, &[&cancel_button, &ok_button]),
        0,
        2,
        2,
        1,
    );

    dialog.set_default_widget(Some(&ok_button));

    dialog.present();
    let result = receiver.recv().await.unwrap_or_default();
    dialog.close();

    let range = if result {
        let inversed = option.is_active();

        if let Some((begin, end)) = entry.selection_bounds() {
            let len = i32::from(entry.text_length());
            if !inversed {
                if end == len {
                    Some(format!("{placeholder}({begin}:)"))
                } else {
                    Some(format!("{placeholder}({begin}:{end})"))
                }
            } else {
                let b = (begin != 0).then(|| format!("{placeholder}(:{begin})"));
                let e = (end != len).then(|| format!("{placeholder}({end}:)"));
                Some(format!(
                    "{}{}",
                    b.unwrap_or_default(),
                    e.unwrap_or_default()
                ))
            }
        } else {
            if !inversed {
                None
            } else {
                Some(placeholder.to_owned())
            }
        }
    } else {
        None
    };

    range
}

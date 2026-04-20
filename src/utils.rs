// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    file::{File, FileOps},
    options::ProgramsOptions,
    types::SizeDisplayMode,
    user_actions::UserAction,
};
use gettextrs::{gettext, ngettext};
use gtk::{gdk, gio, glib, pango, prelude::*};
use std::{
    cell::Cell,
    ffi::{OsStr, OsString},
    fmt,
    mem::swap,
    rc::Rc,
    sync::OnceLock,
    time::Duration,
};

pub const GNOME_CMD_PERM_USER_READ: u32 = 256; //r--------
pub const GNOME_CMD_PERM_USER_WRITE: u32 = 128; //-w-------
pub const GNOME_CMD_PERM_USER_EXEC: u32 = 64; //--x------
pub const GNOME_CMD_PERM_GROUP_READ: u32 = 32; //---r-----
pub const GNOME_CMD_PERM_GROUP_WRITE: u32 = 16; //----w----
pub const GNOME_CMD_PERM_GROUP_EXEC: u32 = 8; //-----x---
pub const GNOME_CMD_PERM_OTHER_READ: u32 = 4; //------r--
pub const GNOME_CMD_PERM_OTHER_WRITE: u32 = 2; //-------w-
pub const GNOME_CMD_PERM_OTHER_EXEC: u32 = 1; //--------x

pub const PERMISSION_MASKS: [[u32; 3]; 3] = [
    [
        GNOME_CMD_PERM_USER_READ,
        GNOME_CMD_PERM_USER_WRITE,
        GNOME_CMD_PERM_USER_EXEC,
    ],
    [
        GNOME_CMD_PERM_GROUP_READ,
        GNOME_CMD_PERM_GROUP_WRITE,
        GNOME_CMD_PERM_GROUP_EXEC,
    ],
    [
        GNOME_CMD_PERM_OTHER_READ,
        GNOME_CMD_PERM_OTHER_WRITE,
        GNOME_CMD_PERM_OTHER_EXEC,
    ],
];

pub fn permissions_to_text(permissions: u32) -> String {
    let mut result = String::with_capacity(9);
    for grantee_masks in PERMISSION_MASKS {
        for (mask, symbol) in grantee_masks.iter().zip(['r', 'w', 'x']) {
            result.push(if (permissions & mask) != 0 {
                symbol
            } else {
                '-'
            });
        }
    }
    result
}

pub fn permissions_to_numbers(permissions: u32) -> String {
    format!("{:03o}", permissions)
}

pub const NO_MOD: gdk::ModifierType = gdk::ModifierType::NO_MODIFIER_MASK;
pub const SHIFT: gdk::ModifierType = gdk::ModifierType::SHIFT_MASK;
pub const CONTROL: gdk::ModifierType = gdk::ModifierType::CONTROL_MASK;
pub const ALT: gdk::ModifierType = gdk::ModifierType::ALT_MASK;
pub const CONTROL_ALT: gdk::ModifierType =
    gdk::ModifierType::CONTROL_MASK.union(gdk::ModifierType::ALT_MASK);

pub fn temp_directory() -> &'static tempfile::TempDir {
    static TEMP_DIRECTORY: OnceLock<tempfile::TempDir> = OnceLock::new();
    TEMP_DIRECTORY
        .get_or_init(|| tempfile::tempdir().expect("Cannot create a temporary directory."))
}

pub fn temp_file(f: &File) -> Result<File, ErrorMessage> {
    let name = f.name();
    let name_parts = name.rsplit_once('.');

    let temp_file = tempfile::Builder::new()
        .prefix(name_parts.map(|p| p.0).unwrap_or("tmp"))
        .suffix(name_parts.map(|p| p.0).unwrap_or(".tmp"))
        .tempfile_in(temp_directory().path())
        .map_err(|error| ErrorMessage {
            message: gettext("Cannot create a temporary file."),
            secondary_text: Some(error.to_string()),
        })?;

    let path = temp_file.into_temp_path();
    File::new_from_path(&path)
}

fn substitute_command_argument(command_template: &str, arg: &OsStr) -> OsString {
    let mut cmd = OsString::new();
    match command_template.split_once("%s") {
        Some((before, after)) => {
            cmd.push(before);
            cmd.push(arg);
            cmd.push(after);
        }
        None => {
            cmd.push(command_template);
            cmd.push(" ");
            cmd.push(arg);
        }
    }
    cmd
}

pub fn make_run_in_terminal_command(command: &OsStr, options: &ProgramsOptions) -> OsString {
    let shell = std::env::var("SHELL").unwrap_or_else(|_| "/bin/sh".to_owned());
    let arg = glib::shell_quote(substitute_command_argument(
        &format!("{shell} -c %s"),
        &glib::shell_quote(command),
    ));
    substitute_command_argument(&options.terminal_exec_cmd.get(), &arg)
}

pub trait SenderExt<T> {
    fn toss(&self, message: T);
}

impl<T> SenderExt<T> for async_channel::Sender<T> {
    fn toss(&self, message: T) {
        if let Err(err) = self.send_blocking(message) {
            eprintln!("Cannot send a message: {}", err);
        }
    }
}

pub trait WindowExt {
    fn set_cancel_widget(&self, cancel_widget: &impl IsA<gtk::Widget>);
}

impl<T: IsA<gtk::Window> + WidgetExt> WindowExt for T {
    fn set_cancel_widget(&self, cancel_widget: &impl IsA<gtk::Widget>) {
        let cancel_widget = cancel_widget.upcast_ref::<gtk::Widget>().clone();
        self.connect_close_request(glib::clone!(
            #[weak]
            cancel_widget,
            #[upgrade_or]
            glib::Propagation::Proceed,
            move |_| {
                // Activating won’t trigger clicked signal at this stage, so emit it explicitly.
                if let Some(button) = cancel_widget.downcast_ref::<gtk::Button>() {
                    button.emit_clicked();
                };
                glib::Propagation::Proceed
            }
        ));

        let controller = gtk::ShortcutController::new();
        controller.add_shortcut(
            gtk::Shortcut::builder()
                .trigger(&gtk::KeyvalTrigger::new(gdk::Key::Escape, NO_MOD))
                .action(&gtk::CallbackAction::new(move |_, _| {
                    cancel_widget.activate();
                    glib::Propagation::Stop
                }))
                .build(),
        );
        self.add_controller(controller);
    }
}

pub const NO_BUTTONS: &[&gtk::Button] = &[];

pub fn dialog_button_box(
    buttons_start: &[impl AsRef<gtk::Widget>],
    buttons_end: &[impl AsRef<gtk::Widget>],
) -> gtk::Widget {
    let bx = gtk::Box::builder()
        .orientation(gtk::Orientation::Horizontal)
        .spacing(6)
        .margin_top(6)
        .hexpand(true)
        .build();
    let size_group = gtk::SizeGroup::new(gtk::SizeGroupMode::Both);

    for button in buttons_start {
        let button = button.as_ref();
        bx.append(button);
        size_group.add_widget(button);
    }

    if let Some((first, rest)) = buttons_end.split_first() {
        let first = first.as_ref();
        first.set_hexpand(true);
        first.set_halign(gtk::Align::End);
        bx.append(first);
        size_group.add_widget(first);

        for button in rest {
            let button = button.as_ref();
            bx.append(button);
            size_group.add_widget(button);
        }
    }

    bx.upcast()
}

#[derive(Debug)]
pub struct ErrorMessage {
    pub message: String,
    pub secondary_text: Option<String>,
}

impl ErrorMessage {
    pub fn new<M: Into<String>, S: Into<String>>(message: M, secondary_text: Option<S>) -> Self {
        Self {
            message: message.into(),
            secondary_text: secondary_text.map(|s| s.into()),
        }
    }

    pub fn brief<M: Into<String>>(message: M) -> Self {
        Self {
            message: message.into(),
            secondary_text: None,
        }
    }

    pub fn with_error(message: impl Into<String>, error: &dyn std::error::Error) -> Self {
        Self {
            message: message.into(),
            secondary_text: Some(error.to_string()),
        }
    }

    pub async fn show(&self, parent: &gtk::Window) {
        let alert = gtk::AlertDialog::builder()
            .modal(true)
            .message(&self.message)
            .buttons([gettext("_Dismiss")])
            .cancel_button(0)
            .default_button(0)
            .build();
        if let Some(ref secondary_text) = self.secondary_text {
            alert.set_detail(secondary_text);
        }
        if let Err(error) = alert.choose_future(Some(parent)).await {
            eprintln!("{error}");
        }
    }
}

impl fmt::Display for ErrorMessage {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        writeln!(f, "{}", self.message)?;
        if let Some(ref secondary_text) = self.secondary_text {
            writeln!(f, "  {}", secondary_text)?;
        }
        Ok(())
    }
}

pub async fn display_help(parent_window: &gtk::Window, link_id: Option<&str>) {
    let mut help_uri = format!("help:{}", crate::config::PACKAGE);
    if let Some(link_id) = link_id {
        help_uri.push('/');
        help_uri.push_str(link_id);
    }
    if let Err(error) = gtk::UriLauncher::new(&help_uri)
        .launch_future(Some(parent_window))
        .await
    {
        ErrorMessage::with_error(
            gettext("There was an error while opening help uri {uri}.").replace("{uri}", &help_uri),
            &error,
        )
        .show(parent_window)
        .await;
    }
}

pub fn toggle_file_name_selection(entry: &gtk::Entry) {
    let text: Vec<char> = entry.text().chars().collect();

    let base = text
        .iter()
        .enumerate()
        .rev()
        .find_map(|(index, ch)| (*ch == '/').then_some(index + 1))
        .unwrap_or_default();

    if let Some((mut beg, end)) = entry.selection_bounds() {
        let text_len = text.len() as i32;

        let ext = text
            .iter()
            .enumerate()
            .skip(base)
            .rev()
            .find_map(|(index, ch)| (*ch == '.').then_some(index as i32))
            .unwrap_or(-1);
        let base = base as i32;

        if beg == 0 && end == text_len {
            entry.select_region(base, ext);
        } else {
            if beg != base {
                beg = if beg > base { base } else { 0 };
            } else if end != ext || end == text_len {
                beg = 0;
            }

            entry.select_region(beg, -1);
        }
    } else {
        entry.select_region(base as i32, -1);
    }
}

pub fn get_modifiers_state(window: &gtk::Window) -> Option<gdk::ModifierType> {
    let gdk_window = window.surface()?;
    let display = gdk_window.display();
    let seat = display.default_seat()?;
    let keyboard = seat.keyboard()?;
    Some(keyboard.modifier_state())
}

pub async fn sleep(millis: u64) {
    glib::timeout_future(Duration::from_millis(millis)).await;
}

pub async fn pending() {
    sleep(1).await;
}

pub fn swap_list_store_items(
    store: &gio::ListStore,
    item1: &impl IsA<glib::Object>,
    item2: &impl IsA<glib::Object>,
) {
    if item1.as_ref() == item2.as_ref() {
        return;
    }
    let Some(mut position1) = store.find(item1) else {
        eprintln!("Item1 wasn't found.");
        return;
    };
    let Some(mut position2) = store.find(item2) else {
        eprintln!("Item2 wasn't found.");
        return;
    };
    if position1 > position2 {
        swap(&mut position1, &mut position2);
    }
    let mut items: Vec<_> = (position1..=position2)
        .map(|p| store.item(p).unwrap())
        .collect();
    let len = items.len();
    items.swap(0, len - 1);

    store.splice(position1, len as u32, &items);
}

pub fn size_to_string(size: u64, mode: SizeDisplayMode) -> String {
    let size_u32 = u32::try_from(size).unwrap_or(u32::MAX);
    match mode {
        SizeDisplayMode::Powered => {
            const PIBI: u64 = 1024 * 1024 * 1024 * 1024 * 1024;
            const TIBI: u64 = 1024 * 1024 * 1024 * 1024;
            const GIBI: u64 = 1024 * 1024 * 1024;
            const MIBI: u64 = 1024 * 1024;
            const KIBI: u64 = 1024;

            if size >= PIBI {
                ngettext("{} PiB", "{} PiB", size_u32)
                    .replace("{}", &format!("{:.1}", size as f64 / PIBI as f64))
            } else if size >= TIBI {
                ngettext("{} TiB", "{} TiB", size_u32)
                    .replace("{}", &format!("{:.1}", size as f64 / TIBI as f64))
            } else if size >= GIBI {
                ngettext("{} GiB", "{} GiB", size_u32)
                    .replace("{}", &format!("{:.1}", size as f64 / GIBI as f64))
            } else if size >= MIBI {
                ngettext("{} MiB", "{} MiB", size_u32)
                    .replace("{}", &format!("{:.1}", size as f64 / MIBI as f64))
            } else if size >= KIBI {
                ngettext("{} kiB", "{} kiB", size_u32)
                    .replace("{}", &format!("{:.1}", size as f64 / KIBI as f64))
            } else {
                ngettext("{} byte", "{} bytes", size_u32).replace("{}", &size.to_string())
            }
        }
        SizeDisplayMode::Grouped => {
            let mut digits: Vec<char> = size.to_string().chars().collect();
            let mut i = digits.len() as isize - 3;
            while i > 0 {
                digits.insert(i as usize, ' ');
                i -= 3;
            }
            let value: String = digits.into_iter().collect();
            ngettext("{} byte", "{} bytes", size_u32).replace("{}", &value.to_string())
        }
        SizeDisplayMode::Locale => size.to_string(), // TODO
        SizeDisplayMode::Plain => {
            ngettext("{} byte", "{} bytes", size_u32).replace("{}", &size.to_string())
        }
    }
}

pub fn nice_size(size: u64, mode: SizeDisplayMode) -> String {
    match mode {
        SizeDisplayMode::Powered if size >= 1024 => {
            format!(
                "{} ({})",
                size_to_string(size, mode),
                nice_size(size, SizeDisplayMode::Grouped)
            )
        }
        _ => size_to_string(size, mode),
    }
}

pub fn time_to_string(
    date_time: glib::DateTime,
    format: &str,
) -> Result<String, Box<dyn std::error::Error>> {
    let local = date_time.to_local()?;
    let string = local.format(format).or_else(|_| local.format("%c"))?;
    Ok(string.to_string())
}

pub fn bold(text: &str) -> String {
    format!(
        "<span weight=\"bold\">{}</span>",
        glib::markup_escape_text(text)
    )
}

pub fn attributes_bold() -> pango::AttrList {
    let attrs = pango::AttrList::new();
    attrs.insert(pango::AttrInt::new_weight(pango::Weight::Bold));
    attrs
}

pub fn grid_attach(
    parent: &impl IsA<gtk::Widget>,
    child: &impl IsA<gtk::Widget>,
    column: i32,
    row: i32,
    column_span: i32,
    row_span: i32,
) {
    child.set_parent(parent);
    if let Some(layout_child) = parent
        .layout_manager()
        .and_downcast::<gtk::GridLayout>()
        .map(|l| l.layout_child(child))
        .and_downcast::<gtk::GridLayoutChild>()
    {
        layout_child.set_column(column);
        layout_child.set_row(row);
        layout_child.set_column_span(column_span);
        layout_child.set_row_span(row_span);
    }
}

pub trait MenuBuilderExt {
    fn item(self, label: impl Into<String>, detailed_action: impl AsRef<str>) -> Self;

    fn item_param(
        self,
        label: impl Into<String>,
        action: impl AsRef<str>,
        param: impl glib::variant::ToVariant,
    ) -> Self;

    fn item_accel(
        self,
        label: impl Into<String>,
        detailed_action: impl AsRef<str>,
        accel: &str,
    ) -> Self;

    fn item_param_accel(
        self,
        label: impl Into<String>,
        action: impl AsRef<str>,
        param: impl glib::variant::ToVariant,
        accel: &str,
    ) -> Self;

    fn action(self, action: UserAction) -> Self;

    fn section(self, section: gio::Menu) -> Self;

    fn submenu(self, label: impl Into<String>, section: gio::Menu) -> Self;
}

impl MenuBuilderExt for gio::Menu {
    fn item(self, label: impl Into<String>, detailed_action: impl AsRef<str>) -> Self {
        self.append(Some(&label.into()), Some(detailed_action.as_ref()));
        self
    }

    fn item_param(
        self,
        label: impl Into<String>,
        action: impl AsRef<str>,
        param: impl glib::variant::ToVariant,
    ) -> Self {
        let item = gio::MenuItem::new(Some(&label.into()), None);
        item.set_action_and_target_value(Some(action.as_ref()), Some(&param.to_variant()));
        self.append_item(&item);
        self
    }

    fn item_accel(
        self,
        label: impl Into<String>,
        detailed_action: impl AsRef<str>,
        accel: &str,
    ) -> Self {
        let item = gio::MenuItem::new(Some(&label.into()), Some(detailed_action.as_ref()));
        item.set_attribute_value("accel", Some(&accel.to_variant()));
        self.append_item(&item);
        self
    }

    fn item_param_accel(
        self,
        label: impl Into<String>,
        action: impl AsRef<str>,
        param: impl glib::variant::ToVariant,
        accel: &str,
    ) -> Self {
        let item = gio::MenuItem::new(Some(&label.into()), None);
        item.set_action_and_target_value(Some(action.as_ref()), Some(&param.to_variant()));
        item.set_attribute_value("accel", Some(&accel.to_variant()));
        self.append_item(&item);
        self
    }

    fn action(self, action: UserAction) -> Self {
        self.item(action.menu_description(), action.name())
    }

    fn section(self, section: gio::Menu) -> Self {
        self.append_section(None, &section);
        self
    }

    fn submenu(self, label: impl Into<String>, section: gio::Menu) -> Self {
        self.append_submenu(Some(&label.into()), &section);
        self
    }
}

pub fn extract_menu_shortcuts(menu: &gio::MenuModel) -> gio::ListStore {
    fn collect(menu: &gio::MenuModel, item_index: i32, accel: &str, store: &gio::ListStore) {
        let Some(trigger) = gtk::ShortcutTrigger::parse_string(accel) else {
            eprintln!("Failed to parse '{}' as an accelerator.", accel);
            return;
        };
        let Some(action_name) = menu
            .item_attribute_value(item_index, gio::MENU_ATTRIBUTE_ACTION, None)
            .as_ref()
            .and_then(|a| a.str())
            .map(ToString::to_string)
        else {
            eprintln!("No action for an accelerator {}.", accel);
            return;
        };
        let target_value = menu.item_attribute_value(item_index, gio::MENU_ATTRIBUTE_TARGET, None);

        let shortcut = gtk::Shortcut::new(Some(trigger), Some(gtk::NamedAction::new(&action_name)));
        shortcut.set_arguments(target_value.as_ref());

        store.append(&shortcut);
    }

    fn traverse(store: &gio::ListStore, menu: &gio::MenuModel) {
        for item_index in 0..menu.n_items() {
            if let Some(accel) = menu
                .item_attribute_value(item_index, "accel", None)
                .as_ref()
                .and_then(|v| v.str())
            {
                collect(menu, item_index, accel, store);
            }
            let iter = menu.iterate_item_links(item_index);
            while let Some((_link_name, model)) = iter.next() {
                traverse(store, &model);
            }
        }
    }

    let store = gio::ListStore::new::<gtk::Shortcut>();
    traverse(&store, menu);
    store
}

pub trait GnomeCmdFileExt {
    fn all_children(
        &self,
        attributes: &str,
        flags: gio::FileQueryInfoFlags,
        cancellable: Option<&impl IsA<gio::Cancellable>>,
    ) -> Result<Vec<gio::FileInfo>, glib::Error>;
}

impl GnomeCmdFileExt for gio::File {
    fn all_children(
        &self,
        attributes: &str,
        flags: gio::FileQueryInfoFlags,
        cancellable: Option<&impl IsA<gio::Cancellable>>,
    ) -> Result<Vec<gio::FileInfo>, glib::Error> {
        let enumerator = self.enumerate_children(attributes, flags, cancellable)?;
        let mut vec = Vec::new();
        let result = loop {
            match enumerator.next_file(cancellable) {
                Ok(Some(file_info)) => {
                    vec.push(file_info);
                }
                Ok(None) => {
                    break Ok(());
                }
                Err(error) => {
                    break Err(error);
                }
            };
        };
        let close_result = enumerator.close(cancellable);
        result?;
        match close_result {
            (true, None) => Ok(vec),
            (false, None) => Err(glib::Error::new(
                gio::IOErrorEnum::Failed,
                &gettext("Closing of a file enumerator failed"),
            )),
            (_, Some(error)) => Err(error),
        }
    }
}

/// A helper class allowing to chain modifications for a `gtk::Box`.
pub struct BoxBuilder {
    inner: gtk::Box,
}

impl BoxBuilder {
    pub fn append(self, child: &impl IsA<gtk::Widget>) -> Self {
        self.inner.append(child);
        self
    }

    pub fn add_css_class(self, css_class: &str) -> Self {
        self.inner.add_css_class(css_class);
        self
    }

    pub fn build(self) -> gtk::Box {
        self.inner
    }
}

impl From<gtk::Box> for BoxBuilder {
    fn from(inner: gtk::Box) -> Self {
        Self { inner }
    }
}

impl From<BoxBuilder> for gtk::Box {
    fn from(builder: BoxBuilder) -> Self {
        builder.inner
    }
}

/// Creates a `BoxBuilder` instance for a box with arbitrary orientation.
fn box_builder(orientation: gtk::Orientation) -> BoxBuilder {
    gtk::Box::builder().orientation(orientation).build().into()
}

/// Creates a `BoxBuilder` instance for a box with horizontal orientation.
pub fn hbox_builder() -> BoxBuilder {
    box_builder(gtk::Orientation::Horizontal)
}

/// Creates a `BoxBuilder` instance for a box with vertical orientation.
pub fn vbox_builder() -> BoxBuilder {
    box_builder(gtk::Orientation::Vertical)
}

/// Gtk doesn't allow changing focused row while the list isn't focused, things end up in an
/// invalid state. This type encapsulates the row selection logic, delaying focus change if
/// necessary.
pub struct ListRowSelector {
    view: gtk::ColumnView,
    focus_controller: gtk::EventControllerFocus,
    delayed_focus: Cell<Option<u32>>,
}

impl ListRowSelector {
    pub fn new(view: &gtk::ColumnView) -> Rc<Self> {
        let focus_controller = gtk::EventControllerFocus::new();
        view.add_controller(focus_controller.clone());

        let result = Rc::new(Self {
            view: view.clone(),
            focus_controller,
            delayed_focus: Cell::new(None),
        });

        let weak_ref = Rc::downgrade(&result);
        result
            .focus_controller
            .connect_contains_focus_notify(move |controller| {
                if controller.contains_focus()
                    && let Some(this) = weak_ref.upgrade()
                    && let Some(pos) = this.delayed_focus.replace(None)
                {
                    let view = this.view.clone();
                    glib::spawn_future_local(async move {
                        view.scroll_to(pos, None, gtk::ListScrollFlags::FOCUS, None);
                    });
                }
            });

        result
    }

    pub fn select_row(&self, pos: u32) {
        if pos != gtk::INVALID_LIST_POSITION {
            if self.focus_controller.contains_focus() {
                self.view.scroll_to(
                    pos,
                    None,
                    gtk::ListScrollFlags::FOCUS | gtk::ListScrollFlags::SELECT,
                    None,
                );
                self.delayed_focus.set(None);
            } else {
                self.view
                    .scroll_to(pos, None, gtk::ListScrollFlags::SELECT, None);
                self.delayed_focus.set(Some(pos));
            }
        }
    }
}

#[derive(Default)]
pub struct Max<A>(Option<A>);

impl<A> Max<A> {
    pub fn take(self) -> Option<A> {
        self.0
    }
}

impl<A: PartialOrd> Extend<A> for Max<A> {
    fn extend<T: IntoIterator<Item = A>>(&mut self, iter: T) {
        for item in iter {
            if let Some(ref mut max) = self.0 {
                if item > *max {
                    *max = item;
                }
            } else {
                self.0 = Some(item);
            }
        }
    }
}

pub struct MinMax<A>(Option<(A, A)>);

impl<A> MinMax<A> {
    pub fn take(self) -> Option<(A, A)> {
        self.0
    }
}

impl<A: PartialOrd + Ord + Clone> FromIterator<A> for MinMax<A> {
    fn from_iter<T: IntoIterator<Item = A>>(iter: T) -> Self {
        Self(iter.into_iter().fold(None, |min_max, item| match min_max {
            Some((min, max)) => Some((A::min(min, item.clone()), A::max(max, item))),
            None => Some((item.clone(), item)),
        }))
    }
}

#[allow(dead_code)] // used by unit tests
pub trait IterableEnum {
    fn iter() -> impl Iterator<Item = Self>;
}

/// Defines an enum type that can be represented as a u32 value. One of the enum variants has to
/// be marked with #[default], it will be used when converting numeric values that don't map to any
/// of the enum variants.
macro_rules! u32_enum {
    ($(#[$type_meta:meta])* $vis:vis enum $type:ident {
        $($(#[$meta:meta])* $variant:ident,)+
    }) => {
        #[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Default)]
        #[derive(::gtk::glib::ValueDelegate, ::gtk::glib::Variant)]
        #[value_delegate(from = u32)]
        #[variant_enum(repr)]
        #[repr(u32)]
        $(#[$type_meta])*
        $vis enum $type {
            $($(#[$meta])* $variant,)+
        }

        #[allow(dead_code)]
        impl $type {
            pub fn all() -> impl Iterator<Item=Self> {
                $(
                    let _max_index: u32 = Self::$variant.into();
                )*
                (0..=_max_index).map(|n| Self::from(n))
            }

            pub const fn count() -> usize {
                let count = 0;
                $(
                    let count = (count + 1, Self::$variant).0;
                )*
                count
            }
        }

        impl From<$type> for u32 {
            fn from(value: $type) -> Self {
                value as Self
            }
        }

        impl From<&$type> for u32 {
            fn from(value: &$type) -> Self {
                *value as Self
            }
        }

        impl From<u32> for $type {
            fn from(value: u32) -> Self {
                match value {
                    $(v if v == u32::from($type::$variant) => $type::$variant,)+
                    _ => Default::default(),
                }
            }
        }

        impl $crate::utils::IterableEnum for $type {
            fn iter() -> impl Iterator<Item = Self> {
                Self::all()
            }
        }
    }
}

pub(crate) use u32_enum;

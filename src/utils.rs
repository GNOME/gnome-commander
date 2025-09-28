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

use crate::{
    config::PREFIX, file::File, options::options::ProgramsOptions, types::SizeDisplayMode,
};
use gettextrs::{gettext, ngettext};
use gtk::{gdk, gio, glib, pango, prelude::*};
use std::{
    ffi::{OsStr, OsString},
    fmt,
    mem::swap,
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
pub const CONTROL_SHIFT: gdk::ModifierType =
    gdk::ModifierType::CONTROL_MASK.union(gdk::ModifierType::SHIFT_MASK);
pub const ALT_SHIFT: gdk::ModifierType =
    gdk::ModifierType::ALT_MASK.union(gdk::ModifierType::SHIFT_MASK);

pub fn temp_directory() -> &'static tempfile::TempDir {
    static TEMP_DIRECTORY: OnceLock<tempfile::TempDir> = OnceLock::new();
    TEMP_DIRECTORY
        .get_or_init(|| tempfile::tempdir().expect("Cannot create a temporary directory."))
}

pub fn temp_file(f: &File) -> Result<File, ErrorMessage> {
    let name = f.get_name();
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
    let arg = if options.use_gcmd_block.get() {
        glib::shell_quote(substitute_command_argument(
            &format!("bash -c \"%s; {PREFIX}/bin/gcmd-block\""),
            command,
        ))
    } else {
        glib::shell_quote(command)
    };
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

pub fn channel_send_action<T>(sender: &async_channel::Sender<T>, message: T) -> gtk::ShortcutAction
where
    T: Clone + 'static,
{
    gtk::CallbackAction::new(glib::clone!(
        #[strong]
        sender,
        move |_, _| {
            sender.toss(message.clone());
            glib::Propagation::Proceed
        }
    ))
    .upcast()
}

pub fn handle_escape_key(window: &gtk::Window, action: &impl IsA<gtk::ShortcutAction>) {
    if let Some(trigger) = gtk::ShortcutTrigger::parse_string("Escape") {
        let controller = gtk::ShortcutController::new();
        controller.add_shortcut(
            gtk::Shortcut::builder()
                .trigger(&trigger)
                .action(action)
                .build(),
        );
        window.add_controller(controller);
    } else {
        eprintln!("\"Escape\" wasn't recognized as a legit shortcut.");
    }
}

pub const NO_BUTTONS: &'static [&'static gtk::Button] = &[];

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
            .buttons([gettext("_OK")])
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
            } else {
                if end != ext || end == text_len {
                    beg = 0;
                }
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

pub fn key_sorter<K, O>(key: K) -> gtk::Sorter
where
    K: Fn(&glib::Object) -> O + 'static,
    O: std::cmp::Ord,
{
    gtk::CustomSorter::new(move |a, b| {
        let key_a = (key)(a);
        let key_b = (key)(b);
        key_a.cmp(&key_b).into()
    })
    .upcast()
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

    fn item_accel(
        self,
        label: impl Into<String>,
        detailed_action: impl AsRef<str>,
        accel: &str,
    ) -> Self;

    fn item_icon(
        self,
        label: impl Into<String>,
        detailed_action: impl AsRef<str>,
        icon: &str,
    ) -> Self;

    fn item_accel_and_icon(
        self,
        label: impl Into<String>,
        detailed_action: impl AsRef<str>,
        accel: &str,
        icon: &str,
    ) -> Self;

    fn section(self, section: gio::Menu) -> Self;

    fn submenu(self, label: impl Into<String>, section: gio::Menu) -> Self;
}

impl MenuBuilderExt for gio::Menu {
    fn item(self, label: impl Into<String>, detailed_action: impl AsRef<str>) -> Self {
        self.append(Some(&label.into()), Some(detailed_action.as_ref()));
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

    fn item_icon(
        self,
        label: impl Into<String>,
        detailed_action: impl AsRef<str>,
        icon: &str,
    ) -> Self {
        let item = gio::MenuItem::new(Some(&label.into()), Some(detailed_action.as_ref()));
        item.set_icon(&gio::ThemedIcon::new(icon));
        self.append_item(&item);
        self
    }

    fn item_accel_and_icon(
        self,
        label: impl Into<String>,
        detailed_action: impl AsRef<str>,
        accel: &str,
        icon: &str,
    ) -> Self {
        let item = gio::MenuItem::new(Some(&label.into()), Some(detailed_action.as_ref()));
        item.set_attribute_value("accel", Some(&accel.to_variant()));
        item.set_icon(&gio::ThemedIcon::new(icon));
        self.append_item(&item);
        self
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

pub fn extract_menu_shortcuts(menu: &gio::MenuModel) -> gio::ListModel {
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
    store.upcast()
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
        let _ = result?;
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

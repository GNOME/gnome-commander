// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

#[repr(C)]
pub enum FileSelectorID {
    Left = 0,
    Right,
    Active,
    Inactive,
}

/// The (reversed) order of following enums compared to the occurrence in the GUI is significant
#[repr(C)]
#[derive(Clone, Copy, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdConfirmOverwriteMode")]
pub enum ConfirmOverwriteMode {
    Silently = 0,
    SkipAll,
    RenameAll,
    Query,
}

#[repr(C)]
#[derive(Clone, Copy, Default, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdDndMode")]
pub enum DndMode {
    #[default]
    Query = 0,
    Copy,
    Move,
}

#[repr(C)]
#[derive(Clone, Copy, glib::Variant, glib::Enum)]
#[enum_type(name = "GnomeCmdTransferType")]
pub enum GnomeCmdTransferType {
    Copy = 0,
    Move,
    Link,
}

#[repr(C)]
#[derive(Clone, Copy, Default, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdGraphicalLayoutMode")]
pub enum GraphicalLayoutMode {
    Text = 0,
    TypeIcons,
    #[default]
    MimeIcons,
}

#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Eq, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdExtensionDisplayMode")]
pub enum ExtensionDisplayMode {
    #[enum_value(nick = "with-fname")]
    WithFileName = 0,
    Stripped,
    #[default]
    Both,
}

#[repr(C)]
#[derive(Clone, Copy, Default, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdSizeDisplayMode")]
pub enum SizeDisplayMode {
    Plain = 0,
    Locale,
    Grouped,
    #[default]
    Powered,
}

#[repr(C)]
#[derive(Clone, Copy, Default, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdPermissionDisplayMode")]
pub enum PermissionDisplayMode {
    #[default]
    Text = 0,
    Number,
}

#[repr(C)]
#[derive(Clone, Copy, Default, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdIconScaleQuality")]
pub enum IconScaleQuality {
    Nearest = 0,
    Tiles,
    Bilinear,
    #[default]
    Hyper,
}

#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Eq, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdQuickSearchShortcut")]
pub enum QuickSearchShortcut {
    #[default]
    CtrlAlt = 0,
    Alt,
    JustACharacter,
}

#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Eq, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdLeftMouseButtonMode")]
pub enum LeftMouseButtonMode {
    #[enum_value(nick = "single-click")]
    OpensWithSingleClick,
    #[default]
    #[enum_value(nick = "double-click")]
    OpensWithDoubleClick,
}

#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Eq, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdMiddleMouseButtonMode")]
pub enum MiddleMouseButtonMode {
    #[default]
    GoesUpDir,
    OpensNewTab,
}

#[repr(C)]
#[derive(Clone, Copy, Default, PartialEq, Eq, strum::FromRepr, glib::Enum)]
#[enum_type(name = "GnomeCmdRightMouseButtonMode")]
pub enum RightMouseButtonMode {
    #[default]
    PopupsMenu,
    Selects,
}

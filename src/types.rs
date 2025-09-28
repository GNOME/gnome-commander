/*
 * Copyright 2001-2006 Marcus Bjurman
 * Copyright 2007-2012 Piotr Eljasiak
 * Copyright 2013-2024 Uwe Scholz
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

#[repr(C)]
#[allow(non_camel_case_types)]
pub enum FileSelectorID {
    LEFT = 0,
    RIGHT,
    ACTIVE,
    INACTIVE,
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
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, glib::Variant, glib::Enum)]
#[enum_type(name = "GnomeCmdTransferType")]
pub enum GnomeCmdTransferType {
    COPY = 0,
    MOVE,
    LINK,
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

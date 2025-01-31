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
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, strum::FromRepr)]
pub enum GnomeCmdConfirmOverwriteMode {
    GNOME_CMD_CONFIRM_OVERWRITE_SILENTLY = 0,
    GNOME_CMD_CONFIRM_OVERWRITE_SKIP_ALL,
    GNOME_CMD_CONFIRM_OVERWRITE_RENAME_ALL,
    GNOME_CMD_CONFIRM_OVERWRITE_QUERY,
}

#[repr(C)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy)]
pub enum GnomeCmdTransferType {
    COPY = 0,
    MOVE,
    LINK,
}

#[repr(C)]
#[derive(Clone, Copy, Default, strum::FromRepr)]
pub enum GraphicalLayoutMode {
    Text = 0,
    TypeIcons,
    #[default]
    MimeIcons,
}

#[repr(C)]
#[derive(Clone, Copy, Default, strum::FromRepr)]
pub enum ExtensionDisplayMode {
    WithFileName = 0,
    Stripped,
    #[default]
    Both,
}

#[repr(C)]
#[derive(Clone, Copy, Default, strum::FromRepr)]
pub enum SizeDisplayMode {
    Plain = 0,
    Locale,
    Grouped,
    #[default]
    Powered,
}

#[repr(C)]
#[derive(Clone, Copy, Default, strum::FromRepr)]
pub enum PermissionDisplayMode {
    #[default]
    Text = 0,
    Number,
}

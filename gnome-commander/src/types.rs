// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::utils::u32_enum;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FileSelectorID {
    Left,
    Right,
    Active,
    Inactive,
}

u32_enum! {
    /// The (reversed) order of following enums compared to the occurrence in the GUI is significant
    pub enum ConfirmOverwriteMode {
        Silently,
        SkipAll,
        RenameAll,
        #[default]
        Query,
    }
}

u32_enum! {
    pub enum DndMode {
        #[default]
        Query,
        Copy,
        Move,
    }
}

u32_enum! {
    pub enum TransferType {
        #[default]
        Copy,
        Move,
        Link,
    }
}

u32_enum! {
    pub enum GraphicalLayoutMode {
        Text,
        TypeIcons,
        #[default]
        MimeIcons,
    }
}

u32_enum! {
    pub enum ExtensionDisplayMode {
        WithFname,
        Stripped,
        #[default]
        Both,
    }
}

u32_enum! {
    pub enum SizeDisplayMode {
        Plain,
        Locale,
        Grouped,
        #[default]
        Powered,
    }
}

u32_enum! {
    pub enum PermissionDisplayMode {
        #[default]
        Text,
        Number,
    }
}

u32_enum! {
    pub enum IconScaleQuality {
        Nearest,
        Tiles,
        Bilinear,
        #[default]
        Hyper,
    }
}

u32_enum! {
    pub enum QuickSearchShortcut {
        #[default]
        CtrlAlt,
        Alt,
        JustACharacter,
    }
}

u32_enum! {
    pub enum LeftMouseButtonMode {
        SingleClick,
        #[default]
        DoubleClick,
    }
}

u32_enum! {
    pub enum MiddleMouseButtonMode {
        #[default]
        GoesUpDir,
        OpensNewTab,
    }
}

u32_enum! {
    pub enum RightMouseButtonMode {
        #[default]
        PopupsMenu,
        Selects,
    }
}

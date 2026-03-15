// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/

// SPDX-License-Identifier: GPL-3.0-or-later

pub mod device;
pub mod home;
pub mod list;
pub mod remote;
pub mod smb;

pub mod bookmark;

#[path = "connection.rs"]
pub mod connection_inner;
pub use connection_inner::*;

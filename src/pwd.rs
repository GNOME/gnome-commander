/*
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

use libc::{endgrent, endpwent, geteuid, getgrent, getpwent, setgrent, setpwent};
pub use libc::{gid_t, uid_t};
use std::ffi::{CStr, CString};

pub fn uid() -> uid_t {
    unsafe { geteuid() }
}

#[derive(Clone)]
pub struct SystemUser {
    pub id: uid_t,
    pub gid: gid_t,
    pub name: CString,
}

pub fn system_users() -> Vec<SystemUser> {
    let mut users = Vec::new();
    unsafe {
        setpwent();
        loop {
            let entry = getpwent();
            if entry.is_null() {
                break;
            }
            users.push(SystemUser {
                id: (*entry).pw_uid,
                gid: (*entry).pw_gid,
                name: CStr::from_ptr((*entry).pw_name).to_owned(),
            });
        }
        endpwent();
    }
    users
}

#[derive(Clone)]
pub struct SystemGroup {
    pub id: gid_t,
    pub name: CString,
    pub members: Vec<CString>,
}

pub fn system_groups() -> Vec<SystemGroup> {
    let mut groups = Vec::new();
    unsafe {
        setgrent();
        loop {
            let entry = getgrent();
            if entry.is_null() {
                break;
            }

            let mut members = Vec::new();
            let mut i = 0;
            loop {
                let member = *(*entry).gr_mem.offset(i);
                if member.is_null() {
                    break;
                }
                members.push(CStr::from_ptr(member).to_owned());
                i += 1;
            }

            groups.push(SystemGroup {
                id: (*entry).gr_gid,
                name: CStr::from_ptr((*entry).gr_name).to_owned(),
                members,
            });
        }
        endgrent();
    }
    groups
}

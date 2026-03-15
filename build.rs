// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

fn main() {
    println!("cargo:rustc-link-lib=gcmd");
    system_deps::Config::new().probe().unwrap();
}

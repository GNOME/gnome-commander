// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use glib_build_tools::compile_resources;
use std::{
    env,
    fs::File,
    io::Write,
    path,
    path::{Path, PathBuf},
};
use walkdir::WalkDir;

fn main() {
    system_deps::Config::new().probe().unwrap();

    compile_resources(
        &["pixmaps"],
        &generate_gresource("pixmaps").as_os_str().to_string_lossy(),
        "icons.gresource",
    );
}

fn generate_gresource(root_dir: impl AsRef<Path>) -> PathBuf {
    let outdir = env::var_os("OUT_DIR").unwrap();
    let mut path = PathBuf::from(outdir);
    path.push("icons.gresource.xml");

    let mut file = File::create(&path).unwrap();
    file.write_all(
        br#"<?xml version="1.0" encoding="UTF-8"?>
        <gresources>
            <gresource prefix="/org/gnome/gnome-commander/icons">
    "#,
    )
    .unwrap();

    for entry in WalkDir::new(path::absolute(root_dir).unwrap()) {
        let entry = entry.unwrap();
        if !entry.file_type().is_file() {
            continue;
        }

        let Some(extension) = entry.path().extension().and_then(|ext| ext.to_str()) else {
            continue;
        };
        if extension == "svg" {
            write!(
                file,
                r#"<file preprocess="xml-stripblanks" alias="{}">{}</file>"#,
                entry.file_name().display(),
                entry.path().display()
            )
            .unwrap();
        } else if extension == "png" {
            write!(
                file,
                r#"<file alias="{}">{}</file>"#,
                entry.file_name().display(),
                entry.path().display()
            )
            .unwrap();
        }
    }
    file.write_all(br#"</gresource></gresources>"#).unwrap();
    path
}

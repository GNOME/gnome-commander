// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    env,
    fs::File,
    io::Write,
    path,
    path::{Path, PathBuf},
    process::Command,
};
use walkdir::WalkDir;

fn main() {
    system_deps::Config::new().probe().unwrap();

    compile_resources(
        "pixmaps",
        "/org/gnome/gnome-commander/icons",
        "icons.gresource",
    );
}

fn compile_resources(root_dir: impl AsRef<Path>, prefix: &str, outfile: &str) {
    let outdir = env::var_os("OUT_DIR").unwrap();
    let mut xml_path = PathBuf::from(&outdir);
    xml_path.push(&format!("{outfile}.xml"));

    let mut file = File::create(&xml_path).unwrap();
    write!(
        file,
        r#"<?xml version="1.0" encoding="UTF-8"?><gresources><gresource prefix="{prefix}">"#
    )
    .unwrap();

    for entry in WalkDir::new(path::absolute(&root_dir).unwrap()) {
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

    let mut out_path = PathBuf::from(&outdir);
    out_path.push(outfile);
    let output = Command::new("glib-compile-resources")
        .arg("--target")
        .arg(out_path)
        .arg(xml_path)
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "glib-compile-resources failed with exit status {} and stderr:\n{}",
        output.status,
        String::from_utf8_lossy(&output.stderr)
    );

    println!("cargo:rerun-if-changed={}", root_dir.as_ref().display());
}

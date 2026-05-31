// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    env, fs,
    fs::File,
    io::Write,
    path,
    path::{Path, PathBuf},
    process::Command,
};
use walkdir::WalkDir;

const PACKAGE: &str = env!("CARGO_PKG_NAME");

fn main() {
    system_deps::Config::new().probe().unwrap();

    compile_resources(
        "pixmaps",
        "/org/gnome/gnome-commander/icons",
        "icons.gresource",
    );

    compile_locales("po");
}

fn compile_resources(root_dir: impl AsRef<Path>, prefix: &str, outfile: &str) {
    let outdir = env::var_os("OUT_DIR").unwrap();
    let mut xml_path = PathBuf::from(&outdir);
    xml_path.push(format!("{outfile}.xml"));

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

    println!("cargo::rerun-if-changed={}", root_dir.as_ref().display());
}

fn compile_locales(root_dir: impl AsRef<Path>) {
    let outdir = env::var_os("OUT_DIR").unwrap();

    let mut locale_dir = PathBuf::from(&outdir);
    locale_dir.push("locale");

    let linguas = fs::read(root_dir.as_ref().join("LINGUAS")).unwrap();
    let linguas = String::from_utf8(linguas).unwrap();
    for lang in linguas.split(['\n', '\r']) {
        let lang = lang.trim();
        if lang.is_empty() || lang.starts_with('#') {
            continue;
        }

        let _ = fs::create_dir_all(locale_dir.join(format!("{lang}/LC_MESSAGES")));
        let output = Command::new("msgfmt")
            .arg("-o")
            .arg(locale_dir.join(format!("{lang}/LC_MESSAGES/{PACKAGE}.mo")))
            .arg(root_dir.as_ref().join(format!("{lang}.po")))
            .output()
            .unwrap();
        assert!(
            output.status.success(),
            "msgfmt failed with exit status {} and stderr:\n{}",
            output.status,
            String::from_utf8_lossy(&output.stderr)
        );
    }
    println!("cargo::rerun-if-changed={}", root_dir.as_ref().display());
    println!("cargo::rustc-env=LOCAL_LOCALE_DIR={}", locale_dir.display());
}

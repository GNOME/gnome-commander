// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    i18n::I18N_CONTEXT_SINGULAR,
    plugins::PluginMetadata,
    utils::{NO_BUTTONS, WindowExt, dialog_button_box},
};
use gettextrs::{gettext, pgettext};
use gtk::{glib, pango, prelude::*};

pub fn about_plugin_dialog(parent: &gtk::Window, info: &PluginMetadata) -> gtk::Window {
    let dialog = gtk::Window::builder()
        .title(gettext("About %s").replace("%s", info.name().as_deref().unwrap_or_default()))
        .transient_for(parent)
        .resizable(false)
        .build();
    dialog.add_css_class("dialog");

    let content_area = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .build();
    dialog.set_child(Some(&content_area));

    content_area.append(
        &gtk::Label::builder()
            .selectable(true)
            .justify(gtk::Justification::Center)
            .label(
                format!(
                    "{} {}",
                    info.name().as_deref().unwrap_or_default(),
                    info.version().as_deref().unwrap_or_default(),
                )
                .trim(),
            )
            .attributes(&{
                let attrs = pango::AttrList::new();
                attrs.insert(pango::AttrFloat::new_scale(pango::SCALE_XX_LARGE));
                attrs.insert(pango::AttrInt::new_weight(pango::Weight::Bold));
                attrs
            })
            .build(),
    );

    if let Some(comments) = info.comments() {
        content_area.append(
            &gtk::Label::builder()
                .selectable(true)
                .justify(gtk::Justification::Center)
                .wrap(true)
                .label(comments)
                .build(),
        );
    }

    if let Some(copyright) = info.copyright() {
        content_area.append(
            &gtk::Label::builder()
                .selectable(true)
                .justify(gtk::Justification::Center)
                .wrap(true)
                .use_markup(true)
                .label(copyright)
                .attributes(&{
                    let attrs = pango::AttrList::new();
                    attrs.insert(pango::AttrFloat::new_scale(pango::SCALE_SMALL));
                    attrs
                })
                .build(),
        );
    }

    if let Some(webpage) = info.webpage() {
        content_area.append(
            &gtk::LinkButton::builder()
                .label(pgettext(I18N_CONTEXT_SINGULAR, "Plugin Webpage"))
                .uri(webpage)
                .build(),
        );
    }

    let authors = info.authors();
    let documenters = info.documenters();
    let translators = info.translators();
    if !authors.is_empty() || !documenters.is_empty() || !translators.is_empty() {
        let vbox = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .css_classes(["spacing"])
            .build();

        if !authors.is_empty() {
            vbox.append(&section(&gettext("Written by"), &authors));
        }

        if !documenters.is_empty() {
            vbox.append(&section(&gettext("Documented by"), &documenters));
        }

        if !translators.is_empty() {
            vbox.append(&section(&gettext("Translated by"), &translators));
        }

        content_area.append(
            &gtk::ScrolledWindow::builder()
                .vexpand(true)
                .hscrollbar_policy(gtk::PolicyType::Never)
                .vscrollbar_policy(gtk::PolicyType::Automatic)
                .propagate_natural_height(true)
                .child(&vbox)
                .build(),
        );
    }

    let close_button = gtk::Button::with_mnemonic(&gettext("_Close"));
    close_button.connect_clicked(glib::clone!(
        #[weak]
        dialog,
        move |_| dialog.close()
    ));
    content_area.append(&dialog_button_box(NO_BUTTONS, &[&close_button]));

    dialog.set_default_widget(Some(&close_button));
    dialog.set_cancel_widget(&close_button);
    close_button.grab_focus();

    dialog
}

fn section(header: &str, lines: &[String]) -> gtk::Frame {
    let vbox = gtk::Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .css_classes(["offset"])
        .build();
    for line in lines {
        vbox.append(&gtk::Label::builder().label(line).halign(gtk::Align::Start).build());
    }
    gtk::Frame::builder()
        .label(header)
        .child(&vbox)
        .css_classes(["flat"])
        .build()
}

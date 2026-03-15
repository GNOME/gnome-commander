// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{
    i18n::I18N_CONTEXT_SINGULAR,
    plugin_manager::PluginInfoOwned,
    utils::{NO_BUTTONS, WindowExt, dialog_button_box},
};
use gettextrs::{gettext, pgettext};
use gtk::{glib, pango, prelude::*};

pub fn about_plugin_dialog(parent: &gtk::Window, info: &PluginInfoOwned) -> gtk::Window {
    let dialog = gtk::Window::builder()
        .title(gettext("About %s").replace("%s", info.name.as_deref().unwrap_or_default()))
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
                    info.name.as_deref().unwrap_or_default(),
                    info.version.as_deref().unwrap_or_default(),
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

    if let Some(ref comments) = info.comments {
        content_area.append(
            &gtk::Label::builder()
                .selectable(true)
                .justify(gtk::Justification::Center)
                .wrap(true)
                .label(comments)
                .build(),
        );
    }

    if let Some(ref copyright) = info.copyright {
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

    if let Some(ref webpage) = info.webpage {
        content_area.append(
            &gtk::LinkButton::builder()
                .label(pgettext(I18N_CONTEXT_SINGULAR, "Plugin Webpage"))
                .uri(webpage)
                .build(),
        );
    }

    if has_credits(info) {
        let notebook = gtk::Notebook::builder().vexpand(true).build();
        content_area.append(&notebook);

        if !info.authors.is_empty() {
            notebook.append_page(
                &text_content(&multiline(&info.authors), true),
                Some(&gtk::Label::builder().label(gettext("Written by")).build()),
            );
        }

        if !info.documenters.is_empty() {
            notebook.append_page(
                &text_content(&multiline(&info.documenters), true),
                Some(
                    &gtk::Label::builder()
                        .label(gettext("Documented by"))
                        .build(),
                ),
            );
        }

        if let Some(ref translator) = info.translator {
            notebook.append_page(
                &text_content(translator, false),
                Some(
                    &gtk::Label::builder()
                        .label(gettext("Translated by"))
                        .build(),
                ),
            );
        }
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

fn has_credits(info: &PluginInfoOwned) -> bool {
    !info.authors.is_empty() || !info.documenters.is_empty() || info.translator.is_some()
}

fn multiline(lines: &[String]) -> String {
    let markups: Vec<_> = lines
        .iter()
        .map(|line| glib::markup_escape_text(line))
        .collect();
    markups.join("\n")
}

fn text_content(text: &str, use_markup: bool) -> gtk::Widget {
    let content = gtk::Label::builder()
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .halign(gtk::Align::Start)
        .valign(gtk::Align::Start)
        .label(text)
        .use_markup(use_markup)
        .selectable(true)
        .build();

    gtk::ScrolledWindow::builder()
        .hscrollbar_policy(gtk::PolicyType::Automatic)
        .vscrollbar_policy(gtk::PolicyType::Automatic)
        .child(&content)
        .build()
        .upcast()
}

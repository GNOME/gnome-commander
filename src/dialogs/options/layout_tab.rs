/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
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

use super::common::create_category;
use crate::{
    dialogs::options::{edit_colors_dialog::edit_colors, edit_palette_dialog::edit_palette},
    layout::{
        color_themes::{ColorTheme, ColorThemeId},
        ls_colors_palette::LsColorsPalette,
    },
    options::{
        options::{ColorOptions, GeneralOptions},
        types::WriteResult,
    },
    select_directory_button::DirectoryButton,
    types::{ExtensionDisplayMode, GraphicalLayoutMode, IconScaleQuality},
};
use gettextrs::gettext;
use gtk::{gio, pango, prelude::*};
use std::{cell::RefCell, rc::Rc};

pub struct LayoutTab {
    vbox: gtk::Box,
    list_font: gtk::FontDialogButton,
    row_height: gtk::SpinButton,
    extension_display_mode: gtk::DropDown,
    graphical_layout_mode: gtk::DropDown,
    color_theme: gtk::DropDown,
    custom_color_theme: Rc<RefCell<ColorTheme>>,
    use_ls_colors: gtk::CheckButton,
    ls_color_palette: Rc<RefCell<LsColorsPalette>>,
    icon_size: gtk::SpinButton,
    icon_scale_quality: gtk::Scale,
    mime_icon_dir: DirectoryButton,
}

impl LayoutTab {
    pub fn new() -> Self {
        let vbox = gtk::Box::builder()
            .orientation(gtk::Orientation::Vertical)
            .spacing(6)
            .margin_top(6)
            .margin_bottom(6)
            .margin_start(6)
            .margin_end(6)
            .build();

        // File panes

        let grid = gtk::Grid::builder()
            .column_spacing(12)
            .row_spacing(6)
            .build();
        vbox.append(&create_category(&gettext("File panes"), &grid));

        grid.attach(
            &gtk::Label::builder()
                .label(gettext("Font:"))
                .halign(gtk::Align::Start)
                .build(),
            0,
            0,
            1,
            1,
        );
        let list_font =
            gtk::FontDialogButton::new(Some(gtk::FontDialog::builder().modal(true).build()));
        grid.attach(&list_font, 1, 0, 2, 1);

        grid.attach(
            &gtk::Label::builder()
                .label(gettext("Row height:"))
                .halign(gtk::Align::Start)
                .build(),
            0,
            1,
            1,
            1,
        );
        let row_height = gtk::SpinButton::builder()
            .adjustment(
                &gtk::Adjustment::builder()
                    .lower(8.0)
                    .upper(64.0)
                    .step_increment(1.0)
                    .page_increment(10.0)
                    .page_size(0.0)
                    .build(),
            )
            .climb_rate(1.0)
            .digits(0)
            .numeric(true)
            .build();
        grid.attach(&row_height, 1, 1, 2, 1);

        grid.attach(
            &gtk::Label::builder()
                .label(gettext("Display file extensions:"))
                .halign(gtk::Align::Start)
                .build(),
            0,
            2,
            1,
            1,
        );
        let extension_display_mode = gtk::DropDown::builder()
            .model(&gtk::StringList::new(&[
                &gettext("With file name"),
                &gettext("In separate column"),
                &gettext("In both columns"),
            ]))
            .hexpand(true)
            .build();
        grid.attach(&extension_display_mode, 1, 2, 2, 1);

        grid.attach(
            &gtk::Label::builder()
                .label(gettext("Graphical mode:"))
                .halign(gtk::Align::Start)
                .build(),
            0,
            3,
            1,
            1,
        );
        let graphical_layout_mode = gtk::DropDown::builder()
            .model(&gtk::StringList::new(&[
                &gettext("No icons"),
                &gettext("File type icons"),
                &gettext("MIME icons"),
            ]))
            .hexpand(true)
            .build();
        grid.attach(&graphical_layout_mode, 1, 3, 2, 1);

        grid.attach(
            &gtk::Label::builder()
                .label(gettext("Color scheme:"))
                .halign(gtk::Align::Start)
                .build(),
            0,
            4,
            1,
            1,
        );
        let color_theme = gtk::DropDown::builder()
            .model(&gtk::StringList::new(&[
                &gettext("Respect theme colors"),
                &gettext("Modern"),
                &gettext("Fusion"),
                &gettext("Classic"),
                &gettext("Deep blue"),
                &gettext("Cafezinho"),
                &gettext("Green tiger"),
                &gettext("Winter"),
                &gettext("Custom"),
            ]))
            .hexpand(true)
            .build();
        grid.attach(&color_theme, 1, 4, 1, 1);
        let custom_color_theme = Rc::new(RefCell::new(ColorTheme::default()));
        let color_btn = gtk::Button::builder().label(gettext("Edit…")).build();
        color_btn.connect_clicked(glib::clone!(
            #[strong]
            custom_color_theme,
            move |button| {
                if let Some(parent_window) = button.root().and_downcast::<gtk::Window>() {
                    let custom_color_theme = custom_color_theme.clone();
                    glib::spawn_future_local(async move {
                        let theme = custom_color_theme.borrow().clone();
                        if let Some(new_theme) = edit_colors(&parent_window, theme).await {
                            custom_color_theme.replace(new_theme);
                        }
                    });
                }
            }
        ));
        grid.attach(&color_btn, 2, 4, 1, 1);

        let hbox = gtk::Box::builder()
            .orientation(gtk::Orientation::Horizontal)
            .spacing(12)
            .build();
        let use_ls_colors = gtk::CheckButton::builder()
            .label(gettext(
                "Colorize files according to the LS_COLORS environment variable",
            ))
            .hexpand(true)
            .build();
        hbox.append(&use_ls_colors);
        let ls_color_palette = Rc::new(RefCell::new(LsColorsPalette::default()));
        let ls_colors_edit_btn = gtk::Button::builder()
            .label(gettext("Edit colors…"))
            .build();
        ls_colors_edit_btn.connect_clicked(glib::clone!(
            #[strong]
            ls_color_palette,
            move |button| {
                if let Some(parent_window) = button.root().and_downcast::<gtk::Window>() {
                    let ls_color_palette = ls_color_palette.clone();
                    glib::spawn_future_local(async move {
                        let palette = ls_color_palette.borrow().clone();
                        if let Some(new_palette) = edit_palette(&parent_window, palette).await {
                            ls_color_palette.replace(new_palette);
                        }
                    });
                }
            }
        ));
        hbox.append(&ls_colors_edit_btn);
        grid.attach(&hbox, 0, 5, 3, 1);

        // MIME icon settings

        let grid = gtk::Grid::builder()
            .column_spacing(12)
            .row_spacing(6)
            .build();
        let mime_icon_settings_frame = create_category(&gettext("MIME icon settings"), &grid);
        vbox.append(&mime_icon_settings_frame);

        grid.attach(
            &gtk::Label::builder()
                .label(gettext("Icon size:"))
                .halign(gtk::Align::Start)
                .build(),
            0,
            0,
            1,
            1,
        );
        let icon_size = gtk::SpinButton::builder()
            .adjustment(
                &gtk::Adjustment::builder()
                    .lower(8.0)
                    .upper(64.0)
                    .step_increment(1.0)
                    .page_increment(10.0)
                    .page_size(0.0)
                    .build(),
            )
            .climb_rate(1.0)
            .digits(0)
            .numeric(true)
            .build();
        grid.attach(&icon_size, 1, 0, 1, 1);

        grid.attach(
            &gtk::Label::builder()
                .label(gettext("Scaling quality:"))
                .halign(gtk::Align::Start)
                .build(),
            0,
            1,
            1,
            1,
        );
        let icon_scale_quality = gtk::Scale::builder()
            .orientation(gtk::Orientation::Horizontal)
            .adjustment(&gtk::Adjustment::builder().lower(0.0).upper(3.0).build())
            .digits(0)
            .build();
        grid.attach(&icon_scale_quality, 1, 1, 1, 1);

        grid.attach(
            &gtk::Label::builder()
                .label(gettext("Theme icon directory:"))
                .halign(gtk::Align::Start)
                .build(),
            0,
            2,
            1,
            1,
        );
        let mime_icon_dir = DirectoryButton::default();
        grid.attach(&mime_icon_dir, 1, 2, 1, 1);

        graphical_layout_mode
            .bind_property("selected", &mime_icon_settings_frame, "sensitive")
            .transform_to(|_, selected: u32| Some(selected == 2))
            .sync_create()
            .build();

        color_theme
            .bind_property("selected", &color_btn, "sensitive")
            .transform_to(|_, selected: u32| {
                Some(ColorThemeId::from_repr(selected as usize) == Some(ColorThemeId::Custom))
            })
            .sync_create()
            .build();

        use_ls_colors
            .bind_property("active", &ls_colors_edit_btn, "sensitive")
            .sync_create()
            .build();

        Self {
            vbox,
            list_font,
            row_height,
            extension_display_mode,
            graphical_layout_mode,
            color_theme,
            custom_color_theme,
            use_ls_colors,
            ls_color_palette,
            icon_size,
            icon_scale_quality,
            mime_icon_dir,
        }
    }

    pub fn widget(&self) -> gtk::Widget {
        self.vbox.clone().upcast()
    }

    pub fn read(&self, general_options: &GeneralOptions, color_options: &ColorOptions) {
        self.list_font
            .set_font_desc(&pango::FontDescription::from_string(
                &general_options.list_font.get(),
            ));
        self.row_height
            .set_value(general_options.list_row_height.get() as f64);
        self.extension_display_mode
            .set_selected(general_options.extension_display_mode.get() as u32);
        self.graphical_layout_mode
            .set_selected(general_options.graphical_layout_mode.get() as u32);
        self.color_theme.set_selected(
            color_options
                .theme
                .get()
                .map(|t| t as u32)
                .unwrap_or_default(),
        );
        self.custom_color_theme
            .replace(color_options.custom_theme());
        self.use_ls_colors
            .set_active(color_options.use_ls_colors.get());
        self.ls_color_palette
            .replace(color_options.ls_color_palette());
        self.icon_size
            .set_value(general_options.icon_size.get() as f64);
        self.icon_scale_quality
            .set_value(general_options.icon_scale_quality.get() as i32 as f64);
        self.mime_icon_dir
            .set_file(general_options.mime_icon_dir.get().map(gio::File::for_path));
    }

    pub fn write(
        &self,
        general_options: &GeneralOptions,
        color_options: &ColorOptions,
    ) -> WriteResult {
        if let Some(font_desc) = self.list_font.font_desc() {
            general_options.list_font.set(font_desc.to_str())?;
        }
        general_options.list_row_height.set(
            <i32 as TryInto<u32>>::try_into(self.row_height.value_as_int()).unwrap_or_default(),
        )?;
        general_options.extension_display_mode.set(
            self.extension_display_mode
                .selected()
                .try_into()
                .ok()
                .and_then(ExtensionDisplayMode::from_repr)
                .unwrap_or_default(),
        )?;
        general_options.graphical_layout_mode.set(
            self.graphical_layout_mode
                .selected()
                .try_into()
                .ok()
                .and_then(GraphicalLayoutMode::from_repr)
                .unwrap_or_default(),
        )?;
        color_options.theme.set(
            self.color_theme
                .selected()
                .try_into()
                .ok()
                .and_then(ColorThemeId::from_repr),
        )?;
        color_options.set_custom_theme(&*self.custom_color_theme.borrow())?;
        color_options
            .use_ls_colors
            .set(self.use_ls_colors.is_active())?;
        color_options.set_ls_color_palette(&*self.ls_color_palette.borrow())?;
        general_options
            .icon_size
            .set(self.icon_size.value_as_int() as u32)?;
        general_options.icon_scale_quality.set(
            IconScaleQuality::from_repr(self.icon_scale_quality.value() as usize)
                .unwrap_or_default(),
        )?;
        general_options
            .mime_icon_dir
            .set(self.mime_icon_dir.file().and_then(|f| f.path()))?;
        Ok(())
    }
}

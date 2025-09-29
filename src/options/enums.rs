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

#[cfg(test)]
mod test {
    use crate::{
        layout::color_themes::ColorThemeId,
        tab_label::TabLockIndicator,
        types::{
            ConfirmOverwriteMode, DndMode, ExtensionDisplayMode, GnomeCmdTransferType,
            GraphicalLayoutMode, LeftMouseButtonMode, MiddleMouseButtonMode, PermissionDisplayMode,
            QuickSearchShortcut, RightMouseButtonMode, SizeDisplayMode,
        },
    };
    use gtk::prelude::*;

    fn enum_xml(type_: glib::Type, indent: &str) -> String {
        let enum_class = glib::EnumClass::with_type(type_).unwrap();

        let mut xml = String::new();
        xml.push_str(indent);

        xml.push_str(&format!(
            "<enum id=\"org.gnome.gnome-commander.{}\">\n",
            type_.name()
        ));
        for enum_value in enum_class.values() {
            xml.push_str(&format!(
                "{}  <value nick=\"{}\" value=\"{}\"/>\n",
                indent,
                enum_value.nick(),
                enum_value.value()
            ));
        }
        xml.push_str(indent);
        xml.push_str("</enum>\n");

        xml
    }

    fn color_theme_xml(indent: &str) -> String {
        let mut xml = format!(
            "{}<enum id=\"org.gnome.gnome-commander.GnomeCmdColorMode\">\n",
            indent
        );
        xml.push_str(indent);
        xml.push_str("  <value nick=\"none\" value=\"0\"/>\n");
        for (enum_value, name) in <ColorThemeId as strum::VariantArray>::VARIANTS
            .iter()
            .zip(<ColorThemeId as strum::VariantNames>::VARIANTS.iter())
        {
            xml.push_str(indent);
            xml.push_str(&format!(
                "  <value nick=\"{}\" value=\"{}\"/>\n",
                camel_case_to_kebab_case(name),
                *enum_value as i32
            ));
        }
        xml.push_str(indent);
        xml.push_str("</enum>\n");

        xml
    }

    fn camel_case_to_kebab_case(name: &str) -> String {
        let mut kebab = String::new();
        for (index, char) in name.chars().enumerate() {
            if index > 0 && char.is_ascii_uppercase() {
                kebab.push('-');
                kebab.push(char.to_ascii_lowercase());
            } else {
                kebab.push(char.to_ascii_lowercase());
            }
        }
        kebab
    }

    #[test]
    fn test_schema_xml() {
        let enum_types = [
            enum_xml(GraphicalLayoutMode::static_type(), "  "),
            enum_xml(SizeDisplayMode::static_type(), "  "),
            enum_xml(GnomeCmdTransferType::static_type(), "  "),
            enum_xml(PermissionDisplayMode::static_type(), "  "),
            enum_xml(QuickSearchShortcut::static_type(), "  "),
            enum_xml(ExtensionDisplayMode::static_type(), "  "),
            enum_xml(ConfirmOverwriteMode::static_type(), "  "),
            enum_xml(DndMode::static_type(), "  "),
            enum_xml(LeftMouseButtonMode::static_type(), "  "),
            enum_xml(MiddleMouseButtonMode::static_type(), "  "),
            enum_xml(RightMouseButtonMode::static_type(), "  "),
            enum_xml(TabLockIndicator::static_type(), "  "),
            color_theme_xml("  "),
        ];
        let schemalist = format!("<schemalist>\n{}</schemalist>\n", enum_types.join(""));

        let expected = include_str!("../../data/org.gnome.gnome-commander.enums.xml");

        assert_eq!(schemalist, expected);
    }
}

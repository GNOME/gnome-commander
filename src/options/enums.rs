// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#[cfg(test)]
mod test {
    use crate::{
        file_list::quick_search::QuickSearchMode,
        layout::color_themes::ColorThemeId,
        tab_label::TabLockIndicator,
        types::{
            ConfirmOverwriteMode, DndMode, ExtensionDisplayMode, GnomeCmdTransferType,
            GraphicalLayoutMode, LeftMouseButtonMode, MiddleMouseButtonMode, PermissionDisplayMode,
            QuickSearchShortcut, RightMouseButtonMode, SizeDisplayMode,
        },
        utils::IterableEnum,
    };

    fn enum_xml<T>(name: &str, indent: &str) -> String
    where
        T: IterableEnum + std::fmt::Debug,
        u32: From<T>,
    {
        let mut xml = String::new();
        xml.push_str(indent);

        xml.push_str(&format!("<enum id=\"org.gnome.gnome-commander.{name}\">\n",));
        for variant in T::iter() {
            let mut nick = format!("{variant:?}");

            // Convert CamelCase to kebab-case
            let (first, _) = nick.split_at_mut(1);
            first.make_ascii_lowercase();
            let nick = nick
                .chars()
                .map(|c| {
                    if !c.is_ascii_uppercase() {
                        c.to_string()
                    } else {
                        ['-', c.to_ascii_lowercase()].into_iter().collect()
                    }
                })
                .collect::<String>();

            xml.push_str(&format!(
                "{indent}  <value nick=\"{nick}\" value=\"{}\"/>\n",
                u32::from(variant)
            ));
        }
        xml.push_str(indent);
        xml.push_str("</enum>\n");

        xml
    }

    #[test]
    fn test_schema_xml() {
        let enum_types = [
            enum_xml::<GraphicalLayoutMode>("GnomeCmdGraphicalLayoutMode", "  "),
            enum_xml::<SizeDisplayMode>("GnomeCmdSizeDisplayMode", "  "),
            enum_xml::<GnomeCmdTransferType>("GnomeCmdTransferType", "  "),
            enum_xml::<PermissionDisplayMode>("GnomeCmdPermissionDisplayMode", "  "),
            enum_xml::<QuickSearchShortcut>("GnomeCmdQuickSearchShortcut", "  "),
            enum_xml::<QuickSearchMode>("GnomeCmdQuickSearchMode", "  "),
            enum_xml::<ExtensionDisplayMode>("GnomeCmdExtensionDisplayMode", "  "),
            enum_xml::<ConfirmOverwriteMode>("GnomeCmdConfirmOverwriteMode", "  "),
            enum_xml::<DndMode>("GnomeCmdDndMode", "  "),
            enum_xml::<LeftMouseButtonMode>("GnomeCmdLeftMouseButtonMode", "  "),
            enum_xml::<MiddleMouseButtonMode>("GnomeCmdMiddleMouseButtonMode", "  "),
            enum_xml::<RightMouseButtonMode>("GnomeCmdRightMouseButtonMode", "  "),
            enum_xml::<TabLockIndicator>("GnomeCmdTabLockIndicator", "  "),
            enum_xml::<ColorThemeId>("GnomeCmdColorMode", "  "),
        ];
        let schemalist = format!("<schemalist>\n{}</schemalist>\n", enum_types.join(""));

        let mut expected =
            include_str!("../../data/org.gnome.gnome-commander.enums.xml").to_owned();
        while let Some((before_comment, rest)) = expected.split_once("<!--") {
            let Some((_, after_comment)) = rest.split_once("-->") else {
                break;
            };
            expected = before_comment.to_owned() + after_comment;
        }

        assert_eq!(schemalist.trim(), expected.trim());
    }
}

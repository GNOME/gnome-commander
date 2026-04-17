// SPDX-FileCopyrightText: 2024 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! ### Internal Viewer
//!
//! This module contains all the functionality of the internal viewer. Normally, the entry point
//! to the module’s functionality will be [`window::ViewerWindow`] which is the Gtk widget
//! representing the viewer window. All other modules and types are meant to be used internally.
//! How these play together:
//!
//! * `window::ViewerWindow` can display either text content (Text, Fixed Width and Hexadecimal
//!     modes) or images (Image mode). The former is handled by the [`text_render::TextRender`]
//!     widget, the latter [`image_render::ImageRender`] widget. Both widgets are always present in
//!     the viewer window but only one is visible at a time.
//! * `image_render::ImageRender` is the simpler component because it relies on [`gtk::gdk_pixbuf`]
//!     crate for most of its functionality. It has no internal dependencies.
//! * `text_render::TextRender` relies on a number of components working together:
//!     * [`input_modes::InputSource`] trait provides access to raw file bytes. For anything other
//!         than tests the implementation used will be [`file_input_source::FileInputSource`].
//!     * `input_modes::CharacterEncoding` trait provides functionality mapping bytes to
//!         characters. There are implementations of this trait handling single-byte encodings and
//!         UTF-8.
//!     * [`input_modes::InputMode`] type manages `input_modes::InputSource` and
//!         `input_modes::CharacterEncoding` internally and exposes an interface allowing access to
//!         the file on both byte and character level.
//!     * [`data_presentation::DataPresentation`] somewhat abstracts away how bytes or characters
//!         are mapped to lines depending on the selected viewer mode.
//! * [`search_bar::SearchBar`] widget provides the user interface to enter text search parameters.
//!     It also contains controls to initiate a search, but it merely passes on the signal to
//!     `window::ViewerWindow`. The latter also has some ways of initiating a search such as
//!     keyboard shortcuts.
//! * [`searcher::Searcher`] is the type tasked with performing a search. It will scan the file
//!     starting from a given position forward or backward.
//! * [`boyer_moore::BoyerMoore`] is used by `searcher::Searcher` internally and is our
//!     implementation of the Boyer-Moore string-search algorithm.
//!
//! #### Offsets
//!
//! File offsets used by the internal viewer are *always* byte offsets. The vertical scrollbar of
//! the viewer window also calculates positions as byte offsets. This means that with multi-byte
//! encodings such as UTF-8 some offsets produced by scrolling vertically will not be valid, these
//! point inside a character. Similar offsets not aligned to character boundaries can be produced by
//! the search. [`input_modes::InputMode`] type provides methods to correct unaligned file offsets.
//!
//! Note that *horizontal* scrolling calculates positions in characters as currently displayed. The
//! required information on the longest line is provided by the rendering code. This means that the
//! size of the horizontal scrollbar can increase while scrolling through the file as longer lines
//! are encountered. This is a performance decision: having a fixed size horizontal scrollbar would
//! require scanning the entire file when it is opened.
//!
//! #### Operational modes
//!
//! When text content is displayed, [`window::ViewerWindow`] and
//! [`data_presentation::DataPresentation`] have different means of representing the current
//! operational mode. The mapping between the two is not entirely trivial.
//!
//! `window::ViewerWindow` represents operational mode in the same terms that are displayed to the
//! user:
//!
//! * [`display_mode`](window::ViewerWindow::display_mode()) enum which can take the values
//!     `Text`, `FixedWidth` or `Hexdump` (as well as `Image` to display an image)
//! * [`wrap_lines`](window::ViewerWindow::wrap_lines()) flag, only considered in `Text` mode
//! * [`chars_per_line`](window::ViewerWindow::chars_per_line()) setting, only_considered in
//!     `FixedWidth` mode
//!
//! `data_presentation::DataPresentation` uses different internal modes:
//!
//! * [`mode`](data_presentation::DataPresentation::mode) enum which can take the values `NoWrap`
//!     (corresponds to `Text` with `wrap_lines` flag off), `Wrap` (`Text` with `wrap_lines` on),
//!     `Fixed` (used for `FixedWidth` with variable-length encodings) and `Binary` (used for
//!     `FixedWidth` with fixed-length encodings and for `Hexdump`).
//! * [`wrap_limit`](data_presentation::DataPresentation::wrap_limit) is used in `Wrap` mode, a
//!     maximum number of characters to display in a line.
//! * [`fixed_count`](data_presentation::DataPresentation::fixed_count) is used to determine line
//!     wrapping in both `Fixed` and `Binary` mode. However, in `Fixed` mode it is a number of
//!     *characters* (corresponds to viewer window’s `chars_per_line`) where as in `Binary` mode it
//!     it is the number of *bytes* at which a line should be wrapped.
//!
//! Note that the viewer’s `FixedWidth` mode always displays characters regardless of which data
//! presentation mode it is mapped to, never bytes. So when the simpler logic of the data
//! presentation’s `Binary` mode is used, `fixed_count` will be set to the number of bytes in
//! `chars_per_line` characters, meaning that `chars_per_line` will be multiplied with the character
//! length.
//!
//! The `Hexdump` mode on the other hand displays bytes. So `fixed_count` will always be set to 16
//! here, representing the number of bytes shown in a hex dump line. The selected encoding is
//! respected nevertheless: if a single-byte encoding is chosen it will be used to map the bytes to
//! their respective Unicode characters. For other encodings the bytes will be mapped to the first
//! 256 Unicode codepoints.

pub mod boyer_moore;
pub mod data_presentation;
pub mod file_input_source;
pub mod image_render;
pub mod input_modes;
pub mod search_bar;
pub mod searcher;
pub mod text_render;
pub mod window;

// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use gtk::prelude::*;
use std::cell::{Cell, RefCell};

/// A helper wrapping `gtk4::Grid`, allows structured adding of rows.
#[derive(Debug, Default)]
pub struct Grid {
    grid: gtk::Grid,
    current_row: Cell<i32>,
}

impl std::ops::Deref for Grid {
    type Target = gtk::Grid;

    fn deref(&self) -> &Self::Target {
        &self.grid
    }
}

impl AsRef<gtk::Widget> for Grid {
    fn as_ref(&self) -> &gtk::Widget {
        self.grid.upcast_ref()
    }
}

impl Grid {
    /// Adds a row to the end of the grid.
    pub fn append_row(&self, grid_row: &GridRow) {
        let mut col = 0;
        let row = self.current_row.get();
        for (span, widget) in grid_row.widgets.take().into_iter() {
            self.grid.attach(&widget, col, row, span, 1);
            col += span;
        }
        self.current_row.set(row + 1);
    }
}

/// Keeps track of widgets in a grid row before they are added.
#[derive(Debug, Default)]
pub struct GridRow {
    span: Cell<i32>,
    label: RefCell<Option<gtk::Label>>,
    widgets: RefCell<Vec<(i32, gtk::Widget)>>,
}

impl AsRef<GridRow> for GridRow {
    fn as_ref(&self) -> &GridRow {
        self
    }
}

impl GridRow {
    /// Marks the next widget to be preceded by a label with the given text.
    pub fn labeled(&self, text: &str) {
        self.labeled_with(text, |_label| {});
    }

    /// Marks the next widget to be preceded by a label with the given text. Allows additional
    /// customization of the label via the provided callback.
    pub fn labeled_with(&self, text: &str, callback: impl Fn(&gtk::Label)) {
        let label = gtk::Label::builder()
            .label(text)
            .use_underline(true)
            .halign(gtk::Align::Start)
            .valign(gtk::Align::Center)
            .build();
        callback(&label);
        self.label.replace(Some(label));
    }

    /// Marks the next widget to span the given number of columns instead of the default 1.
    pub fn spanned(&self, span: i32) {
        assert!(span >= 1);
        self.span.replace(span);
    }

    /// Adds a widget to the row.
    pub fn append(&self, widget: &impl IsA<gtk::Widget>) {
        if let Some(label) = self.label.take() {
            label.set_mnemonic_widget(Some(widget.as_ref()));
            self.widgets.borrow_mut().push((1, label.upcast()));
        }

        let span = self.span.take().max(1);
        self.widgets
            .borrow_mut()
            .push((span, widget.as_ref().clone()));
    }

    /// No-op for compatibility with Gtk widget types.
    #[doc(hidden)]
    pub fn unparent(&self) {}
}

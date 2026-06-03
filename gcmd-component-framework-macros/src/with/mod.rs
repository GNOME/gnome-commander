// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod block;
mod for_statement;
mod if_statement;
mod method_call;
mod statement;
mod widget;

use proc_macro::TokenStream;
use quote::ToTokens;
use syn::parse_macro_input;
use widget::Widget;

pub(crate) fn with(input: TokenStream) -> TokenStream {
    let widget = parse_macro_input!(input as Widget);
    widget.to_token_stream().into()
}

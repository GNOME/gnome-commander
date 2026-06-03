// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    for_statement::ForStatement, if_statement::IfStatement, method_call::MethodCall, widget::Widget,
};
use quote::{ToTokens, quote_spanned};
use syn::{
    Error, Token,
    parse::{Parse, ParseStream},
    spanned::Spanned,
};

pub enum Statement {
    For(ForStatement),
    If(IfStatement),
    Method(MethodCall),
    Widget(Widget),
}

impl Parse for Statement {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        Ok(if input.peek(Token![for]) {
            Self::For(input.parse()?)
        } else if input.peek(Token![if]) {
            Self::If(input.parse()?)
        } else if input.peek(Token![.]) {
            Self::Method(input.parse()?)
        } else {
            Self::Widget(input.parse()?)
        })
    }
}

impl ToTokens for Statement {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        match self {
            Self::For(for_statement) => for_statement.to_tokens(stream),
            Self::If(if_statement) => if_statement.to_tokens(stream),
            Self::Method(method_call) => method_call.to_tokens(stream),
            Self::Widget(widget) => {
                stream.extend(quote_spanned! {widget.span()=>
                    ::component_framework::container::ContainerExt::container_set_child(
                        &__context, #widget
                    );
                });
            }
        }
    }
}

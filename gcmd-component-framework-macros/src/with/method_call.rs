// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use quote::{ToTokens, quote};
use syn::{
    Error, Expr, PathSegment, Token, parenthesized,
    parse::{Parse, ParseStream},
    punctuated::Punctuated,
    token,
};

pub struct MethodCall {
    _dot_token: Token![.],
    method: PathSegment,
    _paren_token: token::Paren,
    params: Punctuated<Expr, token::Comma>,
    _semi_token: Token![;],
}

impl Parse for MethodCall {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        let content;
        Ok(Self {
            _dot_token: input.parse()?,
            method: input.parse()?,
            _paren_token: parenthesized!(content in input),
            params: content.parse_terminated(Expr::parse, token::Comma)?,
            _semi_token: input.parse()?,
        })
    }
}

impl ToTokens for MethodCall {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        let Self { method, params, .. } = self;
        stream.extend(quote! {
            __context . #method ( #params );
        });
    }
}

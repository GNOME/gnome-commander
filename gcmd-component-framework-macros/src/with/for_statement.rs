// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::block::Block;
use quote::{ToTokens, quote};
use syn::{
    Error, Expr, Pat, Token,
    parse::{Parse, ParseStream},
};

pub struct ForStatement {
    _for_token: Token![for],
    pat: Pat,
    _in_token: Token![in],
    expr: Expr,
    block: Block,
}

impl Parse for ForStatement {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        Ok(Self {
            _for_token: input.parse()?,
            pat: Pat::parse_multi(input)?,
            _in_token: input.parse()?,
            expr: input.parse()?,
            block: input.parse()?,
        })
    }
}

impl ToTokens for ForStatement {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        let Self {
            pat, expr, block, ..
        } = self;
        stream.extend(quote! {
            for #pat in #expr #block
        });
    }
}

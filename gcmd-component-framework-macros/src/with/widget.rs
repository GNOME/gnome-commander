// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::block::Block;
use quote::{ToTokens, quote};
use syn::{
    Error, Expr, TypePath,
    parse::{Parse, ParseStream},
};

struct WidgetSource(Expr);

impl Parse for WidgetSource {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        let mut expr = Expr::parse_without_eager_brace(input)?;
        if let Ok(path) = syn::parse2::<TypePath>(expr.to_token_stream()) {
            expr = syn::parse2(quote! {
                #path :: default()
            })?;
        }
        Ok(Self(expr))
    }
}

impl ToTokens for WidgetSource {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        self.0.to_tokens(stream);
    }
}

pub struct Widget {
    source: WidgetSource,
    block: Block,
}

impl Parse for Widget {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        Ok(Self {
            source: input.parse()?,
            block: input.parse()?,
        })
    }
}

impl ToTokens for Widget {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        let Self { source, block } = self;
        stream.extend(quote! {
            {
                let __context = #source;
                #block
                __context
            }
        });
    }
}

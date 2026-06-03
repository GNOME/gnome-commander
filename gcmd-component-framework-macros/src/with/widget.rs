// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::block::Block;
use quote::{ToTokens, quote};
use syn::{
    Error, Expr, Token, TypePath,
    parse::{Parse, ParseStream},
};

enum WidgetSource {
    Reference(Expr),
    Call(Expr),
}

impl Parse for WidgetSource {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        Ok(if input.peek(Token![&]) {
            Self::Reference(Expr::parse_without_eager_brace(input)?)
        } else {
            let mut call = Expr::parse_without_eager_brace(input)?;
            if let Ok(path) = syn::parse2::<TypePath>(call.to_token_stream()) {
                call = syn::parse2(quote! {
                    #path :: default()
                })?;
            }
            Self::Call(call)
        })
    }
}

impl ToTokens for WidgetSource {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        match self {
            Self::Reference(expr) | Self::Call(expr) => expr.to_tokens(stream),
        }
    }
}

pub struct Widget {
    source: WidgetSource,
    block: Block,
}

impl Widget {
    pub fn is_reference(&self) -> bool {
        matches!(self.source, WidgetSource::Reference(..))
    }
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
        let reference = if matches!(source, WidgetSource::Reference(..)) {
            quote! {
                let __context_ref = __context;
            }
        } else {
            quote! {
                let __context_ref = &__context;
            }
        };
        stream.extend(quote! {
            {
                let __context = #source;
                #reference
                #block
                __context
            }
        });
    }
}

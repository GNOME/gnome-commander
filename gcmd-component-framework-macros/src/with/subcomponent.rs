// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use quote::{ToTokens, quote_spanned};

use syn::{
    Error, Expr, Token,
    parse::{Parse, ParseStream},
    spanned::Spanned,
};

pub struct Subcomponent {
    _plus_token: Token![+],
    subcomponent: Expr,
    message_conversion: Option<Expr>,
    _semi_token: Token![;],
}

impl Subcomponent {
    fn parse_message_conversion(input: ParseStream) -> Result<Option<Expr>, Error> {
        Ok(if input.peek(Token![~]) && input.peek2(Token![>]) {
            input.parse::<Token![~]>()?;
            input.parse::<Token![>]>()?;
            Some(input.parse()?)
        } else {
            None
        })
    }
}

impl Parse for Subcomponent {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        Ok(Self {
            _plus_token: input.parse()?,
            subcomponent: input.parse()?,
            message_conversion: Self::parse_message_conversion(input)?,
            _semi_token: input.parse()?,
        })
    }
}

impl ToTokens for Subcomponent {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        let Self {
            subcomponent,
            message_conversion,
            ..
        } = self;
        stream.extend(quote_spanned! {subcomponent.span()=>
            ::component_framework::container::ContainerExt::container_set_child(
                __context_ref, #subcomponent . root()
            );
        });
        if let Some(message_conversion) = message_conversion {
            stream.extend(quote_spanned! {subcomponent.span()=>
                #subcomponent.forward_messages(sender, #message_conversion);
            });
        }
    }
}

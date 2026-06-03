// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::statement::Statement;
use quote::{ToTokens, quote};
use syn::{
    Error, braced,
    parse::{Parse, ParseStream},
    token,
};

pub struct Block {
    _brace_token: token::Brace,
    statements: Vec<Statement>,
}

impl Block {
    fn parse_statements(input: ParseStream) -> Result<Vec<Statement>, Error> {
        let mut result = Vec::new();
        while !input.is_empty() {
            result.push(input.parse()?);
        }
        Ok(result)
    }
}

impl Parse for Block {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        let content;
        Ok(Self {
            _brace_token: braced!(content in input),
            statements: Self::parse_statements(&content)?,
        })
    }
}

impl ToTokens for Block {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        let statements = &self.statements;
        stream.extend(quote! {
            {
                #(#statements)*
            }
        });
    }
}

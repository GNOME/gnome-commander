// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::block::Block;
use quote::{ToTokens, quote};
use syn::{
    Error, Expr, Token,
    parse::{Parse, ParseStream},
};

pub struct IfStatement {
    _if_token: Token![if],
    expr: Expr,
    block: Block,
    elseifs: Vec<ElseIfBranch>,
    else_branch: Option<ElseBranch>,
}

impl IfStatement {
    fn parse_elseifs(input: ParseStream) -> Result<Vec<ElseIfBranch>, Error> {
        let mut result = Vec::new();
        while input.peek(Token![else]) && input.peek2(Token![if]) {
            result.push(input.parse()?);
        }
        Ok(result)
    }

    fn parse_else_branch(input: ParseStream) -> Result<Option<ElseBranch>, Error> {
        if input.peek(Token![else]) {
            Ok(Some(input.parse()?))
        } else {
            Ok(None)
        }
    }
}

impl Parse for IfStatement {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        Ok(Self {
            _if_token: input.parse()?,
            expr: input.parse()?,
            block: input.parse()?,
            elseifs: Self::parse_elseifs(input)?,
            else_branch: Self::parse_else_branch(input)?,
        })
    }
}

impl ToTokens for IfStatement {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        let Self {
            expr,
            block,
            elseifs,
            else_branch,
            ..
        } = self;
        stream.extend(quote! {
            if #expr #block #(#elseifs)*
        });
        if let Some(else_branch) = else_branch {
            else_branch.to_tokens(stream);
        }
    }
}

struct ElseIfBranch {
    _else_token: Token![else],
    _if_token: Token![if],
    expr: Expr,
    block: Block,
}

impl Parse for ElseIfBranch {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        Ok(Self {
            _else_token: input.parse()?,
            _if_token: input.parse()?,
            expr: input.parse()?,
            block: input.parse()?,
        })
    }
}

impl ToTokens for ElseIfBranch {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        let Self { expr, block, .. } = self;
        stream.extend(quote! {
            else if #expr #block
        });
    }
}

struct ElseBranch {
    _else_token: Token![else],
    block: Block,
}

impl Parse for ElseBranch {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        Ok(Self {
            _else_token: input.parse()?,
            block: input.parse()?,
        })
    }
}

impl ToTokens for ElseBranch {
    fn to_tokens(&self, stream: &mut proc_macro2::TokenStream) {
        let block = &self.block;
        stream.extend(quote! {
            else #block
        });
    }
}

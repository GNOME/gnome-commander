// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use proc_macro::TokenStream;
use proc_macro2::Span;
use quote::{ToTokens, quote};
use syn::{
    Attribute, Error, Expr, Ident, LitStr, Token, Type, Visibility, braced, parenthesized,
    parse::{Parse, ParseStream},
    parse_macro_input,
    punctuated::Punctuated,
    token::{Brace, Comma, Paren},
};

struct Action {
    attrs: Vec<Attribute>,
    name: LitStr,
    _as_token: Token![as],
    ident: Ident,
    param: Option<(Paren, Type)>,
    state: Option<(Token![=], Type, Option<Expr>)>,
}

impl Parse for Action {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        Ok(Self {
            attrs: input.call(Attribute::parse_outer)?,
            name: input.parse()?,
            _as_token: input.parse()?,
            ident: input.parse()?,
            param: if input.peek(Paren) {
                let content;
                Some((parenthesized!(content in input), content.parse()?))
            } else {
                None
            },
            state: if input.peek(Token![=]) {
                Some((
                    input.parse()?,
                    input.parse()?,
                    if input.peek(Paren) {
                        let content;
                        parenthesized!(content in input);
                        Some(content.parse()?)
                    } else {
                        None
                    },
                ))
            } else {
                None
            },
        })
    }
}

impl Action {
    pub fn list_decl(&self) -> impl ToTokens {
        let attrs = &self.attrs;
        let ident = &self.ident;
        quote! {#(#attrs)* #ident}
    }

    pub fn output_decl(&self) -> impl ToTokens {
        let ident = &self.ident;
        if let Some((_, param_type)) = self.param.as_ref() {
            quote! {#ident(#param_type)}
        } else {
            quote! {#ident}
        }
    }

    pub fn to_action_params(&self) -> impl ToTokens {
        let ident = &self.ident;
        if let Some((_, param_type)) = self.param.as_ref() {
            quote! {
                Self::#ident(param) => (
                    List::#ident,
                    ::std::option::Option::Some(
                        <#param_type as ::gtk::glib::variant::ToVariant>::to_variant(param)
                    ),
                )
            }
        } else {
            quote! {
                Self::#ident => (List::#ident, ::std::option::Option::None)
            }
        }
    }

    pub fn name(&self) -> impl ToTokens {
        let ident = &self.ident;
        let name = &self.name;
        quote! {Self::#ident => #name}
    }

    pub fn parameter_type(&self) -> impl ToTokens {
        let ident = &self.ident;
        let param_type = if let Some((_, param_type)) = &self.param {
            quote! {
                ::std::option::Option::Some(
                    <#param_type as ::gtk::glib::variant::StaticVariantType>::static_variant_type()
                )
            }
        } else {
            quote! {::std::option::Option::None}
        };
        quote! {Self::#ident => #param_type}
    }

    pub fn has_default_state(&self) -> impl ToTokens {
        let ident = &self.ident;
        let has_default_state = self.state.is_some();
        quote! {Self::#ident => #has_default_state}
    }

    pub fn default_state(&self) -> impl ToTokens {
        let ident = &self.ident;
        let default_state = if let Some((_, state_type, default_state)) = self.state.as_ref() {
            let default_state = if let Some(default_state) = default_state {
                quote! {<#state_type as ::std::convert::From<_>>::from(#default_state)}
            } else {
                quote! {<#state_type as ::std::default::Default>::default()}
            };
            quote! {
                ::std::option::Option::Some(
                    <#state_type as ::gtk::glib::variant::ToVariant>::to_variant(&#default_state)
                )
            }
        } else {
            quote! {::std::option::Option::None}
        };
        quote! {Self::#ident => #default_state}
    }

    pub fn to_output(&self) -> impl ToTokens {
        let ident = &self.ident;
        if let Some((_, param_type)) = &self.param {
            quote! {
                Self::#ident => {
                    if let ::std::option::Option::Some(param) = param {
                        Self::Output::#ident(
                            <#param_type as ::gtk::glib::variant::FromVariant>::from_variant(param)
                                .expect("Wrong variant type received as parameter")
                        )
                    } else {
                        panic!("Missing parameter for action {}", self.name());
                    }
                }
            }
        } else {
            quote! {
                Self::#ident => {
                    if param.is_none() {
                        Self::Output::#ident
                    } else {
                        panic!("Unexpected parameter for action {}", self.name());
                    }
                }
            }
        }
    }

    pub fn state_decl(&self) -> Option<impl ToTokens> {
        let ident = &self.ident;
        let (_, state_type, _) = self.state.as_ref()?;
        Some(quote! {#ident(#state_type)})
    }

    pub fn to_change_params(&self) -> Option<impl ToTokens> {
        let ident = &self.ident;
        let (_, state_type, _) = self.state.as_ref()?;
        Some(quote! {
            Self::#ident(param) => (
                List::#ident,
                <#state_type as ::gtk::glib::variant::ToVariant>::to_variant(param),
            )
        })
    }
}

struct Actions {
    attrs: Vec<Attribute>,
    vis: Visibility,
    _enum_token: Token![enum],
    ident: Ident,
    _brace_token: Brace,
    actions: Punctuated<Action, Comma>,
}

impl Parse for Actions {
    fn parse(input: ParseStream) -> Result<Self, Error> {
        let content;
        Ok(Self {
            attrs: input.call(Attribute::parse_outer)?,
            vis: input.parse()?,
            _enum_token: input.parse()?,
            ident: input.parse()?,
            _brace_token: braced!(content in input),
            actions: content.parse_terminated(Action::parse, Comma)?,
        })
    }
}

impl Actions {
    pub fn list_decl(&self) -> impl ToTokens {
        let attrs = &self.attrs;
        let action_list_decl = self.actions.iter().map(Action::list_decl);
        let action_ident = self.actions.iter().map(|action| &action.ident);
        let action_name = self.actions.iter().map(Action::name);
        let parameter_type = self.actions.iter().map(Action::parameter_type);
        let has_default_state = self.actions.iter().map(Action::has_default_state);
        let default_state = self.actions.iter().map(Action::default_state);
        let to_output = self.actions.iter().map(Action::to_output);
        quote! {
            #[derive(Debug, Copy, Clone, PartialEq, Eq)]
            #(#attrs)* pub enum List {
                #(#action_list_decl,)*
            }
            impl ::component_framework::helpers::ActionList for List {
                type Output = Output;
                type State = State;
                fn all() -> impl ::std::iter::Iterator<Item = Self> {
                    [#(Self::#action_ident,)*].into_iter()
                }
                fn name(&self) -> &'static str {
                    match self {
                        #(#action_name,)*
                    }
                }
                fn parameter_type(&self)
                    -> ::std::option::Option<::std::borrow::Cow<'static, ::gtk::glib::VariantTy>>
                {
                    match self {
                        #(#parameter_type,)*
                    }
                }
                fn has_default_state(&self) -> bool {
                    match self {
                        #(#has_default_state,)*
                    }
                }
                fn default_state(&self) -> ::std::option::Option<::gtk::glib::Variant> {
                    match self {
                        #(#default_state,)*
                    }
                }
                fn to_output(&self, param: ::std::option::Option<&::gtk::glib::Variant>)
                    -> Self::Output
                {
                    match self {
                        #(#to_output)*
                    }
                }
            }
        }
    }

    pub fn output_decl(&self) -> impl ToTokens {
        let action_output_decl = self.actions.iter().map(Action::output_decl);
        let to_action_params = self.actions.iter().map(Action::to_action_params);
        quote! {
            #[derive(Debug)]
            pub enum Output {
                #(#action_output_decl,)*
            }
            impl ::component_framework::helpers::ActionListOutput<List> for Output {
                fn to_action_params(&self) -> (List, ::std::option::Option<::gtk::glib::Variant>)
                {
                    match self {
                        #(#to_action_params,)*
                    }
                }
            }
        }
    }

    pub fn state_decl(&self) -> impl ToTokens {
        let action_state_decl = self.actions.iter().filter_map(Action::state_decl);
        let to_change_params = self
            .actions
            .iter()
            .filter_map(Action::to_change_params)
            .collect::<Vec<_>>();
        let to_change_params = if !to_change_params.is_empty() {
            quote! {
                match self {
                    #(#to_change_params,)*
                }
            }
        } else {
            quote! {
                panic!("Action list state has no variants");
            }
        };
        quote! {
            #[derive(Debug)]
            pub enum State {
                #(#action_state_decl,)*
            }
            impl ::component_framework::helpers::ActionListState<List> for State {
                fn to_change_params(&self) -> (List, ::gtk::glib::Variant) {
                    #to_change_params
                }
            }
        }
    }

    pub fn prefix_func(&self) -> impl ToTokens {
        fn err(span: Span, error: &str) -> proc_macro2::TokenStream {
            Error::new(span, error).into_compile_error()
        }

        let names = self
            .actions
            .iter()
            .map(|action| action.name.clone())
            .collect::<Vec<_>>();
        if names.is_empty() {
            return err(Span::call_site(), "Action list cannot be empty");
        }

        let value = names[0].value();
        let Some((prefix, _)) = value.split_once('.') else {
            return err(
                names[0].span(),
                "Action name must contain a prefix separated by a dot",
            );
        };
        for name in &names {
            let value = name.value();
            let Some((p, _)) = value.split_once('.') else {
                return err(
                    name.span(),
                    "Action name must contain a prefix separated by a dot",
                );
            };
            if p != prefix {
                return err(
                    name.span(),
                    &format!("Prefix doesn't match previous actions, expected {prefix}"),
                );
            }
        }

        let prefix = LitStr::new(prefix, names[0].span());
        quote! {
            pub const fn prefix() -> &'static str {
                #prefix
            }
        }
    }
}

pub(crate) fn action_list(input: TokenStream) -> TokenStream {
    let actions = parse_macro_input!(input as Actions);

    let vis = &actions.vis;
    let ident = &actions.ident;
    let list_decl = actions.list_decl();
    let output_decl = actions.output_decl();
    let state_decl = actions.state_decl();
    let prefix_func = actions.prefix_func();

    quote! {
        #[allow(non_snake_case)]
        #vis mod #ident {
            use super::*;
            #list_decl
            #output_decl
            #state_decl
            #prefix_func
        }
    }
    .into()
}

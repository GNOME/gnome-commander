// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::prelude::*;
use gtk::{
    gio::{self, prelude::*},
    glib,
};
use std::{borrow::Cow, marker::PhantomData};

/// This trait needs to be implemented by an action list that [ActionGroup](ActionGroup) will use.
/// Normally an implementation of this trait is produced automatically by the
/// [action_list! macro](crate::action_list).
pub trait ActionList: Sized {
    type Output: ActionListOutput<Self>;
    type State: ActionListState<Self>;
    fn all() -> impl Iterator<Item = Self>;
    fn name(&self) -> &'static str;
    fn parameter_type(&self) -> Option<Cow<'static, glib::VariantTy>>;
    fn has_default_state(&self) -> bool;
    fn default_state(&self) -> Option<glib::Variant>;
    fn to_output(&self, param: Option<&glib::Variant>) -> Self::Output;

    fn prefix(&self) -> &'static str {
        self.name()
            .split_once('.')
            .map(|(prefix, _)| prefix)
            .unwrap_or_default()
    }
    fn short_name(&self) -> &'static str {
        self.name()
            .split_once('.')
            .map(|(_, name)| name)
            .unwrap_or_else(|| self.name())
    }
}

/// Trait implemented by an action list’s output type, provides helpers to work with keyboard
/// shortcuts and menus.
pub trait ActionListOutput<T: ActionList<Output = Self>>: Sized {
    fn to_action_params(&self) -> (T, Option<glib::Variant>);

    /// Produces a `gtk4::Shortcut` instance that will trigger this action. `key` is a shortcut
    /// trigger like `"<Control>x"` which can be parsed by `gtk4::ShortcutTrigger::parse_string()`.
    fn shortcut(&self, key: &str) -> gtk::Shortcut {
        let (action, param) = self.to_action_params();
        let shortcut = gtk::Shortcut::new(
            gtk::ShortcutTrigger::parse_string(key),
            Some(gtk::NamedAction::new(action.name())),
        );
        shortcut.set_arguments(param.as_ref());
        shortcut
    }

    // Produces a `gio::MenuItem` instance that will trigger this action.
    fn menuitem(&self, label: impl Into<String>) -> gio::MenuItem {
        let (action, param) = self.to_action_params();
        let menuitem = gio::MenuItem::new(Some(&label.into()), None);
        menuitem.set_action_and_target_value(Some(action.name()), param.as_ref());
        menuitem
    }
}

/// Trait implemented by an action list’s state type.
pub trait ActionListState<T: ActionList<State = Self>>: Sized {
    fn to_change_params(&self) -> (T, glib::Variant);
}

/// These input messages should normally not be used directly. Use
/// [enable_action()](ComponentController::enable_action) and
/// [change_action_state()](ComponentController::change_action_state) methods instead.
#[derive(Debug)]
pub enum ActionGroupInput<L: ActionList> {
    Enable(L, bool),
    SetState(L, glib::Variant),
}

/// A helper component handling a list of actions which is typically defined by
/// [`action_list!` macro](crate::action_list):
///
/// ```rust
/// # use component_framework::{action_list, with, helpers::ActionGroup, prelude::*};
/// # use gtk::prelude::*;
/// # gtk::init();
/// action_list! {
///     pub enum MyWidgetActions {
///         "mywidget.copy" as Copy,
///         "mywidget.paste" as Paste(String),
///     }
/// }
/// # struct MyComponent;
/// # impl Component for MyComponent {
/// #   type Input = MyWidgetActions::Output;
/// #   type Output = ();
/// #   type Root = ();
/// #   type View = ();
/// #   fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {}
/// #   fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root { view }
/// # }
///
/// let action_group = ActionGroup::<MyWidgetActions::List>::default().build();
/// # let component = MyComponent.build();
/// # let sender = component.sender();
/// with!(gtk::Window {
///     .insert_action_group(
///         MyWidgetActions::prefix(),
///         Some(action_group.attach(sender, |message| message))
///     );
/// });
/// ```
///
/// The output type of the component above is `MyWidgetActions::Output`, the corresponding messages
/// are produced when the action is triggered.
#[derive(Debug)]
pub struct ActionGroup<L: ActionList> {
    actions: PhantomData<L>,
}

// #[derive(Default)] will unnecessarily require L to implement Default
impl<L: ActionList> Default for ActionGroup<L> {
    fn default() -> Self {
        Self {
            actions: Default::default(),
        }
    }
}

impl<L: ActionList + Copy + 'static> Component for ActionGroup<L> {
    type Input = ActionGroupInput<L>;
    type Output = L::Output;
    type View = gio::SimpleActionGroup;
    type Root = gio::SimpleActionGroup;

    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
        let group = Self::View::default();

        for action in L::all() {
            let name = action.short_name();
            let item = if let Some(default_state) = action.default_state() {
                gio::SimpleAction::new_stateful(
                    name,
                    action.parameter_type().as_deref(),
                    &default_state,
                )
            } else {
                gio::SimpleAction::new(name, action.parameter_type().as_deref())
            };
            item.connect_activate({
                let sender = sender.clone();
                move |_, param| sender.output(action.to_output(param))
            });
            group.add_action(&item);
        }

        group
    }

    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
        view
    }

    async fn update(
        &mut self,
        msg: Self::Input,
        _sender: &ComponentSender<Self>,
        view: &mut Self::View,
    ) {
        match msg {
            Self::Input::Enable(action, enable) => {
                if let Some(item) = view
                    .lookup_action(action.short_name())
                    .and_downcast::<gio::SimpleAction>()
                {
                    item.set_enabled(enable);
                } else {
                    eprintln!("Unexpected: failed to look up action {}", action.name());
                }
            }
            Self::Input::SetState(action, state) => {
                view.change_action_state(action.short_name(), &state);
            }
        }
    }
}

impl<L: ActionList + Copy + 'static> ComponentController<ActionGroup<L>> {
    pub fn enable_action(&self, action: L, enabled: bool) {
        self.sender.input(ActionGroupInput::Enable(action, enabled));
    }

    pub fn change_action_state(&self, state: L::State) {
        let (action, variant) = state.to_change_params();
        self.sender
            .input(ActionGroupInput::SetState(action, variant));
    }
}

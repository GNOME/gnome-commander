// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod action_list;
mod with;

use proc_macro::TokenStream;

/// A helper macro to compose a view.
///
/// The basic syntax is:
///
/// ```rust
/// # use component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// let view = with!(gtk::Window {
///     .set_title(Some("My Window"));
///     .set_modal(true);
///     .set_resizable(true);
/// });
///
/// assert!(view.is::<gtk::Window>());
/// ```
///
/// This will create a new window and call a bunch of methods on it. If you already have a window,
/// say in the `view.window` property, you can pass a reference to the existing object. Or you can
/// call the constructor method explicitly instead of relying on the type’s `Default`
/// implementation:
///
/// ```rust
/// # use component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// # #[derive(Default)]
/// # struct View {
/// #     window: gtk::Window,
/// # }
/// # let view = View::default();
/// with!(&view.window {
///     .set_title(Some("My Window"));
///     .set_modal(true);
///     .set_resizable(true);
/// });
///
/// assert!(view.window.is_modal());
///
/// let view = with!(gtk::Box::new(gtk::Orientation::Vertical, 10) {
///     .set_baseline_position(gtk::BaselinePosition::Top);
/// });
///
/// assert_eq!(view.spacing(), 10);
/// ```
///
/// You can nest additional widgets within the root widget:
///
/// ```rust
/// # use component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// # #[derive(Default)]
/// # struct View {
/// #     window: gtk::Window,
/// #     button: gtk::Button,
/// # }
/// # let view = View::default();
/// with!(&view.window {
///     .set_title(Some("My Window"));
///
///     gtk::Box {
///         .set_orientation(gtk::Orientation::Vertical);
///
///         &view.button {
///             .set_label("My Button");
///         }
///     }
/// });
///
/// assert_eq!(view.button.label(), Some("My Button".into()));
/// assert_eq!(view.window.child(), view.button.parent());
/// ```
///
/// By default, child widgets will be added using ContainerExt trait which is implemented for
/// various Gtk widget types. Some container types have multiple methods to add children however, or
/// they require additional parameters. In such cases the method can be called explicitly:
///
/// ```rust
/// # use component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// let view = with!(gtk::Grid {
///     .add_controller(with!(gtk::EventControllerFocus {}));
///     .attach(&with!(gtk::Label {}), 2, 4, 1, 1);
/// });
/// ```
///
/// An empty block `{}` can be replaced by a semicolon `;`. So `with!(gtk::Box {})` and
/// `with!(gtk::Box;)` are equivalent.
///
/// You can use `if` blocks to make calls conditional:
///
/// ```rust
/// # use component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// let state = 1;
/// let view = with!(gtk::Box {
///     .set_orientation(gtk::Orientation::Vertical);
///     if state == 0 {
///         .set_visible(false);
///     } else  if state == 1 {
///         gtk::Label {
///             .set_label("State is 1");
///         }
///     } else {
///         gtk::Label {
///             .set_label("Unknown state");
///         }
///     }
/// });
///
/// assert_eq!(
///     view.first_child().and_downcast::<gtk::Label>().map(|label| label.label()),
///     Some("State is 1".into()),
/// );
/// ```
///
/// It is also possible to iterate over a collection using a `for` loop:
///
/// ```rust
/// # use component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// let view = with!(gtk::Box {
///     .set_orientation(gtk::Orientation::Horizontal);
///     for i in 0..5 {
///         gtk::Button {
///             .set_label(&format!("Set to {i}"));
///         }
///     }
/// });
///
/// assert_eq!(view.observe_children().n_items(), 5);
/// ```
///
/// In more complicated scenarios your component might embed other components. This can be done via
/// `ComponentController::attach()` method that both sets up message translation and produces the
/// widget to be added to the view.
///
/// ```rust
/// # use component_framework::{Component, ComponentController, ComponentSender, with};
/// # use gtk::prelude::*;
/// # gtk::init();
/// enum ParentInput {
///     MessageFromChild(ChildOutput),
/// }
///
/// struct Parent {
///     child: ComponentController<Child>,
/// }
///
/// enum ChildOutput {
///     ImportantMessage,
/// }
///
/// struct Child;
///
/// fn translate_child_message(message: ChildOutput) -> ParentInput {
///     ParentInput::MessageFromChild(message)
/// }
///
/// impl Component for Parent {
///     type View = gtk::Box;
///     type Input = ParentInput;
///     type Output = ();
///     type Root = gtk::Box;
///
///     fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
///         with!(Self::View {
///             .set_orientation(gtk::Orientation::Vertical);
///             gtk::Label {
///                 .set_label("Behold, child below");
///             }
///             self.child.attach(sender, translate_child_message) {
///                 .set_visible(true);
///             }
///         })
///     }
///
///     fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
///         view
///     }
///
///     async fn handle_subcomponents(&mut self) {
///         self.child.handle_incoming().await;
///     }
/// }
///
/// impl Component for Child {
///     type Output = ChildOutput;
/// #   type View = gtk::Box;
/// #   type Input = ();
/// #   type Root = gtk::Box;
/// #   fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View {
/// #       Self::View::default()
/// #   }
/// #   fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root {
/// #       view
/// #   }
/// }
/// ```
#[proc_macro]
pub fn with(input: TokenStream) -> TokenStream {
    with::with(input)
}

/// A macro to produce an action list to be used with the `ActionGroup` component.
///
/// ```rust
/// # mod wrapper {
/// # use component_framework::action_list;
/// # use gtk::glib;
/// #[derive(Debug, glib::Variant)]
/// pub enum Mode {
///     Text,
///     Image,
/// }
///
/// action_list! {
///     pub enum MyActions {
///         "mywidget.copy" as Copy,
///         "mywidget.paste" as Paste(String),
///         "mywidget.active" as ToggleActive = bool,
///         "mywidget.set-mode" as SetMode(Mode) = Mode(Mode::Text),
///     }
/// }
/// # }
/// # use wrapper::MyActions;
///
/// assert_eq!(MyActions::prefix(), "mywidget");
/// ```
///
/// All action names should contain the same prefix. Actions can have a parameter and a state. For
/// actions with a parameter its type should be specified in parentheses after the action
/// identifier. For actions with a state its type and initial value should be specified after the
/// equals sign. Initial value can be omitted for types implementing `Default`.
///
/// The above example will produce three enums:
///
/// * `MyActionList::List` implements `ActionList` trait and enumerates only action identifiers:
///   ```rust,ignore
///   pub enum List {
///       Copy,
///       Paste,
///       ToggleActive,
///       SetMode,
///   }
///   ```
///   This type is used e.g. when an action has to be enabled or disabled.
/// * `MyActionList::Output` implements `ActionListOutput` trait and enumerates actions with their
///   parameters if any:
///   ```rust,ignore
///   pub enum Output {
///       Copy,
///       Paste(String),
///       ToggleActive,
///       SetMode(Mode),
///   }
///   ```
///   This type is generated when the action is triggered. It is also used to produce something that
///   can trigger an action like a shortcut or a menu item.
/// * `MyActionList::State` implements `ActionListState` trait and enumerates actions with their
///   state. Actions without a state are omitted:
///   ```rust,ignore
///   pub enum State {
///       ToggleActive(bool),
///       SetMode(Mode),
///   }
///   ```
///   This type is used when an action’s state has to be changed.
#[proc_macro]
pub fn action_list(input: TokenStream) -> TokenStream {
    action_list::action_list(input)
}

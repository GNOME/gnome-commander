// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub mod container;

use futures::{
    FutureExt,
    future::{pending, select_all},
};

/// A helper macro to compose a view.
///
/// The basic syntax is:
///
/// ```rust
/// # use gcmd_component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// let view = with!(gtk::Window => {
///     set_title(Some("My Window"));
///     set_modal(true);
///     set_resizable(true);
/// });
///
/// assert!(view.is::<gtk::Window>());
/// ```
///
/// This will create a new window and call a bunch of methods on it. If you already have a window,
/// say in the `view.window` property, you can pass the existing object to manipulate:
///
/// ```rust
/// # use gcmd_component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// # #[derive(Default)]
/// # struct View {
/// #     window: gtk::Window,
/// # }
/// # let view = View::default();
/// with!(&view.window => {
///     set_title(Some("My Window"));
///     set_modal(true);
///     set_resizable(true);
/// });
///
/// assert!(view.window.is_modal());
/// ```
///
/// You can nest additional widgets within the root widget:
///
/// ```rust
/// # use gcmd_component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// # #[derive(Default)]
/// # struct View {
/// #     window: gtk::Window,
/// #     button: gtk::Button,
/// # }
/// # let view = View::default();
/// with!(&view.window => {
///     set_title(Some("My Window"));
///
///     gtk::Box => {
///         set_orientation(gtk::Orientation::Vertical);
///
///         &view.button => {
///             set_label("My Button");
///         }
///     }
/// });
///
/// assert_eq!(view.button.label(), Some("My Button".into()));
/// assert_eq!(view.window.child(), view.button.parent());
/// ```
///
/// By default, child widgets will be added using [ContainerExt trait](container::ContainerExt)
/// which is implemented for various Gtk widget types. Some container types have multiple types to
/// add children however. In such cases the method can be called explicitly:
///
/// ```rust
/// # use gcmd_component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// let view = with!(gtk::Box => {
///     add_controller(gtk::EventControllerFocus => {});
/// });
/// ```
///
/// Some calls can be made conditional using the `if_!()` pseudo-macro:
///
/// ```rust
/// # use gcmd_component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// let state = 1;
/// let view = with!(gtk::Box => {
///     set_orientation(gtk::Orientation::Vertical);
///     if_!(state == 0 => {
///         set_visible(false);
///     },
///     state == 1 => {
///         gtk::Label => {
///             set_label("State is 1");
///         }
///     },
///     else => {
///         gtk::Label => {
///             set_label("Unknown state");
///         }
///     });
/// });
///
/// assert_eq!(
///     view.first_child().and_downcast::<gtk::Label>().map(|label| label.label()),
///     Some("State is 1".into()),
/// );
/// ```
///
/// It is also possible to iterate over a collection using the `for_!()` pseudo-macro:
///
/// ```rust
/// # use gcmd_component_framework::with;
/// # use gtk::prelude::*;
/// # gtk::init();
/// let view = with!(gtk::Box => {
///     set_orientation(gtk::Orientation::Horizontal);
///     for_!(i in 0..5 => {
///         gtk::Button => {
///             set_label(&format!("Set to {i}"));
///         }
///     });
/// });
///
/// assert_eq!(view.observe_children().n_items(), 5);
/// ```
#[macro_export]
macro_rules! with {
    ($type:ty => {
        $($ops:tt)*
    }) => {
        $crate::with!{{<$type>::default()} => { $($ops)* }}
    };

    ($expr:expr => {
        $($ops:tt)*
    }) => {{
        let __obj = $expr;
        $crate::with!{@block {$($ops)*}, __obj}
        __obj
    }};

    (@block {}, $obj:ident) => {};

    (@block {
        $method:ident($($params:tt)*);
        $($rest:tt)*
    }, $obj:ident) => {
        $crate::with!{@call $obj.$method($($params)*)}
        $crate::with!{@block {$($rest)*}, $obj}
    };

    (@block {
        $type:ty => {
            $($ops:tt)*
        }
        $($rest:tt)*
    }, $obj:ident) => {
        $crate::container::ContainerExt::container_set_child(
            &$obj, &$crate::with!($type => {$($ops)*})
        );
        $crate::with!{@block {$($rest)*}, $obj}
    };

    (@block {
        $expr:expr => {
            $($ops:tt)*
        }
        $($rest:tt)*
    }, $obj:ident) => {
        $crate::container::ContainerExt::container_set_child(
            &$obj, $crate::with!($expr => {$($ops)*})
        );
        $crate::with!{@block {$($rest)*}, $obj}
    };

    (@block {
        if_!($expr_if:expr => {
            $($ops_if:tt)*
        }
        $(,$expr_elseif:expr => {
            $($ops_elseif:tt)*
        })*
        $(,else => {
            $($ops_else:tt)*
        })?
        $(,)?);
        $($rest:tt)*
    }, $obj:ident) => {
        if $expr_if {
            $crate::with!{@block {$($ops_if)*}, $obj}
        }
        $(else if $expr_elseif {
            $crate::with!{@block {$($ops_elseif)*}, $obj}
        })*
        $(else {
            $crate::with!{@block {$($ops_else)*}, $obj}
        })?
        $crate::with!{@block {$($rest)*}, $obj}
    };

    (@block {
        for_!($pat:pat in $expr:expr => {
            $($ops:tt)*
        });
        $($rest:tt)*
    }, $obj:ident) => {
        for $pat in $expr {
            $crate::with!{@block {$($ops)*}, $obj}
        }
        $crate::with!{@block {$($rest)*}, $obj}
    };

    (@call $obj:ident.$method:ident($($param:expr),*$(,)?)) => {
        $obj.$method($($param,)*);
    };

    (@call $obj:ident.$method:ident($type:ty => {
        $($inner:tt)*
    })) => {
        $obj.$method($crate::with!($type => {$($inner)*}));
    };

    (@call $obj:ident.$method:ident($expr:expr => {
        $($inner:tt)*
    })) => {
        $obj.$method($crate::with!($expr => {$($inner)*}));
    };
}

/// Produces a simple signal handler that will always send a particular message to the sender’s
/// input channel. This macro is meant to be used within the [with! macro](crate::with):
///
/// ```rust,ignore
/// # use gcmd_component_framework::{forward_input, with};
/// # use gtk::prelude::*;
/// # gtk::init();
/// with!(gtk::Button => {
///     set_label("Increase counter");
///     connect_clicked(forward_input!(sender, Self::Input::Increase));
/// });
/// ```
#[macro_export]
macro_rules! forward_input {
    ($sender:ident, $expr:expr) => {{
        let __sender = $sender.clone();
        move |_| {
            __sender.input($expr);
        }
    }};
}

/// Produces a simple signal handler that will always send a particular message to the sender’s
/// output channel. This macro is meant to be used within the [with! macro](crate::with):
///
/// ```rust,ignore
/// # use gcmd_component_framework::{forward_input, with};
/// # use gtk::prelude::*;
/// # gtk::init();
/// with!(gtk::Button => {
///     set_label("Cancel");
///     connect_clicked(forward_input!(sender, Self::Output::Cancelled));
/// });
/// ```
#[macro_export]
macro_rules! forward_output {
    ($sender:ident, $expr:expr) => {{
        let __sender = $sender.clone();
        move |_| {
            __sender.output($expr);
        }
    }};
}

/// Trait to be implemented by component model types.
pub trait Component {
    /// Type of messages that will be handled by [update() method](#method.update).
    type Input;
    /// Type of messages that will be send to the component’s owner.
    type Output;
    /// Type of the component’s view.
    ///
    /// This should contain the root widget at the very least, and possibly also other widgets that
    /// might need to be accessed after view instantiation.
    type View;
    /// Type of the component view’s root widget.
    type Root;

    /// Instantiates and composes the widgets within the component’s view according to the model
    /// data.
    fn init(&mut self, sender: &ComponentSender<Self>) -> Self::View;

    /// Extracts the root widget from the component’s view.
    fn root<'a>(&self, view: &'a Self::View) -> &'a Self::Root;

    /// Processes incoming messages, updating model as required. This should call [update_view()
    /// method](#method.update_view) if changes require the view to be updated.
    fn update(
        &mut self,
        _msg: Self::Input,
        _sender: &ComponentSender<Self>,
        _view: &mut Self::View,
    ) -> impl Future<Output = ()> {
        async {}
    }

    /// Overwrite this method with the code necessary to keep model and view in sync.
    ///
    /// This method will be called automatically if the model is modified externally via
    /// controller’s [model_mut() method](ComponentController#method.model_mut). [update()
    /// method](#method.update) is expected to call this method explicitly whenever state changes
    /// require view updates.
    fn update_view(&self, _sender: &ComponentSender<Self>, _view: &mut Self::View) {}

    /// Helper method to implement [forward_messages() method](#method.forward_messages). This will
    /// process messages from given subcomponents by forwarding them to the input channel unchanged.
    fn forward_messages_ident<T>(
        sender: &ComponentSender<Self>,
        controllers: &mut [ComponentController<T>],
    ) -> impl Future<Output = ()>
    where
        T: Component<Output = Self::Input>,
        Self: Sized,
    {
        async {
            if !controllers.is_empty() {
                loop {
                    let (result, _, _) = select_all(
                        controllers
                            .iter_mut()
                            .map(|controller| controller.receive().boxed_local()),
                    )
                    .await;
                    match result {
                        Ok(msg) => sender.input(msg),
                        Err(error) => {
                            eprintln!("Something is wrong, failed forwarding messages: {error}");
                            break;
                        }
                    }
                }
            } else {
                pending::<()>().await;
            }
        }
    }

    /// Process the messages of subcomponents forever. This method MUST be overwritten if this
    /// component has subcomponents. Usually this should call [forward_messages_ident()
    /// method](#method.forward_messages_ident). If subcomponents of different types exist, multiple
    /// calls to `forward_messages_ident()` can be combined via [futures::select!()
    /// macro](futures::select).
    ///
    /// This method should never return.
    fn forward_messages(&mut self, _sender: &ComponentSender<Self>) -> impl Future<Output = ()> {
        pending()
    }

    /// Sets up the component. This will call the model’s [init() method](#method.init) to create
    /// the view.
    fn build(mut self) -> ComponentController<Self>
    where
        Self: Sized,
    {
        let (input_sender, input_receiver) = async_channel::bounded(16);
        let (output_sender, output_receiver) = async_channel::bounded(16);

        let sender = ComponentSender {
            input_sender,
            output_sender,
        };
        let view = self.init(&sender);
        ComponentController {
            component: self,
            view,
            sender,
            input_receiver,
            output_receiver,
        }
    }
}

/// This type combines the senders for the component’s input and output channels.
#[derive(Debug)]
pub struct ComponentSender<T: Component + ?Sized> {
    input_sender: async_channel::Sender<T::Input>,
    output_sender: async_channel::Sender<T::Output>,
}

impl<T: Component> Clone for ComponentSender<T> {
    fn clone(&self) -> Self {
        ComponentSender {
            input_sender: self.input_sender.clone(),
            output_sender: self.output_sender.clone(),
        }
    }
}

impl<T: Component> ComponentSender<T> {
    /// Sends a message to the component’s input channel.
    pub fn input(&self, msg: T::Input) {
        self.input_sender
            .try_send(msg)
            .expect("Something is wrong, input channel isn't writable");
    }

    /// Sends a message to the component’s output channel.
    pub fn output(&self, msg: T::Output) {
        self.output_sender
            .try_send(msg)
            .expect("Something is wrong, output channel isn't writable");
    }
}

/// A guard type allowing mutable access to the component’s model. When this goes out of scope the
/// model’s [update_view() method](Component#method.update_view) will be run.
pub struct Ref<'a, T: Component + Sized> {
    component: &'a mut T,
    view: &'a mut T::View,
    sender: &'a ComponentSender<T>,
}

impl<'a, T: Component + Sized> std::ops::Deref for Ref<'a, T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.component
    }
}

impl<'a, T: Component + Sized> std::ops::DerefMut for Ref<'a, T> {
    fn deref_mut(&mut self) -> &mut T {
        self.component
    }
}

impl<'a, T: Component + Sized> std::ops::Drop for Ref<'a, T> {
    fn drop(&mut self) {
        self.component.update_view(self.sender, self.view);
    }
}

/// Controller of an active component encapsulating the component’s model, view as well as input and
/// output channels. Transparent read access to model’s properties is possible, for mutable access
/// use [model_mut() method](ComponentController#method.model_mut).
#[derive(Debug)]
pub struct ComponentController<T: Component + Sized> {
    component: T,
    view: T::View,
    sender: ComponentSender<T>,
    input_receiver: async_channel::Receiver<T::Input>,
    output_receiver: async_channel::Receiver<T::Output>,
}

impl<T: Component + Sized> std::ops::Deref for ComponentController<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        self.model()
    }
}

impl<T: Component + Sized> ComponentController<T> {
    /// Retrieves the component’s root widget.
    pub fn root(&self) -> &T::Root {
        self.component.root(&self.view)
    }

    /// Processes the component’s messages until a message is received on the output channel.
    pub async fn receive(&mut self) -> Result<T::Output, async_channel::RecvError> {
        loop {
            futures::select! {
                input_result = self.input_receiver.recv().fuse() => {
                    let msg = input_result.expect("Something is wrong, input channel got closed");
                    self.component.update(msg, &self.sender, &mut self.view).await;
                }
                output_result = self.output_receiver.recv().fuse() => return output_result,
                _ = self.component.forward_messages(&self.sender).fuse() => {
                    panic!("forward_messages() returned, this should never happen");
                }
            }
        }
    }

    /// Provides access to the component’s model.
    pub fn model(&self) -> &T {
        &self.component
    }

    /// Provides mutable access to the component’s model. When the reference goes out of scope the
    /// components [update_view() method](Component#method.update_view) will be run automatically.
    pub fn model_mut(&mut self) -> Ref<'_, T> {
        Ref {
            component: &mut self.component,
            view: &mut self.view,
            sender: &self.sender,
        }
    }

    /// Deactivates the component and retrieves the original model data. Note that the component’s
    /// widgets are *not* removed automatically and may remain visible.
    pub fn into_model(self) -> T {
        self.component
    }
}

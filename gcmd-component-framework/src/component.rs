// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{ComponentController, ComponentSender};
use futures::{
    FutureExt,
    future::{pending, select_all},
};

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

    /// Helper method to implement [handle_subcomponents() method](#method.handle_subcomponents).
    /// This will handle messages for a given subcomponent list.
    fn handle_subcomponent_list<T>(
        controllers: &mut [ComponentController<T>],
    ) -> impl Future<Output = ()>
    where
        T: Component,
        Self: Sized,
    {
        async {
            if !controllers.is_empty() {
                select_all(
                    controllers
                        .iter_mut()
                        .map(|controller| controller.handle_incoming().boxed_local()),
                )
                .await;
            } else {
                pending::<()>().await;
            }
        }
    }

    /// Process the messages of subcomponents forever by waiting on their respective
    /// [handle_incoming() method](ComponentController#method.handle_incoming). This method MUST be
    /// overwritten if this component has subcomponents.
    ///
    /// To handle multiple subcomponent this can use [handle_subcomponent_list()
    /// method](#method.handle_subcomponent_list) or [futures::select!()
    /// macro](futures::select).
    ///
    /// This method should never return.
    fn handle_subcomponents(&mut self) -> impl Future<Output = ()> {
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
            forward_handler: Default::default(),
        }
    }
}

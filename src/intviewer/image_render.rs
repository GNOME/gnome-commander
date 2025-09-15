/*
 * Copyright 2025 Andrey Kutejko <andy128k@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * For more details see the file COPYING.
 */

use gtk::{gdk, gdk_pixbuf, glib, graphene, prelude::*, subclass::prelude::*};
use std::path::Path;

const IMAGE_RENDER_DEFAULT_WIDTH: i32 = 100;
const IMAGE_RENDER_DEFAULT_HEIGHT: i32 = 200;
const INC_VALUE: f64 = 25.0;

pub mod imp {
    use super::*;
    use crate::utils::get_modifiers_state;
    use std::{
        cell::{Cell, RefCell},
        sync::OnceLock,
    };

    pub struct DragState {
        /// The button pressed to drag an image
        pub button: u32,
        /// Old x position before mouse move
        pub x: f64,
        /// Old y position before mouse move
        pub y: f64,
    }

    #[derive(glib::Properties)]
    #[properties(wrapper_type = super::ImageRender)]
    pub struct ImageRender {
        pub original_pixbuf: RefCell<Option<gdk_pixbuf::Pixbuf>>,
        pub display_pixbuf: RefCell<Option<gdk_pixbuf::Pixbuf>>,
        pub drag_state: RefCell<Option<DragState>>,

        #[property(get, set = Self::set_hadjustment, override_interface = gtk::Scrollable)]
        pub hadjustment: RefCell<Option<gtk::Adjustment>>,
        pub hadjustment_handler_ids: RefCell<Vec<glib::SignalHandlerId>>,

        #[property(get, set = Self::set_vadjustment, override_interface = gtk::Scrollable)]
        pub vadjustment: RefCell<Option<gtk::Adjustment>>,
        pub vadjustment_handler_ids: RefCell<Vec<glib::SignalHandlerId>>,

        #[property(get, set = Self::set_hscroll_policy, override_interface = gtk::Scrollable)]
        pub hscroll_policy: Cell<gtk::ScrollablePolicy>,
        #[property(get, set = Self::set_vscroll_policy, override_interface = gtk::Scrollable)]
        pub vscroll_policy: Cell<gtk::ScrollablePolicy>,

        #[property(get, set = Self::set_best_fit)]
        pub best_fit: Cell<bool>,
        #[property(get, set = Self::set_scale_factor)]
        pub scale_factor: Cell<f64>,
    }

    #[glib::object_subclass]
    impl ObjectSubclass for ImageRender {
        const NAME: &'static str = "GnomeCmdImageRender";
        type Type = super::ImageRender;
        type ParentType = gtk::Widget;
        type Interfaces = (gtk::Scrollable,);

        fn new() -> Self {
            Self {
                original_pixbuf: Default::default(),
                display_pixbuf: Default::default(),
                drag_state: Default::default(),

                hadjustment: Default::default(),
                hadjustment_handler_ids: Default::default(),

                vadjustment: Default::default(),
                vadjustment_handler_ids: Default::default(),

                hscroll_policy: Cell::new(gtk::ScrollablePolicy::Minimum),
                vscroll_policy: Cell::new(gtk::ScrollablePolicy::Minimum),

                best_fit: Default::default(),
                scale_factor: Cell::new(1.3),
            }
        }
    }

    #[glib::derived_properties]
    impl ObjectImpl for ImageRender {
        fn constructed(&self) {
            self.parent_constructed();

            let widget = self.obj();

            widget.set_focusable(true);

            let scroll_controller =
                gtk::EventControllerScroll::new(gtk::EventControllerScrollFlags::BOTH_AXES);
            scroll_controller.connect_scroll(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, dx, dy| {
                    imp.scroll(dx, dy);
                    glib::Propagation::Proceed
                }
            ));
            widget.add_controller(scroll_controller);

            let button_gesture = gtk::GestureClick::new();
            button_gesture.connect_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |g, n_press, x, y| imp.button_press(n_press, x, y, g.current_button())
            ));
            button_gesture.connect_released(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |g, _, _, _| imp.button_release(g.current_button())
            ));
            widget.add_controller(button_gesture);

            let motion_controller = gtk::EventControllerMotion::new();
            motion_controller.connect_motion(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                move |_, x, y| imp.motion(x, y)
            ));
            widget.add_controller(motion_controller);

            let key_controller = gtk::EventControllerKey::new();
            key_controller.connect_key_pressed(glib::clone!(
                #[weak(rename_to = imp)]
                self,
                #[upgrade_or]
                glib::Propagation::Proceed,
                move |_, key, _, _| imp.key_press(key)
            ));
            widget.add_controller(key_controller);
        }

        fn signals() -> &'static [glib::subclass::Signal] {
            static SIGNALS: OnceLock<Vec<glib::subclass::Signal>> = OnceLock::new();
            SIGNALS.get_or_init(|| {
                vec![glib::subclass::Signal::builder("image-status-changed").build()]
            })
        }
    }

    impl WidgetImpl for ImageRender {
        fn measure(&self, orientation: gtk::Orientation, _for_size: i32) -> (i32, i32, i32, i32) {
            if orientation == gtk::Orientation::Horizontal {
                (
                    IMAGE_RENDER_DEFAULT_WIDTH,
                    IMAGE_RENDER_DEFAULT_WIDTH,
                    -1,
                    -1,
                )
            } else {
                (
                    IMAGE_RENDER_DEFAULT_HEIGHT,
                    IMAGE_RENDER_DEFAULT_HEIGHT,
                    -1,
                    -1,
                )
            }
        }

        fn size_allocate(&self, _width: i32, _height: i32, _baseline: i32) {
            self.prepare_display_pixbuf();
        }

        fn snapshot(&self, snapshot: &gtk::Snapshot) {
            let Some(disp_pixbuf) = self.display_pixbuf.borrow().clone() else {
                return;
            };

            let width = self.obj().width();
            let height = self.obj().height();
            let disp_pixbuf_width = disp_pixbuf.width();
            let disp_pixbuf_height = disp_pixbuf.height();

            let dw = (width - disp_pixbuf_width) as f64;
            let dh = (height - disp_pixbuf_height) as f64;

            let rect = graphene::Rect::new(0_f32, 0_f32, width as f32, height as f32);
            let cr = snapshot.append_cairo(&rect);

            let paint_result = if self.best_fit.get() || (dw >= 0.0 && dh >= 0.0) {
                cr.translate(dw / 2.0, dh / 2.0);
                cr.set_source_pixbuf(&disp_pixbuf, 0.0, 0.0);
                cr.paint()
            } else {
                let src_x;
                let src_y;
                let dst_x;
                let dst_y;

                if dw >= 0.0 {
                    src_x = 0.0;
                    dst_x = dw / 2.0;
                } else {
                    src_x = self
                        .obj()
                        .hadjustment()
                        .map(|a| a.value().clamp(0.0, -dw))
                        .unwrap_or_default();
                    dst_x = 0.0;
                }

                if dh >= 0.0 {
                    src_y = 0.0;
                    dst_y = dh / 2.0;
                } else {
                    src_y = self
                        .obj()
                        .vadjustment()
                        .map(|a| a.value().clamp(0.0, -dh))
                        .unwrap_or_default();
                    dst_y = 0.0;
                }

                cr.translate(-src_x, -src_y);
                cr.set_source_pixbuf(&disp_pixbuf, dst_x, dst_y);
                cr.paint()
            };

            if let Err(error) = paint_result {
                eprintln!("Cairo paint failure: {error}");
            }
        }
    }

    impl ScrollableImpl for ImageRender {}

    impl ImageRender {
        fn set_hadjustment(&self, adjustment: Option<gtk::Adjustment>) {
            if let Some(adjustment) = self.obj().hadjustment() {
                for handler_id in self.hadjustment_handler_ids.take() {
                    adjustment.disconnect(handler_id);
                }
            }

            let adjustment = adjustment.unwrap_or_default();
            self.hadjustment.replace(Some(adjustment.clone()));

            adjustment.set_lower(0.0);
            adjustment.set_upper(
                self.display_pixbuf
                    .borrow()
                    .as_ref()
                    .map(|p| p.width())
                    .unwrap_or_default() as f64,
            );
            adjustment.set_step_increment(INC_VALUE);
            adjustment.set_page_increment(100.0);
            adjustment.set_page_size(100.0);

            self.hadjustment_handler_ids
                .borrow_mut()
                .push(adjustment.connect_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.redraw()
                )));
            self.hadjustment_handler_ids
                .borrow_mut()
                .push(adjustment.connect_value_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.redraw()
                )));

            self.redraw();
        }

        fn set_vadjustment(&self, adjustment: Option<gtk::Adjustment>) {
            if let Some(adjustment) = self.obj().vadjustment() {
                for handler_id in self.vadjustment_handler_ids.take() {
                    adjustment.disconnect(handler_id);
                }
            }

            let adjustment = adjustment.unwrap_or_default();
            self.vadjustment.replace(Some(adjustment.clone()));

            adjustment.set_lower(0.0);
            adjustment.set_upper(
                self.display_pixbuf
                    .borrow()
                    .as_ref()
                    .map(|p| p.height())
                    .unwrap_or_default() as f64,
            );
            adjustment.set_step_increment(INC_VALUE);
            adjustment.set_page_increment(100.0);
            adjustment.set_page_size(100.0);

            self.vadjustment_handler_ids
                .borrow_mut()
                .push(adjustment.connect_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.redraw()
                )));
            self.vadjustment_handler_ids
                .borrow_mut()
                .push(adjustment.connect_value_changed(glib::clone!(
                    #[weak(rename_to = imp)]
                    self,
                    move |_| imp.redraw()
                )));

            self.redraw();
        }

        fn set_hscroll_policy(&self, policy: gtk::ScrollablePolicy) {
            self.hscroll_policy.set(policy);
            self.obj().queue_resize();
        }

        fn set_vscroll_policy(&self, policy: gtk::ScrollablePolicy) {
            self.vscroll_policy.set(policy);
            self.obj().queue_resize();
        }

        fn redraw(&self) {
            self.obj().notify_status_changed();
            self.obj().queue_draw();
        }

        fn move_rel_x(&self, dx: f64) {
            if let Some(hadjustment) = self.obj().hadjustment() {
                hadjustment.set_value(
                    (hadjustment.value() + dx).clamp(hadjustment.lower(), hadjustment.upper()),
                );
            }
        }

        fn move_rel_y(&self, dy: f64) {
            if let Some(vadjustment) = self.obj().vadjustment() {
                vadjustment.set_value(
                    (vadjustment.value() + dy).clamp(vadjustment.lower(), vadjustment.upper()),
                );
            }
        }

        fn scroll(&self, mut dx: f64, mut dy: f64) {
            let state = self
                .obj()
                .root()
                .and_downcast_ref::<gtk::Window>()
                .and_then(get_modifiers_state);

            if state == Some(gdk::ModifierType::CONTROL_MASK) {
                std::mem::swap(&mut dx, &mut dy);
            }

            self.move_rel_x(INC_VALUE * dx);
            self.move_rel_y(INC_VALUE * dy);
        }

        fn button_press(&self, n_press: i32, x: f64, y: f64, button: u32) {
            if n_press == 1 && self.drag_state.borrow().is_none() {
                self.drag_state.replace(Some(DragState { button, x, y }));
            }
        }

        fn button_release(&self, button: u32) {
            if self
                .drag_state
                .borrow()
                .as_ref()
                .map(|s| s.button == button)
                .unwrap_or_default()
            {
                self.drag_state.replace(None);
            }
        }

        fn motion(&self, x: f64, y: f64) {
            if let Some(state) = self.drag_state.borrow_mut().as_mut() {
                self.move_rel_x(state.x - x);
                self.move_rel_y(state.y - y);
                state.x = x;
                state.y = y;
            }
        }

        fn key_press(&self, key: gdk::Key) -> glib::Propagation {
            match key {
                gdk::Key::Up => {
                    self.move_rel_y(-INC_VALUE);
                    glib::Propagation::Stop
                }
                gdk::Key::Down => {
                    self.move_rel_y(INC_VALUE);
                    glib::Propagation::Stop
                }
                gdk::Key::Left => {
                    self.move_rel_x(-INC_VALUE);
                    glib::Propagation::Stop
                }
                gdk::Key::Right => {
                    self.move_rel_x(INC_VALUE);
                    glib::Propagation::Stop
                }
                _ => glib::Propagation::Proceed,
            }
        }

        fn set_best_fit(&self, best_fit: bool) {
            self.best_fit.set(best_fit);
            self.prepare_display_pixbuf();
            self.redraw();
        }

        fn set_scale_factor(&self, scale_factor: f64) {
            self.scale_factor.set(scale_factor);
            self.prepare_display_pixbuf();
            self.redraw();
        }

        pub(super) fn prepare_display_pixbuf(&self) {
            self.display_pixbuf.replace(None);
            let Some(pixbuf) = self.original_pixbuf.borrow().clone() else {
                return;
            };

            let pixbuf_width = pixbuf.width();
            let pixbuf_height = pixbuf.height();

            if pixbuf_width == 0 || pixbuf_height == 0 {
                return;
            }

            if self.best_fit.get() {
                let width = self.obj().width();
                let height = self.obj().height();

                if pixbuf_width < width && pixbuf_height < height {
                    // no need to scale down
                    self.display_pixbuf.replace(Some(pixbuf));
                    return;
                }

                let dest_width;
                let dest_height;
                if height * pixbuf_width >= width * pixbuf_height {
                    dest_width = width;
                    dest_height = width * pixbuf_height / pixbuf_width;
                } else {
                    dest_width = height * pixbuf_width / pixbuf_height;
                    dest_height = height;
                }

                if dest_width >= 1 && dest_height >= 1 {
                    self.display_pixbuf.replace(pixbuf.scale_simple(
                        dest_width,
                        dest_height,
                        gdk_pixbuf::InterpType::Nearest,
                    ));
                }
            } else {
                let scale_factor = self.scale_factor.get();

                self.display_pixbuf.replace(pixbuf.scale_simple(
                    (pixbuf_width as f64 * scale_factor) as i32,
                    (pixbuf_height as f64 * scale_factor) as i32,
                    gdk_pixbuf::InterpType::Nearest,
                ));
            }

            self.update_adjustments();
        }

        fn update_adjustments(&self) {
            let Some(pixbuf) = self.display_pixbuf.borrow().clone() else {
                return;
            };

            let obj_width = self.obj().width();
            let obj_height = self.obj().height();
            let pixbuf_width = pixbuf.width();
            let pixbuf_height = pixbuf.height();

            if self.best_fit.get() || pixbuf_width <= obj_width && pixbuf_height <= obj_height {
                if let Some(hadjustment) = self.obj().hadjustment() {
                    hadjustment.set_lower(0.0);
                    hadjustment.set_upper(0.0);
                    hadjustment.set_value(0.0);
                }
                if let Some(vadjustment) = self.obj().vadjustment() {
                    vadjustment.set_lower(0.0);
                    vadjustment.set_upper(0.0);
                    vadjustment.set_value(0.0);
                }
            } else {
                if let Some(hadjustment) = self.obj().hadjustment() {
                    hadjustment.set_lower(0.0);
                    hadjustment.set_upper(pixbuf_width as f64);
                    hadjustment.set_page_size(obj_width as f64);
                }
                if let Some(vadjustment) = self.obj().vadjustment() {
                    vadjustment.set_lower(0.0);
                    vadjustment.set_upper(pixbuf_height as f64);
                    vadjustment.set_page_size(obj_height as f64);
                }
            }
        }
    }
}

glib::wrapper! {
    pub struct ImageRender(ObjectSubclass<imp::ImageRender>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::Scrollable;
}

impl ImageRender {
    pub fn new() -> Self {
        glib::Object::builder().build()
    }

    pub fn origin_pixbuf(&self) -> Option<gdk_pixbuf::Pixbuf> {
        self.imp().original_pixbuf.borrow().clone()
    }

    pub fn operation(&self, operation: ImageOperation) {
        let Some(pixbuf) = self.imp().original_pixbuf.borrow_mut().clone() else {
            return;
        };

        let new_pixbuf = match operation {
            ImageOperation::RotateClockwise => {
                pixbuf.rotate_simple(gdk_pixbuf::PixbufRotation::Clockwise)
            }
            ImageOperation::RotateCounterclockwise => {
                pixbuf.rotate_simple(gdk_pixbuf::PixbufRotation::Counterclockwise)
            }
            ImageOperation::RotateUpsideDown => {
                pixbuf.rotate_simple(gdk_pixbuf::PixbufRotation::Upsidedown)
            }
            ImageOperation::FlipVertical => pixbuf.flip(false),
            ImageOperation::FlipHorizontal => pixbuf.flip(true),
        };

        if let Some(new_pixbuf) = new_pixbuf {
            self.imp().original_pixbuf.replace(Some(new_pixbuf));
            self.imp().prepare_display_pixbuf();
        }
    }

    pub fn load_file(&self, filename: &Path) {
        self.imp().original_pixbuf.replace(None);
        self.imp().display_pixbuf.replace(None);

        match gdk_pixbuf::Pixbuf::from_file(filename) {
            Ok(pixbuf) => {
                self.imp().original_pixbuf.replace(Some(pixbuf));
            }
            Err(error) => {
                eprintln!("Failed to load image: {error}");
            }
        }
    }

    pub fn notify_status_changed(&self) {
        self.emit_by_name::<()>("image-status-changed", &[]);
    }

    pub fn connect_image_status_changed<F: Fn(&Self) + 'static>(
        &self,
        f: F,
    ) -> glib::SignalHandlerId {
        self.connect_closure(
            "image-status-changed",
            false,
            glib::closure_local!(move |self_: &Self| (f)(self_)),
        )
    }
}

#[derive(Clone, Copy, strum::FromRepr)]
#[repr(i32)]
pub enum ImageOperation {
    RotateClockwise,
    RotateCounterclockwise,
    RotateUpsideDown,
    FlipVertical,
    FlipHorizontal,
}

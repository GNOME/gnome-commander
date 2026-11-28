// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::utils::u32_enum;
use gtk::{cairo, gdk, gio, glib, graphene, prelude::*, subclass::prelude::*};

const IMAGE_RENDER_DEFAULT_WIDTH: i32 = 100;
const IMAGE_RENDER_DEFAULT_HEIGHT: i32 = 200;
const INC_VALUE: f64 = 25.0;

#[derive(Debug, Default, Clone, Copy)]
pub struct Transformation {
    rotation: u32,
    mirror: bool,
}

impl Transformation {
    pub fn rotate_clockwise(self) -> Self {
        Self {
            rotation: (self.rotation + if !self.mirror { 90 } else { 270 }) % 360,
            mirror: self.mirror,
        }
    }

    pub fn rotate_counterclockwise(self) -> Self {
        Self {
            rotation: (self.rotation + if !self.mirror { 270 } else { 90 }) % 360,
            mirror: self.mirror,
        }
    }

    pub fn flip_horizontal(self) -> Self {
        Self {
            rotation: self.rotation,
            mirror: !self.mirror,
        }
    }

    pub fn flip_vertical(self) -> Self {
        Self {
            rotation: (self.rotation + 180) % 360,
            mirror: !self.mirror,
        }
    }

    pub fn exchanges_dimensions(&self) -> bool {
        self.rotation == 90 || self.rotation == 270
    }

    pub fn to_matrix(self, mut width: f64, mut height: f64) -> cairo::Matrix {
        let mirror_factor = if self.mirror { -1.0 } else { 1.0 };
        let mut matrix = match self.rotation {
            90 => cairo::Matrix::new(0.0, 1.0, -mirror_factor, 0.0, 0.0, 0.0),
            180 => cairo::Matrix::new(-mirror_factor, 0.0, 0.0, -1.0, 0.0, 0.0),
            270 => cairo::Matrix::new(0.0, -1.0, mirror_factor, 0.0, 0.0, 0.0),
            _ => cairo::Matrix::new(mirror_factor, 0.0, 0.0, 1.0, 0.0, 0.0),
        };

        // Add translation to ensure that image coordinates stay non-negative
        let (x1, y1) = matrix.transform_point(1.0, 0.0);
        let (x2, y2) = matrix.transform_point(0.0, 1.0);
        if self.exchanges_dimensions() {
            (width, height) = (height, width);
        }
        matrix.set_x0(width * (1.0 - x1.max(x2)));
        matrix.set_y0(height * (1.0 - y1.max(y2)));
        matrix
    }
}

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
        pub image: RefCell<Option<glycin::Image>>,
        pub current_frame: RefCell<Option<cairo::ImageSurface>>,
        pub transformation: Cell<Transformation>,
        pub display_dimensions: Cell<(i32, i32)>,
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
                image: Default::default(),
                current_frame: Default::default(),
                transformation: Default::default(),
                display_dimensions: Cell::new((0, 0)),
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
            self.update_display_dimensions();
        }

        fn snapshot(&self, snapshot: &gtk::Snapshot) {
            let Ok(frame_ref) = self.current_frame.try_borrow() else {
                return;
            };
            let Some(frame) = frame_ref.as_ref() else {
                return;
            };
            let (display_width, display_height) = self.display_dimensions.get();

            let width = self.obj().width();
            let height = self.obj().height();

            let dw = width as f64 - display_width as f64;
            let dh = height as f64 - display_height as f64;

            let best_fit = self.best_fit.get();
            let translate_x = if best_fit || dw >= 0.0 {
                dw / 2.0
            } else {
                self.obj()
                    .hadjustment()
                    .map(|a| (-a.value()).clamp(dw, 0.0))
                    .unwrap_or_default()
            };
            let translate_y = if best_fit || dh >= 0.0 {
                dh / 2.0
            } else {
                self.obj()
                    .vadjustment()
                    .map(|a| (-a.value()).clamp(dh, 0.0))
                    .unwrap_or_default()
            };

            let rect = graphene::Rect::new(0_f32, 0_f32, width as f32, height as f32);
            let cr = snapshot.append_cairo(&rect);

            let (frame_width, frame_height) = self.frame_dimensions(frame);
            let scale_x = display_width as f64 / frame_width as f64;
            let scale_y = display_height as f64 / frame_height as f64;
            cr.translate(translate_x, translate_y);
            cr.scale(scale_x, scale_y);

            cr.transform(
                self.transformation
                    .get()
                    .to_matrix(frame.width() as f64, frame.height() as f64),
            );

            if let Err(error) = cr
                .set_source_surface(frame, 0.0, 0.0)
                .and_then(|_| cr.paint())
            {
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
            adjustment.set_upper(0.0);
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

            self.update_adjustments();
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
            adjustment.set_upper(0.0);
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

            self.update_adjustments();
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
            if !self.obj().has_focus() {
                self.obj().grab_focus();
            }

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
            self.update_display_dimensions();
        }

        fn set_scale_factor(&self, scale_factor: f64) {
            self.scale_factor.set(scale_factor);
            self.update_display_dimensions();
        }

        pub(super) fn frame_dimensions(&self, frame: &cairo::ImageSurface) -> (i32, i32) {
            if self.transformation.get().exchanges_dimensions() {
                (frame.height(), frame.width())
            } else {
                (frame.width(), frame.height())
            }
        }

        pub(super) fn update_display_dimensions(&self) {
            self.display_dimensions.set((0, 0));
            let Ok(frame_ref) = self.current_frame.try_borrow() else {
                return;
            };
            let Some(frame) = frame_ref.as_ref() else {
                return;
            };

            let (frame_width, frame_height) = self.frame_dimensions(frame);
            if frame_width <= 0 || frame_height <= 0 {
                return;
            }

            let scale_factor = if self.best_fit.get() {
                let width = self.obj().width();
                let height = self.obj().height();
                if frame_width <= width && frame_height <= height {
                    // no need to scale down
                    1.0
                } else {
                    (height as f64 / frame_height as f64).min(width as f64 / frame_width as f64)
                }
            } else {
                self.scale_factor.get()
            };

            self.display_dimensions.set((
                (frame_width as f64 * scale_factor) as i32,
                (frame_height as f64 * scale_factor) as i32,
            ));

            self.update_adjustments();
            self.redraw();
        }

        fn update_adjustments(&self) {
            let width = self.obj().width();
            let height = self.obj().height();
            let (display_width, display_height) = self.display_dimensions.get();

            if self.best_fit.get() || display_width <= width && display_height <= height {
                if let Some(hadjustment) = self.obj().hadjustment() {
                    hadjustment.set_upper(0.0);
                    hadjustment.set_value(0.0);
                }
                if let Some(vadjustment) = self.obj().vadjustment() {
                    vadjustment.set_upper(0.0);
                    vadjustment.set_value(0.0);
                }
            } else {
                if let Some(hadjustment) = self.obj().hadjustment() {
                    hadjustment.set_upper(display_width as f64);
                    hadjustment.set_page_size(width as f64);
                }
                if let Some(vadjustment) = self.obj().vadjustment() {
                    vadjustment.set_upper(display_height as f64);
                    vadjustment.set_page_size(height as f64);
                }
            }
            self.redraw();
        }
    }
}

glib::wrapper! {
    pub struct ImageRender(ObjectSubclass<imp::ImageRender>)
        @extends gtk::Widget,
        @implements gtk::Accessible, gtk::Buildable, gtk::ConstraintTarget, gtk::Scrollable;
}

impl Default for ImageRender {
    fn default() -> Self {
        glib::Object::builder().build()
    }
}

impl ImageRender {
    pub fn image_details(&self) -> Option<glycin::ImageDetails> {
        self.imp()
            .image
            .try_borrow()
            .ok()?
            .as_ref()
            .map(|image| image.details())
    }

    pub fn operation(&self, operation: ImageOperation) {
        let transformation = self.imp().transformation.get();
        self.imp().transformation.set(match operation {
            ImageOperation::RotateClockwise => transformation.rotate_clockwise(),
            ImageOperation::RotateCounterclockwise => transformation.rotate_counterclockwise(),
            ImageOperation::RotateUpsideDown => {
                transformation.rotate_clockwise().rotate_clockwise()
            }
            ImageOperation::FlipVertical => transformation.flip_vertical(),
            ImageOperation::FlipHorizontal => transformation.flip_horizontal(),
        });

        self.imp().update_display_dimensions();
    }

    #[allow(clippy::await_holding_refcell_ref)]
    async fn load_next_frame(&self) {
        // We can hold this reference across .await because the only other place accessing the image
        // uses try_borrow() and releases synchronously. Also, this function never runs multiple
        // times in parallel.
        let mut image_ref = self.imp().image.borrow_mut();
        let Some(image) = image_ref.as_mut() else {
            return;
        };
        let frame = match image.next_frame().await {
            Ok(frame) => frame,
            Err(error) => {
                eprintln!("Failed loading image frame: {error}");
                return;
            }
        };

        let texture = frame.texture();
        let mut surface = match cairo::ImageSurface::create(
            cairo::Format::ARgb32,
            texture.width(),
            texture.height(),
        ) {
            Ok(surface) => surface,
            Err(error) => {
                eprintln!("Failed creating cairo surface: {error}");
                return;
            }
        };

        {
            let stride = surface.stride().try_into().unwrap_or_default();
            let mut data = match surface.data() {
                Ok(data) => data,
                Err(error) => {
                    eprintln!("Failed accessing cairo surface data: {error}");
                    return;
                }
            };
            texture.download(&mut data, stride);
        }

        self.imp().current_frame.replace(Some(surface));
        self.imp().update_display_dimensions();

        if let Some(delay) = frame.delay() {
            glib::timeout_add_local_once(
                delay,
                glib::clone!(
                    #[weak(rename_to = this)]
                    self,
                    move || {
                        glib::spawn_future_local(async move {
                            this.load_next_frame().await;
                        });
                    }
                ),
            );
        }
    }

    pub fn load_file(&self, file: &gio::File) {
        self.imp().image.replace(None);
        self.imp().current_frame.replace(None);

        let this = self.clone();
        let file = file.clone();
        glib::spawn_future_local(async move {
            match glycin::Loader::new(file).load().await {
                Ok(image) => {
                    this.imp().image.replace(Some(image));
                    this.load_next_frame().await;
                    this.notify_status_changed();
                }
                Err(error) => {
                    // glycin errors can get very long, use the first line only
                    let error = error.to_string();
                    eprintln!(
                        "Failed to load image: {}",
                        error
                            .split_once('\n')
                            .map(|(first, _)| first)
                            .unwrap_or(&error)
                    );
                }
            }
        });
    }

    pub fn real_scale_factor(&self) -> f64 {
        self.imp()
            .current_frame
            .borrow()
            .as_ref()
            .map(|frame| {
                let (frame_width, _) = self.imp().frame_dimensions(frame);
                let (display_width, _) = self.imp().display_dimensions.get();
                display_width as f64 / frame_width as f64
            })
            .unwrap_or(1.0)
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

u32_enum! {
    pub enum ImageOperation {
        #[default]
        RotateClockwise,
        RotateCounterclockwise,
        RotateUpsideDown,
        FlipVertical,
        FlipHorizontal,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_transformations() {
        const WIDTH: f64 = 40.0;
        const HEIGHT: f64 = 20.0;

        fn corners(transformation: Transformation) -> [(f64, f64); 4] {
            let matrix = transformation.to_matrix(WIDTH, HEIGHT);
            [
                matrix.transform_point(0.0, 0.0),
                matrix.transform_point(WIDTH, 0.0),
                matrix.transform_point(0.0, HEIGHT),
                matrix.transform_point(WIDTH, HEIGHT),
            ]
        }

        let transformation = Transformation::default();
        assert_eq!(
            corners(transformation),
            [(0.0, 0.0), (WIDTH, 0.0), (0.0, HEIGHT), (WIDTH, HEIGHT)]
        );

        // Rotate clockwise until we are back at identity
        let transformation = transformation.rotate_clockwise();
        assert_eq!(
            corners(transformation),
            [(HEIGHT, 0.0), (HEIGHT, WIDTH), (0.0, 0.0), (0.0, WIDTH)]
        );

        let transformation = transformation.rotate_clockwise();
        assert_eq!(
            corners(transformation),
            [(WIDTH, HEIGHT), (0.0, HEIGHT), (WIDTH, 0.0), (0.0, 0.0)]
        );

        let transformation = transformation.rotate_clockwise();
        assert_eq!(
            corners(transformation),
            [(0.0, WIDTH), (0.0, 0.0), (HEIGHT, WIDTH), (HEIGHT, 0.0)]
        );

        let transformation = transformation.rotate_clockwise();
        assert_eq!(
            corners(transformation),
            [(0.0, 0.0), (WIDTH, 0.0), (0.0, HEIGHT), (WIDTH, HEIGHT)]
        );

        // Rotate counter-clockwise until we are back at identity
        let transformation = transformation.rotate_counterclockwise();
        assert_eq!(
            corners(transformation),
            [(0.0, WIDTH), (0.0, 0.0), (HEIGHT, WIDTH), (HEIGHT, 0.0)]
        );

        let transformation = transformation.rotate_counterclockwise();
        assert_eq!(
            corners(transformation),
            [(WIDTH, HEIGHT), (0.0, HEIGHT), (WIDTH, 0.0), (0.0, 0.0)]
        );

        let transformation = transformation.rotate_counterclockwise();
        assert_eq!(
            corners(transformation),
            [(HEIGHT, 0.0), (HEIGHT, WIDTH), (0.0, 0.0), (0.0, WIDTH)]
        );

        let transformation = transformation.rotate_counterclockwise();
        assert_eq!(
            corners(transformation),
            [(0.0, 0.0), (WIDTH, 0.0), (0.0, HEIGHT), (WIDTH, HEIGHT)]
        );

        // Combine flipping and rotation
        let transformation = transformation.flip_horizontal();
        assert_eq!(
            corners(transformation),
            [(WIDTH, 0.0), (0.0, 0.0), (WIDTH, HEIGHT), (0.0, HEIGHT)]
        );

        let transformation = transformation.flip_vertical();
        assert_eq!(
            corners(transformation),
            [(WIDTH, HEIGHT), (0.0, HEIGHT), (WIDTH, 0.0), (0.0, 0.0)]
        );

        let transformation = transformation.flip_horizontal();
        assert_eq!(
            corners(transformation),
            [(0.0, HEIGHT), (WIDTH, HEIGHT), (0.0, 0.0), (WIDTH, 0.0)]
        );

        let transformation = transformation.rotate_clockwise();
        assert_eq!(
            corners(transformation),
            [(0.0, 0.0), (0.0, WIDTH), (HEIGHT, 0.0), (HEIGHT, WIDTH)]
        );

        let transformation = transformation.flip_vertical();
        assert_eq!(
            corners(transformation),
            [(0.0, WIDTH), (0.0, 0.0), (HEIGHT, WIDTH), (HEIGHT, 0.0)]
        );

        let transformation = transformation.rotate_clockwise();
        assert_eq!(
            corners(transformation),
            [(0.0, 0.0), (WIDTH, 0.0), (0.0, HEIGHT), (WIDTH, HEIGHT)]
        );
    }
}

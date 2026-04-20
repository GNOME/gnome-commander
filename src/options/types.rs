// SPDX-FileCopyrightText: 2025 Andrey Kutejko <andy128k@gmail.com>
// SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
//
// SPDX-License-Identifier: GPL-3.0-or-later

use gtk::{gdk, gio, prelude::*};
use std::{marker::PhantomData, path::PathBuf, time::Duration};

pub type WriteResult = Result<(), glib::BoolError>;

pub struct EnumRepr(i32);

pub trait SettingsRepr {
    fn value(settings: &gio::Settings, key: &str) -> Self;
    fn set_value(settings: &gio::Settings, key: &str, value: Self) -> WriteResult;
}

impl SettingsRepr for bool {
    fn value(settings: &gio::Settings, key: &str) -> bool {
        settings.boolean(key)
    }

    fn set_value(settings: &gio::Settings, key: &str, value: bool) -> WriteResult {
        settings.set_boolean(key, value)
    }
}

impl SettingsRepr for i32 {
    fn value(settings: &gio::Settings, key: &str) -> i32 {
        settings.int(key)
    }

    fn set_value(settings: &gio::Settings, key: &str, value: i32) -> WriteResult {
        settings.set_int(key, value)
    }
}

impl SettingsRepr for u32 {
    fn value(settings: &gio::Settings, key: &str) -> u32 {
        settings.uint(key)
    }

    fn set_value(settings: &gio::Settings, key: &str, value: u32) -> WriteResult {
        settings.set_uint(key, value)
    }
}

impl SettingsRepr for String {
    fn value(settings: &gio::Settings, key: &str) -> String {
        settings.string(key).to_string()
    }

    fn set_value(settings: &gio::Settings, key: &str, value: String) -> WriteResult {
        settings.set_string(key, &value)
    }
}

impl SettingsRepr for Vec<String> {
    fn value(settings: &gio::Settings, key: &str) -> Vec<String> {
        settings.strv(key).iter().map(|v| v.to_string()).collect()
    }

    fn set_value(settings: &gio::Settings, key: &str, value: Vec<String>) -> WriteResult {
        settings.set_strv(key, value)
    }
}

impl SettingsRepr for glib::Variant {
    fn value(settings: &gio::Settings, key: &str) -> glib::Variant {
        settings.value(key)
    }

    fn set_value(settings: &gio::Settings, key: &str, value: glib::Variant) -> WriteResult {
        settings.set_value(key, &value)
    }
}

impl SettingsRepr for EnumRepr {
    fn value(settings: &gio::Settings, key: &str) -> EnumRepr {
        EnumRepr(settings.enum_(key))
    }

    fn set_value(settings: &gio::Settings, key: &str, value: EnumRepr) -> WriteResult {
        settings.set_enum(key, value.0)
    }
}

#[derive(Debug, Clone)]
pub struct AppOption<T, R = T, Convert = TypeConvertIdentity<T>> {
    settings: gio::Settings,
    key: &'static str,
    convert: PhantomData<(T, R, Convert)>,
}

impl<T, R, Convert> AppOption<T, R, Convert>
where
    R: SettingsRepr,
    Convert: TypeConvert<T, R>,
{
    pub fn new(settings: &gio::Settings, key: &'static str) -> Self {
        Self {
            settings: settings.clone(),
            key,
            convert: Default::default(),
        }
    }

    pub fn get(&self) -> T {
        Convert::from_repr(<R as SettingsRepr>::value(&self.settings, self.key))
    }

    pub fn set(&self, value: impl Into<T>) -> WriteResult {
        <R as SettingsRepr>::set_value(&self.settings, self.key, Convert::to_repr(value.into()))
    }

    pub fn bind<'o>(
        &'o self,
        object: &'o impl IsA<glib::Object>,
        property: &'o str,
    ) -> gio::BindingBuilder<'o> {
        self.settings.bind(self.key, object, property)
    }

    pub fn connect_changed(&self, f: impl Fn(T) + 'static) -> glib::SignalHandlerId {
        let f = Box::new(f);
        let key = self.key;
        self.settings
            .connect_changed(Some(key), move |settings, _| {
                let value = Convert::from_repr(<R as SettingsRepr>::value(settings, key));
                (f)(value);
            })
    }

    pub fn disconnect(&self, handler_id: glib::SignalHandlerId) {
        self.settings.disconnect(handler_id);
    }
}

impl<T> EnumOption<T>
where
    T: From<u32> + Into<glib::Value>,
    u32: From<T>,
{
    /// Regular Glib functionality can only bind enums to string properties, not to numerical ones.
    /// This method will bind an enum setting to a numerical property "manually."
    pub fn bind_enum(&self, object: &impl IsA<glib::Object>, property: &str) {
        object.set_property(property, self.get());

        let weak_ref = object.downgrade();
        let prop = property.to_owned();
        let settings_handler = self.connect_changed(move |value| {
            if let Some(object) = weak_ref.upgrade() {
                object.set_property(&prop, value);
            }
        });

        let settings = self.settings.clone();
        let key = self.key;
        object.connect_notify_local(Some(property), move |object, param| {
            if let Ok(value) = i32::try_from(object.property::<u32>(param.name())) {
                let _ = EnumRepr::set_value(&settings, key, EnumRepr(value));
            }
        });

        let settings = self.settings.clone();
        object.add_weak_ref_notify_local(move || settings.disconnect(settings_handler));
    }
}

pub type BoolOption = AppOption<bool>;
pub type I32Option = AppOption<i32>;
pub type U32Option = AppOption<u32>;
pub type StringOption = AppOption<String>;
pub type StrvOption = AppOption<Vec<String>>;
pub type VariantOption<T> = AppOption<T, glib::Variant, TypeConvertVariant<T>>;
pub type EnumOption<T> = AppOption<T, EnumRepr, TypeConvertEnum<T>>;
pub type RGBAOption = AppOption<gdk::RGBA, String, TypeConvertRGBA>;
pub type OptionalPathOption = AppOption<Option<PathBuf>, String, TypeConvertOptionalPath>;
pub type DurationOption = AppOption<Duration, u32, TypeConvertDuration>;

pub trait TypeConvert<T, R> {
    fn from_repr(value: R) -> T;
    fn to_repr(value: T) -> R;
}

#[derive(Debug, Clone)]
pub struct TypeConvertIdentity<T>(PhantomData<T>);

impl<T> TypeConvert<T, T> for TypeConvertIdentity<T> {
    fn from_repr(value: T) -> T {
        value
    }

    fn to_repr(value: T) -> T {
        value
    }
}

#[derive(Debug, Clone)]
pub struct TypeConvertVariant<T>(PhantomData<T>);

impl<T: FromVariant + ToVariant + Default> TypeConvert<T, glib::Variant> for TypeConvertVariant<T> {
    fn from_repr(value: glib::Variant) -> T {
        T::from_variant(&value).unwrap_or_default()
    }

    fn to_repr(value: T) -> glib::Variant {
        T::to_variant(&value)
    }
}

#[derive(Debug, Clone)]
pub struct TypeConvertEnum<T>(PhantomData<T>);

impl<T> TypeConvert<T, EnumRepr> for TypeConvertEnum<T>
where
    T: From<u32>,
    u32: From<T>,
{
    fn from_repr(value: EnumRepr) -> T {
        u32::try_from(value.0).unwrap_or_default().into()
    }

    fn to_repr(value: T) -> EnumRepr {
        EnumRepr(u32::from(value).try_into().unwrap_or_default())
    }
}

#[derive(Debug, Clone)]
pub struct TypeConvertRGBA;

impl TypeConvert<gdk::RGBA, String> for TypeConvertRGBA {
    fn from_repr(value: String) -> gdk::RGBA {
        gdk::RGBA::parse(&value).unwrap_or(gdk::RGBA::BLACK)
    }

    fn to_repr(value: gdk::RGBA) -> String {
        value.to_str().to_string()
    }
}

#[derive(Debug, Clone)]
pub struct TypeConvertOptionalPath;

impl TypeConvert<Option<PathBuf>, String> for TypeConvertOptionalPath {
    fn from_repr(value: String) -> Option<PathBuf> {
        Some(value).filter(|s| !s.is_empty()).map(PathBuf::from)
    }

    fn to_repr(value: Option<PathBuf>) -> String {
        value
            .as_ref()
            .and_then(|path| path.to_str())
            .unwrap_or_default()
            .to_owned()
    }
}

#[derive(Debug, Clone)]
pub struct TypeConvertDuration;

impl TypeConvert<Duration, u32> for TypeConvertDuration {
    fn from_repr(value: u32) -> Duration {
        Duration::from_millis(value.into())
    }

    fn to_repr(value: Duration) -> u32 {
        value.as_millis().try_into().unwrap_or_default()
    }
}

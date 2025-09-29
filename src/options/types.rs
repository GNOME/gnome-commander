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

use gtk::{gio, prelude::*};
use std::{marker::PhantomData, path::PathBuf, rc::Rc, time::Duration};

pub type WriteResult = Result<(), glib::BoolError>;

pub struct EnumRepr(pub i32);

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

pub struct AppOption<T, R = T> {
    settings: gio::Settings,
    key: &'static str,
    convert: Rc<dyn TypeConvert<T, R>>,
}

impl<T: 'static, R: SettingsRepr + 'static> AppOption<T, R> {
    pub fn new(
        settings: &gio::Settings,
        key: &'static str,
        convert: impl TypeConvert<T, R> + 'static,
    ) -> Self {
        Self {
            settings: settings.clone(),
            key,
            convert: Rc::new(convert),
        }
    }

    pub fn get(&self) -> T {
        self.convert
            .from_repr(<R as SettingsRepr>::value(&self.settings, self.key))
    }

    pub fn set(&self, value: impl Into<T>) -> WriteResult {
        <R as SettingsRepr>::set_value(&self.settings, self.key, self.convert.to_repr(value.into()))
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
        let convert = self.convert.clone();
        self.settings
            .connect_changed(Some(key), move |settings, _| {
                let value = convert.from_repr(<R as SettingsRepr>::value(settings, key));
                (f)(value);
            })
    }

    pub fn disconnect(&self, handler_id: glib::SignalHandlerId) {
        self.settings.disconnect(handler_id);
    }
}

impl<T: SettingsRepr + 'static> AppOption<T, T> {
    pub fn simple(settings: &gio::Settings, key: &'static str) -> Self {
        Self::new(settings, key, TypeConvertIdentity(PhantomData))
    }
}

impl<T: FromVariant + ToVariant + Default + 'static> AppOption<T, glib::Variant> {
    pub fn variant(settings: &gio::Settings, key: &'static str) -> Self {
        Self::new(
            settings,
            key,
            TypeConvertCallback::<T, glib::Variant> {
                from_repr: |r| <T as FromVariant>::from_variant(&r).unwrap_or_default(),
                to_repr: |t| <T as ToVariant>::to_variant(&t),
            },
        )
    }
}

pub type BoolOption = AppOption<bool>;
pub type U32Option = AppOption<u32>;
pub type StringOption = AppOption<String>;
pub type StrvOption = AppOption<Vec<String>>;
pub type VariantOption<T> = AppOption<T, glib::Variant>;
pub type EnumOption<T> = AppOption<T, EnumRepr>;

pub trait TypeConvert<T, R> {
    fn from_repr(&self, value: R) -> T;
    fn to_repr(&self, value: T) -> R;
}

pub struct TypeConvertIdentity<T>(PhantomData<T>);

impl<T> TypeConvert<T, T> for TypeConvertIdentity<T> {
    fn from_repr(&self, value: T) -> T {
        value
    }

    fn to_repr(&self, value: T) -> T {
        value
    }
}

pub struct TypeConvertCallback<T, R> {
    pub from_repr: fn(R) -> T,
    pub to_repr: fn(T) -> R,
}

impl<T, R> TypeConvert<T, R> for TypeConvertCallback<T, R> {
    fn from_repr(&self, value: R) -> T {
        (self.from_repr)(value)
    }

    fn to_repr(&self, value: T) -> R {
        (self.to_repr)(value)
    }
}

#[macro_export]
macro_rules! enum_convert_strum {
    ($t:ty) => {
        enum_convert_strum!($t, <$t>::default())
    };
    ($t:ty, $d:expr) => {
        crate::options::types::TypeConvertCallback::<$t, crate::options::types::EnumRepr> {
            from_repr: |e| e.0.try_into().ok().and_then(<$t>::from_repr).unwrap_or($d),
            to_repr: |e| crate::options::types::EnumRepr(e as i32),
        }
    };
}

pub const OPTIONAL_PATH_TYPE: TypeConvertCallback<Option<PathBuf>, String> = TypeConvertCallback {
    from_repr: |s: String| Some(s).filter(|s| !s.is_empty()).map(PathBuf::from),
    to_repr: |value| {
        value
            .as_ref()
            .and_then(|v| v.to_str())
            .unwrap_or_default()
            .to_owned()
    },
};

pub const DURATION_MILLIS_TYPE: TypeConvertCallback<Duration, u32> = TypeConvertCallback {
    from_repr: |d: u32| Duration::from_millis(d as u64),
    to_repr: |value| value.as_millis().try_into().unwrap_or_default(),
};

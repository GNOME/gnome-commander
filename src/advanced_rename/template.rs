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

use crate::{
    file::File,
    tags::{file_metadata::FileMetadata, tags::GnomeCmdTag},
};
use std::rc::Rc;
use winnow::{
    Result as PResult,
    ascii::{dec_int, dec_uint},
    combinator::{alt, delimited, opt, preceded, repeat},
    prelude::*,
    seq,
    stream::Accumulate,
    token::{any, one_of, take_until, take_while},
};

const MAX_PRECISION: usize = 16;
const MAX_XRANDOM_PRECISION: usize = 8;

#[derive(PartialEq, Debug, Clone)]
enum Chunk {
    Char(char),
    String(String),
    MetaTag(Metatag),
    Name(Range),
    Extension(Range),
    FullName(Range),
    ParentDir(Range),
    GrandparentDir(Range),
    Counter(Counter),
    Random {
        uppercase: bool,
        width: Option<usize>,
    },
}

#[derive(PartialEq, Debug, Clone)]
struct Metatag {
    name: String,
    opts1: Vec<String>,
    opts2: Vec<String>,
}

#[derive(PartialEq, Debug, Clone)]
enum CounterWidth {
    Global,
    Auto,
    Fixed(usize),
}

#[derive(PartialEq, Debug, Clone)]
struct Counter {
    step: Option<i64>,
    width: CounterWidth,
}

#[derive(Clone, Copy, PartialEq, Debug, Default)]
struct Range {
    from: isize,
    to: isize,
}

impl Range {
    fn with_len((from, len): (isize, usize)) -> Self {
        let to = from.saturating_add_unsigned(len);
        Range {
            from,
            to: if from < 0 && to > 0 { 0 } else { to },
        }
    }

    fn substr(&self, src: &str) -> Option<String> {
        let src: Vec<_> = src.chars().collect();
        let src_len = src.len();
        let begin: usize = if self.from < 0 {
            src_len.saturating_add_signed(self.from)
        } else {
            self.from as usize
        };

        if begin >= src_len {
            None
        } else {
            let end: usize = if self.to > 0 {
                self.to as usize
            } else {
                src_len.saturating_add_signed(self.to)
            }
            .clamp(0, src_len);
            Some(src[begin..end].iter().collect())
        }
    }
}

fn range(input: &mut &str) -> PResult<Range> {
    alt((
        seq!(Range {
            from : opt(dec_int).map(Option::unwrap_or_default),
            _: ':',
            to: opt(dec_int).map(Option::unwrap_or_default),
        }),
        seq!(
            dec_int,
            _: ',',
            dec_uint,
        )
        .map(Range::with_len),
        dec_int.map(|from| Range { from, to: 0 }),
    ))
    .parse_next(input)
}

fn file_tag(input: &mut &str) -> PResult<Chunk> {
    seq!(
        _: '$',
        one_of(['n', 'N', 'e', 'p', 'g']),
        opt(delimited('(', range, ')')).map(|r| r.unwrap_or_default())
    )
    .map(|(c, range)| match c {
        'n' => Chunk::Name(range),
        'e' => Chunk::Extension(range),
        'N' => Chunk::FullName(range),
        'p' => Chunk::ParentDir(range),
        'g' => Chunk::GrandparentDir(range),
        _ => Chunk::FullName(range),
    })
    .parse_next(input)
}

fn counter(input: &mut &str) -> PResult<Chunk> {
    preceded(
        "$c",
        opt(delimited(
            '(',
            alt((dec_uint.map(Option::Some::<usize>), 'a'.value(None))),
            ')',
        )),
    )
    .map(|precision| {
        Chunk::Counter(Counter {
            step: None, // global
            width: match precision {
                None => CounterWidth::Global,
                Some(None) => CounterWidth::Auto,
                Some(Some(value)) => CounterWidth::Fixed(value.min(MAX_PRECISION)),
            },
        })
    })
    .parse_next(input)
}

fn random(input: &mut &str) -> PResult<Chunk> {
    seq!(
        _: '$',
        alt(['x', 'X']),
        opt(delimited('(', dec_uint,')'))
    )
    .map(|(x, precision)| Chunk::Random {
        uppercase: x.is_uppercase(),
        width: precision
            .filter(|p| *p > 0)
            .map(|p: usize| p.clamp(1, MAX_XRANDOM_PRECISION)),
    })
    .parse_next(input)
}

fn ident<'s>(input: &mut &'s str) -> PResult<&'s str> {
    seq!(
        take_while(1.., ('a'..='z', 'A'..='Z')),
        take_while(0.., ('a'..='z', 'A'..='Z', '_', '0'..='9')),
    )
    .take()
    .parse_next(input)
}

fn metatag(input: &mut &str) -> PResult<Chunk> {
    seq!(
        _: "$T(",
        ident,
        repeat(1..,
            preceded(
                '.',
                ident,
            )),
        repeat(0..,
            preceded(
                ',',
                take_until(1.., (',', ')')).take(),
            )),
        _: ')'
    )
    .map(|(tag_name, opts1, opts2): (&str, Vec<&str>, Vec<&str>)| {
        Chunk::MetaTag(Metatag {
            name: tag_name.to_owned(),
            opts1: opts1.into_iter().map(|s| s.to_owned()).collect(),
            opts2: opts2.into_iter().map(|s| s.to_owned()).collect(),
        })
    })
    .parse_next(input)
}

fn escape_blocked_date_formats(input: &mut &str) -> PResult<Chunk> {
    seq!(
        '%',
        take_while(0.., ['O', '_', '-', '0', '^', '#']),
        one_of(['D', 'n', 't']),
    )
    .take()
    .map(|format| Chunk::String(format!("%{format}")))
    .parse_next(input)
}

fn template(input: &mut &str) -> PResult<Template> {
    repeat(
        0..,
        alt((
            file_tag,
            counter,
            random,
            metatag,
            preceded('$', '$').value(Chunk::Char('$')),
            escape_blocked_date_formats,
            any.map(|c| Chunk::Char(c)),
        )),
    )
    .parse_next(input)
}

#[derive(PartialEq, Debug)]
pub struct Template(Vec<Chunk>);

impl Template {
    pub fn new(template_string: &str) -> Result<Rc<Self>, String> {
        template
            .parse(template_string)
            .map(Rc::new)
            .map_err(|error| error.to_string())
    }
}

impl Accumulate<Chunk> for Template {
    fn initial(capacity: Option<usize>) -> Self {
        Self(match capacity {
            Some(capacity) => Vec::with_capacity(capacity.min(100)),
            None => Vec::new(),
        })
    }

    fn accumulate(&mut self, chunk: Chunk) {
        match (self.0.last_mut(), chunk) {
            (Some(last @ &mut Chunk::Char(ch0)), Chunk::Char(ch)) => {
                let _ = std::mem::replace(last, Chunk::String(format!("{ch0}{ch}")));
            }
            (Some(last @ &mut Chunk::Char(ch0)), Chunk::String(s)) => {
                let _ = std::mem::replace(last, Chunk::String(format!("{ch0}{s}")));
            }
            (Some(Chunk::String(s0)), Chunk::Char(ch)) => s0.push(ch),
            (Some(Chunk::String(s0)), Chunk::String(s)) => s0.push_str(&s),
            (_, c) => self.0.push(c),
        }
    }
}

#[derive(Clone, Copy)]
pub struct CounterOptions {
    pub start: i64,
    pub step: i64,
    pub precision: usize,
}

impl Default for CounterOptions {
    fn default() -> Self {
        Self {
            start: 1,
            step: 1,
            precision: 1,
        }
    }
}

impl CounterOptions {
    fn auto_precision(&self, n: u64) -> usize {
        if n < 1 {
            return 1;
        }
        let end = ((n - 1) as i64) * self.step + self.start;
        let max_abs_value = i64::max(self.start.abs(), end.abs());
        ((max_abs_value as f64).log10().floor() as usize + 1).clamp(1, MAX_PRECISION)
    }
}

pub fn generate_file_name(
    template: &Template,
    options: CounterOptions,
    index: u64,
    count: u64,
    file: &File,
    metadata: &FileMetadata,
) -> String {
    let mut result = String::new();
    for chunk in &template.0 {
        match chunk {
            Chunk::Char(ch) => result.push(*ch),
            Chunk::String(str) => {
                if let Some(formatted) = file
                    .file_info()
                    .modification_date_time()
                    .and_then(|dt| dt.format(str).ok())
                {
                    result.push_str(&formatted);
                } else {
                    result.push_str(str);
                }
            }
            Chunk::MetaTag(metatag) => {
                let name = format!("{}.{}", metatag.name, metatag.opts1.join("."));
                if let Some(value) = metadata.get(&GnomeCmdTag(name.into())) {
                    result.push_str(&value);
                }
            }
            Chunk::Name(range) => {
                if let Some(s) = file
                    .file_info()
                    .name()
                    .file_stem()
                    .and_then(|n| n.to_str())
                    .and_then(|n| range.substr(n))
                {
                    result.push_str(&s);
                }
            }
            Chunk::Extension(range) => {
                if let Some(s) = file
                    .file_info()
                    .name()
                    .extension()
                    .and_then(|e| e.to_str())
                    .and_then(|e| range.substr(e))
                {
                    result.push_str(&s);
                }
            }
            Chunk::FullName(range) => {
                if let Some(s) = file
                    .file_info()
                    .name()
                    .to_str()
                    .and_then(|n| range.substr(n))
                {
                    result.push_str(&s);
                }
            }
            Chunk::ParentDir(range) => {
                if let Some(s) = file
                    .get_path_string_through_parent()
                    .parent()
                    .and_then(|p| p.file_name())
                    .and_then(|d| d.to_str())
                    .and_then(|d| range.substr(d))
                {
                    result.push_str(&s);
                }
            }
            Chunk::GrandparentDir(range) => {
                if let Some(s) = file
                    .get_path_string_through_parent()
                    .parent()
                    .and_then(|p| p.parent())
                    .and_then(|p| p.file_name())
                    .and_then(|d| d.to_str())
                    .and_then(|d| range.substr(d))
                {
                    result.push_str(&s);
                }
            }
            Chunk::Counter(counter) => {
                let step = counter.step.unwrap_or(options.step);
                let value = options.start + (index as i64 * step);
                let precision = match counter.width {
                    CounterWidth::Auto => options.auto_precision(count),
                    CounterWidth::Global => {
                        if options.precision > 0 {
                            options.precision.clamp(1, MAX_PRECISION)
                        } else {
                            options.auto_precision(count)
                        }
                    }
                    CounterWidth::Fixed(p) => p,
                };
                result.push_str(&format!("{:0precision$}", value));
            }
            Chunk::Random {
                uppercase,
                width: precision,
            } => {
                let p = precision.unwrap_or(MAX_XRANDOM_PRECISION);
                let mut tag = format!("{:0p$}", glib::random_int());
                if *uppercase {
                    tag = tag.to_uppercase();
                }
                result.push_str(&tag);
            }
        }
    }
    result
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_range() {
        assert_eq!(range.parse("123"), Ok(Range { from: 123, to: 0 }));
        assert_eq!(range.parse(":"), Ok(Range { from: 0, to: 0 }));
        assert_eq!(range.parse("2:"), Ok(Range { from: 2, to: 0 }));
        assert_eq!(range.parse("-5:"), Ok(Range { from: -5, to: 0 }));
        assert_eq!(range.parse(":3"), Ok(Range { from: 0, to: 3 }));
        assert_eq!(range.parse(":-3"), Ok(Range { from: 0, to: -3 }));
        assert_eq!(range.parse("1:15"), Ok(Range { from: 1, to: 15 }));
        assert_eq!(range.parse("-3:-1"), Ok(Range { from: -3, to: -1 }));
        assert_eq!(range.parse("0"), Ok(Range { from: 0, to: 0 }));
        assert_eq!(range.parse("-4"), Ok(Range { from: -4, to: 0 }));
        assert_eq!(range.parse("1,3"), Ok(Range { from: 1, to: 4 }));
        assert_eq!(range.parse("-1,3"), Ok(Range { from: -1, to: 0 }));
        assert_eq!(range.parse("-3,2"), Ok(Range { from: -3, to: -1 }));
    }

    #[test]
    fn test_range_substr() {
        assert_eq!(
            Range::default().substr("Hello, World!").as_deref(),
            Some("Hello, World!")
        );
        assert_eq!(
            Range::default().substr("Привет, Мир!").as_deref(),
            Some("Привет, Мир!")
        );
        assert_eq!(
            Range { from: -6, to: 0 }.substr("Hello, World!").as_deref(),
            Some("World!")
        );
        assert_eq!(
            Range { from: -4, to: 0 }.substr("Привет, Мир!").as_deref(),
            Some("Мир!")
        );
        assert_eq!(
            Range { from: 6, to: 7 }.substr("Привет, Мир!").as_deref(),
            Some(",")
        );
        assert_eq!(
            Range { from: -4, to: -1 }.substr("Привет, Мир!").as_deref(),
            Some("Мир")
        );
        assert_eq!(Range { from: 40, to: 42 }.substr("Hello, World!"), None);
    }

    #[test]
    fn test_pattern() {
        assert_eq!(
            template.parse("new$$-$c-$n.$e(1:3)"),
            Ok(Template(vec![
                Chunk::String("new$-".to_string()),
                Chunk::Counter(Counter {
                    width: CounterWidth::Global,
                    step: None,
                }),
                Chunk::Char('-'),
                Chunk::Name(Range::default()),
                Chunk::Char('.'),
                Chunk::Extension(Range { from: 1, to: 3 }),
            ]))
        );

        assert_eq!(
            template.parse("$r$N"),
            Ok(Template(vec![
                Chunk::String("$r".to_string()),
                Chunk::FullName(Range::default()),
            ]))
        );

        assert_eq!(
            template.parse("$T(Audio.AlbumArtist) - $T(Audio.Title).$e"),
            Ok(Template(vec![
                Chunk::MetaTag(Metatag {
                    name: "Audio".to_string(),
                    opts1: vec!["AlbumArtist".to_string()],
                    opts2: vec![],
                }),
                Chunk::String(" - ".to_string()),
                Chunk::MetaTag(Metatag {
                    name: "Audio".to_string(),
                    opts1: vec!["Title".to_string()],
                    opts2: vec![],
                }),
                Chunk::Char('.'),
                Chunk::Extension(Range::default()),
            ]))
        );

        assert_eq!(
            template.parse("$n-%_D-%0H.$e"),
            Ok(Template(vec![
                Chunk::Name(Range::default()),
                Chunk::String("-%%_D-%0H.".to_string()),
                Chunk::Extension(Range::default()),
            ]))
        );
    }

    #[test]
    fn test_auto_precision() {
        let c = CounterOptions {
            start: 1,
            step: 1,
            precision: 0,
        };
        assert_eq!(c.auto_precision(0), 1);
        assert_eq!(c.auto_precision(1), 1);
        assert_eq!(c.auto_precision(5), 1);
        assert_eq!(c.auto_precision(9), 1);
        assert_eq!(c.auto_precision(10), 2);
        assert_eq!(c.auto_precision(99), 2);
        assert_eq!(c.auto_precision(100), 3);
        assert_eq!(c.auto_precision(12345), 5);
        assert_eq!(c.auto_precision(99999), 5);
        assert_eq!(c.auto_precision(1_234_567_890_123_456), 16);
        assert_eq!(c.auto_precision(100_000_000_000_000_000), 16);

        let c2 = CounterOptions {
            start: 100,
            step: 5,
            precision: 0,
        };
        assert_eq!(c2.auto_precision(0), 1);
        assert_eq!(c2.auto_precision(1), 3);
        assert_eq!(c2.auto_precision(4), 3);
        assert_eq!(c2.auto_precision(181), 4);
    }
}

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

use super::{backend::SearchMessage, profile::SearchProfile};
use crate::{debug::debug, dir::Directory, file::File, filter::PatternType, utils::ErrorMessage};
use gettextrs::gettext;
use gtk::gio::{self, prelude::*};
use std::{
    io::{self, BufRead},
    path::Path,
    process::{Command, Stdio},
    thread::JoinHandle,
};

pub async fn local_search(
    profile: &SearchProfile,
    start_dir: &Directory,
    on_message: &dyn Fn(SearchMessage),
    cancellable: &gio::Cancellable,
) -> Result<(), ErrorMessage> {
    let mut command = build_search_command(profile, &start_dir.path().path());
    debug!('g', "running: {command:?}");

    let mut child = command.stdout(Stdio::piped()).spawn().map_err(|error| {
        ErrorMessage::with_error(gettext("Error running the search command."), &error)
    })?;

    let stdout = child
        .stdout
        .take()
        .ok_or_else(|| ErrorMessage::brief("Child process has no stdout stream"))?;

    let (sender, receiver) = async_channel::unbounded::<String>();
    let handle: JoinHandle<Result<(), ErrorMessage>> = std::thread::spawn(move || {
        let buf_reader = io::BufReader::new(stdout);
        for line in buf_reader.lines() {
            match line {
                Ok(line) => {
                    if let Err(error) = sender.send_blocking(line) {
                        eprintln!("Send to a channel failed: {error}");
                        break;
                    }
                }
                Err(error) => {
                    debug!('g', "search command error: {error}");
                }
            }
        }
        match child.wait() {
            Ok(status) if status.success() => {
                debug!('g', "search command finished successfully")
            }
            Ok(status) => {
                eprintln!("search command finished with a status {status}")
            }
            Err(error) => {
                eprintln!("search command failed with an error: {error}")
            }
        }
        Ok(())
    });

    while let Ok(line) = receiver.recv().await {
        if cancellable.is_cancelled() {
            break;
        }
        match File::new_from_path(&Path::new(&line)) {
            Ok(file) => (on_message)(SearchMessage::File(file)),
            Err(error) => eprintln!("Cannot create a file for a path '{line}': {error}"),
        }
    }
    match handle.join() {
        Ok(Ok(_)) => Ok(()),
        Ok(Err(error)) => Err(error),
        Err(error) => Err(ErrorMessage::brief(format!(
            "Thread join failure: {:?}",
            error.type_id()
        ))),
    }
}

fn build_search_command(profile: &SearchProfile, start_dir: &Path) -> Command {
    const FIND_COMMAND: &str = "find";
    const GREP_COMMAND: &str = "grep";

    let mut command = Command::new(FIND_COMMAND);

    command.arg(start_dir).args(["-mindepth", "1"]); // exclude the directory itself

    let max_depth = profile.max_depth();
    if max_depth != -1 {
        command.arg("-maxdepth").arg((max_depth + 1).to_string());
    }

    let filename_pattern = profile.filename_pattern();
    if !filename_pattern.is_empty() {
        match profile.pattern_type() {
            PatternType::FnMatch => {
                if filename_pattern.contains('*') || filename_pattern.contains('?') {
                    command.arg("-iname").arg(filename_pattern);
                } else {
                    command.arg("-iname").arg(format!("*{filename_pattern}*"));
                }
            }
            PatternType::Regex => {
                command
                    .arg("-regextype")
                    .arg("posix-extended")
                    .arg("-iregex")
                    .arg(format!(".*/.*{filename_pattern}.*"));
            }
        }
    }

    if profile.content_search() {
        command
            .arg("!")
            .arg("-type")
            .arg("p")
            .arg("-exec")
            .arg(GREP_COMMAND)
            .arg("-E")
            .arg("-q");
        if !profile.match_case() {
            command.arg("-i");
        }
        command.arg(profile.text_pattern()).arg("{}").arg(";");
    }

    command.arg("-print");

    command
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_build_search_command_default() {
        let profile = SearchProfile::default();
        let command = build_search_command(&profile, &Path::new("/home/user/Documents"));
        assert_eq!(
            format!("{command:?}"),
            r#""find" "/home/user/Documents" "-mindepth" "1" "-maxdepth" "1" "-print""#
        );
    }

    #[test]
    fn test_build_search_command_glob_pattern() {
        let profile = SearchProfile::default();
        profile.set_filename_pattern("*.jp?g");
        profile.set_pattern_type(PatternType::FnMatch);
        let command = build_search_command(&profile, &Path::new("/home/user/Documents"));
        assert_eq!(
            format!("{command:?}"),
            r#""find" "/home/user/Documents" "-mindepth" "1" "-maxdepth" "1" "-iname" "*.jp?g" "-print""#
        );
    }

    #[test]
    fn test_build_search_command_glob_pattern_substring() {
        let profile = SearchProfile::default();
        profile.set_filename_pattern("cat");
        profile.set_pattern_type(PatternType::FnMatch);
        let command = build_search_command(&profile, &Path::new("/home/user/Documents"));
        assert_eq!(
            format!("{command:?}"),
            r#""find" "/home/user/Documents" "-mindepth" "1" "-maxdepth" "1" "-iname" "*cat*" "-print""#
        );
    }

    #[test]
    fn test_build_search_command_regex_pattern() {
        let profile = SearchProfile::default();
        profile.set_filename_pattern("\\.jpg");
        profile.set_pattern_type(PatternType::Regex);
        let command = build_search_command(&profile, &Path::new("/home/user/Documents"));
        assert_eq!(
            format!("{command:?}"),
            r#""find" "/home/user/Documents" "-mindepth" "1" "-maxdepth" "1" "-regextype" "posix-extended" "-iregex" ".*/.*\\.jpg.*" "-print""#
        );
    }

    #[test]
    fn test_build_search_command_content_search() {
        let profile = SearchProfile::default();
        profile.set_filename_pattern("*.txt");
        profile.set_pattern_type(PatternType::FnMatch);
        profile.set_content_search(true);
        profile.set_text_pattern("GNOME");

        let command = build_search_command(&profile, &Path::new("/home/user/Documents"));
        assert_eq!(
            format!("{command:?}"),
            r#""find" "/home/user/Documents" "-mindepth" "1" "-maxdepth" "1" "-iname" "*.txt" "!" "-type" "p" "-exec" "grep" "-E" "-q" "-i" "GNOME" "{}" ";" "-print""#
        );

        profile.set_match_case(true);

        let command = build_search_command(&profile, &Path::new("/home/user/Documents"));
        assert_eq!(
            format!("{command:?}"),
            r#""find" "/home/user/Documents" "-mindepth" "1" "-maxdepth" "1" "-iname" "*.txt" "!" "-type" "p" "-exec" "grep" "-E" "-q" "GNOME" "{}" ";" "-print""#
        );
    }
}

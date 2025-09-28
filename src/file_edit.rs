/*
 * Copyright 2024 Andrey Kutejko <andy128k@gmail.com>
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
    options::options::ProgramsOptions,
    spawn::{SpawnError, spawn_async},
    utils::ErrorMessage,
};
use gettextrs::gettext;
use gtk::gio;

pub async fn file_edit(
    files: &glib::List<File>,
    options: &ProgramsOptions,
) -> Result<(), ErrorMessage> {
    for file in files {
        if file.file_info().file_type() == gio::FileType::Directory {
            return Err(ErrorMessage::new(
                gettext("Not an ordinary file."),
                Some(file.file_info().display_name().as_str()),
            ));
        }
        if !file.is_local() {
            return Ok(());
        }
    }

    spawn_async(None, files, &options.editor_cmd.get()).map_err(SpawnError::into_message)?;
    Ok(())
}

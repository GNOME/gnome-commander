/*
  GNOME Commander - A GNOME based file manager
  Copyright (C) 2001-2006 Marcus Bjurman

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef __USERACTIONS_H__
#define __USERACTIONS_H__

#include "gnome-cmd-main-win.h"


/************** File Menu **************/
void
file_cap_cut                        (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_cap_copy                       (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_cap_paste                      (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_copy                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_move                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_delete                         (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_view                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used);

void
file_internal_view                  (GtkMenuItem     *menuitem,
                                     gpointer        not_used);

void
file_external_view                  (GtkMenuItem     *menuitem,
                                     gpointer        not_used);

void
file_edit                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_chmod                          (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_chown                          (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_mkdir                          (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_properties                     (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_diff                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_rename                         (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_create_symlink                 (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
file_advrename                      (GtkMenuItem     *menuitem,
                                     gpointer        not_used);

void
file_run                           (GtkMenuItem     *menuitem,
                                    gpointer        not_used);

void
file_umount                        (GtkMenuItem     *menuitem,
                                    gpointer        not_used);

void
file_exit                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


/************** Mark Menu **************/
void
mark_toggle                         (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
mark_toggle_and_step                (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
mark_select_all                     (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


void
mark_unselect_all                   (GtkMenuItem     *menuitem,
                                       gpointer        not_used);

void
mark_select_with_pattern            (GtkMenuItem    *menuitem,
                                     gpointer        not_used);

void
mark_unselect_with_pattern            (GtkMenuItem    *menuitem,
                                       gpointer        not_used);

void
mark_invert_selection                 (GtkMenuItem    *menuitem,
                                       gpointer        not_used);

void
mark_select_all_with_same_extension   (GtkMenuItem    *menuitem,
                                       gpointer        not_used);

void
mark_unselect_all_with_same_extension   (GtkMenuItem    *menuitem,
                                         gpointer        not_used);

void
mark_restore_selection                 (GtkMenuItem    *menuitem,
                                        gpointer        not_used);

void
mark_compare_directories               (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


/************** Edit Menu **************/
void
edit_search                         (GtkMenuItem     *menuitem,
                                      gpointer        not_used);

void
edit_quick_search                   (GtkMenuItem     *menuitem,
                                      gpointer        not_used);

void
edit_filter                         (GtkMenuItem     *menuitem,
                                      gpointer        not_used);


void
edit_copy_fnames                    (GtkMenuItem     *menuitem,
                                      gpointer        not_used);


/************** View Menu **************/
void
view_conbuttons                        (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used);

void
view_toolbar                           (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used);

void
view_buttonbar                         (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used);

void
view_cmdline                           (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used);

void
view_hidden_files                      (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used);

void
view_backup_files                      (GtkCheckMenuItem     *menuitem,
                                         gpointer              not_used);

void
view_up                                (GtkMenuItem     *menuitem,
                                         gpointer        not_used);

void
view_first                             (GtkMenuItem     *menuitem,
                                         gpointer        not_used);

void
view_back                              (GtkMenuItem     *menuitem,
                                         gpointer        not_used);

void
view_forward                           (GtkMenuItem     *menuitem,
                                         gpointer        not_used);

void
view_last                              (GtkMenuItem     *menuitem,
                                         gpointer        not_used);

void
view_refresh                           (GtkMenuItem     *menuitem,
                                         gpointer        not_used);


/************** Bookmarks Menu **************/
void
bookmarks_add_current               (GtkMenuItem     *menuitem,
                                     gpointer        not_used);
void
bookmarks_edit                      (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


/************** Options Menu **************/
void
options_edit                        (GtkMenuItem     *menuitem,
                                     gpointer        not_used);

void
options_edit_mime_types             (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


/************** Connections Menu **************/
void
connections_ftp_connect             (GtkMenuItem     *menuitem,
                                     gpointer        not_used);

void
connections_ftp_quick_connect       (GtkMenuItem     *menuitem,
                                     gpointer        not_used);

void
connections_change                  (GtkMenuItem     *menuitem,
                                     GnomeCmdCon     *con);

void
connections_close                   (GtkMenuItem     *menuitem,
                                     GnomeCmdCon     *con);

void
connections_close_current            (GtkMenuItem     *menuitem,
                                      gpointer         not_used);


/************** Plugins Menu ***********/
void
plugins_configure                   (GtkMenuItem     *menuitem,
                                     gpointer        not_used);



/************** Help Menu **************/

void
help_help                           (GtkMenuItem     *menuitem,
                                     gpointer        not_used);

void
help_keyboard                       (GtkMenuItem     *menuitem,
                                     gpointer        not_used);

void
help_about                          (GtkMenuItem     *menuitem,
                                     gpointer        not_used);


#endif //__USERACTIONS_H__

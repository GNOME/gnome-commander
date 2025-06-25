/** 
 * @file gnome-cmd-file-selector.h
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 *
 * @copyright This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * @copyright This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @copyright You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

#define GNOME_CMD_TYPE_FILE_SELECTOR              (gnome_cmd_file_selector_get_type ())
#define GNOME_CMD_FILE_SELECTOR(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GNOME_CMD_TYPE_FILE_SELECTOR, GnomeCmdFileSelector))
#define GNOME_CMD_FILE_SELECTOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GNOME_CMD_TYPE_FILE_SELECTOR, GnomeCmdFileSelectorClass))
#define GNOME_CMD_IS_FILE_SELECTOR(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), GNOME_CMD_TYPE_FILE_SELECTOR))
#define GNOME_CMD_IS_FILE_SELECTOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GNOME_CMD_TYPE_FILE_SELECTOR))
#define GNOME_CMD_FILE_SELECTOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GNOME_CMD_TYPE_FILE_SELECTOR, GnomeCmdFileSelectorClass))

#include "gnome-cmd-file-list.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-dir.h"


struct GnomeCmdFileSelector
{
    GtkGrid parent;

  public:

    operator GObject * () const             {  return G_OBJECT (this);    }
    operator GtkWidget * () const           {  return GTK_WIDGET (this);  }
    operator GtkBox * () const              {  return GTK_BOX (this);     }

    GnomeCmdFileList *file_list();
    GnomeCmdFileList *file_list(gint n);

    GnomeCmdDir *get_directory()            {  return gnome_cmd_file_list_get_directory (file_list()); }
    void goto_directory(const gchar *dir)   {  file_list()->goto_directory(dir); }

    void set_active(gboolean value);

    GnomeCmdCon *get_connection()           {  return gnome_cmd_file_list_get_connection (file_list()); }
    void set_connection(GnomeCmdCon *con, GnomeCmdDir *start_dir=NULL);

    gboolean is_local()                     {  return gnome_cmd_con_is_local (get_connection ());  }
    gboolean is_active();

    GtkWidget *new_tab();
    GtkWidget *new_tab(GnomeCmdDir *dir, gboolean activate=TRUE);
    GtkWidget *new_tab(GnomeCmdDir *dir, GnomeCmdFileList::ColumnID sort_col, GtkSortType sort_order, gboolean locked, gboolean activate, gboolean grab_focus);

    void show_filter();
    void update_files();
    void update_direntry();
    void update_vol_label();
    void update_style();
    void update_connections();

    void do_file_specific_action (GnomeCmdFileList *fl, GnomeCmdFile *f);
};

inline void GnomeCmdFileSelector::set_connection(GnomeCmdCon *con, GnomeCmdDir *start_dir)
{
    file_list()->set_connection(con, start_dir);
}

inline GtkWidget *GnomeCmdFileSelector::new_tab()
{
    return new_tab(NULL, GnomeCmdFileList::COLUMN_NAME, GTK_SORT_ASCENDING, FALSE, TRUE, TRUE);
}

inline GtkWidget *GnomeCmdFileSelector::new_tab(GnomeCmdDir *dir, gboolean activate)
{
    return new_tab(dir, file_list()->get_sort_column(), file_list()->get_sort_order(), FALSE, activate, activate);
}

extern "C" GType gnome_cmd_file_selector_get_type ();

inline FileSelectorID operator ! (FileSelectorID id)
{
    switch (id)
    {
        case LEFT:      return RIGHT;
        case RIGHT:     return LEFT;
        case INACTIVE:  return ACTIVE;
        case ACTIVE:    return INACTIVE;

        default:        return id;
    }
}

// FFI
extern "C" GnomeCmdFileList *gnome_cmd_file_selector_file_list (GnomeCmdFileSelector *fs);
extern "C" GnomeCmdFileList *gnome_cmd_file_selector_file_list_nth (GnomeCmdFileSelector *fs, gint n);

extern "C" GtkWidget *gnome_cmd_file_selector_new_tab_full (GnomeCmdFileSelector *fs, GnomeCmdDir *dir, gint sort_col, gint sort_order, gboolean locked, gboolean activate, gboolean grab_focus);

extern "C" gboolean gnome_cmd_file_selector_is_active (GnomeCmdFileSelector *fs);
extern "C" void gnome_cmd_file_selector_set_active (GnomeCmdFileSelector *fs, gboolean active);

extern "C" gboolean gnome_cmd_file_selector_can_forward (GnomeCmdFileSelector *fs);
extern "C" gboolean gnome_cmd_file_selector_can_back (GnomeCmdFileSelector *fs);
extern "C" void gnome_cmd_file_selector_forward (GnomeCmdFileSelector *fs);
extern "C" void gnome_cmd_file_selector_back (GnomeCmdFileSelector *fs);

extern "C" gboolean gnome_cmd_file_selector_is_tab_locked (GnomeCmdFileSelector *fs, GnomeCmdFileList *fl);
extern "C" gboolean gnome_cmd_file_selector_is_current_tab_locked (GnomeCmdFileSelector *fs);
extern "C" void gnome_cmd_file_selector_set_tab_locked (GnomeCmdFileSelector *fs, GnomeCmdFileList *fl, gboolean lock);

extern "C" void gnome_cmd_file_selector_update_tab_label (GnomeCmdFileSelector *fs, GnomeCmdFileList *fl);

extern "C" void gnome_cmd_file_selector_update_style (GnomeCmdFileSelector *fs);

extern "C" void gnome_cmd_file_selector_update_connections (GnomeCmdFileSelector *fs);

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

struct GnomeCmdMainWin;

#include "gnome-cmd-file-list.h"
#include "gnome-cmd-con.h"
#include "gnome-cmd-dir.h"
#include "gnome-cmd-notebook.h"


struct GnomeCmdCombo;


struct GnomeCmdFileSelector
{
    GtkBox vbox;

    FileSelectorID fs_id;
    GtkWidget *con_btns_sw;
    GtkWidget *con_btns_hbox;
    GtkWidget *con_hbox;
    GtkWidget *dir_indicator;
    GtkWidget *dir_label;
    GtkWidget *info_label;
    GnomeCmdCombo *con_combo;
    GtkWidget *vol_label;

    GtkNotebook *notebook;
    GnomeCmdFileList *list;

  public:

    class Private;

    Private *priv;

    operator GObject * () const             {  return G_OBJECT (this);    }
    operator GtkWidget * () const           {  return GTK_WIDGET (this);  }
    operator GtkBox * () const              {  return GTK_BOX (this);     }

    GnomeCmdFileList *file_list() const     {  return list;               }
    GnomeCmdFileList *file_list(gint n) const;

    GnomeCmdDir *get_directory() const      {  return list->cwd;          }
    void goto_directory(const gchar *dir)   {  list->goto_directory(dir); }

    void first();
    void back();
    void forward();
    void last();

    gboolean can_back();
    gboolean can_forward();

    void set_active(gboolean value);

    GnomeCmdCon *get_connection() const     {  return file_list()->con;   }
    void set_connection(GnomeCmdCon *con, GnomeCmdDir *start_dir=NULL);

    gboolean is_local() const               {  return gnome_cmd_con_is_local (get_connection ());  }
    gboolean is_active();

    GtkWidget *new_tab();
    GtkWidget *new_tab(GnomeCmdDir *dir, gboolean activate=TRUE);
    GtkWidget *new_tab(GnomeCmdDir *dir, GnomeCmdFileList::ColumnID sort_col, GtkSortType sort_order, gboolean locked, gboolean activate);
    void close_tab();
    void close_tab(gint n);

    void prev_tab();
    void next_tab();

    void update_tab_label(GnomeCmdFileList *fl);
    GnomeCmdFileList get_gnome_cmd_file_list(GnomeCmdFileSelector &fs);

    void show_filter();
    void update_files();
    void update_direntry();
    void update_vol_label();
    void update_selected_files_label();
    void update_style();
    void update_connections();
    void update_show_devbuttons();
    void update_show_devlist();
    void update_show_tabs();

    void do_file_specific_action (GnomeCmdFileList *fl, GnomeCmdFile *f);

    gboolean key_pressed(GnomeCmdKeyPress *event);

    GList* GetTabs();
};

inline GnomeCmdFileList *GnomeCmdFileSelector::file_list(gint n) const
{
    auto page = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), n);
    auto file_list = gtk_scrolled_window_get_child (GTK_SCROLLED_WINDOW (page));
    return GNOME_CMD_FILE_LIST (file_list);
}

inline void GnomeCmdFileSelector::set_connection(GnomeCmdCon *con, GnomeCmdDir *start_dir)
{
    file_list()->set_connection(con, start_dir);
}

inline GtkWidget *GnomeCmdFileSelector::new_tab()
{
    return new_tab(NULL, GnomeCmdFileList::COLUMN_NAME, GTK_SORT_ASCENDING, FALSE, TRUE);
}

inline GtkWidget *GnomeCmdFileSelector::new_tab(GnomeCmdDir *dir, gboolean activate)
{
    return new_tab(dir, file_list()->get_sort_column(), file_list()->get_sort_order(), FALSE, activate);
}

extern "C" GType gnome_cmd_file_selector_get_type ();

inline GtkWidget *gnome_cmd_file_selector_new (FileSelectorID fs_id)
{
    GnomeCmdFileSelector *fs = static_cast<GnomeCmdFileSelector *>(g_object_new (GNOME_CMD_TYPE_FILE_SELECTOR, NULL));
    fs->fs_id = fs_id;
    return *fs;
}

void gnome_cmd_file_selector_show_new_textfile_dialog (GnomeCmdFileSelector *fs);

void gnome_cmd_file_selector_cap_paste (GnomeCmdFileSelector *fs);

extern "C" void gnome_cmd_file_selector_create_symlink (GnomeCmdFileSelector *fs, GnomeCmdFile *f);
extern "C" void gnome_cmd_file_selector_create_symlinks (GnomeCmdFileSelector *fs, GList *files);

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
extern "C" GnomeCmdDir *gnome_cmd_file_selector_get_directory(GnomeCmdFileSelector *fs);

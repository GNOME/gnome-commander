/**
 * @file gnome-cmd-cmdline.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2023 Uwe Scholz\n
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

#include <config.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-cmdline.h"
#include "gnome-cmd-combo.h"
#include "gnome-cmd-style.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-dir.h"
#include "utils.h"

using namespace std;


static GtkHBoxClass *parent_class = nullptr;

struct GnomeCmdCmdlinePrivate
{
    GnomeCmdCombo *combo;
    GtkWidget *cwd;

    GList *history;
};


inline void update_history_combo (GnomeCmdCmdline *cmdline)
{
    cmdline->priv->combo->clear();

    for (GList *i = cmdline->priv->history; i; i = i->next)
    {
        gchar *command = (gchar *) i->data;

        cmdline->priv->combo->append(command, command, -1);
    }
}


inline void add_to_history (GnomeCmdCmdline *cmdline, const gchar *command)
{
    cmdline->priv->history = string_history_add (cmdline->priv->history, command, gnome_cmd_data.cmdline_history_length);

    update_history_combo (cmdline);
}


static void on_exec (GnomeCmdCmdline *cmdline, gboolean term)
{
    const gchar *cmdline_text;

    cmdline_text = gtk_entry_get_text (GTK_ENTRY (cmdline->priv->combo->get_entry()));
    cmdline_text = g_strstrip (g_strdup (cmdline_text));

    GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);

    if (g_str_has_prefix (cmdline_text, "cd "))   // if cmdline_text starts with "cd " then it MUST have at least 4 chars (as it's been previously g_strstrip'ed)
    {
        const gchar *dest_dir = g_strchug (const_cast<gchar *>(cmdline_text) + 3);

        if (strcmp (dest_dir, "-")==0)
        {
            auto *testGFile = gnome_cmd_dir_get_child_gfile (fs->get_directory(), "-");

            if (g_file_query_exists (testGFile, nullptr))
                fs->goto_directory(dest_dir);
            else
                fs->back();
            g_object_unref(testGFile);
        }
        else
            fs->goto_directory(dest_dir);
    }
    else
        if (strcmp (cmdline_text, "cd")==0)
            fs->goto_directory("~");
        else
            if (fs->is_local())
            {
                gchar *fpath = GNOME_CMD_FILE (fs->get_directory())->get_real_path ();

                run_command_indir (cmdline_text, fpath, term);
                g_free (fpath);
            }

    add_to_history (cmdline, cmdline_text);
    gnome_cmd_cmdline_set_text (cmdline, "");
}


static gboolean on_key_pressed (GtkWidget *entry, GdkEventKey *event, GnomeCmdCmdline *cmdline)
{
    switch (event->keyval)
    {
        case GDK_BackSpace:
        case GDK_space:
        case GDK_Home:
        case GDK_End:
        case GDK_Delete:
        case GDK_Left:
        case GDK_Right:
            return gnome_cmd_cmdline_keypressed (cmdline, event);

        default:
            break;
    }

    return gnome_cmd_cmdline_keypressed (cmdline, event) || main_win->key_pressed(event);
}


static void on_combo_item_selected (GnomeCmdCombo *combo, const gchar *command, GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_COMBO (combo));
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));
    g_return_if_fail (command != nullptr);

    gnome_cmd_cmdline_set_text (cmdline, command);
    gtk_widget_grab_focus (combo->get_entry());
}


static void on_combo_popwin_hidden (GnomeCmdCombo *combo, GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_COMBO (combo));

    gtk_widget_grab_focus (combo->get_entry());
}


static void on_switch_fs (GnomeCmdMainWin *mw, GnomeCmdFileSelector *fs, GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_MAIN_WIN (mw));
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    gchar *dpath = gnome_cmd_dir_get_display_path (fs->get_directory());
    gnome_cmd_cmdline_set_dir (cmdline, dpath);
    g_free (dpath);
}


static void on_fs_changed_dir (GnomeCmdFileSelector *fs, GnomeCmdDir *dir, GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_FILE_SELECTOR (fs));
    g_return_if_fail (dir != nullptr);
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    if (fs != main_win->fs(ACTIVE))
        return;

    gchar *dpath = gnome_cmd_dir_get_display_path (dir);
    gnome_cmd_cmdline_set_dir (cmdline, dpath);
    g_free (dpath);
}


/*******************************
 * Gtk class implementation
 *******************************/

static void destroy (GtkObject *object)
{
    g_signal_handlers_disconnect_by_func (main_win, (gpointer) on_switch_fs, object);
    g_signal_handlers_disconnect_by_func (main_win->fs(LEFT), (gpointer) on_fs_changed_dir, object);
    g_signal_handlers_disconnect_by_func (main_win->fs(RIGHT), (gpointer) on_fs_changed_dir, object);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != nullptr)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void class_init (GnomeCmdCmdlineClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    parent_class = (GtkHBoxClass *) gtk_type_class (gtk_hbox_get_type ());
    object_class->destroy = destroy;
    widget_class->map = ::map;
}


static void init (GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    GtkWidget *label;

    cmdline->priv = g_new0 (GnomeCmdCmdlinePrivate, 1);
    // cmdline->priv->history = nullptr;

    cmdline->priv->cwd = gtk_label_new ("cwd");
    g_object_ref (cmdline->priv->cwd);
    g_object_set_data_full (G_OBJECT (cmdline), "cwdlabel", cmdline->priv->cwd, g_object_unref);
    gtk_widget_show (cmdline->priv->cwd);
    gtk_box_pack_start (GTK_BOX (cmdline), cmdline->priv->cwd,
                        FALSE, TRUE, 2);
    gtk_label_set_selectable (GTK_LABEL (cmdline->priv->cwd), TRUE);

    label = gtk_label_new ("#");
    g_object_ref (label);
    g_object_set_data_full (G_OBJECT (cmdline), "label", label, g_object_unref);
    gtk_widget_show (label);
    gtk_box_pack_start (GTK_BOX (cmdline), label, FALSE, FALSE, 0);

    GtkListStore *store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
    cmdline->priv->combo = GNOME_CMD_COMBO (gnome_cmd_combo_new_with_store(store, 1, 0, 1));
    g_object_ref (cmdline->priv->combo);
    g_object_set_data_full (G_OBJECT (cmdline), "combo", cmdline->priv->combo, g_object_unref);
    gtk_box_pack_start (GTK_BOX (cmdline), *cmdline->priv->combo, TRUE, TRUE, 2);
    gtk_widget_show (*cmdline->priv->combo);
    gtk_entry_set_editable (GTK_ENTRY (cmdline->priv->combo->get_entry()), TRUE);
    gtk_widget_set_can_focus (cmdline->priv->combo->get_entry(), TRUE);

    g_signal_connect (cmdline->priv->combo->get_entry(), "key-press-event", G_CALLBACK (on_key_pressed), cmdline);
    g_signal_connect (cmdline->priv->combo, "item-selected", G_CALLBACK (on_combo_item_selected), cmdline);
    g_signal_connect (cmdline->priv->combo, "popwin-hidden", G_CALLBACK (on_combo_popwin_hidden), cmdline);
    g_signal_connect_after (main_win, "switch-fs", G_CALLBACK (on_switch_fs), cmdline);
    g_signal_connect (main_win->fs(LEFT), "dir-changed", G_CALLBACK (on_fs_changed_dir), cmdline);
    g_signal_connect (main_win->fs(RIGHT), "dir-changed", G_CALLBACK (on_fs_changed_dir), cmdline);

    gnome_cmd_cmdline_update_style (cmdline);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_cmdline_new ()
{
    auto *cmdline = static_cast<GnomeCmdCmdline *> (g_object_new (GNOME_CMD_TYPE_CMDLINE, nullptr));

    gnome_cmd_cmdline_set_history (cmdline, gnome_cmd_data.cmdline_history);
    update_history_combo (cmdline);

    return GTK_WIDGET (cmdline);
}


GtkType gnome_cmd_cmdline_get_type ()
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            (gchar*) "GnomeCmdCmdline",
            sizeof (GnomeCmdCmdline),
            sizeof (GnomeCmdCmdlineClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ nullptr,
            /* reserved_2 */ nullptr,
            (GtkClassInitFunc) nullptr
        };

        dlg_type = gtk_type_unique (gtk_hbox_get_type (), &dlg_info);
    }
    return dlg_type;
}


void gnome_cmd_cmdline_set_dir (GnomeCmdCmdline *cmdline, const gchar *cwd)
{
    g_return_if_fail (cmdline != nullptr);
    g_return_if_fail (cmdline->priv != nullptr);

    gchar *s = get_utf8 (cwd);

    gtk_label_set_text (GTK_LABEL (cmdline->priv->cwd), s);
    g_free (s);
}


void gnome_cmd_cmdline_append_text (GnomeCmdCmdline *cmdline, const gchar *text)
{
    g_return_if_fail (cmdline != nullptr);
    g_return_if_fail (cmdline->priv != nullptr);
    g_return_if_fail (cmdline->priv->combo != nullptr);

    GtkEntry *entry = GTK_ENTRY (cmdline->priv->combo->get_entry());
    const gchar *curtext = gtk_entry_get_text (entry);

    if (curtext[strlen(curtext)-1] != ' ' && strlen(curtext) > 0)
        gtk_entry_append_text (entry, " ");

    gint curpos = gtk_editable_get_position (GTK_EDITABLE (entry));
    gtk_entry_append_text (entry, text);
    gtk_editable_set_position (GTK_EDITABLE (entry), curpos + strlen (text));
}


void gnome_cmd_cmdline_insert_text (GnomeCmdCmdline *cmdline, const gchar *text)
{
    g_return_if_fail (cmdline != nullptr);
    g_return_if_fail (cmdline->priv != nullptr);
    g_return_if_fail (cmdline->priv->combo != nullptr);

    GtkEntry *entry = GTK_ENTRY (cmdline->priv->combo->get_entry());
    gint curpos = gtk_editable_get_position (GTK_EDITABLE (entry));
    gint tmp = curpos;
    gtk_editable_insert_text (GTK_EDITABLE (entry), text, strlen (text), &tmp);
    gtk_editable_set_position (GTK_EDITABLE (entry), curpos + strlen (text));
}


void gnome_cmd_cmdline_set_text (GnomeCmdCmdline *cmdline, const gchar *text)
{
    g_return_if_fail (cmdline != nullptr);
    g_return_if_fail (cmdline->priv != nullptr);
    g_return_if_fail (cmdline->priv->combo != nullptr);

    gtk_entry_set_text (GTK_ENTRY (cmdline->priv->combo->get_entry()), text);
}


gboolean gnome_cmd_cmdline_is_empty (GnomeCmdCmdline *cmdline)
{
    const gchar *text = gtk_entry_get_text (GTK_ENTRY (cmdline->priv->combo->get_entry()));

    if (text == nullptr || strcmp (text, ""))
        return TRUE;

    return FALSE;
}


void gnome_cmd_cmdline_exec (GnomeCmdCmdline *cmdline)
{
    on_exec (cmdline, FALSE);
}


void gnome_cmd_cmdline_focus (GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));
    g_return_if_fail (cmdline->priv->combo != nullptr);

    gtk_widget_grab_focus (GTK_WIDGET (cmdline->priv->combo->get_entry()));
    gtk_editable_set_position (GTK_EDITABLE (cmdline->priv->combo->get_entry()), -1);
}


void gnome_cmd_cmdline_update_style (GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    cmdline->priv->combo->update_style();
}


void gnome_cmd_cmdline_show_history (GnomeCmdCmdline *cmdline)
{
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    cmdline->priv->combo->popup_list();
}


GList *gnome_cmd_cmdline_get_history (GnomeCmdCmdline *cmdline)
{
    g_return_val_if_fail (GNOME_CMD_IS_CMDLINE (cmdline), nullptr);

    return cmdline->priv->history;
}


void gnome_cmd_cmdline_set_history (GnomeCmdCmdline *cmdline, GList *history)
{
    g_return_if_fail (GNOME_CMD_IS_CMDLINE (cmdline));

    // free the old history

    for (GList *i = cmdline->priv->history; i; i = i->next)
        g_free (i->data);

    cmdline->priv->history = history;
}


gboolean gnome_cmd_cmdline_keypressed (GnomeCmdCmdline *cmdline, GdkEventKey *event)
{
    g_return_val_if_fail (GNOME_CMD_IS_CMDLINE (cmdline), FALSE);
    g_return_val_if_fail (event != nullptr, FALSE);

    if (state_is_ctrl (event->state))
    {
        switch (event->keyval)
        {
            case GDK_Down:
                gnome_cmd_cmdline_show_history (cmdline);
                return TRUE;

            case GDK_Return:
                event->string[0] = '\0';
                return FALSE;
            default:
                return FALSE;
        }
    }
    else
    {
        if (state_is_shift (event->state))
        {
            switch (event->keyval)
            {
                case GDK_Return:
                    on_exec (cmdline, TRUE);
                    event->keyval = GDK_Escape;
                    return TRUE;
                default:
                    return FALSE;
            }
        }
        else
        {
            if (state_is_ctrl_shift (event->state))
            {
                switch (event->keyval)
                {
                    case GDK_Return:
                        event->string[0] = '\0';
                        return FALSE;
                    default:
                        return FALSE;
                }
            }
            else
            {
                if (state_is_blank (event->state))
                {
                    switch (event->keyval)
                    {
                        case GDK_Return:
                            on_exec (cmdline, FALSE);
                            event->keyval = GDK_Escape;
                            return TRUE;

                        case GDK_Escape:
                            gnome_cmd_cmdline_set_text (cmdline, "");
                            main_win->focus_file_lists();
                            return TRUE;

                        case GDK_Up:
                        case GDK_Down:
                            {
                                gboolean ret;
                                GdkEventKey event2 = *event;
                                GnomeCmdFileSelector *fs = main_win->fs(ACTIVE);
                                GtkWidget *file_list = *fs->file_list();

                                gtk_widget_grab_focus (file_list);
                                fs->set_active(TRUE);

                                gtk_signal_emit_by_name (GTK_OBJECT (file_list), "key-press-event", &event2, &ret);
                                event->keyval = 0;
                            }
                            return FALSE;
                        default:
                            return FALSE;
                    }
                }
            }
        }
    }
    return FALSE;
}

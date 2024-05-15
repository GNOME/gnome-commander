/**
 * @file gnome-cmd-dir-indicator.cc
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

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-file-selector.h"
#include "gnome-cmd-main-win.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-user-actions.h"
#include "dialogs/gnome-cmd-manage-bookmarks-dialog.h"
#include "imageloader.h"
#include "utils.h"

using namespace std;


struct GnomeCmdDirIndicatorPrivate
{
    GtkWidget *event_box;
    GtkWidget *label;
    GtkWidget *history_button;
    GtkWidget *bookmark_button;
    GnomeCmdFileSelector *fs;
    int *slashCharPosition;
    int *slashPixelPosition;
    int numPositions;
};


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdDirIndicator, gnome_cmd_dir_indicator, GTK_TYPE_FRAME)


static void select_path (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void add_bookmark (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void manage_bookmarks (GSimpleAction *action, GVariant *parameter, gpointer user_data);


/*******************************
 * Gtk class implementation
 *******************************/
static void dispose (GObject *object)
{
    GnomeCmdDirIndicator *dir_indicator = GNOME_CMD_DIR_INDICATOR (object);
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (dir_indicator));

    g_clear_pointer (&priv->slashCharPosition, g_free);
    g_clear_pointer (&priv->slashPixelPosition, g_free);

    G_OBJECT_CLASS (gnome_cmd_dir_indicator_parent_class)->dispose (object);
}


static void gnome_cmd_dir_indicator_class_init (GnomeCmdDirIndicatorClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = dispose;
}


static void set_clipboard_from_indicator(GnomeCmdDirIndicator *indicator, const gchar *labelText, GdkEventButton *event)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    gchar *chTo = g_strdup (labelText);
    gint x = (gint) event->x;

    for (gint i=0; i < priv->numPositions; ++i)
    {
        if (x < priv->slashPixelPosition[i])
        {
            chTo[priv->slashCharPosition[i]] = 0;
            gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), chTo, priv->slashCharPosition[i]);
            break;
        }
    }
    g_free (chTo);
}


static void change_dir_from_clicked_indicator(GdkEventButton *event, const gchar* chToDir, GnomeCmdFileSelector *fs)
{
    main_win->switch_fs(fs);
    if (event->button==2 || event->state & GDK_CONTROL_MASK || fs->file_list()->locked)
    {
        GnomeCmdCon *con = fs->get_connection();
        GnomeCmdDir *dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, chToDir));
        fs->new_tab(dir);
    }
    else
    {
        fs->goto_directory(chToDir);
    }
}


static gboolean handle_remote_indicator_clicked (GnomeCmdDirIndicator *indicator, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    auto labelText = gtk_label_get_text (GTK_LABEL (priv->label));
    auto colonPtr = strchr(labelText, ':');
    auto colonIndex = (int)(colonPtr - labelText) + 1; // + 1 as we want to ignore "host:" until the colon

    if (event->button==1 || event->button==2)
    {
        // left click - work out the path
        auto chTo = g_strdup (colonPtr+1);
        auto x = (gint) event->x;

        for (gint slashIdx=0; slashIdx < priv->numPositions; ++slashIdx)
        {
            if (x < priv->slashPixelPosition[slashIdx])
            {
                chTo[priv->slashCharPosition[slashIdx] - colonIndex] = 0;
                change_dir_from_clicked_indicator(event, chTo, fs);
                break;
            }
        }
        // pointer is after directory name - just return
        g_free (chTo);
        return TRUE;
    }
    else if (event->button==3)
    {
        set_clipboard_from_indicator(indicator, labelText, event);
        return TRUE;
    }
    return FALSE;
}


static gboolean handle_local_indicator_clicked (GnomeCmdDirIndicator *indicator, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    auto labelText = gtk_label_get_text (GTK_LABEL (priv->label));

    if (event->button==1 || event->button==2)
    {
        // left click - work out the path
        auto chTo = g_strdup (labelText);
        auto x = (gint) event->x;

        for (gint slashIdx = 0; slashIdx < priv->numPositions; ++slashIdx)
        {
            if (x < priv->slashPixelPosition[slashIdx])
            {
                chTo[priv->slashCharPosition[slashIdx]] = 0;
                change_dir_from_clicked_indicator(event, chTo, fs);
                break;
            }
        }
        // pointer is after directory name - just return
        g_free (chTo);
        return TRUE;
    }
    else if (event->button==3)
    {
        set_clipboard_from_indicator(indicator, labelText, event);
        return TRUE;
    }

    return FALSE;
}

/*******************************
 * Event handlers
 *******************************/
static gboolean on_dir_indicator_clicked (GnomeCmdDirIndicator *indicator, GdkEventButton *event, GnomeCmdFileSelector *fs)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator), FALSE);
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    if (event->type!=GDK_BUTTON_PRESS)
        return FALSE;

    const gchar *labelText = gtk_label_get_text (GTK_LABEL (priv->label));

    if(strchr(labelText, ':'))
    {
        return handle_remote_indicator_clicked(indicator, event, fs);
    }
    else
    {
        return handle_local_indicator_clicked(indicator, event, fs);
    }

}


static gchar* update_markup_for_remote(GnomeCmdDirIndicator *indicator, gint slashIdx)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    auto tmpLabelText = g_strdup (gtk_label_get_text (GTK_LABEL (priv->label)));
    auto colonPtr = strchr(tmpLabelText, ':');
    if (!colonPtr)
    {
        g_free(tmpLabelText);
        return nullptr;
    }

    if (slashIdx == 0)
    {
        auto monoText = get_mono_text (tmpLabelText);
        g_free(tmpLabelText);
        return monoText;
    }

    auto colonIndex = (int)(colonPtr - tmpLabelText) + 1; // +1 as we want to ignore "host:" after the colon
    auto startText = g_strndup(tmpLabelText, colonIndex);
    auto startMonoText = get_mono_text (startText);

    auto middleText = slashIdx > 0
        ? g_strndup(&tmpLabelText[colonIndex], priv->slashCharPosition[slashIdx] - colonIndex)
        : g_strdup("");
    gchar *middleBoldText = get_bold_mono_text (middleText);

    gchar *restText = slashIdx > 0
        ? g_strdup (&tmpLabelText[priv->slashCharPosition[slashIdx]])
        : &tmpLabelText[colonIndex];
    gchar *restMonoText = get_mono_text (restText);

    auto markupText = g_strconcat (startMonoText, middleBoldText, restMonoText, nullptr);

    g_free (startText);
    g_free (startMonoText);
    g_free (middleText);
    g_free (middleBoldText);
    g_free (restText);
    g_free (restMonoText);
    g_free (tmpLabelText);
    return markupText;
}


static gchar* update_markup_for_local(GnomeCmdDirIndicator *indicator, gint i)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    auto tmpLabelText = g_strdup (gtk_label_get_text (GTK_LABEL (priv->label)));
    gchar *restText = g_strdup (&tmpLabelText[priv->slashCharPosition[i]]);

    tmpLabelText[priv->slashCharPosition[i]] = '\0';
    gchar *boldMonoText = get_bold_mono_text (tmpLabelText);
    gchar *monoText = get_mono_text (restText);
    auto markupText = g_strconcat (boldMonoText, monoText, nullptr);
    g_free (restText);
    g_free (monoText);
    g_free (boldMonoText);
    g_free (tmpLabelText);
    return markupText;
}


static void update_markup (GnomeCmdDirIndicator *indicator, gint i)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    if (!priv->slashCharPosition)
        return;

    gchar *tmpLabelText, *markupText;

    tmpLabelText = g_strdup (gtk_label_get_text (GTK_LABEL (priv->label)));

    if (i >= 0)
    {
        // If we are on a remote connection, there is a colon in the label
        auto colonPosition = strstr(tmpLabelText, ":");

        if (colonPosition)
        {
            markupText = update_markup_for_remote(indicator, i);
        }
        else
        {
            markupText = update_markup_for_local(indicator, i);
        }
    }
    else
        markupText = get_mono_text (tmpLabelText);

    gtk_label_set_markup (GTK_LABEL (priv->label), markupText);
    g_free (tmpLabelText);
    g_free (markupText);
}


static gint on_dir_indicator_motion (GnomeCmdDirIndicator *indicator, GdkEventMotion *event, gpointer user_data)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator), FALSE);
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    if (priv->slashCharPosition == nullptr)
        return FALSE;
    if (priv->slashPixelPosition == nullptr)
        return FALSE;

    // find out where in the label the pointer is at
    gint iX = (gint) event->x;

    for (gint i=0; i < priv->numPositions; i++)
    {
        if (iX < priv->slashPixelPosition[i])
        {
            // underline the part that is selected
            GdkCursor *cursor = gdk_cursor_new (GDK_HAND2);
            gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (indicator)), cursor);
            gdk_cursor_unref (cursor);

            update_markup (indicator, i);

            return TRUE;
        }

        // clear underline, cursor=pointer
        update_markup (indicator, -1);
        gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (indicator)), nullptr);
    }

    return TRUE;
}


static gint on_dir_indicator_leave (GnomeCmdDirIndicator *indicator, GdkEventMotion *event, gpointer user_data)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator), FALSE);

    // clear underline, cursor=pointer
    update_markup (indicator, -1);
    gdk_window_set_cursor (gtk_widget_get_window (GTK_WIDGET (indicator)), nullptr);

    return TRUE;
}


static gboolean on_bookmark_button_clicked (GtkWidget *button, GnomeCmdDirIndicator *indicator)
{
    gnome_cmd_dir_indicator_show_bookmarks (indicator);
    return TRUE;
}


static gboolean on_history_button_clicked (GtkWidget *button, GnomeCmdDirIndicator *indicator)
{
    gnome_cmd_dir_indicator_show_history (indicator);
    return TRUE;
}


static void select_path (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto indicator = static_cast<GnomeCmdDirIndicator*> (user_data);
    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator));
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    const gchar *path;
    g_variant_get (parameter, "s", &path);
    g_return_if_fail (path != nullptr);

    GdkModifierType mask = get_modifiers_state();

    main_win->switch_fs(priv->fs);

    if (mask & GDK_CONTROL_MASK || priv->fs->file_list()->locked)
    {
        GnomeCmdCon *con = priv->fs->get_connection();
        GnomeCmdDir *dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, path));
        priv->fs->new_tab(dir);
    }
    else
        priv->fs->goto_directory(path);
}


static void get_popup_pos (GtkMenu *menu, gint *x, gint *y, gboolean push_in, GnomeCmdDirIndicator *indicator)
{
    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator));
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    GtkWidget *w = GTK_WIDGET (priv->fs->file_list());

    gdk_window_get_origin (gtk_widget_get_window (w), x, y);
}


void gnome_cmd_dir_indicator_show_history (GnomeCmdDirIndicator *indicator)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    GnomeCmdCon *con = priv->fs->get_connection();
    History *history = gnome_cmd_con_get_dir_history (con);

    GMenu *menu = g_menu_new ();
    for (GList *l=history->ents; l; l=l->next)
    {
        gchar *path = (gchar *) l->data;

        GMenuItem *item = g_menu_item_new (path, nullptr);
        g_menu_item_set_action_and_target (item, "indicator.select-path", "s", path);
        g_menu_append_item (menu, item);
    }

    GtkWidget *popover = gtk_popover_new_from_model (GTK_WIDGET (indicator), G_MENU_MODEL (menu));
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);

    GtkAllocation indicator_allocation;
    gtk_widget_get_allocation (GTK_WIDGET (indicator), &indicator_allocation);

    if (indicator_allocation.width > 100)
        gtk_widget_set_size_request (popover, indicator_allocation.width, -1);

    gtk_popover_popup (GTK_POPOVER (popover));
}


static void add_bookmark (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto indicator = static_cast<GnomeCmdDirIndicator*> (user_data);
    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator));
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    gnome_cmd_bookmark_add_current (priv->fs->get_directory());
}


static void manage_bookmarks (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto indicator = static_cast<GnomeCmdDirIndicator*> (user_data);
    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator));

}


void gnome_cmd_dir_indicator_show_bookmarks (GnomeCmdDirIndicator *indicator)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    GnomeCmdCon *con = priv->fs->get_connection();
    GnomeCmdBookmarkGroup *group = gnome_cmd_con_get_bookmarks (con);

    GMenu *bookmarks_section = g_menu_new ();
    for (GList *l = group->bookmarks; l; l = l->next)
    {
        auto bm = static_cast<GnomeCmdBookmark*> (l->data);

        GMenuItem *item = g_menu_item_new (bm->name, nullptr);
        g_menu_item_set_action_and_target (item, "indicator.select-path", "s", bm->path);
        g_menu_append_item (bookmarks_section, item);
    }

    GMenu *manage_section = g_menu_new ();
    g_menu_append (manage_section, _("Add current dir"), "indicator.add-bookmark");
    g_menu_append (manage_section, _("Manage bookmarks…"), "indicator.manage-bookmarks");

    GMenu *menu = g_menu_new ();
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (bookmarks_section));
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (manage_section));

    GtkWidget *popover = gtk_popover_new_from_model (GTK_WIDGET (indicator), G_MENU_MODEL (menu));
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);

    GtkAllocation indicator_allocation;
    gtk_widget_get_allocation (GTK_WIDGET (indicator), &indicator_allocation);

    if (indicator_allocation.width > 100)
        gtk_widget_set_size_request (popover, indicator_allocation.width, -1);

    gtk_popover_popup (GTK_POPOVER (popover));
}


static void gnome_cmd_dir_indicator_init (GnomeCmdDirIndicator *indicator)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    GtkWidget *hbox, *bbox;

    // create the directory label and its event box
    priv->event_box = gtk_event_box_new ();
    g_object_ref (priv->event_box);
    g_signal_connect_swapped (priv->event_box, "motion-notify-event", G_CALLBACK (on_dir_indicator_motion), indicator);
    g_signal_connect_swapped (priv->event_box, "leave-notify-event", G_CALLBACK (on_dir_indicator_leave), indicator);
    gtk_widget_set_events (priv->event_box, GDK_POINTER_MOTION_MASK);

    gtk_widget_show (priv->event_box);

    priv->label = create_label (GTK_WIDGET (indicator), "not initialized");
    gtk_container_add (GTK_CONTAINER (priv->event_box), priv->label);

    // create the history popup button
    priv->history_button = gtk_button_new ();
    gtk_button_set_image (GTK_BUTTON (priv->history_button), gtk_image_new_from_gicon (g_themed_icon_new ("gnome-commander-down"), GTK_ICON_SIZE_SMALL_TOOLBAR));
    gtk_widget_set_can_focus (priv->history_button, FALSE);
    g_object_ref (priv->history_button);
    gtk_button_set_relief (GTK_BUTTON (priv->history_button), GTK_RELIEF_NONE);
    g_object_set_data_full (G_OBJECT (indicator), "button", priv->history_button, g_object_unref);
    gtk_widget_show (priv->history_button);

    // create the bookmark popup button
    priv->bookmark_button = create_styled_pixbuf_button (nullptr, IMAGE_get_pixbuf (PIXMAP_BOOKMARK));
    gtk_widget_set_can_focus (priv->bookmark_button, FALSE);
    gtk_button_set_relief (GTK_BUTTON (priv->bookmark_button), GTK_RELIEF_NONE);
    g_object_set_data_full (G_OBJECT (indicator), "button", priv->bookmark_button, g_object_unref);
    gtk_widget_show (priv->bookmark_button);

    // pack
    hbox = create_hbox (GTK_WIDGET (indicator), FALSE, 10);
    gtk_container_add (GTK_CONTAINER (indicator), hbox);
    gtk_box_pack_start (GTK_BOX (hbox), priv->event_box, TRUE, TRUE, 0);
    bbox = create_hbox (GTK_WIDGET (indicator), FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (bbox), priv->bookmark_button, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (bbox), priv->history_button, FALSE, FALSE, 0);

    g_signal_connect (priv->history_button, "clicked", G_CALLBACK (on_history_button_clicked), indicator);
    g_signal_connect (priv->bookmark_button, "clicked", G_CALLBACK (on_bookmark_button_clicked), indicator);

    GSimpleActionGroup *action_group = g_simple_action_group_new ();
    static GActionEntry entries[] = {
        { "select-path", select_path, "s" },
        { "add-bookmark", add_bookmark },
        { "manage-bookmarks", manage_bookmarks }
    };
    g_action_map_add_action_entries (G_ACTION_MAP (action_group), entries, G_N_ELEMENTS (entries), indicator);

    gtk_widget_insert_action_group (GTK_WIDGET (indicator), "indicator", G_ACTION_GROUP (action_group));
}

/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_dir_indicator_new (GnomeCmdFileSelector *fs)
{
    auto dir_indicator = static_cast<GnomeCmdDirIndicator*> (g_object_new (GNOME_CMD_TYPE_DIR_INDICATOR, nullptr));
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (dir_indicator));

    g_signal_connect (dir_indicator, "button-press-event", G_CALLBACK (on_dir_indicator_clicked), fs);

    priv->fs = fs;

    return GTK_WIDGET (dir_indicator);
}


static void update_slash_positions(GnomeCmdDirIndicator *indicator, const gchar* path)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    g_clear_pointer (&priv->slashCharPosition, g_free);
    g_clear_pointer (&priv->slashPixelPosition, g_free);
    priv->numPositions = 0;

    if (!path)
        return;

    gboolean isUNC = g_str_has_prefix (path, "\\\\");

    if (!isUNC && (*path!=G_DIR_SEPARATOR && !strstr(path, ":")))
        return;

    const gchar sep = isUNC ? '\\' : G_DIR_SEPARATOR;
    GArray *sepPositions = g_array_sized_new (FALSE, FALSE, sizeof(gint), 16);

    gchar *tmpPath = get_utf8 (path);

    for (tmpPath = isUNC ? (gchar*) path+2 : (gchar*) path+1; *tmpPath; ++tmpPath)
    {
        if (*tmpPath == sep)
        {
            gint i = tmpPath - path;
            g_array_append_val (sepPositions, i);
        }
    }

    gint path_len = tmpPath - path;

    // allocate memory for storing (back)slashes positions
    // (both char positions within the string and their pixel positions in the display)

    // now there will be pos->len entries for UNC:
    //    [0..pos->len-2] is '\\host\share\dir' etc.
    //    last entry [pos->len-1] will be the whole UNC
    // pos->len+1 (1) entries for '/' (root):
    //    last entry [pos->len] will be the whole path ('/')
    // and pos->len+2 entries otherwise:
    //    first entry [0] is just '/' (root)
    //    [1..pos->len] is '/dir/dir' etc.
    //    last entry [pos->len+1] will be the whole path

    priv->numPositions = isUNC ? sepPositions->len :
                                    path_len==1 ? sepPositions->len+1 : sepPositions->len+2;
    priv->slashCharPosition = g_new (gint, priv->numPositions);
    priv->slashPixelPosition = g_new (gint, priv->numPositions);

    gint pos_idx = 0;

    if (!isUNC && path_len>1)
    {
        priv->slashCharPosition[pos_idx] = 1;
        //auto colonPtr = strchr(path, ':');
        //colonPtr = colonPtr ? colonPtr : path;
        //auto startLength = (int)(colonPtr - path) + 1; // +1 as we want to ignore "host:"
        //indicator->priv->slashPixelPosition[pos_idx++] = get_string_pixel_size (path, startLength);
        priv->slashPixelPosition[pos_idx++] = get_string_pixel_size (path, 1);
    }

    for (guint ii = isUNC ? 1 : 0; ii < sepPositions->len; ii++)
    {
        priv->slashCharPosition[pos_idx] = g_array_index (sepPositions, gint, ii);
        priv->slashPixelPosition[pos_idx++] = get_string_pixel_size (path, g_array_index (sepPositions, gint, ii)+1);
    }

    if (priv->numPositions>0)
    {
        priv->slashCharPosition[pos_idx] = path_len;
        priv->slashPixelPosition[pos_idx] = get_string_pixel_size (path, path_len);
    }

    g_array_free (sepPositions, TRUE);
}


void gnome_cmd_dir_indicator_set_dir (GnomeCmdDirIndicator *indicator, GnomeCmdDir *dir)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    gchar *path = gnome_cmd_dir_get_display_path (dir);
    g_return_if_fail (path != nullptr);

    gchar* host = nullptr; // show host in dir indicator if we are not on a local connection
    if (!gnome_cmd_dir_is_local(dir))
    {
        GError *error = nullptr;
        auto dirUri = g_file_get_uri(GNOME_CMD_FILE(dir)->get_file());
        auto gUri = g_uri_parse(dirUri, G_URI_FLAGS_NONE, &error);
        if (error)
        {
            g_warning("g_uri_parse error %s: %s", dirUri, error->message);
            g_error_free(error);
        }
        else
        {
            host = g_strdup(g_uri_get_host(gUri));
        }
        g_free(dirUri);
    }

    gchar *tmpPath = get_utf8 (path);
    gchar *indicatorString = host
        ? g_strdup_printf("%s:%s", host, tmpPath)
        : g_strdup_printf("%s", tmpPath);
    g_free (tmpPath);
    gchar *monoIndicatorString = get_mono_text (indicatorString);
    gtk_label_set_markup (GTK_LABEL (priv->label), monoIndicatorString);
    g_free (monoIndicatorString);
    update_markup (indicator, -1);

    update_slash_positions(indicator, indicatorString);
    g_free(indicatorString);
    g_free(path);
}


void gnome_cmd_dir_indicator_set_active (GnomeCmdDirIndicator *indicator, gboolean active)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    switch (active)
    {
        case true:
        {
            auto tmpLabelText = g_strdup (gtk_label_get_text (GTK_LABEL (priv->label)));
            gchar *result = g_strdup_printf("<span font_family=\"monospace\" underline=\"low\">%s</span>", tmpLabelText);
            gtk_label_set_markup (GTK_LABEL (priv->label), result);
            g_free (tmpLabelText);
            break;
        }
        case false:
        {
            auto tmpLabelText = g_strdup (gtk_label_get_text (GTK_LABEL (priv->label)));
            gchar *result = g_strdup_printf("<span font_family=\"monospace\">%s</span>", tmpLabelText);
            gtk_label_set_markup (GTK_LABEL (priv->label), result);
            g_free (tmpLabelText);
            break;
        }
    }
}

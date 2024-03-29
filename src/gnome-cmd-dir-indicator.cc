/**
 * @file gnome-cmd-dir-indicator.cc
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
#include "gnome-cmd-dir-indicator.h"
#include "gnome-cmd-style.h"
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
    GtkWidget *dir_history_popup;
    GtkWidget *bookmark_popup;
    gboolean history_is_popped;
    gboolean bookmark_is_popped;
    GnomeCmdFileSelector *fs;
    int *slashCharPosition;
    int *slashPixelPosition;
    int numPositions;
};


static GtkFrameClass *parent_class = nullptr;


/*******************************
 * Gtk class implementation
 *******************************/
static void destroy (GtkObject *object)
{
    GnomeCmdDirIndicator *dir_indicator = GNOME_CMD_DIR_INDICATOR (object);

    g_free (dir_indicator->priv->slashCharPosition);
    g_free (dir_indicator->priv->slashPixelPosition);
    g_free (dir_indicator->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void class_init (GnomeCmdDirIndicatorClass *klass)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

    parent_class = (GtkFrameClass *) gtk_type_class (gtk_frame_get_type ());

    object_class->destroy = destroy;
}


static void set_clipboard_from_indicator(GnomeCmdDirIndicator *indicator, const gchar *labelText, GdkEventButton *event)
{
    gchar *chTo = g_strdup (labelText);
    gint x = (gint) event->x;

    for (gint i=0; i < indicator->priv->numPositions; ++i)
    {
        if (x < indicator->priv->slashPixelPosition[i])
        {
            chTo[indicator->priv->slashCharPosition[i]] = 0;
            gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), chTo, indicator->priv->slashCharPosition[i]);
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
    auto labelText = gtk_label_get_text (GTK_LABEL (indicator->priv->label));
    auto colonPtr = strchr(labelText, ':');
    auto colonIndex = (int)(colonPtr - labelText) + 1; // + 1 as we want to ignore "host:" until the colon

    if (event->button==1 || event->button==2)
    {
        // left click - work out the path
        auto chTo = g_strdup (colonPtr+1);
        auto x = (gint) event->x;

        for (gint slashIdx=0; slashIdx < indicator->priv->numPositions; ++slashIdx)
        {
            if (x < indicator->priv->slashPixelPosition[slashIdx])
            {
                chTo[indicator->priv->slashCharPosition[slashIdx] - colonIndex] = 0;
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
    auto labelText = gtk_label_get_text (GTK_LABEL (indicator->priv->label));

    if (event->button==1 || event->button==2)
    {
        // left click - work out the path
        auto chTo = g_strdup (labelText);
        auto x = (gint) event->x;

        for (gint slashIdx = 0; slashIdx < indicator->priv->numPositions; ++slashIdx)
        {
            if (x < indicator->priv->slashPixelPosition[slashIdx])
            {
                chTo[indicator->priv->slashCharPosition[slashIdx]] = 0;
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

    if (event->type!=GDK_BUTTON_PRESS)
        return FALSE;

    const gchar *labelText = gtk_label_get_text (GTK_LABEL (indicator->priv->label));

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
    auto tmpLabelText = g_strdup (gtk_label_get_text (GTK_LABEL (indicator->priv->label)));
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
        ? g_strndup(&tmpLabelText[colonIndex], indicator->priv->slashCharPosition[slashIdx] - colonIndex)
        : g_strdup("");
    gchar *middleBoldText = get_bold_mono_text (middleText);

    gchar *restText = slashIdx > 0
        ? g_strdup (&tmpLabelText[indicator->priv->slashCharPosition[slashIdx]])
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
    auto tmpLabelText = g_strdup (gtk_label_get_text (GTK_LABEL (indicator->priv->label)));
    gchar *restText = g_strdup (&tmpLabelText[indicator->priv->slashCharPosition[i]]);

    tmpLabelText[indicator->priv->slashCharPosition[i]] = '\0';
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
    if (!indicator->priv->slashCharPosition)
        return;

    gchar *tmpLabelText, *markupText;

    tmpLabelText = g_strdup (gtk_label_get_text (GTK_LABEL (indicator->priv->label)));

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

    gtk_label_set_markup (GTK_LABEL (indicator->priv->label), markupText);
    g_free (tmpLabelText);
    g_free (markupText);
}


static gint on_dir_indicator_motion (GnomeCmdDirIndicator *indicator, GdkEventMotion *event, gpointer user_data)
{
    g_return_val_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator), FALSE);

    if (indicator->priv->slashCharPosition == nullptr)
        return FALSE;
    if (indicator->priv->slashPixelPosition == nullptr)
        return FALSE;

    // find out where in the label the pointer is at
    gint iX = (gint) event->x;

    for (gint i=0; i < indicator->priv->numPositions; i++)
    {
        if (iX < indicator->priv->slashPixelPosition[i])
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
    if (indicator->priv->bookmark_is_popped)
    {
        gtk_widget_hide (indicator->priv->bookmark_popup);
        indicator->priv->bookmark_is_popped = FALSE;
    }
    else
    {
        gnome_cmd_dir_indicator_show_bookmarks (indicator);
        indicator->priv->bookmark_is_popped = TRUE;
    }

    return TRUE;
}


static gboolean on_history_button_clicked (GtkWidget *button, GnomeCmdDirIndicator *indicator)
{
    if (indicator->priv->history_is_popped)
    {
        gtk_widget_hide (indicator->priv->dir_history_popup);
        indicator->priv->history_is_popped = FALSE;
    }
    else
    {
        gnome_cmd_dir_indicator_show_history (indicator);
        indicator->priv->history_is_popped = TRUE;
    }

    return TRUE;
}


static void on_dir_history_popup_hide (GtkMenu *menu, GnomeCmdDirIndicator *indicator)
{
    indicator->priv->dir_history_popup = nullptr;
    indicator->priv->history_is_popped = FALSE;
}


static void on_bookmark_popup_hide (GtkMenu *menu, GnomeCmdDirIndicator *indicator)
{
    indicator->priv->bookmark_popup = nullptr;
    indicator->priv->bookmark_is_popped = FALSE;
}


static void on_dir_history_item_selected (GtkMenuItem *item, const gchar *path)
{
    g_return_if_fail (path != nullptr);

    GdkModifierType mask;
    gdk_window_get_pointer (nullptr, nullptr, nullptr, &mask);

    auto indicator = static_cast<GnomeCmdDirIndicator*> (g_object_get_data (G_OBJECT (item), "indicator"));

    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator));

    main_win->switch_fs(indicator->priv->fs);

    if (mask&GDK_CONTROL_MASK || indicator->priv->fs->file_list()->locked)
    {
        GnomeCmdCon *con = indicator->priv->fs->get_connection();
        GnomeCmdDir *dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, path));
        indicator->priv->fs->new_tab(dir);
    }
    else
        indicator->priv->fs->goto_directory(path);
}


static void on_bookmark_item_selected (GtkMenuItem *item, GnomeCmdBookmark *bm)
{
    g_return_if_fail (bm != nullptr);

    GdkModifierType mask;
    gdk_window_get_pointer (nullptr, nullptr, nullptr, &mask);

    auto indicator = static_cast<GnomeCmdDirIndicator*> (g_object_get_data (G_OBJECT (item), "indicator"));

    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator));

    main_win->switch_fs(indicator->priv->fs);

    if (mask&GDK_CONTROL_MASK || indicator->priv->fs->file_list()->locked)
    {
        GnomeCmdCon *con = indicator->priv->fs->get_connection();
        GnomeCmdDir *dir = gnome_cmd_dir_new (con, gnome_cmd_con_create_path (con, bm->path));
        indicator->priv->fs->new_tab(dir);
    }
    else
        indicator->priv->fs->goto_directory(bm->path);
}


static void get_popup_pos (GtkMenu *menu, gint *x, gint *y, gboolean push_in, GnomeCmdDirIndicator *indicator)
{
    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator));

    GtkWidget *w = GTK_WIDGET (indicator->priv->fs->file_list());

    gdk_window_get_origin (gtk_widget_get_window (w), x, y);
}


static void add_menu_item (GnomeCmdDirIndicator *indicator, GtkMenuShell *shell, const gchar *text, GtkSignalFunc func, gpointer data)
{
    GtkWidget *item = text ? gtk_menu_item_new_with_label (text) : gtk_menu_item_new ();

    g_object_ref (item);
    g_object_set_data (G_OBJECT (item), "indicator", indicator);
    g_object_set_data_full (G_OBJECT (shell), "menu_item", item, g_object_unref);
    if (func)
        g_signal_connect (item, "activate", G_CALLBACK (func), data);
    gtk_widget_show (item);
    if (!text)
        gtk_widget_set_sensitive (item, FALSE);
    gtk_menu_shell_append (shell, item);
}


void gnome_cmd_dir_indicator_show_history (GnomeCmdDirIndicator *indicator)
{
    if (indicator->priv->dir_history_popup) return;

    indicator->priv->dir_history_popup = gtk_menu_new ();
    g_object_ref (indicator->priv->dir_history_popup);
    g_object_set_data_full (G_OBJECT (indicator), "dir_history_popup", indicator->priv->dir_history_popup, g_object_unref);
    g_signal_connect (indicator->priv->dir_history_popup, "hide", G_CALLBACK (on_dir_history_popup_hide), indicator);

    GnomeCmdCon *con = indicator->priv->fs->get_connection();
    History *history = gnome_cmd_con_get_dir_history (con);

    for (GList *l=history->ents; l; l=l->next)
    {
        gchar *path = (gchar *) l->data;
        add_menu_item (indicator,
                       GTK_MENU_SHELL (indicator->priv->dir_history_popup),
                       path,
                       GTK_SIGNAL_FUNC (on_dir_history_item_selected),
                       path);
    }

    gtk_menu_popup (GTK_MENU (indicator->priv->dir_history_popup),
                    nullptr,
                    nullptr,
                    (GtkMenuPositionFunc) get_popup_pos,
                    indicator,
                    0,
                    gtk_get_current_event_time());

    gint w = -1;

    GtkAllocation indicator_allocation;
    gtk_widget_get_allocation (GTK_WIDGET (indicator), &indicator_allocation);

    if (indicator_allocation.width > 100)
        w = indicator_allocation.width;

    gtk_widget_set_size_request (indicator->priv->dir_history_popup, w, -1);
}


static void on_bookmarks_add_current (GtkMenuItem *item, GnomeCmdDirIndicator *indicator)
{
    gnome_cmd_bookmark_add_current (indicator->priv->fs->get_directory());
}


static void on_bookmarks_manage (GtkMenuItem *item, GnomeCmdDirIndicator *indicator)
{
    bookmarks_edit (nullptr, nullptr);
}


void gnome_cmd_dir_indicator_show_bookmarks (GnomeCmdDirIndicator *indicator)
{
    if (indicator->priv->bookmark_popup) return;

    indicator->priv->bookmark_popup = gtk_menu_new ();
    g_object_ref (indicator->priv->bookmark_popup);
    g_object_set_data_full (G_OBJECT (indicator), "bookmark_popup", indicator->priv->bookmark_popup, g_object_unref);
    g_signal_connect (indicator->priv->bookmark_popup, "hide", G_CALLBACK (on_bookmark_popup_hide), indicator);

    GnomeCmdCon *con = indicator->priv->fs->get_connection();
    GnomeCmdBookmarkGroup *group = gnome_cmd_con_get_bookmarks (con);

    for (GList *l = group->bookmarks; l; l = l->next)
    {
        auto bm = static_cast<GnomeCmdBookmark*> (l->data);
        add_menu_item (indicator,
                       GTK_MENU_SHELL (indicator->priv->bookmark_popup),
                       bm->name,
                       GTK_SIGNAL_FUNC (on_bookmark_item_selected),
                       bm);
    }

    add_menu_item (indicator, GTK_MENU_SHELL (indicator->priv->bookmark_popup), nullptr, nullptr, indicator);
    add_menu_item (indicator, GTK_MENU_SHELL (indicator->priv->bookmark_popup), _("Add current dir"), GTK_SIGNAL_FUNC (on_bookmarks_add_current), indicator);
    add_menu_item (indicator, GTK_MENU_SHELL (indicator->priv->bookmark_popup), _("Manage bookmarks…"), GTK_SIGNAL_FUNC (on_bookmarks_manage), indicator);

    gtk_menu_popup (GTK_MENU (indicator->priv->bookmark_popup),
                    nullptr,
                    nullptr,
                    (GtkMenuPositionFunc) get_popup_pos,
                    indicator,
                    0,
                    gtk_get_current_event_time());

    gint w = -1;

    GtkAllocation indicator_allocation;
    gtk_widget_get_allocation (GTK_WIDGET (indicator), &indicator_allocation);

    if (indicator_allocation.width > 100)
        w = indicator_allocation.width;

    gtk_widget_set_size_request (indicator->priv->bookmark_popup, w, -1);
}


static void init (GnomeCmdDirIndicator *indicator)
{
    GtkWidget *hbox, *arrow, *bbox;

    indicator->priv = g_new0 (GnomeCmdDirIndicatorPrivate, 1);
    // below assignments are not necessary any longer due to above g_new0()
    //  indicator->priv->dir_history_popup = nullptr;
    //  indicator->priv->bookmark_popup = nullptr;
    //  indicator->priv->history_is_popped = FALSE;
    //  indicator->priv->slashCharPosition = nullptr;
    //  indicator->priv->slashPixelPosition = nullptr;
    //  indicator->priv->numPositions = 0;

    // create the directory label and its event box
    indicator->priv->event_box = gtk_event_box_new ();
    g_object_ref (indicator->priv->event_box);
    g_signal_connect_swapped (indicator->priv->event_box, "motion-notify-event", G_CALLBACK (on_dir_indicator_motion), indicator);
    g_signal_connect_swapped (indicator->priv->event_box, "leave-notify-event", G_CALLBACK (on_dir_indicator_leave), indicator);
    gtk_widget_set_events (indicator->priv->event_box, GDK_POINTER_MOTION_MASK);

    gtk_widget_show (indicator->priv->event_box);

    indicator->priv->label = create_label (GTK_WIDGET (indicator), "not initialized");
    gtk_container_add (GTK_CONTAINER (indicator->priv->event_box), indicator->priv->label);

    // create the history popup button
    indicator->priv->history_button = gtk_button_new ();
    gtk_widget_set_can_focus (indicator->priv->history_button, FALSE);
    g_object_ref (indicator->priv->history_button);
    gtk_button_set_relief (GTK_BUTTON (indicator->priv->history_button), GTK_RELIEF_NONE);
    g_object_set_data_full (G_OBJECT (indicator), "button", indicator->priv->history_button, g_object_unref);
    gtk_widget_show (indicator->priv->history_button);

    arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
    g_object_ref (arrow);
    g_object_set_data_full (G_OBJECT (indicator), "arrow", arrow, g_object_unref);
    gtk_widget_show (arrow);
    gtk_container_add (GTK_CONTAINER (indicator->priv->history_button), arrow);

    // create the bookmark popup button
    indicator->priv->bookmark_button = create_styled_pixmap_button (nullptr, IMAGE_get_gnome_cmd_pixmap (PIXMAP_BOOKMARK));
    gtk_widget_set_can_focus (indicator->priv->bookmark_button, FALSE);
    gtk_button_set_relief (GTK_BUTTON (indicator->priv->bookmark_button), GTK_RELIEF_NONE);
    g_object_set_data_full (G_OBJECT (indicator), "button", indicator->priv->bookmark_button, g_object_unref);
    gtk_widget_show (indicator->priv->bookmark_button);

    // pack
    hbox = create_hbox (GTK_WIDGET (indicator), FALSE, 10);
    gtk_container_add (GTK_CONTAINER (indicator), hbox);
    gtk_box_pack_start (GTK_BOX (hbox), indicator->priv->event_box, TRUE, TRUE, 0);
    bbox = create_hbox (GTK_WIDGET (indicator), FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (bbox), indicator->priv->bookmark_button, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (bbox), indicator->priv->history_button, FALSE, FALSE, 0);

    g_signal_connect (indicator->priv->history_button, "clicked", G_CALLBACK (on_history_button_clicked), indicator);
    g_signal_connect (indicator->priv->bookmark_button, "clicked", G_CALLBACK (on_bookmark_button_clicked), indicator);
}


/***********************************
 * Public functions
 ***********************************/
GtkType gnome_cmd_dir_indicator_get_type ()
{
    static GtkType type = 0;

    if (type == 0)
    {
        GtkTypeInfo info = {
            (gchar*) "GnomeCmdDirIndicator",
            sizeof(GnomeCmdDirIndicator),
            sizeof(GnomeCmdDirIndicatorClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ nullptr,
            /* reserved_2 */ nullptr,
            (GtkClassInitFunc) nullptr
        };

        type = gtk_type_unique (gtk_frame_get_type (), &info);
    }

    return type;
}


GtkWidget *gnome_cmd_dir_indicator_new (GnomeCmdFileSelector *fs)
{
    auto dir_indicator = static_cast<GnomeCmdDirIndicator*> (g_object_new (GNOME_CMD_TYPE_DIR_INDICATOR, nullptr));

    g_signal_connect (dir_indicator, "button-press-event", G_CALLBACK (on_dir_indicator_clicked), fs);

    dir_indicator->priv->fs = fs;

    return GTK_WIDGET (dir_indicator);
}


static void update_slash_positions(GnomeCmdDirIndicator *indicator, const gchar* path)
{
    g_free (indicator->priv->slashCharPosition);
    g_free (indicator->priv->slashPixelPosition);
    indicator->priv->numPositions = 0;
    indicator->priv->slashCharPosition = nullptr;
    indicator->priv->slashPixelPosition = nullptr;

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

    indicator->priv->numPositions = isUNC ? sepPositions->len :
                                    path_len==1 ? sepPositions->len+1 : sepPositions->len+2;
    indicator->priv->slashCharPosition = g_new (gint, indicator->priv->numPositions);
    indicator->priv->slashPixelPosition = g_new (gint, indicator->priv->numPositions);

    gint pos_idx = 0;

    if (!isUNC && path_len>1)
    {
        indicator->priv->slashCharPosition[pos_idx] = 1;
        //auto colonPtr = strchr(path, ':');
        //colonPtr = colonPtr ? colonPtr : path;
        //auto startLength = (int)(colonPtr - path) + 1; // +1 as we want to ignore "host:"
        //indicator->priv->slashPixelPosition[pos_idx++] = get_string_pixel_size (path, startLength);
        indicator->priv->slashPixelPosition[pos_idx++] = get_string_pixel_size (path, 1);
    }

    for (guint ii = isUNC ? 1 : 0; ii < sepPositions->len; ii++)
    {
        indicator->priv->slashCharPosition[pos_idx] = g_array_index (sepPositions, gint, ii);
        indicator->priv->slashPixelPosition[pos_idx++] = get_string_pixel_size (path, g_array_index (sepPositions, gint, ii)+1);
    }

    if (indicator->priv->numPositions>0)
    {
        indicator->priv->slashCharPosition[pos_idx] = path_len;
        indicator->priv->slashPixelPosition[pos_idx] = get_string_pixel_size (path, path_len);
    }

    g_array_free (sepPositions, TRUE);
}


void gnome_cmd_dir_indicator_set_dir (GnomeCmdDirIndicator *indicator, GnomeCmdDir *dir)
{
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
    gtk_label_set_markup (GTK_LABEL (indicator->priv->label), monoIndicatorString);
    g_free (monoIndicatorString);
    update_markup (indicator, -1);

    update_slash_positions(indicator, indicatorString);
    g_free(indicatorString);
    g_free(path);
}


void gnome_cmd_dir_indicator_set_active (GnomeCmdDirIndicator *indicator, gboolean active)
{
    switch (active)
    {
        case true:
        {
            auto tmpLabelText = g_strdup (gtk_label_get_text (GTK_LABEL (indicator->priv->label)));
            gchar *result = g_strdup_printf("<span font_family=\"monospace\" underline=\"low\">%s</span>", tmpLabelText);
            gtk_label_set_markup (GTK_LABEL (indicator->priv->label), result);
            g_free (tmpLabelText);
            break;
        }
        case false:
        {
            auto tmpLabelText = g_strdup (gtk_label_get_text (GTK_LABEL (indicator->priv->label)));
            gchar *result = g_strdup_printf("<span font_family=\"monospace\">%s</span>", tmpLabelText);
            gtk_label_set_markup (GTK_LABEL (indicator->priv->label), result);
            g_free (tmpLabelText);
            break;
        }
    }
}

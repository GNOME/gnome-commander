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

using namespace std;


struct GnomeCmdDirIndicatorPrivate
{
    GtkWidget *label;
    GtkWidget *history_button;
    GtkWidget *bookmark_button;
    GnomeCmdFileSelector *fs;
    int *slashCharPosition;
    int *slashPixelPosition;
    int numPositions;
};


enum {
    NAVIGATE,
    LAST_SIGNAL
};


static guint signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE_WITH_PRIVATE (GnomeCmdDirIndicator, gnome_cmd_dir_indicator, GTK_TYPE_FRAME)


static void select_path (GSimpleAction *action, GVariant *parameter, gpointer user_data);
static void add_bookmark (GSimpleAction *action, GVariant *parameter, gpointer user_data);


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

    signals[NAVIGATE] =
        g_signal_new ("navigate",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdDirIndicatorClass, navigate),
            NULL, NULL, NULL,
            G_TYPE_NONE,
            2, G_TYPE_STRING, G_TYPE_BOOLEAN);
}


/*******************************
 * Event handlers
 *******************************/
static void on_dir_indicator_clicked (GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (user_data));
    auto indicator = static_cast<GnomeCmdDirIndicator*>(user_data);
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    if (n_press != 1)
        return;

    guint button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));

    const gchar *labelText = gtk_label_get_text (GTK_LABEL (priv->label));
    gchar *chTo = nullptr;
    for (gint slashIdx = 0; slashIdx < priv->numPositions; ++slashIdx)
    {
        if (x < priv->slashPixelPosition[slashIdx])
        {
            chTo = g_strndup (labelText, priv->slashCharPosition[slashIdx]);
            break;
        }
    }
    if (chTo == nullptr)
        return;

    if (button == 1 || button == 2)
    {
        gchar *colonPtr = strchr(chTo, ':');
        gchar *chToDir = colonPtr != nullptr ? colonPtr + 1 : chTo;

        gboolean new_tab = button == 2 || get_modifiers_state() & GDK_CONTROL_MASK;

        g_signal_emit (indicator, signals[NAVIGATE], 0, chToDir, new_tab);
    }
    else if (button == 3)
    {
        auto clipboard = gtk_widget_get_clipboard (GTK_WIDGET (indicator));
        gdk_clipboard_set_text (clipboard, chTo);
    }

    g_free (chTo);
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


static void on_dir_indicator_motion (GtkEventControllerMotion *controller, double x, double y, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (user_data));
    auto indicator = static_cast<GnomeCmdDirIndicator*>(user_data);
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    if (priv->slashCharPosition == nullptr)
        return;
    if (priv->slashPixelPosition == nullptr)
        return;

    // find out where in the label the pointer is at
    for (gint i=0; i < priv->numPositions; i++)
    {
        if (x < priv->slashPixelPosition[i])
        {
            // underline the part that is selected
            GdkCursor *cursor = gdk_cursor_new_from_name ("pointer", nullptr);
            gtk_widget_set_cursor (GTK_WIDGET (indicator), cursor);
            g_object_unref (cursor);

            update_markup (indicator, i);

            return;
        }
    }

    // clear underline, cursor=pointer
    update_markup (indicator, -1);
    gtk_widget_set_cursor (GTK_WIDGET (indicator), nullptr);
}


static void on_dir_indicator_leave (GtkEventControllerMotion *controller, gpointer user_data)
{
    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (user_data));
    auto indicator = static_cast<GnomeCmdDirIndicator*>(user_data);

    // clear underline, cursor=pointer
    update_markup (indicator, -1);
    gtk_widget_set_cursor (GTK_WIDGET (indicator), nullptr);
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

    const gchar *path;
    g_variant_get (parameter, "s", &path);
    g_return_if_fail (path != nullptr);

    GdkModifierType mask = get_modifiers_state();
    gboolean new_tab = mask & GDK_CONTROL_MASK;

    g_signal_emit (indicator, signals[NAVIGATE], 0, path, new_tab);
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

    GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
    gtk_widget_set_parent (popover, GTK_WIDGET (indicator));
    gtk_popover_set_position (GTK_POPOVER (popover), GTK_POS_BOTTOM);

    GtkAllocation indicator_allocation;
    gtk_widget_get_allocation (GTK_WIDGET (indicator), &indicator_allocation);

    if (indicator_allocation.width > 100)
        gtk_widget_set_size_request (popover, indicator_allocation.width, -1);

    gtk_popover_popup (GTK_POPOVER (popover));
}


extern "C" void gnome_cmd_bookmark_add_current (GnomeCmdMainWin *main_win, GnomeCmdDir *dir);

static void add_bookmark (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    auto indicator = static_cast<GnomeCmdDirIndicator*> (user_data);
    g_return_if_fail (GNOME_CMD_IS_DIR_INDICATOR (indicator));
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    gnome_cmd_bookmark_add_current (main_win, priv->fs->get_directory());
}


void gnome_cmd_dir_indicator_show_bookmarks (GnomeCmdDirIndicator *indicator)
{
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (indicator));

    GnomeCmdCon *con = priv->fs->get_connection();
    GListModel *bookmarks = gnome_cmd_con_get_bookmarks (con);

    GMenu *bookmarks_section = g_menu_new ();
    for (guint i = 0; i < g_list_model_get_n_items (bookmarks); ++i)
    {
        auto bookmark = static_cast<GnomeCmdBookmark*> (g_list_model_get_item(bookmarks, i));

        gchar *name = gnome_cmd_bookmark_get_name (bookmark);
        gchar *path = gnome_cmd_bookmark_get_path (bookmark);

        GMenuItem *item = g_menu_item_new (name, nullptr);
        g_menu_item_set_action_and_target (item, "indicator.select-path", "s", path);
        g_menu_append_item (bookmarks_section, item);

        g_free (name);
        g_free (path);
    }

    GMenu *manage_section = g_menu_new ();
    g_menu_append (manage_section, _("Add current dir"), "indicator.add-bookmark");
    g_menu_append (manage_section, _("Manage bookmarks…"), "win.bookmarks-edit");

    GMenu *menu = g_menu_new ();
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (bookmarks_section));
    g_menu_append_section (menu, nullptr, G_MENU_MODEL (manage_section));

    GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
    gtk_widget_set_parent (popover, GTK_WIDGET (indicator));
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

    // create the directory label
    priv->label = create_label (GTK_WIDGET (indicator), "not initialized");
    GtkEventController* motion_controller = gtk_event_controller_motion_new ();
    gtk_widget_add_controller (GTK_WIDGET (priv->label), GTK_EVENT_CONTROLLER (motion_controller));
    gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (motion_controller), GTK_PHASE_CAPTURE); // TODO: test if this is still needed in Gtk4
    g_signal_connect (motion_controller, "enter", G_CALLBACK (on_dir_indicator_motion), indicator);
    g_signal_connect (motion_controller, "motion", G_CALLBACK (on_dir_indicator_motion), indicator);
    g_signal_connect (motion_controller, "leave", G_CALLBACK (on_dir_indicator_leave), indicator);

    GtkWidget *sw = gtk_scrolled_window_new ();
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), priv->label);
    gtk_widget_show (sw);

    // create the history popup button
    priv->history_button = gtk_button_new_from_icon_name ("gnome-commander-down");
    gtk_widget_set_can_focus (priv->history_button, FALSE);
    gtk_button_set_has_frame (GTK_BUTTON (priv->history_button), FALSE);

    // create the bookmark popup button
    priv->bookmark_button = gtk_button_new_from_icon_name ("gnome-commander-bookmark-outline");
    gtk_widget_set_can_focus (priv->bookmark_button, FALSE);
    gtk_button_set_has_frame (GTK_BUTTON (priv->bookmark_button), FALSE);

    // pack
    hbox = create_hbox (GTK_WIDGET (indicator), FALSE, 10);
    gtk_frame_set_child (GTK_FRAME (indicator), hbox);
    gtk_widget_set_hexpand (sw, TRUE);
    gtk_box_append (GTK_BOX (hbox), sw);
    bbox = create_hbox (GTK_WIDGET (indicator), FALSE, 0);
    gtk_box_append (GTK_BOX (hbox), bbox);
    gtk_box_append (GTK_BOX (bbox), priv->bookmark_button);
    gtk_box_append (GTK_BOX (bbox), priv->history_button);

    g_signal_connect (priv->history_button, "clicked", G_CALLBACK (on_history_button_clicked), indicator);
    g_signal_connect (priv->bookmark_button, "clicked", G_CALLBACK (on_bookmark_button_clicked), indicator);

    GSimpleActionGroup *action_group = g_simple_action_group_new ();
    static GActionEntry entries[] = {
        { "select-path", select_path, "s" },
        { "add-bookmark", add_bookmark }
    };
    g_action_map_add_action_entries (G_ACTION_MAP (action_group), entries, G_N_ELEMENTS (entries), indicator);

    gtk_widget_insert_action_group (GTK_WIDGET (indicator), "indicator", G_ACTION_GROUP (action_group));

    GtkGesture *button_gesture = gtk_gesture_click_new ();
    gtk_widget_add_controller (GTK_WIDGET (indicator), GTK_EVENT_CONTROLLER (button_gesture));
    gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (button_gesture), 0);
    g_signal_connect (button_gesture, "pressed", G_CALLBACK (on_dir_indicator_clicked), indicator);
}

/***********************************
 * Public functions
 ***********************************/

GtkWidget *gnome_cmd_dir_indicator_new (GnomeCmdFileSelector *fs)
{
    auto dir_indicator = static_cast<GnomeCmdDirIndicator*> (g_object_new (GNOME_CMD_TYPE_DIR_INDICATOR, nullptr));
    auto priv = static_cast<GnomeCmdDirIndicatorPrivate*>(gnome_cmd_dir_indicator_get_instance_private (dir_indicator));

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
    if (!gnome_cmd_file_is_local (GNOME_CMD_FILE (dir)))
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

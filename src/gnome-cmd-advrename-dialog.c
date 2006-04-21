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

#include <config.h>
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>
#include <errno.h>
#include "gnome-cmd-includes.h"
#include "gnome-cmd-advrename-dialog.h"
#include "gnome-cmd-advrename-lexer.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-clist.h"
#include "gnome-cmd-data.h"
#include "utils.h"


// static GdkColor black   = {0,0,0,0};
// static GdkColor red     = {0,0xffff,0,0};


typedef struct
{
    GnomeCmdFile *finfo;
    gchar *new_name;
} RenameEntry;


struct _GnomeCmdAdvrenameDialogPrivate
{
    GList *files;
    GList *entries;
    PatternEntry *sel_entry;
    AdvrenameDefaults *defaults;

    GtkWidget *pat_list;
    GtkWidget *res_list;
    GtkWidget *move_up_btn;
    GtkWidget *move_down_btn;
    GtkWidget *add_btn;
    GtkWidget *edit_btn;
    GtkWidget *remove_btn;
    GtkWidget *remove_all_btn;
    GtkWidget *templ_combo;
    GtkWidget *templ_entry;
};


static GnomeCmdDialogClass *parent_class = NULL;

guint advrename_dialog_default_pat_column_width[ADVRENAME_DIALOG_PAT_NUM_COLUMNS] = {150, 150, 30};
guint advrename_dialog_default_res_column_width[ADVRENAME_DIALOG_RES_NUM_COLUMNS] = {200, 200};


static void do_test (GnomeCmdAdvrenameDialog *dialog);


static void
free_data (GnomeCmdAdvrenameDialog *dialog)
{
    GList *tmp;

    gnome_cmd_file_list_free (dialog->priv->files);

    for (tmp = dialog->priv->entries; tmp; tmp = tmp->next)
    {
        RenameEntry *entry = (RenameEntry*)tmp->data;
        g_free (entry->new_name);
        g_free (entry);
    }
    g_list_free (dialog->priv->entries);

    g_free (dialog->priv);
    dialog->priv = NULL;
}


static void
format_entry (PatternEntry *entry, gchar **text)
{
    text[0] = entry->from;
    text[1] = entry->to;
    text[2] = entry->case_sens?_("Yes"):_("No");
    text[3] = NULL;
}


static RenameEntry *
rename_entry_new (void)
{
    return g_new0 (RenameEntry, 1);
}


static PatternEntry *
get_pattern_entry_from_row (GnomeCmdAdvrenameDialog *dialog, gint row)
{
    return gtk_clist_get_row_data (GTK_CLIST (dialog->priv->pat_list), row);
}


static gint
get_row_from_rename_entry (GnomeCmdAdvrenameDialog *dialog, RenameEntry *entry)
{
    return gtk_clist_find_row_from_data (GTK_CLIST (dialog->priv->res_list), entry);
}


static void
add_rename_entry (GnomeCmdAdvrenameDialog *dialog, GnomeCmdFile *finfo)
{
    gint row;
    gchar *text[3],
          *fname = get_utf8 (finfo->info->name);
    RenameEntry *entry = rename_entry_new ();

    entry->finfo = finfo;

    text[0] = fname;
    text[1] = NULL;
    text[2] = NULL;

    row = gtk_clist_append (GTK_CLIST (dialog->priv->res_list), text);
    gtk_clist_set_row_data (GTK_CLIST (dialog->priv->res_list), row, entry);
    dialog->priv->entries = g_list_append (dialog->priv->entries, entry);

    g_free (fname);
}


static void update_move_buttons (GnomeCmdAdvrenameDialog *dialog, int row)
{
    if (row == 0) {
        gtk_widget_set_sensitive (dialog->priv->move_up_btn, FALSE);
        gtk_widget_set_sensitive (dialog->priv->move_down_btn, g_list_length (dialog->priv->defaults->patterns) > 1);
    }
    else if (row == g_list_length (dialog->priv->defaults->patterns) - 1) {
        gtk_widget_set_sensitive (dialog->priv->move_down_btn, FALSE);
        gtk_widget_set_sensitive (dialog->priv->move_up_btn, g_list_length (dialog->priv->defaults->patterns) > 1);
    }
    else {
        gtk_widget_set_sensitive (dialog->priv->move_up_btn, TRUE);
        gtk_widget_set_sensitive (dialog->priv->move_down_btn, TRUE);
    }
}


static void update_remove_all_button (GnomeCmdAdvrenameDialog *dialog)
{
    gtk_widget_set_sensitive (dialog->priv->remove_all_btn, dialog->priv->defaults->patterns != NULL);
}


static void
add_pattern_entry (GnomeCmdAdvrenameDialog *dialog, PatternEntry *entry)
{
    gint row;
    gchar *text[3];

    if (!entry) return;

    format_entry (entry, text);

    row = gtk_clist_append (GTK_CLIST (dialog->priv->pat_list), text);
    //gtk_clist_set_foreground(GTK_CLIST (dialog->priv->pat_list), row, entry->malformed_pattern ? &red : &black);
    gtk_clist_set_row_data (GTK_CLIST (dialog->priv->pat_list), row, entry);
    update_move_buttons (dialog, GTK_CLIST (dialog->priv->pat_list)->focus_row);
}


static gchar*
update_entry (PatternEntry *entry,
              GnomeCmdStringDialog *string_dialog,
              const gchar **values)
{
    GtkWidget *case_check;

    if (!values[0])
        return g_strdup (_("Invalid source pattern"));

    case_check = lookup_widget (GTK_WIDGET (string_dialog), "case_check");

    if (entry->from) g_free (entry->from);
    if (entry->to) g_free (entry->to);
    entry->from = g_strdup (values[0]);
    entry->to = g_strdup (values[1]);
    entry->case_sens = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (case_check));

    return NULL;
}


static gboolean
on_add_rule_dialog_ok (GnomeCmdStringDialog *string_dialog,
                       const gchar **values,
                       GnomeCmdAdvrenameDialog *dialog)
{
    gchar *error_desc;
    PatternEntry *entry = g_new0 (PatternEntry, 1);

    error_desc = update_entry (entry, string_dialog, values);
    if (error_desc != NULL) {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, error_desc);
        return FALSE;
    }

    add_pattern_entry (dialog, entry);
    dialog->priv->defaults->patterns = g_list_append (dialog->priv->defaults->patterns, entry);
    update_remove_all_button (dialog);
    do_test (dialog);

    return TRUE;
}


static gboolean
on_edit_rule_dialog_ok (GnomeCmdStringDialog *string_dialog,
                        const gchar **values,
                        GnomeCmdAdvrenameDialog *dialog)
{
    GtkWidget *pat_list = dialog->priv->pat_list;
    gint row = GTK_CLIST (pat_list)->focus_row;
    PatternEntry *entry = g_list_nth_data (dialog->priv->defaults->patterns, row);
    gchar *text[3], *error_desc;

    g_return_val_if_fail (entry != NULL, TRUE);

    error_desc = update_entry (entry, string_dialog, values);
    if (error_desc != NULL) {
        gnome_cmd_string_dialog_set_error_desc (string_dialog, error_desc);
        return FALSE;
    }

    format_entry (entry, text);
    //gtk_clist_set_foreground(GTK_CLIST (pat_list), row, entry->malformed_pattern ? &red : &black);
    gtk_clist_set_text (GTK_CLIST (pat_list), row, 0, text[0]);
    gtk_clist_set_text (GTK_CLIST (pat_list), row, 1, text[1]);
    gtk_clist_set_text (GTK_CLIST (pat_list), row, 2, text[2]);
    gtk_clist_set_text (GTK_CLIST (pat_list), row, 3, text[3]);
    update_remove_all_button (dialog);
    do_test (dialog);

    return TRUE;
}


static GtkWidget*
create_rule_dialog (GnomeCmdAdvrenameDialog *parent_dialog,
                    const gchar *title,
                    GnomeCmdStringDialogCallback on_ok_func,
                    PatternEntry *entry)
{
    const gchar *labels[] = {_("Replace this:"), _("With this")};
    GtkWidget *dialog;
    GtkWidget *case_check;

    dialog = gnome_cmd_string_dialog_new (title, labels, 2, on_ok_func, parent_dialog);
    gtk_widget_ref (dialog);
    gtk_object_set_data_full (GTK_OBJECT (parent_dialog),
                              "rule-dialog", dialog,
                              (GtkDestroyNotify)gtk_widget_unref);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 0, entry?entry->from:"");
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dialog), 1, entry?entry->to:"");

    case_check = create_check (dialog, _("Case sensitive matching"), "case_check");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (case_check), entry?entry->case_sens:FALSE);
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), case_check);

    gtk_widget_show (dialog);

    return dialog;
}


static gchar *
apply_one_pattern (gchar *in, PatternEntry *entry, int eflags)
{
    regex_t re_exp;
    regmatch_t re_match_info;
    gchar *out = NULL;

    if (!in || !entry) return NULL;

    if (regcomp (&re_exp, entry->from, (entry->case_sens ? REG_EXTENDED : REG_EXTENDED|REG_ICASE) != 0))  
        entry->malformed_pattern = TRUE; 
    else
    {
        if (regexec (&re_exp, in, 1, &re_match_info, eflags) == 0) 
        {
            if (strcmp (entry->from, "$") == 0) 
                out = g_strconcat(in, entry->to, NULL);
            else 
                if (strcmp (entry->from, "^") == 0)
                    out = g_strconcat(entry->to, in, NULL);
                else
                {
                    gint match_size = re_match_info.rm_eo - re_match_info.rm_so;
                    
                    if (match_size)
                    {
                        gchar **v;
                        gchar *match = g_malloc (match_size+1);
                        gchar *tail;
                        
                        g_utf8_strncpy (match, in+re_match_info.rm_so, match_size);
                        match[match_size] = '\0';
                        v = g_strsplit (in, match, 2);
                        tail = apply_one_pattern (v[1], entry, eflags|REG_NOTBOL|REG_NOTEOL);
                        out = g_strjoin (NULL, v[0], entry->to, tail, NULL);
                        g_free (tail);
                        g_free (match);
                        g_strfreev (v);
                    }
                }
        }
    }

    regfree (&re_exp);

    return out ? out : g_strdup (in);
}


static gchar *
create_new_name (const gchar *name, GList *patterns)
{
    gchar *tmp = NULL;
    gchar *new_name = g_strdup (name);

    for ( ; patterns; patterns = patterns->next)
    {
        PatternEntry *entry = (PatternEntry*)patterns->data;

        tmp = new_name;
    
        new_name = apply_one_pattern (tmp, entry, 0);
        
        g_free (tmp);
    }

    return new_name;
}


static void update_new_names (GnomeCmdAdvrenameDialog *dialog)
{
    const gchar *templ_string = gtk_entry_get_text (GTK_ENTRY (dialog->priv->templ_entry));
    GList *tmp;

    gnome_cmd_advrename_reset_counter (
        dialog->priv->defaults->counter_start,
        dialog->priv->defaults->counter_precision,
        dialog->priv->defaults->counter_increment);
    gnome_cmd_advrename_parse_fname (templ_string);

    for ( tmp = dialog->priv->entries; tmp; tmp = tmp->next ) 
	{
        gchar fname[256];
        RenameEntry *entry = (RenameEntry*)tmp->data;
        GList *patterns = dialog->priv->defaults->patterns;
        
        gnome_cmd_advrename_gen_fname (fname, sizeof (fname), entry->finfo);

        entry->new_name = create_new_name (fname, patterns);
    }
}


static void redisplay_new_names (GnomeCmdAdvrenameDialog *dialog)
{
    GList *tmp;

    for ( tmp = dialog->priv->entries; tmp; tmp = tmp->next ) 
	{
        RenameEntry *entry = (RenameEntry*)tmp->data;

        gtk_clist_set_text (GTK_CLIST (dialog->priv->res_list),
                            get_row_from_rename_entry (dialog, entry),
                            1,
                            entry->new_name);
    }
}


static void
change_names (GnomeCmdAdvrenameDialog *dialog)
{
    GList *tmp;

    for ( tmp = dialog->priv->entries; tmp; tmp = tmp->next ) 
	{
        RenameEntry *entry = (RenameEntry*)tmp->data;

        if (strcmp (entry->finfo->info->name, entry->new_name) != 0)
            gnome_cmd_file_rename (entry->finfo, entry->new_name);
    }
}


static void on_rule_add (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    create_rule_dialog (dialog, _("New Rule"), (GnomeCmdStringDialogCallback)on_add_rule_dialog_ok, NULL);
}


static void on_rule_edit (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    create_rule_dialog (dialog, _("Edit Rule"), (GnomeCmdStringDialogCallback)on_edit_rule_dialog_ok, dialog->priv->sel_entry);
}


static void on_rule_remove (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    GtkWidget *pat_list = dialog->priv->pat_list;
    PatternEntry *entry = get_pattern_entry_from_row (dialog, GTK_CLIST (pat_list)->focus_row);

    if (entry) {
        dialog->priv->defaults->patterns = g_list_remove (dialog->priv->defaults->patterns, entry);
        gtk_clist_remove (GTK_CLIST (pat_list), GTK_CLIST (pat_list)->focus_row);
        update_remove_all_button (dialog);
        do_test (dialog);
    }
}


static void on_rule_remove_all (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    gtk_clist_clear (GTK_CLIST (dialog->priv->pat_list));
    g_list_free (dialog->priv->defaults->patterns);
    dialog->priv->defaults->patterns = NULL;
    update_remove_all_button (dialog);
    do_test (dialog);
}


static void
on_pat_list_scroll_vertical (GtkCList *clist,
                             GtkScrollType scroll_type,
                             gfloat position,
                             GnomeCmdAdvrenameDialog *dialog)
{
    gtk_clist_select_row (clist, clist->focus_row, 0);
}


static void on_rule_selected (GtkCList *list, gint row, gint column,
                              GdkEventButton *event, GnomeCmdAdvrenameDialog *dialog)
{
    gtk_widget_set_sensitive (dialog->priv->remove_btn, TRUE);
    gtk_widget_set_sensitive (dialog->priv->edit_btn, TRUE);
    update_move_buttons (dialog, row);
    dialog->priv->sel_entry = (PatternEntry*)g_list_nth_data (dialog->priv->defaults->patterns, row);
}


static void on_rule_unselected (GtkCList *list, gint row, gint column,
                                GdkEventButton *event, GnomeCmdAdvrenameDialog *dialog)
{
    gtk_widget_set_sensitive (dialog->priv->remove_btn, FALSE);
    gtk_widget_set_sensitive (dialog->priv->edit_btn, FALSE);
    gtk_widget_set_sensitive (dialog->priv->move_up_btn, FALSE);
    gtk_widget_set_sensitive (dialog->priv->move_down_btn, FALSE);
    dialog->priv->sel_entry = NULL;
}


static void on_rule_move_up (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    GtkCList *clist = GTK_CLIST (dialog->priv->pat_list);

    if (clist->focus_row >= 1) {
        gtk_clist_row_move (clist, clist->focus_row, clist->focus_row-1);
        update_move_buttons (dialog, clist->focus_row);
        do_test (dialog);
    }
}


static void on_rule_move_down (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    GtkCList *clist = GTK_CLIST (dialog->priv->pat_list);

    if (clist->focus_row >= 0) {
        gtk_clist_row_move (clist, clist->focus_row, clist->focus_row+1);
        update_move_buttons (dialog, clist->focus_row);
        do_test (dialog);
    }
}


static void
on_rule_moved (GtkCList *clist, gint arg1, gint arg2,
               GnomeCmdAdvrenameDialog *dialog)
{
    GList *pats = dialog->priv->defaults->patterns;
    gpointer data;

    if (!pats
        || MAX (arg1, arg2) >= g_list_length (pats)
        || MIN (arg1, arg2) < 0
        || arg1 == arg2)
        return;

    data = g_list_nth_data (pats, arg1);
    pats = g_list_remove (pats, data);

    pats = g_list_insert (pats, data, arg2);

    dialog->priv->defaults->patterns =  pats;
}


static void
save_settings (GnomeCmdAdvrenameDialog *dialog)
{
    const gchar *template = gtk_entry_get_text (GTK_ENTRY (dialog->priv->templ_entry));
    history_add (dialog->priv->defaults->templates, g_strdup (template));
}


static void on_ok (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    update_new_names (dialog);
    change_names (dialog);

    save_settings (dialog);
    free_data (dialog);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void on_cancel (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    save_settings (dialog);
    free_data (dialog);
    gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void on_reset (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    gtk_entry_set_text (GTK_ENTRY (dialog->priv->templ_entry), "$N");
    on_rule_remove_all (NULL, dialog);
    dialog->priv->defaults->counter_start = 1;
    dialog->priv->defaults->counter_precision = 1;
    dialog->priv->defaults->counter_increment = 1;
}


static gboolean
on_dialog_keypress (GnomeCmdAdvrenameDialog *dialog, GdkEventKey *event)
{
    if (event->keyval == GDK_Escape) {
        gtk_widget_destroy (GTK_WIDGET (dialog));
        return TRUE;
    }

    return FALSE;
}


static void do_test (GnomeCmdAdvrenameDialog *dialog)
{
    update_new_names (dialog);
    redisplay_new_names (dialog);
}


static void
on_templ_entry_changed (GtkEntry *entry,
                        GnomeCmdAdvrenameDialog *dialog)
{
    if (dialog->priv->defaults->auto_update)
        do_test (dialog);
}


static gboolean
on_templ_entry_keypress (GtkEntry *entry, GdkEventKey *event,
                         GnomeCmdAdvrenameDialog *dialog)
{
    if (event->keyval == GDK_Return) {
        do_test (dialog);
        return TRUE;
    }

    return FALSE;
}


static gboolean
on_template_options_ok (GnomeCmdStringDialog *string_dialog,
                        const gchar **values,
                        GnomeCmdAdvrenameDialog *dialog)
{
    guint start, precision, inc;

    if (sscanf (values[0], "%u", &start) != 1)
        return TRUE;

    if (sscanf (values[1], "%u", &inc) != 1)
        return TRUE;

    if (sscanf (values[2], "%u", &precision) != 1)
        return TRUE;

    dialog->priv->defaults->counter_start = start;
    dialog->priv->defaults->counter_increment = inc;
    dialog->priv->defaults->counter_precision = precision;
    dialog->priv->defaults->auto_update = gtk_toggle_button_get_active (
        GTK_TOGGLE_BUTTON (lookup_widget (GTK_WIDGET (string_dialog), "auto-update-check")));

    do_test (dialog);

    return TRUE;
}


static void
on_template_options_clicked (GtkButton *button,
                             GnomeCmdAdvrenameDialog *dialog)
{
    GtkWidget *dlg;
    GtkWidget *check;
    const gchar *labels[] = {
        _("Counter start value:"),
        _("Counter increment:"),
        _("Counter minimum digit count:")
    };
    gchar *s;

    dlg = gnome_cmd_string_dialog_new (
        _("Template Options"), labels, 3,
        (GnomeCmdStringDialogCallback)on_template_options_ok, dialog);
    gtk_widget_ref (dlg);

    check = create_check (GTK_WIDGET (dlg), _("Auto-update when the template is entered"), "auto-update-check");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), dialog->priv->defaults->auto_update);
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dlg), check);

    s = g_strdup_printf ("%d", dialog->priv->defaults->counter_start);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dlg), 0, s);
    g_free (s);
    s = g_strdup_printf ("%d", dialog->priv->defaults->counter_increment);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dlg), 1, s);
    g_free (s);
    s = g_strdup_printf ("%d", dialog->priv->defaults->counter_precision);
    gnome_cmd_string_dialog_set_value (GNOME_CMD_STRING_DIALOG (dlg), 2, s);
    g_free (s);

    gtk_widget_show (dlg);
}


static void
on_res_list_column_resize (GtkCList *clist, gint column, gint width, GnomeCmdAdvrenameDialog *dialog)
{
    advrename_dialog_default_res_column_width[column] = width;
}


static void
on_pat_list_column_resize (GtkCList *clist, gint column, gint width, GnomeCmdAdvrenameDialog *dialog)
{
    advrename_dialog_default_pat_column_width[column] = width;
}


static void
on_dialog_size_allocate (GtkWidget       *widget,
                         GtkAllocation   *allocation,
                         GnomeCmdAdvrenameDialog *dialog)
{
    dialog->priv->defaults->width  = allocation->width;
    dialog->priv->defaults->height = allocation->height;
}


/*******************************
 * Gtk class implementation
 *******************************/

static void
destroy (GtkObject *object)
{
    GnomeCmdAdvrenameDialog *dialog = GNOME_CMD_ADVRENAME_DIALOG (object);

    g_free (dialog->priv);

    if (GTK_OBJECT_CLASS (parent_class)->destroy)
        (*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
map (GtkWidget *widget)
{
    if (GTK_WIDGET_CLASS (parent_class)->map != NULL)
        GTK_WIDGET_CLASS (parent_class)->map (widget);
}


static void
class_init (GnomeCmdAdvrenameDialogClass *class)
{
    GtkObjectClass *object_class = GTK_OBJECT_CLASS (class);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

    parent_class = gtk_type_class (gnome_cmd_dialog_get_type ());
    object_class->destroy = destroy;
    widget_class->map = map;
}


static void
init (GnomeCmdAdvrenameDialog *in_dialog)
{
    GtkWidget *dialog;
    GtkWidget *vbox;
    GtkWidget *sw;
    GtkWidget *bbox;
    GtkWidget *hbox;
    GtkWidget *btn;
    GtkWidget *cat;
    GtkWidget *table;
    GtkAccelGroup *accel_group;
    GList *tmp;

    in_dialog->priv = g_new0 (GnomeCmdAdvrenameDialogPrivate, 1);

    in_dialog->priv->entries = NULL;
    in_dialog->priv->sel_entry = NULL;
    in_dialog->priv->defaults = gnome_cmd_data_get_advrename_defaults ();

    accel_group = gtk_accel_group_new ();

    dialog = GTK_WIDGET (in_dialog);
    gtk_object_set_data (GTK_OBJECT (dialog), "dialog", dialog);
    gtk_window_set_title (GTK_WINDOW (dialog), _("Advanced Rename Tool"));
    gtk_window_set_default_size (GTK_WINDOW (dialog),
                                 in_dialog->priv->defaults->width,
                                 in_dialog->priv->defaults->height);
    gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
    gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, TRUE, FALSE);
    gtk_signal_connect (GTK_OBJECT (dialog), "size-allocate",
                        GTK_SIGNAL_FUNC (on_dialog_size_allocate), dialog);


    /* Template stuff
     */
    hbox = create_hbox (dialog, FALSE, 12);
    cat = create_category (dialog, hbox, _("Template"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), cat);

    in_dialog->priv->templ_combo = create_combo (dialog);
    in_dialog->priv->templ_entry = GTK_COMBO (in_dialog->priv->templ_combo)->entry;
    gtk_signal_connect (GTK_OBJECT (in_dialog->priv->templ_entry),
                        "key-press-event", GTK_SIGNAL_FUNC (on_templ_entry_keypress),
                        dialog);
    gtk_signal_connect (GTK_OBJECT (in_dialog->priv->templ_entry),
                        "changed", GTK_SIGNAL_FUNC (on_templ_entry_changed),
                        dialog);
    gtk_box_pack_start (GTK_BOX (hbox), in_dialog->priv->templ_combo, TRUE, TRUE, 0);
    if (in_dialog->priv->defaults->templates->ents)
        gtk_combo_set_popdown_strings (
            GTK_COMBO (in_dialog->priv->templ_combo),
            in_dialog->priv->defaults->templates->ents);
    else
        gtk_entry_set_text (GTK_ENTRY (in_dialog->priv->templ_entry), "$N");

    bbox = create_hbuttonbox (dialog);
    gtk_box_pack_start (GTK_BOX (hbox), bbox, FALSE, TRUE, 0);

    btn = create_button (dialog, _("Options..."), GTK_SIGNAL_FUNC (on_template_options_clicked));
    gtk_container_add (GTK_CONTAINER (bbox), btn);


    /* Regex stuff
     */
    table = create_table (dialog, 2, 2);
    cat = create_category (dialog, table, _("Regex replacing"));
    gnome_cmd_dialog_add_category (GNOME_CMD_DIALOG (dialog), cat);

    sw = create_clist (dialog, "pat_list", 3, 16, GTK_SIGNAL_FUNC (on_rule_selected), GTK_SIGNAL_FUNC (on_rule_moved));
    gtk_table_attach (GTK_TABLE (table), sw, 0, 1, 0, 1, GTK_EXPAND|GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
    create_clist_column (sw, 0, advrename_dialog_default_pat_column_width[0], _("Replace this"));
    create_clist_column (sw, 1, advrename_dialog_default_pat_column_width[1], _("With this"));
    create_clist_column (sw, 2, advrename_dialog_default_pat_column_width[2], _("Case sens"));

    in_dialog->priv->pat_list = lookup_widget (GTK_WIDGET (sw), "pat_list");
    gtk_signal_connect (GTK_OBJECT (in_dialog->priv->pat_list),
                        "unselect-row",
                        GTK_SIGNAL_FUNC (on_rule_unselected),
                        dialog);

    bbox = create_hbuttonbox (dialog);
    table_add (table, bbox, 0, 1, GTK_EXPAND|GTK_FILL);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);

    in_dialog->priv->add_btn = create_button (dialog, _("_Add..."), GTK_SIGNAL_FUNC (on_rule_add));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->add_btn);

    in_dialog->priv->edit_btn = create_button (dialog, _("_Edit..."), GTK_SIGNAL_FUNC (on_rule_edit));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->edit_btn);
    gtk_widget_set_sensitive (GTK_WIDGET (in_dialog->priv->edit_btn), FALSE);

    in_dialog->priv->remove_btn = create_button (dialog, _("_Remove"), GTK_SIGNAL_FUNC (on_rule_remove));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->remove_btn);
    gtk_widget_set_sensitive (GTK_WIDGET (in_dialog->priv->remove_btn), FALSE);

    in_dialog->priv->remove_all_btn = create_button (dialog, _("Re_move All"), GTK_SIGNAL_FUNC (on_rule_remove_all));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->remove_all_btn);
    if (!in_dialog->priv->defaults->patterns)
        gtk_widget_set_sensitive (GTK_WIDGET (in_dialog->priv->remove_all_btn), FALSE);

    bbox = create_vbuttonbox (dialog);
    table_add (table, bbox, 1, 0, GTK_FILL);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);

    in_dialog->priv->move_up_btn = create_stock_button (dialog, GNOME_STOCK_BUTTON_UP, GTK_SIGNAL_FUNC (on_rule_move_up));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->move_up_btn);
    gtk_widget_set_sensitive (GTK_WIDGET (in_dialog->priv->move_up_btn), FALSE);

    in_dialog->priv->move_down_btn = create_stock_button (dialog, GNOME_STOCK_BUTTON_DOWN, GTK_SIGNAL_FUNC (on_rule_move_down));
    gtk_container_add (GTK_CONTAINER (bbox), in_dialog->priv->move_down_btn);
    gtk_widget_set_sensitive (GTK_WIDGET (in_dialog->priv->move_down_btn), FALSE);


    /* Result list stuff
     */
    vbox = create_vbox (dialog, FALSE, 0);
    cat = create_category (dialog, vbox, _("Result"));
    gnome_cmd_dialog_add_expanding_category (GNOME_CMD_DIALOG (dialog), cat);

    sw = create_clist (dialog, "res_list", 2, 16, NULL, NULL);
    gtk_container_add (GTK_CONTAINER (vbox), sw);
    create_clist_column (sw, 0, advrename_dialog_default_res_column_width[0], _("Current filenames"));
    create_clist_column (sw, 1, advrename_dialog_default_res_column_width[1], _("New filenames"));
    in_dialog->priv->res_list = lookup_widget (GTK_WIDGET (sw), "res_list");


    /* Dialog stuff
     */
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), _("Reset"), GTK_SIGNAL_FUNC (on_reset), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GNOME_STOCK_BUTTON_CANCEL, GTK_SIGNAL_FUNC (on_cancel), dialog);
    gnome_cmd_dialog_add_button (GNOME_CMD_DIALOG (dialog), GNOME_STOCK_BUTTON_OK, GTK_SIGNAL_FUNC (on_ok), dialog);

    gtk_widget_grab_focus (in_dialog->priv->pat_list);
    gtk_window_add_accel_group (GTK_WINDOW (dialog), accel_group);

    for ( tmp = in_dialog->priv->defaults->patterns; tmp; tmp = tmp->next )   
    {
        PatternEntry *entry = (PatternEntry*)tmp->data;
        add_pattern_entry (in_dialog, entry);
    }

    gtk_signal_connect (GTK_OBJECT (dialog), "key-press-event", GTK_SIGNAL_FUNC (on_dialog_keypress), dialog);
    gtk_signal_connect_after (GTK_OBJECT (in_dialog->priv->pat_list),
                              "scroll-vertical",
                              GTK_SIGNAL_FUNC (on_pat_list_scroll_vertical),
                              dialog);
    gtk_signal_connect_after (GTK_OBJECT (in_dialog->priv->pat_list),
                              "resize-column",
                              GTK_SIGNAL_FUNC (on_pat_list_column_resize),
                              dialog);
    gtk_signal_connect_after (GTK_OBJECT (in_dialog->priv->res_list),
                              "resize-column",
                              GTK_SIGNAL_FUNC (on_res_list_column_resize),
                              dialog);

    if (in_dialog->priv->defaults->patterns)
        gtk_clist_select_row (GTK_CLIST (in_dialog->priv->pat_list), 0, 0);
}


/***********************************
 * Public functions
 ***********************************/

GtkWidget*
gnome_cmd_advrename_dialog_new (GList *files)
{
    GnomeCmdAdvrenameDialog *dialog = gtk_type_new (gnome_cmd_advrename_dialog_get_type ());
    GList *tmp;

    dialog->priv->files = gnome_cmd_file_list_copy (files);
    
    for ( tmp = dialog->priv->files; tmp; tmp = tmp->next ) 
    {
        GnomeCmdFile *finfo = (GnomeCmdFile*)tmp->data;
        if (strcmp (finfo->info->name, "..") != 0)
            add_rename_entry (dialog, finfo);
    }

    do_test (dialog);

    return GTK_WIDGET (dialog);
}


GtkType
gnome_cmd_advrename_dialog_get_type         (void)
{
    static GtkType dlg_type = 0;

    if (dlg_type == 0)
    {
        GtkTypeInfo dlg_info =
        {
            "GnomeCmdAdvrenameDialog",
            sizeof (GnomeCmdAdvrenameDialog),
            sizeof (GnomeCmdAdvrenameDialogClass),
            (GtkClassInitFunc) class_init,
            (GtkObjectInitFunc) init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            (GtkClassInitFunc) NULL
        };

        dlg_type = gtk_type_unique (gnome_cmd_dialog_get_type (), &dlg_info);
    }
    return dlg_type;
}

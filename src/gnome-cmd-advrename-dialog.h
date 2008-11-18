/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2008 Piotr Eljasiak

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
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#ifndef __GNOME_CMD_ADVRENAME_DIALOG_H__
#define __GNOME_CMD_ADVRENAME_DIALOG_H__


#if !GLIB_CHECK_VERSION (2, 14, 0)
#include <regex.h>
#endif

#include "gnome-cmd-data.h"
#include "gnome-cmd-file-list.h"

#define GNOME_CMD_TYPE_ADVRENAME_DIALOG         (gnome_cmd_advrename_dialog_get_type())
#define GNOME_CMD_ADVRENAME_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GNOME_CMD_TYPE_ADVRENAME_DIALOG, GnomeCmdAdvrenameDialog))
#define GNOME_CMD_ADVRENAME_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GNOME_CMD_TYPE_ADVRENAME_DIALOG, GnomeCmdAdvrenameDialogClass))
#define GNOME_CMD_IS_ADVRENAME_DIALOG(obj)      (G_TYPE_INSTANCE_CHECK_TYPE ((obj), GNOME_CMD_TYPE_ADVRENAME_DIALOG)


GType gnome_cmd_advrename_dialog_get_type ();


struct GnomeCmdAdvrenameDialog
{
    GtkDialog parent;

    class Private;

    Private *priv;

    operator GtkWidget * ()             {  return GTK_WIDGET (this);  }
    operator GtkWindow * ()             {  return GTK_WINDOW (this);  }
    operator GtkDialog * ()             {  return GTK_DIALOG (this);  }

    void *operator new (size_t size)    {  return g_object_new (GNOME_CMD_TYPE_ADVRENAME_DIALOG, NULL);  }
    void operator delete (void *p)      {  g_free (p);  }

    enum {GCMD_RESPONSE_RESET=123};

    enum {COL_REGEX, COL_MALFORMED_REGEX, COL_PATTERN, COL_REPLACE, COL_MATCH_CASE, NUM_REGEX_COLS};
    enum {COL_FILE, COL_NAME, COL_NEW_NAME, COL_SIZE, COL_DATE, COL_RENAME_FAILED, NUM_FILE_COLS};

    GnomeCmdData::AdvrenameConfig &defaults;

    GtkTreeModel *files;

    class Regex
    {
#if GLIB_CHECK_VERSION (2, 14, 0)
        GRegex *re;
#else
        regex_t re;
        regmatch_t pmatch;
#endif
        gboolean malformed_pattern;

      public:

        std::string from;
        std::string to;
        gboolean case_sensitive;

        Regex(): malformed_pattern(TRUE), case_sensitive(FALSE)                                  {}
        Regex(const gchar *pattern, const gchar *replacement, gboolean case_sensitive);
        ~Regex();

        void assign(const gchar *pattern, const gchar *replacement, gboolean case_sensitive);
#if GLIB_CHECK_VERSION (2, 14, 0)
        gchar *replace(const gchar *s)              {  return g_regex_replace (re, s, -1, 0, to.c_str(), G_REGEX_MATCH_NOTEMPTY, NULL);  }
#else
        gboolean match(const gchar *s)              {  return regexec(&re, s, 1, &pmatch, 0)==0;  }
        int start() const                           {  return pmatch.rm_so;                       }
        int end() const                             {  return pmatch.rm_eo;                       }
        int length() const                          {  return end() - start();                    }
#endif

        operator gboolean ()                        {  return !malformed_pattern;                 }
    };

    GnomeCmdAdvrenameDialog(GnomeCmdData::AdvrenameConfig &defaults);
    ~GnomeCmdAdvrenameDialog();

    void set(GList *files);
    void unset();
    void update_new_filenames();
};

inline GnomeCmdAdvrenameDialog::Regex::Regex(const gchar *pattern, const gchar *replacement, gboolean sensitive=FALSE): case_sensitive(sensitive)
{
    if (pattern)  from = pattern;
    if (replacement)  to = replacement;
#if GLIB_CHECK_VERSION (2, 14, 0)
    GError *error = NULL;
    re = g_regex_new (pattern, GRegexCompileFlags(case_sensitive ? G_REGEX_OPTIMIZE : G_REGEX_OPTIMIZE | G_REGEX_CASELESS), G_REGEX_MATCH_NOTEMPTY, &error);
    malformed_pattern = !pattern || !*pattern || error;
    if (error)  g_error_free (error);
#else
    memset(&pmatch, 0, sizeof(pmatch));
    malformed_pattern = !pattern || !*pattern || regcomp(&re, pattern, (case_sensitive ? REG_EXTENDED : REG_EXTENDED|REG_ICASE))!=0;
#endif
}

inline GnomeCmdAdvrenameDialog::Regex::~Regex()
{
#if GLIB_CHECK_VERSION (2, 14, 0)
    g_regex_unref (re);
#else
    if (!malformed_pattern)  regfree(&re);
#endif
}

inline void GnomeCmdAdvrenameDialog::Regex::assign(const gchar *pattern, const gchar *replacement, gboolean sensitive=FALSE)
{
    from.clear();
    to.clear();
    case_sensitive = sensitive;
    if (pattern)  from = pattern;
    if (replacement)  to = replacement;
#if GLIB_CHECK_VERSION (2, 14, 0)
    g_regex_unref (re);
    GError *error = NULL;
    re = g_regex_new (pattern, GRegexCompileFlags(case_sensitive ? G_REGEX_OPTIMIZE : G_REGEX_OPTIMIZE | G_REGEX_CASELESS), G_REGEX_MATCH_NOTEMPTY, &error);
    malformed_pattern = !pattern || !*pattern || error;
    if (error)  g_error_free (error);
#else
    if (!malformed_pattern)  regfree(&re);
    memset(&pmatch, 0, sizeof(pmatch));
    malformed_pattern = !pattern || !*pattern || regcomp(&re, pattern, (case_sensitive ? REG_EXTENDED : REG_EXTENDED|REG_ICASE))!=0;
#endif
}

#endif // __GNOME_CMD_ADVRENAME_DIALOG_H__

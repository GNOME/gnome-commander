/** 
 * @file gnome-cmd-xml-config.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2016 Uwe Scholz\n
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

#include <stack>
#include <string>
#include <algorithm>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-xml-config.h"
#include "gnome-cmd-con-list.h"
#include "gnome-cmd-regex.h"
#include "gnome-cmd-user-actions.h"
#include "dict.h"
#include "utils.h"
#include "dialogs/gnome-cmd-advrename-dialog.h"

using namespace std;


gchar *XML::xstream::escaped_text = NULL;


#if !GLIB_CHECK_VERSION (2, 16, 0)
enum GMarkupCollectType
{
  G_MARKUP_COLLECT_INVALID,
  G_MARKUP_COLLECT_STRING,
  G_MARKUP_COLLECT_STRDUP,
  G_MARKUP_COLLECT_BOOLEAN,
  G_MARKUP_COLLECT_TRISTATE,

  G_MARKUP_COLLECT_OPTIONAL = (1 << 16)
};


static gboolean
g_markup_parse_boolean (const char  *string,
                        gboolean    *value)
{
  char const * const falses[] = { "false", "f", "no", "n", "0" };
  char const * const trues[] = { "true", "t", "yes", "y", "1" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (falses); i++)
    {
      if (g_ascii_strcasecmp (string, falses[i]) == 0)
        {
          if (value != NULL)
            *value = FALSE;

          return TRUE;
        }
    }

  for (i = 0; i < G_N_ELEMENTS (trues); i++)
    {
      if (g_ascii_strcasecmp (string, trues[i]) == 0)
        {
          if (value != NULL)
            *value = TRUE;

          return TRUE;
        }
    }

  return FALSE;
}


static
gboolean
g_markup_collect_attributes (const gchar         *element_name,
                             const gchar        **attribute_names,
                             const gchar        **attribute_values,
                             GError             **error,
                             GMarkupCollectType   first_type,
                             const gchar         *first_attr,
                             ...)
{
  GMarkupCollectType type;
  const gchar *attr;
  guint64 collected;
  int written;
  va_list ap;
  int i;

  type = first_type;
  attr = first_attr;
  collected = 0;
  written = 0;

  va_start (ap, first_attr);
  while (type != G_MARKUP_COLLECT_INVALID)
    {
      gboolean mandatory;
      const gchar *value;

      mandatory = !(type & G_MARKUP_COLLECT_OPTIONAL);
      type &= (G_MARKUP_COLLECT_OPTIONAL - 1);

      /* tristate records a value != TRUE and != FALSE
       * for the case where the attribute is missing
       */
      if (type == G_MARKUP_COLLECT_TRISTATE)
        mandatory = FALSE;

      for (i = 0; attribute_names[i]; i++)
        if (i >= 40 || !(collected & (G_GUINT64_CONSTANT(1) << i)))
          if (!strcmp (attribute_names[i], attr))
            break;

      /* ISO C99 only promises that the user can pass up to 127 arguments.
       * Subtracting the first 4 arguments plus the final NULL and dividing
       * by 3 arguments per collected attribute, we are left with a maximum
       * number of supported attributes of (127 - 5) / 3 = 40.
       *
       * In reality, nobody is ever going to call us with anywhere close to
       * 40 attributes to collect, so it is safe to assume that if i > 40
       * then the user has given some invalid or repeated arguments.  These
       * problems will be caught and reported at the end of the function.
       *
       * We know at this point that we have an error, but we don't know
       * what error it is, so just continue...
       */
      if (i < 40)
        collected |= (G_GUINT64_CONSTANT(1) << i);

      value = attribute_values[i];

      if (value == NULL && mandatory)
        {
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                       "element '%s' requires attribute '%s'",
                       element_name, attr);

          va_end (ap);
          goto failure;
        }

      switch (type)
        {
        case G_MARKUP_COLLECT_STRING:
          {
            const char **str_ptr;

            str_ptr = va_arg (ap, const char **);

            if (str_ptr != NULL)
              *str_ptr = value;
          }
          break;

        case G_MARKUP_COLLECT_STRDUP:
          {
            char **str_ptr;

            str_ptr = va_arg (ap, char **);

            if (str_ptr != NULL)
              *str_ptr = g_strdup (value);
          }
          break;

        case G_MARKUP_COLLECT_BOOLEAN:
        case G_MARKUP_COLLECT_TRISTATE:
          if (value == NULL)
            {
              gboolean *bool_ptr;

              bool_ptr = va_arg (ap, gboolean *);

              if (bool_ptr != NULL)
                {
                  if (type == G_MARKUP_COLLECT_TRISTATE)
                    /* constructivists rejoice!
                     * neither false nor true...
                     */
                    *bool_ptr = -1;

                  else /* G_MARKUP_COLLECT_BOOLEAN */
                    *bool_ptr = FALSE;
                }
            }
          else
            {
              if (!g_markup_parse_boolean (value, va_arg (ap, gboolean *)))
                {
                  g_set_error (error, G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "element '%s', attribute '%s', value '%s' "
                               "cannot be parsed as a boolean value",
                               element_name, attr, value);

                  va_end (ap);
                  goto failure;
                }
            }

          break;

        default:
          g_assert_not_reached ();
        }

      type = va_arg (ap, GMarkupCollectType);
      attr = va_arg (ap, const char *);
      written++;
    }
  va_end (ap);

  /* ensure we collected all the arguments */
  for (i = 0; attribute_names[i]; i++)
    if ((collected & (G_GUINT64_CONSTANT(1) << i)) == 0)
      {
        /* attribute not collected:  could be caused by two things.
         *
         * 1) it doesn't exist in our list of attributes
         * 2) it existed but was matched by a duplicate attribute earlier
         *
         * find out.
         */
        int j;

        for (j = 0; j < i; j++)
          if (strcmp (attribute_names[i], attribute_names[j]) == 0)
            /* duplicate! */
            break;

        /* j is now the first occurance of attribute_names[i] */
        if (i == j)
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                       "attribute '%s' invalid for element '%s'",
                       attribute_names[i], element_name);
        else
          g_set_error (error, G_MARKUP_ERROR,
                       G_MARKUP_ERROR_INVALID_CONTENT,
                       "attribute '%s' given multiple times for element '%s'",
                       attribute_names[i], element_name);

        goto failure;
      }

  return TRUE;

failure:
  /* replay the above to free allocations */
  type = first_type;
  attr = first_attr;

  va_start (ap, first_attr);
  while (type != G_MARKUP_COLLECT_INVALID)
    {
      gpointer ptr;

      ptr = va_arg (ap, gpointer);

      if (ptr == NULL)
        continue;

      switch (type & (G_MARKUP_COLLECT_OPTIONAL - 1))
        {
        case G_MARKUP_COLLECT_STRDUP:
          if (written)
            g_free (*(char **) ptr);

        case G_MARKUP_COLLECT_STRING:
          *(char **) ptr = NULL;
          break;

        case G_MARKUP_COLLECT_BOOLEAN:
          *(gboolean *) ptr = FALSE;
          break;

        case G_MARKUP_COLLECT_TRISTATE:
          *(gboolean *) ptr = -1;
          break;
        }

      type = va_arg (ap, GMarkupCollectType);
      attr = va_arg (ap, const char *);

      if (written)
        written--;
    }
  va_end (ap);

  return FALSE;
}
#endif


enum {XML_ELEM_NOT_FOUND,
      XML_GNOMECOMMANDER,
      XML_GNOMECOMMANDER_LAYOUT,
      XML_GNOMECOMMANDER_LAYOUT_PANEL,
      XML_GNOMECOMMANDER_LAYOUT_PANEL_TAB,
      XML_GNOMECOMMANDER_HISTORY,
      XML_GNOMECOMMANDER_HISTORY_DIRECTORIES,
      XML_GNOMECOMMANDER_HISTORY_DIRECTORIES_DIRECTORY,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_WINDOWSIZE,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_TEMPLATE,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_COUNTER,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_REGEXES,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_REGEXES_REGEX,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_CASECONVERSION,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_TRIMBLANKS,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_HISTORY,
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_HISTORY_TEMPLATE,
      XML_GNOMECOMMANDER_SEARCHTOOL,
      XML_GNOMECOMMANDER_SEARCHTOOL_WINDOWSIZE,
      XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE,
      XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_PATTERN,
      XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_SUBDIRECTORIES,
      XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_TEXT,
      XML_GNOMECOMMANDER_SEARCHTOOL_HISTORY,
      XML_GNOMECOMMANDER_SEARCHTOOL_HISTORY_PATTERN,
      XML_GNOMECOMMANDER_SEARCHTOOL_HISTORY_TEXT,
      XML_GNOMECOMMANDER_BOOKMARKSTOOL,
      XML_GNOMECOMMANDER_BOOKMARKSTOOL_WINDOWSIZE,
      XML_GNOMECOMMANDER_CONNECTIONS,
      XML_GNOMECOMMANDER_CONNECTIONS_CONNECTION,
      XML_GNOMECOMMANDER_BOOKMARKS,
      XML_GNOMECOMMANDER_BOOKMARKS_GROUP,
      XML_GNOMECOMMANDER_BOOKMARKS_GROUP_BOOKMARK,
      XML_GNOMECOMMANDER_SELECTIONS,
      XML_GNOMECOMMANDER_SELECTIONS_PROFILE,
      XML_GNOMECOMMANDER_SELECTIONS_PROFILE_PATTERN,
      XML_GNOMECOMMANDER_SELECTIONS_PROFILE_SUBDIRECTORIES,
      XML_GNOMECOMMANDER_SELECTIONS_PROFILE_TEXT,
      XML_GNOMECOMMANDER_KEYBINDINGS,
      XML_GNOMECOMMANDER_KEYBINDINGS_KEY};


static DICT<guint> xml_elem_names(XML_ELEM_NOT_FOUND);
static stack<string> xml_paths;

static GnomeCmdData::AdvrenameConfig::Profile xml_adv_profile;
static GnomeCmdData::Selection xml_search_profile;

static DICT<FileSelectorID> xml_fs_names(INACTIVE);
static FileSelectorID xml_fs = INACTIVE;

static GnomeCmdCon *xml_con = NULL;


static bool is_default(GnomeCmdData::AdvrenameConfig::Profile &profile)
{
    return strcmp(profile.name.c_str(),"Default")==0;
}


static void xml_start(GMarkupParseContext *context,
                      const gchar *element_name,
                      const gchar **attribute_names,
                      const gchar **attribute_values,
                      gpointer user_data,
                      GError **error)
{
    string element_path;

    if (!xml_paths.empty())
        element_path = xml_paths.top();

    element_path += G_DIR_SEPARATOR;
    element_path += element_name;

    xml_paths.push(element_path);

    GnomeCmdData *cfg = (GnomeCmdData *) user_data;
    gchar *param1, *param2, *param3;
    gboolean param4, param5;

    switch (xml_elem_names[xml_paths.top()])
    {
        case XML_GNOMECOMMANDER_LAYOUT_PANEL:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "name", &param1,
                                             G_MARKUP_COLLECT_INVALID))
            {
                xml_fs = xml_fs_names[param1];
            }
            break;

        case XML_GNOMECOMMANDER_LAYOUT_PANEL_TAB:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             GMarkupCollectType(G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL), "path", &param1,    //  FIXME: temporarily, G_MARKUP_COLLECT_OPTIONAL to be removed after 1.4
                                             G_MARKUP_COLLECT_STRING|G_MARKUP_COLLECT_OPTIONAL, "dir", &param2,
                                             G_MARKUP_COLLECT_STRING, "sort", &param3,
                                             G_MARKUP_COLLECT_BOOLEAN, "asc", &param4,
                                             G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "lock", &param5,
                                             G_MARKUP_COLLECT_INVALID))
            {
                string dir(param1?param1:(param2?param2:""));     //  FIXME: temporarily, dir(param1) after 1.4
                gint sort = atoi(param3);

                if (!dir.empty() && sort<GnomeCmdFileList::NUM_COLUMNS)
                    cfg->tabs[xml_fs].push_back(make_pair(dir,make_triple((GnomeCmdFileList::ColumnID) sort,(GtkSortType) param4,param5)));
            }
            break;

        case XML_GNOMECOMMANDER_HISTORY_DIRECTORIES_DIRECTORY:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "path", &param1,
                                             G_MARKUP_COLLECT_INVALID))
            {
                gnome_cmd_con_get_dir_history (get_home_con())->add(param1);
            }
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_WINDOWSIZE:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "width", &param1,
                                             G_MARKUP_COLLECT_STRING, "height", &param2,
                                             G_MARKUP_COLLECT_INVALID))
            {
                cfg->advrename_defaults.width = atoi(param1);
                cfg->advrename_defaults.height = atoi(param2);
            }
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "name", &param1,
                                             G_MARKUP_COLLECT_INVALID))
            {
                xml_adv_profile.reset();
                xml_adv_profile.name = param1;
            }
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_COUNTER:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "start", &param1,
                                             G_MARKUP_COLLECT_STRING, "step", &param2,
                                             G_MARKUP_COLLECT_STRING, "width", &param3,
                                             G_MARKUP_COLLECT_INVALID))
            {
                xml_adv_profile.counter_start = atoi(param1);
                xml_adv_profile.counter_step  = atoi(param2);
                xml_adv_profile.counter_width = atoi(param3);
            }
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_REGEXES:
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_REGEXES_REGEX:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "pattern", &param1,
                                             G_MARKUP_COLLECT_STRING, "replace", &param2,
                                             G_MARKUP_COLLECT_BOOLEAN, "match-case", &param4,
                                             G_MARKUP_COLLECT_INVALID))
            {
                xml_adv_profile.regexes.push_back(GnomeCmd::ReplacePattern(param1, param2, param4));
            }
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_CASECONVERSION:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "use", &param1,
                                             G_MARKUP_COLLECT_INVALID))
                xml_adv_profile.case_conversion = atoi(param1);
                else
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_TRIMBLANKS:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "use", &param1,
                                             G_MARKUP_COLLECT_INVALID))
                xml_adv_profile.trim_blanks = atoi(param1);
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_WINDOWSIZE:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "width", &param1,
                                             G_MARKUP_COLLECT_STRING, "height", &param2,
                                             G_MARKUP_COLLECT_INVALID))
            {
                cfg->search_defaults.width = atoi(param1);
                cfg->search_defaults.height = atoi(param2);
            }
            break;

        case XML_GNOMECOMMANDER_BOOKMARKSTOOL_WINDOWSIZE:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "width", &param1,
                                             G_MARKUP_COLLECT_STRING, "height", &param2,
                                             G_MARKUP_COLLECT_INVALID))
            {
                cfg->bookmarks_defaults.width = atoi(param1);
                cfg->bookmarks_defaults.height = atoi(param2);
            }
            break;

        case XML_GNOMECOMMANDER_CONNECTIONS:
            cfg->XML_cfg_has_connections = TRUE;
            break;

        case XML_GNOMECOMMANDER_CONNECTIONS_CONNECTION:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "name", &param1,
                                             G_MARKUP_COLLECT_STRING, "uri", &param2,
                                             G_MARKUP_COLLECT_STRING, "auth", &param3,
                                             G_MARKUP_COLLECT_INVALID))
            {
                if (gnome_cmd_con_list_get()->has_alias(param1))
                {
                    gnome_cmd_con_erase_bookmark (gnome_cmd_con_list_get()->find_alias(param1));
                }
                else
                {
                    GnomeCmdConRemote *server = gnome_cmd_con_remote_new (param1, param2);

                    if (server)
                        gnome_cmd_con_list_get()->add(server);
                    else
                        g_warning ("<Connection> invalid URI: '%s' - ignored", param2);
                }
            }
            break;

        case XML_GNOMECOMMANDER_BOOKMARKS:
            cfg->XML_cfg_has_bookmarks = TRUE;
            break;

        case XML_GNOMECOMMANDER_BOOKMARKS_GROUP:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "name", &param1,
                                             G_MARKUP_COLLECT_BOOLEAN|G_MARKUP_COLLECT_OPTIONAL, "remote", &param4,
                                             G_MARKUP_COLLECT_INVALID))
            {
                if (param4) //  if remote...
                    xml_con = gnome_cmd_con_list_get()->find_alias(param1);
                else
                    if (strcmp(param1,"Home")==0)
                        xml_con = gnome_cmd_con_list_get()->get_home();
                    else
#ifdef HAVE_SAMBA
                        if (strcmp(param1,"SMB")==0)
                            xml_con = gnome_cmd_con_list_get()->get_smb();
                        else
#endif
                            xml_con = NULL;

                if (!xml_con)
                    g_warning ("<Bookmarks> unknown connection: '%s' - ignored", param1);
            }
            break;

        case XML_GNOMECOMMANDER_BOOKMARKS_GROUP_BOOKMARK:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRDUP, "name", &param1,
                                             G_MARKUP_COLLECT_STRDUP, "path", &param2,
                                             G_MARKUP_COLLECT_INVALID))
            {
                if (xml_con)
                    gnome_cmd_con_add_bookmark (xml_con, param1, param2);
                else
                {
                    // g_warning ("<Bookmarks> unknown connection: '%s' - ignored", param1);
                    g_free (param1);
                    g_free (param2);
                }
            }
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE:
        case XML_GNOMECOMMANDER_SELECTIONS_PROFILE:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "name", &param1,
                                             G_MARKUP_COLLECT_INVALID))
            {
                xml_search_profile.reset();
                xml_search_profile.name = param1;
            }
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_PATTERN:
        case XML_GNOMECOMMANDER_SELECTIONS_PROFILE_PATTERN:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "syntax", &param1,
                                             G_MARKUP_COLLECT_BOOLEAN, "match-case", &param4,
                                             G_MARKUP_COLLECT_INVALID))
            {
                xml_search_profile.syntax = strcmp(param1,"regex")==0 ? Filter::TYPE_REGEX : Filter::TYPE_FNMATCH;
                //  FIXME:  xml_search_profile.? = param4;
            }
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_SUBDIRECTORIES:
        case XML_GNOMECOMMANDER_SELECTIONS_PROFILE_SUBDIRECTORIES:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_STRING, "max-depth", &param1,
                                             G_MARKUP_COLLECT_INVALID))
                xml_search_profile.max_depth = atoi(param1);
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_TEXT:
        case XML_GNOMECOMMANDER_SELECTIONS_PROFILE_TEXT:
            if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                             G_MARKUP_COLLECT_BOOLEAN, "match-case", &param4,
                                             G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL, "content-search", &param5,
                                             G_MARKUP_COLLECT_INVALID))
                xml_search_profile.match_case = param4;
                xml_search_profile.content_search = param5;
            break;

        case XML_GNOMECOMMANDER_KEYBINDINGS_KEY:
            {
                gboolean shift, control, alt, super, hyper, meta;

                if (g_markup_collect_attributes (element_name, attribute_names, attribute_values, error,
                                                 G_MARKUP_COLLECT_STRING, "name", &param1,
                                                 G_MARKUP_COLLECT_STRING, "action", &param2,
                                                 G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "options", &param3,
                                                 G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL, "shift", &shift,
                                                 G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL, "control", &control,
                                                 G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL, "alt", &alt,
                                                 G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL, "super", &super,
#if GTK_CHECK_VERSION (2, 10, 0)
                                                 G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL, "hyper", &hyper,
                                                 G_MARKUP_COLLECT_BOOLEAN | G_MARKUP_COLLECT_OPTIONAL, "meta", &meta,
#endif
                                                 G_MARKUP_COLLECT_INVALID))
                    {
                        if (gcmd_user_actions.has_action(param2))
                        {
                            guint keyval = gdk_key_names[param1];

                             if (keyval==GDK_VoidSymbol)
                                if (strlen(param1)==1 && ascii_isalnum (*param1))
                                    keyval = *param1;

                            if (keyval!=GDK_VoidSymbol)
                            {
                                guint accel_mask = 0;

                                if (shift)  accel_mask |= GDK_SHIFT_MASK;
                                if (control)  accel_mask |= GDK_CONTROL_MASK;
                                if (alt)  accel_mask |= GDK_MOD1_MASK;
#if GTK_CHECK_VERSION (2, 10, 0)
                                if (super)  accel_mask |= GDK_SUPER_MASK;
                                if (hyper)  accel_mask |= GDK_HYPER_MASK;
                                if (meta)  accel_mask |= GDK_META_MASK;
#else
                                if (super)  accel_mask |= GDK_MOD4_MASK;
#endif
                                gcmd_user_actions.register_action(accel_mask, keyval, param2, param3);
                            }
                            else
                                g_warning ("<KeyBindings> invalid key name: '%s' - ignored", param1);
                        }
                        else
                            g_warning ("<KeyBindings> unknown user action: '%s' - ignored", param2);
                    }
            }
            break;

        default:
            break;
    }
}


static void xml_end (GMarkupParseContext *context,
                     const gchar *element_name,
                     gpointer user_data,
                     GError **error)
{
    GnomeCmdData *cfg = (GnomeCmdData *) user_data;

    switch (xml_elem_names[xml_paths.top()])
    {
        case XML_GNOMECOMMANDER_HISTORY_DIRECTORIES:
            gnome_cmd_con_get_dir_history (get_home_con())->reverse();
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE:
            cfg->advrename_defaults.profiles.push_back(xml_adv_profile);
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL:
            {
                vector<GnomeCmdData::AdvrenameConfig::Profile>::iterator p;

                p = find_if (cfg->advrename_defaults.profiles.begin(), cfg->advrename_defaults.profiles.end(), is_default);

                if (p!=cfg->advrename_defaults.profiles.end())
                {
                    cfg->advrename_defaults.default_profile = *p;
                    cfg->advrename_defaults.profiles.erase(p);
                }
            }
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_HISTORY:
            cfg->advrename_defaults.templates.reverse();
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE:
            if (xml_search_profile.name=="Default")
                cfg->search_defaults.default_profile = xml_search_profile;
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_HISTORY:
            cfg->search_defaults.name_patterns.reverse();
            cfg->search_defaults.content_patterns.reverse();
            break;

        case XML_GNOMECOMMANDER_SELECTIONS_PROFILE:
            if (xml_search_profile.name!="Default")
                cfg->selections.push_back(xml_search_profile);
            break;

        default:
            break;
    }

    xml_paths.pop();
}


 static void xml_element (GMarkupParseContext *context,
                          const gchar *text,
                          gsize text_len,
                          gpointer user_data,
                          GError **error)
{
    GnomeCmdData *cfg = (GnomeCmdData *) user_data;

    switch (xml_elem_names[xml_paths.top()])
    {
        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_TEMPLATE:
            xml_adv_profile.template_string = text;
            break;

        case XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_HISTORY_TEMPLATE:
            cfg->advrename_defaults.templates.add(text);
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_PATTERN:
        case XML_GNOMECOMMANDER_SELECTIONS_PROFILE_PATTERN:
            xml_search_profile.filename_pattern = text;
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_TEXT:
        case XML_GNOMECOMMANDER_SELECTIONS_PROFILE_TEXT:
            xml_search_profile.text_pattern = text;
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_HISTORY_PATTERN:
            cfg->search_defaults.name_patterns.add(text);
            break;

        case XML_GNOMECOMMANDER_SEARCHTOOL_HISTORY_TEXT:
            cfg->search_defaults.content_patterns.add(text);
            break;

        default:
            break;
    }
}


gboolean gnome_cmd_xml_config_parse (const gchar *xml, gsize xml_len, GnomeCmdData &cfg)
{
    static GMarkupParser parser = {xml_start, xml_end, xml_element};

    static struct
    {
        guint id;
        const gchar *path;
    }
    xml_elem_data[] = {
                        {XML_GNOMECOMMANDER, "/GnomeCommander"},
                        {XML_GNOMECOMMANDER_LAYOUT, "/GnomeCommander/Layout"},
                        {XML_GNOMECOMMANDER_LAYOUT_PANEL, "/GnomeCommander/Layout/Panel"},
                        {XML_GNOMECOMMANDER_LAYOUT_PANEL_TAB, "/GnomeCommander/Layout/Panel/Tab"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL, "/GnomeCommander/AdvancedRenameTool"},
                        {XML_GNOMECOMMANDER_HISTORY, "/GnomeCommander/History"},
                        {XML_GNOMECOMMANDER_HISTORY_DIRECTORIES, "/GnomeCommander/History/Directories"},
                        {XML_GNOMECOMMANDER_HISTORY_DIRECTORIES_DIRECTORY, "/GnomeCommander/History/Directories/Directory"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_WINDOWSIZE, "/GnomeCommander/AdvancedRenameTool/WindowSize"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE, "/GnomeCommander/AdvancedRenameTool/Profile"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_TEMPLATE, "/GnomeCommander/AdvancedRenameTool/Profile/Template"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_COUNTER, "/GnomeCommander/AdvancedRenameTool/Profile/Counter"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_REGEXES, "/GnomeCommander/AdvancedRenameTool/Profile/Regexes"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_REGEXES_REGEX, "/GnomeCommander/AdvancedRenameTool/Profile/Regexes/Regex"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_CASECONVERSION, "/GnomeCommander/AdvancedRenameTool/Profile/CaseConversion"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_PROFILE_TRIMBLANKS, "/GnomeCommander/AdvancedRenameTool/Profile/TrimBlanks"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_HISTORY, "/GnomeCommander/AdvancedRenameTool/History"},
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_HISTORY_TEMPLATE, "/GnomeCommander/AdvancedRenameTool/History/Template"},
                        {XML_GNOMECOMMANDER_SEARCHTOOL, "/GnomeCommander/SearchTool"},
                        {XML_GNOMECOMMANDER_SEARCHTOOL_WINDOWSIZE, "/GnomeCommander/SearchTool/WindowSize"},
                        {XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE, "/GnomeCommander/SearchTool/Profile"},
                        {XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_PATTERN, "/GnomeCommander/SearchTool/Profile/Pattern"},
                        {XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_SUBDIRECTORIES, "/GnomeCommander/SearchTool/Profile/Subdirectories"},
                        {XML_GNOMECOMMANDER_SEARCHTOOL_PROFILE_TEXT, "/GnomeCommander/SearchTool/Profile/Text"},
                        {XML_GNOMECOMMANDER_SEARCHTOOL_HISTORY, "/GnomeCommander/SearchTool/History"},
                        {XML_GNOMECOMMANDER_SEARCHTOOL_HISTORY_PATTERN, "/GnomeCommander/SearchTool/History/Pattern"},
                        {XML_GNOMECOMMANDER_SEARCHTOOL_HISTORY_TEXT, "/GnomeCommander/SearchTool/History/Text"},
                        {XML_GNOMECOMMANDER_BOOKMARKSTOOL, "/GnomeCommander/BookmarksTool"},
                        {XML_GNOMECOMMANDER_BOOKMARKSTOOL_WINDOWSIZE, "/GnomeCommander/BookmarksTool/WindowSize"},
                        {XML_GNOMECOMMANDER_CONNECTIONS, "/GnomeCommander/Connections"},
                        {XML_GNOMECOMMANDER_CONNECTIONS_CONNECTION, "/GnomeCommander/Connections/Connection"},
                        {XML_GNOMECOMMANDER_BOOKMARKS, "/GnomeCommander/Bookmarks"},
                        {XML_GNOMECOMMANDER_BOOKMARKS_GROUP, "/GnomeCommander/Bookmarks/Group"},
                        {XML_GNOMECOMMANDER_BOOKMARKS_GROUP_BOOKMARK, "/GnomeCommander/Bookmarks/Group/Bookmark"},
                        {XML_GNOMECOMMANDER_SELECTIONS, "/GnomeCommander/Selections"},
                        {XML_GNOMECOMMANDER_SELECTIONS_PROFILE, "/GnomeCommander/Selections/Profile"},
                        {XML_GNOMECOMMANDER_SELECTIONS_PROFILE_PATTERN, "/GnomeCommander/Selections/Profile/Pattern"},
                        {XML_GNOMECOMMANDER_SELECTIONS_PROFILE_SUBDIRECTORIES, "/GnomeCommander/Selections/Profile/Subdirectories"},
                        {XML_GNOMECOMMANDER_SELECTIONS_PROFILE_TEXT, "/GnomeCommander/Selections/Profile/Text"},
                        {XML_GNOMECOMMANDER_KEYBINDINGS, "/GnomeCommander/KeyBindings"},
                        {XML_GNOMECOMMANDER_KEYBINDINGS_KEY, "/GnomeCommander/KeyBindings/Key"}
                       };

    load_data (xml_elem_names, xml_elem_data, G_N_ELEMENTS (xml_elem_data));

    xml_fs_names.add(LEFT,"left");
    xml_fs_names.add(RIGHT,"right");

    if (xml_len==-1)
        xml_len = strlen (xml);

    GMarkupParseContext *context = g_markup_parse_context_new (&parser, GMarkupParseFlags(0), &cfg, NULL);

    GError *error = NULL;

    if (!g_markup_parse_context_parse (context, xml, xml_len, &error) ||
        !g_markup_parse_context_end_parse (context, &error))
    {
        g_warning ("%s", error->message);
        g_error_free (error);
    }

    g_markup_parse_context_free (context);

    // FIXME:   "Default" template

    return error==NULL;
}


gboolean gnome_cmd_xml_config_load (const gchar *path, GnomeCmdData &cfg)
{
    gchar *xml = NULL;
    gsize xml_len = 0;

    GError *error = NULL;

    if (!g_file_get_contents (path, &xml, &xml_len, &error))
    {
        g_warning ("%s", error->message);
        g_error_free (error);

        return FALSE;
    }

    gboolean retval = gnome_cmd_xml_config_parse (xml, xml_len, cfg);

    g_free (xml);

    return retval;
}

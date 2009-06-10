/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman
    Copyright (C) 2007-2009 Piotr Eljasiak

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

/* gmarkup.c - Simple XML-like parser
 *
 *  Copyright 2000, 2003 Red Hat, Inc.
 *  Copyright 2007, 2008 Ryan Lortie <desrt@desrt.ca>
 *
 * GLib is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GLib; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *   Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stack>
#include <string>
#include <algorithm>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-xml-config.h"
#include "gnome-cmd-advrename-dialog.h"
#include "gnome-cmd-regex.h"
#include "dict.h"
#include "utils.h"

using namespace std;


#if !GLIB_CHECK_VERSION (2, 16, 0)
typedef enum
{
  G_MARKUP_COLLECT_INVALID,
  G_MARKUP_COLLECT_STRING,
  G_MARKUP_COLLECT_STRDUP,
  G_MARKUP_COLLECT_BOOLEAN,
  G_MARKUP_COLLECT_TRISTATE,

  G_MARKUP_COLLECT_OPTIONAL = (1 << 16)
} GMarkupCollectType;


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
      XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_HISTORY_TEMPLATE};


static DICT<guint> xml_elem_names(XML_ELEM_NOT_FOUND);
static stack<string> xml_paths;

static GnomeCmdData::AdvrenameConfig::Profile xml_adv_profile;


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
    gboolean param4;

    switch (xml_elem_names[xml_paths.top()])
    {
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
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL, "/GnomeCommander/AdvancedRenameTool"},
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
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_HISTORY, "/GnomeCommander/AdvancedRenameTool/TemplateHistory"},                      //  FIXME: for compatibility, to be removed after 1.2.8 release
                        {XML_GNOMECOMMANDER_ADVANCEDRENAMETOOL_HISTORY_TEMPLATE, "/GnomeCommander/AdvancedRenameTool/TemplateHistory/Template"}     //  FIXME: for compatibility, to be removed after 1.2.8 release
                       };

    load_data (xml_elem_names, xml_elem_data, G_N_ELEMENTS(xml_elem_data));

    if (xml_len==-1)
        xml_len = strlen (xml);

    GMarkupParseContext *context = g_markup_parse_context_new (&parser, GMarkupParseFlags(0), &cfg, NULL);

    GError *error = NULL;

    if (!g_markup_parse_context_parse (context, xml, xml_len, &error) ||
        !g_markup_parse_context_end_parse (context, &error))
    {
        g_warning (error->message);
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
        g_warning (error->message);
        g_error_free (error);

        return FALSE;
    }

    gboolean retval = gnome_cmd_xml_config_parse (xml, xml_len, cfg);

    g_free (xml);

    return retval;
}


inline void fprintf_escaped(FILE *f, const char *format, ...)
{
    va_list args;

    va_start (args, format);
    gchar *s = g_markup_vprintf_escaped (format, args);
    va_end (args);

    fputs(s, f);
    g_free (s);
}


void gnome_cmd_xml_config_save (const gchar *path, GnomeCmdData &cfg)
{
    FILE *f = fopen(path,"w");

    if (!f)
        return;

    fputs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n", f);
    fputs("<!-- Created with GNOME Commander (http://www.nongnu.org/gcmd/) -->\n", f);
    fputs("<GnomeCommander version=\"" VERSION "\">\n", f);
    fputs("\t<AdvancedRenameTool>\n", f);
    fprintf (f, "\t\t<WindowSize width=\"%i\" height=\"%i\" />\n", cfg.advrename_defaults.width, cfg.advrename_defaults.height);

    fprintf_escaped(f, "\t\t<Profile name=\"%s\">\n", "Default");
    fprintf_escaped(f, "\t\t\t<Template>%s</Template>\n", cfg.advrename_defaults.templates.empty()  ? "$N" : cfg.advrename_defaults.templates.front());
    fprintf(f, "\t\t\t<Counter start=\"%u\" step=\"%i\" width=\"%u\" />\n", cfg.advrename_defaults.default_profile.counter_start,
                                                                            cfg.advrename_defaults.default_profile.counter_step,
                                                                            cfg.advrename_defaults.default_profile.counter_width);
    fputs("\t\t\t<Regexes>\n", f);

    for (std::vector<GnomeCmd::ReplacePattern>::const_iterator r=cfg.advrename_defaults.default_profile.regexes.begin(); r!=cfg.advrename_defaults.default_profile.regexes.end(); ++r)
        fprintf_escaped(f, "\t\t\t\t<Regex pattern=\"%s\" replace=\"%s\" match-case=\"%u\" />\n", r->pattern.c_str(), r->replacement.c_str(), r->match_case);

    fputs("\t\t\t</Regexes>\n", f);
    fprintf(f, "\t\t\t<CaseConversion use=\"%u\" />\n", cfg.advrename_defaults.default_profile.case_conversion);
    fprintf(f, "\t\t\t<TrimBlanks use=\"%u\" />\n", cfg.advrename_defaults.default_profile.trim_blanks);
    fputs("\t\t</Profile>\n", f);

    for (std::vector<GnomeCmdData::AdvrenameConfig::Profile>::const_iterator p=cfg.advrename_defaults.profiles.begin(); p!=cfg.advrename_defaults.profiles.end(); ++p)
    {
        fprintf_escaped(f, "\t\t<Profile name=\"%s\">\n", p->name.c_str());
        fprintf_escaped(f, "\t\t\t<Template>%s</Template>\n", p->template_string.empty() ? "$N" : p->template_string.c_str());
        fprintf(f, "\t\t\t<Counter start=\"%u\" step=\"%i\" width=\"%u\" />\n", p->counter_start, p->counter_step, p->counter_width);
        fputs("\t\t\t<Regexes>\n", f);
        for (std::vector<GnomeCmd::ReplacePattern>::const_iterator r=p->regexes.begin(); r!=p->regexes.end(); ++r)
            fprintf_escaped(f, "\t\t\t\t<Regex pattern=\"%s\" replace=\"%s\" match-case=\"%u\" />\n", r->pattern.c_str(), r->replacement.c_str(), r->match_case);
        fputs("\t\t\t</Regexes>\n", f);
        fprintf(f, "\t\t\t<CaseConversion use=\"%u\" />\n", p->case_conversion);
        fprintf(f, "\t\t\t<TrimBlanks use=\"%u\" />\n", p->trim_blanks);
        fputs("\t\t</Profile>\n", f);
    }

    fputs("\t\t<History>\n", f);

    for (GList *i=cfg.advrename_defaults.templates.ents; i; i=i->next)
        fprintf_escaped(f, "\t\t\t<Template>%s</Template>\n", (const gchar *) i->data);

    fputs("\t\t</History>\n", f);
    fputs("\t</AdvancedRenameTool>\n", f);
    fputs("</GnomeCommander>\n", f);
    fputs("", f);

    fclose(f);
}

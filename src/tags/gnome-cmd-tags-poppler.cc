/** 
 * @file gnome-cmd-tags-poppler.cc
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
#include <stdio.h>
#include <string.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags-poppler.h"
#include "gnome-cmd-tags.h"
#include "utils.h"
#include "dict.h"

#ifdef HAVE_PDF
#include <poppler/glib/poppler.h>
#endif

using namespace std;

#ifdef HAVE_PDF

static gchar * pgd_format_date (time_t utime)
{
        time_t time = (time_t) utime;
        char s[256];
        const char *fmt_hack = "%c";
        size_t len;
#if defined (__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
#ifdef HAVE_LOCALTIME_R
        struct tm t;
        if (time == 0 || !localtime_r (&time, &t)) return NULL;
        len = strftime (s, sizeof (s), fmt_hack, &t);
#else
        struct tm *t;
        if (time == 0 || !(t = localtime (&time)) ) return NULL;
        len = strftime (s, sizeof (s), fmt_hack, t);
#endif
#if defined (__GNUC__)
#pragma GCC diagnostic pop
#endif

        if (len == 0 || s[0] == '\0') return NULL;

        return g_locale_to_utf8 (s, -1, NULL, NULL, NULL);
}

inline guint enum_bit_to_01(int enum_value, int enum_bit)
{
    return (enum_value & enum_bit) ? 1 : 0;
}


inline gdouble get_tolerance (gdouble size)
{
    if (size < 150.0f)
        return 1.5f;

    return size >= 150.0f && size <= 600.0f ? 2.0f : 3.0f;
}


static gchar *paper_name (gdouble doc_width, double doc_height)
{
    gchar *s = NULL;

    GList *paper_sizes = gtk_paper_size_get_paper_sizes (FALSE);

    for (GList *l = paper_sizes; l && l->data; l = g_list_next (l))
    {
        GtkPaperSize *size = (GtkPaperSize *) l->data;

        gdouble paper_width = gtk_paper_size_get_width (size, GTK_UNIT_MM);
        gdouble paper_height = gtk_paper_size_get_height (size, GTK_UNIT_MM);

        gdouble width_tolerance = get_tolerance (paper_width);
        gdouble height_tolerance = get_tolerance (paper_height);

        if (ABS (doc_height - paper_height) <= height_tolerance &&
            ABS (doc_width - paper_width) <= width_tolerance)
        {
            // Note to translators: first placeholder is the paper name (eg. * A4)
            s = g_strdup_printf (_("%s, Portrait"), gtk_paper_size_get_display_name (size));
            break;
        }
        else
            if (ABS (doc_width - paper_height) <= height_tolerance &&
                ABS (doc_height - paper_width) <= width_tolerance)
            {
                // Note to translators: first placeholder is the paper name (eg. * A4)
                s = g_strdup_printf (_("%s, Landscape"), gtk_paper_size_get_display_name (size));
                break;
            }
    }

    g_list_foreach (paper_sizes, (GFunc) gtk_paper_size_free, NULL);
    g_list_free (paper_sizes);

    return s;
}
#endif


void gcmd_tags_poppler_load_metadata(GnomeCmdFile *f)
{
#ifdef HAVE_PDF
    g_return_if_fail (f != nullptr);
    g_return_if_fail (f->gFileInfo != nullptr);

    if (f->metadata && f->metadata->is_accessed(TAG_PDF))  return;

    if (!f->metadata)
        f->metadata = new GnomeCmdFileMetadata;

    if (!f->metadata)  return;

    f->metadata->mark_as_accessed(TAG_PDF);

    if (!f->is_local())  return;

    // skip non pdf files, as pdf metatags extraction is very expensive...
    if (g_file_info_get_content_type (f->gFileInfo) == nullptr) return;
    if (!strstr (g_file_info_get_content_type (f->gFileInfo), "pdf"))  return;

    DEBUG('t', "Loading PDF metadata for '%s'\n", g_file_info_get_name(f->gFileInfo));

    GError *error = NULL;
    gchar *uri = g_file_get_uri (f->gFile);

    PopplerDocument *document = poppler_document_new_from_file(uri, NULL, &error);
    g_free(uri);
    if (error)
    {
        if (error->code == POPPLER_ERROR_ENCRYPTED)
        {
            f->metadata->mark_as_accessed(TAG_DOC);
            f->metadata->addf(TAG_DOC_SECURITY, "%u", 1);
        }
        g_error_free(error);
        return;
    }

    f->metadata->mark_as_accessed(TAG_DOC);

    gchar *title, *author, *subject, *keywords, *creator, *producer;
    gchar *str;
    GTime creation_date, mod_date;
    PopplerPermissions permissions;
    guint format_major, format_minor;
    g_object_get(document,
                 "title", &title,
                 "author", &author,
                 "subject", &subject,
                 "keywords", &keywords,
                 "creator", &creator,
                 "producer", &producer,
                 "creation-date", &creation_date,
                 "mod-date", &mod_date,
                 "permissions", &permissions,
                 "format-major", &format_major,
                 "format-minor", &format_minor,
                 NULL);

    f->metadata->addf(TAG_PDF_VERSION, "%u.%u", format_major, format_minor);

    f->metadata->addf(TAG_DOC_PAGECOUNT, "%i", poppler_document_get_n_pages(document));

    f->metadata->addf(TAG_PDF_OPTIMIZED, "%u", poppler_document_is_linearized(document));

    f->metadata->addf(TAG_DOC_SECURITY, "%u", 0);

    f->metadata->addf(TAG_PDF_PRINTING, "%u", enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_PRINT));
    f->metadata->addf(TAG_PDF_MODIFYING, "%u", enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_MODIFY));
    f->metadata->addf(TAG_PDF_COPYING, "%u", enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_COPY));
    f->metadata->addf(TAG_PDF_COMMENTING, "%u", enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_ADD_NOTES));
    f->metadata->addf(TAG_PDF_FORMFILLING, "%u", enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_FILL_FORM));
    f->metadata->addf(TAG_PDF_HIRESPRINTING, "%u", enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_PRINT_HIGH_RESOLUTION));
    f->metadata->addf(TAG_PDF_DOCASSEMBLY, "%u", enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_ASSEMBLE));
    f->metadata->addf(TAG_PDF_ACCESSIBILITYSUPPORT, "%u", enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_EXTRACT_CONTENTS));

    f->metadata->add(TAG_DOC_TITLE, title);
    g_free(title);

    f->metadata->add(TAG_DOC_SUBJECT, subject);
    g_free(subject);

    // FIXME:  split keywords here
    f->metadata->add(TAG_DOC_KEYWORDS, keywords);
    f->metadata->add(TAG_FILE_KEYWORDS, keywords);
    g_free(keywords);

    f->metadata->add(TAG_DOC_AUTHOR, author);
    f->metadata->add(TAG_FILE_PUBLISHER, author);
    g_free(author);

    f->metadata->add(TAG_PDF_PRODUCER, creator);
    g_free(creator);

    f->metadata->add(TAG_DOC_GENERATOR, producer);
    g_free(producer);

    str = pgd_format_date (creation_date);
    f->metadata->add(TAG_DOC_DATECREATED, str);

    str = pgd_format_date (mod_date);
    f->metadata->add(TAG_DOC_DATEMODIFIED, str);

    g_free (str);

    if (poppler_document_get_n_pages(document) > 0)
    {
        PopplerPage *page = poppler_document_get_page(document, 0);
        double page_width, page_height;
        poppler_page_get_size(page, &page_width, &page_height);

        double width = page_width/72.0f*25.4f;
        double height = page_height/72.0f*25.4f;

        f->metadata->addf(TAG_PDF_PAGEWIDTH, "%.0f", width);
        f->metadata->addf(TAG_PDF_PAGEHEIGHT, "%.0f", height);

        gchar *paper_size = paper_name (width, height);

        f->metadata->add(TAG_PDF_PAGESIZE, paper_size);

        g_object_unref(page);
        g_free (paper_size);
    }

    if (poppler_document_has_attachments(document))
    {
        GList *list = poppler_document_get_attachments(document);

        f->metadata->addf(TAG_PDF_EMBEDDEDFILES, "%u", g_list_length(list));

        g_list_free_full(list, g_object_unref);
    }
    else
    {
        f->metadata->addf(TAG_PDF_EMBEDDEDFILES, "%u", 0);
    }

    g_object_unref(document);
#endif
}

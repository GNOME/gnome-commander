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

#include <config.h>
#include <stdio.h>
#include <string.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags.h"
#include "utils.h"
#include "dict.h"

#ifdef HAVE_PDF
#include <regex.h>
#include <poppler/PDFDoc.h>
#include <poppler/PDFDocEncoding.h>
#include <poppler/Error.h>
#endif

using namespace std;


#ifdef HAVE_PDF
static regex_t rxDate;
static gboolean rxDate_OK;

static void noErrorReporting(int pos, char *msg, va_list args)
{
}
#endif


void gcmd_tags_poppler_init()
{
#ifdef HAVE_PDF
    rxDate_OK = regcomp (&rxDate, "^(D:)?([12][019][0-9][0-9]([01][0-9]([0-3][0-9]([012][0-9]([0-5][0-9]([0-5][0-9])?)?)?)?)?)", REG_EXTENDED)==0;

    setErrorFunction(noErrorReporting);
#endif
}


void gcmd_tags_poppler_shutdown()
{
#ifdef HAVE_PDF
    if (rxDate_OK)
        regfree(&rxDate);
#endif
}


#ifdef HAVE_PDF
static gchar *convert_to_utf8(GooString *s)
{
    guint len = s->getLength();

    if (s->hasUnicodeMarker())
        return g_convert (s->getCString()+2, len-2, "UTF-8", "UTF-16BE", NULL, NULL, NULL);
    else
    {
        static vector<gunichar> buff(64);

        if (len>buff.size())
            buff.resize(len);

        gunichar *ucs4 = &buff[0];

        for (guint i=0; i<len; ++i, ++ucs4)
            *ucs4 = pdfDocEncoding[(unsigned char) s->getChar(i)];

        return g_ucs4_to_utf8 (&buff[0], len, NULL, NULL, NULL);
    }
}


inline void add(GnomeCmdFileMetadata &metadata, const GnomeCmdTag tag, GooString *s)
{
    if (!s)  return;

    char *value = convert_to_utf8(s);

    metadata.add(tag, value);

    g_free (value);
}


inline void add(GnomeCmdFileMetadata &metadata, const GnomeCmdTag tag1, const GnomeCmdTag tag2, GooString *s)
{
    if (!s)  return;

    char *value = convert_to_utf8(s);

    metadata.add(tag1, value);
    metadata.add(tag2, value);

    g_free (value);
}


static void add_date(GnomeCmdFileMetadata &metadata, GnomeCmdTag tag, GooString *date)
{
    // PDF date format:     D:YYYYMMDDHHmmSSOHH'mm'

    gchar *s = convert_to_utf8(date);

    if (!s)  return;

    regmatch_t m[3];

    if (rxDate_OK && regexec(&rxDate, s, G_N_ELEMENTS(m), m, 0) == 0)
    {
        static char buff[32];

        strcpy(buff, "YYYY-01-01 00:00:00");

        gchar *src = s + m[2].rm_so;

        switch (m[2].rm_eo-m[2].rm_so)
        {
            case 14:    //  SS
                memcpy(buff+17,src+12,2);

            case 12:    // mm
                memcpy(buff+14,src+10,2);

            case 10:    //  HH
                memcpy(buff+11,src+8,2);

            case 8:     //  DD
                memcpy(buff+8,src+6,2);

            case 6:     //  MM
                memcpy(buff+5,src+4,2);

            case 4:     //  YYYY
                memcpy(buff,src,4);
                break;

            default:
                break;
        }

        metadata.add(tag, buff);
    }

    g_free (s);
}


#if GTK_CHECK_VERSION (2, 12, 0)
inline gdouble get_tolerance (gdouble size)
{
    if (size < 150.0f)
        return 1.5f;

    return size >= 150.0f && size <= 600.0f ? 2.0f : 3.0f;
}
#else /* ! GTK 2.12.0 */
struct regular_paper_size
{
    double width;
    double height;
    double width_tolerance;
    double height_tolerance;
    const char *description;
}
const regular_paper_sizes[] =
{
    // ISO 216 paper sizes, all values are in mm.  Source: http://en.wikipedia.org/wiki/Paper_size
    {  210.0f,  297.0f, 2.0f, 2.0f, "A4"  },
    {  841.0f, 1189.0f, 3.0f, 3.0f, "A0"  },
    {  594.0f,  841.0f, 2.0f, 3.0f, "A1"  },
    {  420.0f,  594.0f, 2.0f, 2.0f, "A2"  },
    {  297.0f,  420.0f, 2.0f, 2.0f, "A3"  },
    {  148.0f,  210.0f, 1.5f, 2.0f, "A5"  },
    {  105.0f,  148.0f, 1.5f, 1.5f, "A6"  },
    {   74.0f,  105.0f, 1.5f, 1.5f, "A7"  },
    {   52.0f,   74.0f, 1.5f, 1.5f, "A8"  },
    {   37.0f,   52.0f, 1.5f, 1.5f, "A9"  },
    {   26.0f,   37.0f, 1.5f, 1.5f, "A10" },
    { 1000.0f, 1414.0f, 3.0f, 3.0f, "B0"  },
    {  707.0f, 1000.0f, 3.0f, 3.0f, "B1"  },
    {  500.0f,  707.0f, 2.0f, 3.0f, "B2"  },
    {  353.0f,  500.0f, 2.0f, 2.0f, "B3"  },
    {  250.0f,  353.0f, 2.0f, 2.0f, "B4"  },
    {  176.0f,  250.0f, 2.0f, 2.0f, "B5"  },
    {  125.0f,  176.0f, 1.5f, 2.0f, "B6"  },
    {   88.0f,  125.0f, 1.5f, 1.5f, "B7"  },
    {   62.0f,   88.0f, 1.5f, 1.5f, "B8"  },
    {   44.0f,   62.0f, 1.5f, 1.5f, "B9"  },
    {   31.0f,   44.0f, 1.5f, 1.5f, "B10" },
    {  917.0f, 1297.0f, 3.0f, 3.0f, "C0"  },
    {  648.0f,  917.0f, 3.0f, 3.0f, "C1"  },
    {  458.0f,  648.0f, 2.0f, 3.0f, "C2"  },
    {  324.0f,  458.0f, 2.0f, 2.0f, "C3"  },
    {  229.0f,  324.0f, 2.0f, 2.0f, "C4"  },
    {  162.0f,  229.0f, 2.0f, 2.0f, "C5"  },
    {  114.0f,  162.0f, 1.5f, 2.0f, "C6"  },
    {   81.0f,  114.0f, 1.5f, 1.5f, "C7"  },
    {   57.0f,   81.0f, 1.5f, 1.5f, "C8"  },
    {   40.0f,   57.0f, 1.5f, 1.5f, "C9"  },
    {   28.0f,   40.0f, 1.5f, 1.5f, "C10" },

    // US paper sizes
    {  279.0f,  216.0f, 3.0f, 3.0f, "Letter" },
    {  356.0f,  216.0f, 3.0f, 3.0f, "Legal"  },
    {  432.0f,  279.0f, 3.0f, 3.0f, "Ledger" }
};
#endif


inline gchar *paper_name (gdouble doc_width, double doc_height)
{
    gchar *s = NULL;

#if GTK_CHECK_VERSION (2, 12, 0)
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

#else /* ! GTK 2.12.0 */

    for (int i=G_N_ELEMENTS (regular_paper_sizes)-1; i >= 0; --i)
    {
        const regular_paper_size *size = &regular_paper_sizes[i];

        if (ABS(doc_height - size->height) <= size->height_tolerance &&
            ABS(doc_width - size->width) <= size->width_tolerance)
            // Note to translators: first placeholder is the paper name (eg. * A4)
            return g_strdup_printf (_("%s, Portrait"), size->description);
        else
            if (ABS(doc_width - size->height) <= size->height_tolerance &&
                ABS(doc_height - size->width) <= size->width_tolerance)
                // Note to translators: first placeholder is the paper name (eg. * A4)
                return g_strdup_printf (_("%s, Landscape"), size->description);
    }
#endif

    return s;
}
#endif


void gcmd_tags_poppler_load_metadata(GnomeCmdFile *f)
{
    g_return_if_fail (f != NULL);
    g_return_if_fail (f->info != NULL);

#ifdef HAVE_PDF
    if (f->metadata && f->metadata->is_accessed(TAG_PDF))  return;

    if (!f->metadata)
        f->metadata = new GnomeCmdFileMetadata;

    if (!f->metadata)  return;

    f->metadata->mark_as_accessed(TAG_PDF);

    if (!gnome_cmd_file_is_local (f))  return;

    // skip non pdf files, as pdf metatags extraction is very expensive...
    if (!strstr (f->info->mime_type, "pdf"))  return;

    gchar *fname = gnome_cmd_file_get_real_path (f);

    DEBUG('t', "Loading PDF metadata for '%s'\n", fname);

    PDFDoc doc(new GooString(fname));

    g_free (fname);

    if (!doc.isOk())
        return;

    f->metadata->mark_as_accessed(TAG_DOC);

#ifdef POPPLER_HAS_GET_PDF_VERSION
    f->metadata->addf(TAG_PDF_VERSION, "%.1f", doc.getPDFVersion());
#else
    f->metadata->addf(TAG_PDF_VERSION, "%u.%u", doc.getPDFMajorVersion(), doc.getPDFMinorVersion());
#endif

    f->metadata->addf(TAG_DOC_PAGECOUNT, "%i", doc.getNumPages());
    f->metadata->addf(TAG_PDF_OPTIMIZED, "%u", doc.isLinearized());

    f->metadata->addf(TAG_DOC_SECURITY, "%u", doc.isEncrypted());   //  FIXME: 1 -> "Password protected", 0 -> "No protection" ???

    f->metadata->addf(TAG_PDF_PRINTING, "%u", doc.okToPrint());
    f->metadata->addf(TAG_PDF_HIRESPRINTING, "%u", doc.okToPrintHighRes());
    f->metadata->addf(TAG_PDF_MODIFYING, "%u", doc.okToChange());
    f->metadata->addf(TAG_PDF_COPYING, "%u", doc.okToCopy());
    f->metadata->addf(TAG_PDF_COMMENTING, "%u", doc.okToAddNotes());
    f->metadata->addf(TAG_PDF_FORMFILLING, "%u", doc.okToFillForm());
    f->metadata->addf(TAG_PDF_ACCESSIBILITYSUPPORT, "%u", doc.okToAccessibility());
    f->metadata->addf(TAG_PDF_DOCASSEMBLY, "%u", doc.okToAssemble());

    if (doc.getNumPages()>0)
    {
        double width = doc.getPageCropWidth(1)/72.0f*25.4f;
        double height = doc.getPageCropHeight(1)/72.0f*25.4f;

        f->metadata->addf(TAG_PDF_PAGEWIDTH, "%.0f", width);
        f->metadata->addf(TAG_PDF_PAGEHEIGHT, "%.0f", height);

        gchar *paper_size = paper_name (width, height);

        f->metadata->add(TAG_PDF_PAGESIZE, paper_size);

        g_free (paper_size);
    }

    Catalog *catalog = doc.getCatalog();

    if (catalog)
        f->metadata->addf(TAG_PDF_EMBEDDEDFILES, "%i", catalog->numEmbeddedFiles());

    // GooString *xmp = doc.readMetadata();         // FIXME: future access to XMP metadata

    // if (xmp)
    // {
        // TRACE(xmp->getCString());
    // }

    // delete xmp;

    Object info;

    doc.getDocInfo(&info);

    if (info.isDict())
    {
        Object obj;
        Dict *dict = info.getDict();

        if (dict->lookup("Title", &obj)->isString())
            add(*f->metadata, TAG_DOC_TITLE, obj.getString());

        if (dict->lookup("Subject", &obj)->isString())
            add(*f->metadata, TAG_DOC_SUBJECT, obj.getString());

        if (dict->lookup("Keywords", &obj)->isString())
            add(*f->metadata, TAG_DOC_KEYWORDS, TAG_FILE_KEYWORDS, obj.getString());        //  FIXME:  split keywords here

        if (dict->lookup("Author", &obj)->isString())
            add(*f->metadata, TAG_DOC_AUTHOR, TAG_FILE_PUBLISHER, obj.getString());

        if (dict->lookup("Creator", &obj)->isString())
            add(*f->metadata, TAG_PDF_PRODUCER, obj.getString());

        if (dict->lookup("Producer", &obj)->isString())
            add(*f->metadata, TAG_DOC_GENERATOR, obj.getString());

        if (dict->lookup("CreationDate", &obj)->isString())
            add_date(*f->metadata, TAG_DOC_DATECREATED, obj.getString());

        if (dict->lookup("ModDate", &obj)->isString())
            add_date(*f->metadata, TAG_DOC_DATEMODIFIED, obj.getString());

        obj.free();
    }

    info.free();
#endif
}

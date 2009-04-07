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

/*
    Code for OLE2 data processing comes from libextractor and is adjusted to GNOME Commander needs.

        src/plugins/ole2/ole2extractor.c - 2006-12-29

    This file is part of libextractor.
    (C) 2004, 2005, 2006 Vidyut Samanta and Christian Grothoff
    (C) 2002-2004 Jody Goldberg (jody@gnome.org)

    Part of this code was borrowed from WordLeaker (http://elligre.tk/madelman/)
    (C) 2005 Madelman
*/

#include <config.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags-doc.h"
#include "utils.h"
#include "dict.h"

#ifdef HAVE_GSF
#include <gsf/gsf-infile.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-meta-names.h>
#include <gsf/gsf-msole-utils.h>
#include <gsf/gsf-opendoc-utils.h>
#include <gsf/gsf-utils.h>
#endif

using namespace std;


#ifdef HAVE_GSF
static DICT<GnomeCmdTag> gsf_tags(TAG_NONE);

inline const gchar *lid2lang (guint lid)
{
    switch (lid)
    {
        case 0x0400:
            return _("No Proofing");
        case 0x0401:
            return _("Arabic");
        case 0x0402:
            return _("Bulgarian");
        case 0x0403:
            return _("Catalan");
        case 0x0404:
            return _("Traditional Chinese");
        case 0x0804:
            return _("Simplified Chinese");
        case 0x0405:
            return _("Chechen");
        case 0x0406:
            return _("Danish");
        case 0x0407:
            return _("German");
        case 0x0807:
            return _("Swiss German");
        case 0x0408:
            return _("Greek");
        case 0x0409:
            return _("U.S. English");
        case 0x0809:
            return _("U.K. English");
        case 0x0c09:
            return _("Australian English");
        case 0x040a:
            return _("Castilian Spanish");
        case 0x080a:
            return _("Mexican Spanish");
        case 0x040b:
            return _("Finnish");
        case 0x040c:
            return _("French");
        case 0x080c:
            return _("Belgian French");
        case 0x0c0c:
            return _("Canadian French");
        case 0x100c:
            return _("Swiss French");
        case 0x040d:
            return _("Hebrew");
        case 0x040e:
            return _("Hungarian");
        case 0x040f:
            return _("Icelandic");
        case 0x0410:
            return _("Italian");
        case 0x0810:
            return _("Swiss Italian");
        case 0x0411:
            return _("Japanese");
        case 0x0412:
            return _("Korean");
        case 0x0413:
            return _("Dutch");
        case 0x0813:
            return _("Belgian Dutch");
        case 0x0414:
            return _("Norwegian Bokmal");
        case 0x0814:
            return _("Norwegian Nynorsk");
        case 0x0415:
            return _("Polish");
        case 0x0416:
            return _("Brazilian Portuguese");
        case 0x0816:
            return _("Portuguese");
        case 0x0417:
            return _("Rhaeto-Romanic");
        case 0x0418:
            return _("Romanian");
        case 0x0419:
            return _("Russian");
        case 0x041a:
            return _("Croato-Serbian (Latin)");
        case 0x081a:
            return _("Serbo-Croatian (Cyrillic)");
        case 0x041b:
            return _("Slovak");
        case 0x041c:
            return _("Albanian");
        case 0x041d:
            return _("Swedish");
        case 0x041e:
            return _("Thai");
        case 0x041f:
            return _("Turkish");
        case 0x0420:
            return _("Urdu");
        case 0x0421:
            return _("Indonesian");
        case 0x0422:
            return _("Ukrainian");
        case 0x0423:
            return _("Byelorussian");
        case 0x0424:
            return _("Slovenian");
        case 0x0425:
            return _("Estonian");
        case 0x0426:
            return _("Latvian");
        case 0x0427:
            return _("Lithuanian");
        case 0x0429:
            return _("Farsi");
        case 0x042D:
            return _("Basque");
        case 0x042F:
            return _("Macedonian");
        case 0x0436:
            return _("Afrikaans");
        case 0x043E:
            return _("Malayalam");

        default:
            return NULL;
    }
}


static void process_metadata(gpointer key, gpointer value, gpointer user_data)
{
    char *type = (char *) key;
    const GsfDocProp *prop = (const GsfDocProp *) value;
    GnomeCmdFileMetadata *metadata = (GnomeCmdFileMetadata *) user_data;

    if (!key || !value || !metadata)  return;

    GnomeCmdTag id = gsf_tags[type];

    if (id==TAG_NONE)  return;

    const GValue *gval = gsf_doc_prop_get_val (prop);

    gchar *contents = G_VALUE_TYPE (gval)==G_TYPE_STRING ? g_strdup (g_value_get_string (gval)) : g_strdup_value_contents (gval);

    if (contents)
    {
        switch (id)
        {
            case TAG_DOC_DATECREATED:
            case TAG_DOC_DATEMODIFIED:
            case TAG_DOC_LASTPRINTED:
            case TAG_DOC_PRINTDATE:
                g_strcanon(contents, "0123456789:-", ' ');
                break;

            // case TAG_DOC_EDITINGDURATION: ??
               // break;

            case TAG_DOC_DESCRIPTION:
                metadata->add(TAG_FILE_DESCRIPTION,contents);
                break;

            case TAG_DOC_KEYWORDS:
                metadata->add(TAG_FILE_KEYWORDS,contents);
                break;

            case TAG_DOC_AUTHOR:
            case TAG_DOC_CREATOR:
                metadata->add(TAG_FILE_PUBLISHER,contents);
                break;

            default:
                break;
        }

        g_strdelimit (contents, "\t\n\r", ' ');
        g_strstrip (contents);
        DEBUG('t', "\t\t%s (%s) = %s\n", type, gcmd_tags_get_name(id), contents);
        metadata->add(id,contents);
    }

    g_free (contents);
}


inline void process_opendoc_infile(GsfInfile *infile, GnomeCmdFileMetadata *metadata)
{
    GsfInput *meta_file = gsf_infile_child_by_name (infile, "meta.xml");

    if (!meta_file)
        return;

    GsfDocMetaData *sections = gsf_doc_meta_data_new ();
    GError         *err = gsf_opendoc_metadata_read (meta_file, sections);

    if (!err)
        gsf_doc_meta_data_foreach (sections, &process_metadata, metadata);
    else
        g_error_free (err);

    g_object_unref (G_OBJECT (sections));
}


inline void process_msole_summary(GsfInput *input, GnomeCmdFileMetadata *metadata)
{
    GsfDocMetaData *sections = gsf_doc_meta_data_new ();
    GError         *err = gsf_msole_metadata_read (input, sections);

    if (!err)
        gsf_doc_meta_data_foreach (sections, &process_metadata, metadata);
    else
        g_error_free (err);

    g_object_unref (G_OBJECT (sections));
}


inline void process_msole_SO(GsfInput *input, GnomeCmdFileMetadata *metadata)
{
    static const gchar SfxDocumentInfo[] = "SfxDocumentInfo";

    off_t size = gsf_input_size (input);

    if (size < 0x374) // == 0x375 ??
        return;

    gchar *buf = (gchar *) g_malloc0(size);
    gsf_input_read(input, size, (guint8 *) buf);
    if ((buf[0] != 0x0F) || (buf[1] != 0x0) ||
      !g_str_has_prefix (&buf[2], SfxDocumentInfo) ||
      (buf[0x11] != 0x0B) ||
      (buf[0x13] != 0x00) || /*pw protected! */
      (buf[0x12] != 0x00))
    {
        g_free (buf);
        return;
    }

     // buf[0xd3] = '\0';
     // if (buf[0x94] + buf[0x93] > 0)
       // addKeyword(prev, &buf[0x95], TAG_DOC_TITLE);
     // buf[0x114] = '\0';
     // if (buf[0xd5] + buf[0xd4] > 0)
       // addKeyword(prev, &buf[0xd6], TAG_DOC_SUBJECT);
     // buf[0x215] = '\0';
     // if (buf[0x115] + buf[0x116] > 0)
       // addKeyword(prev, &buf[0x117], TAG_COMMENT_COMMENT);
     // buf[0x296] = '\0';
     // if (buf[0x216] + buf[0x217] > 0)
       // addKeyword(prev, &buf[0x218], TAG_DOC_KEYWORDS);

    // fixme: do timestamps, mime-type, user-defined info's

    g_free (buf);
}


inline void process_msole_infile(GsfInfile *infile, GnomeCmdFileMetadata *metadata)
{
    // static gchar *names[] = {"\005SummaryInformation", "\005DocumentSummaryInformation", "SfxDocumentInfo", NULL};

    // GsfInput *src = NULL;

    // src = gsf_infile_child_by_name (infile, "\005SummaryInformation");

    // if (src)
    // {
        // process_msole_summary (src, metadata);
        // g_object_unref (G_OBJECT(src));
    // }

    // src = gsf_infile_child_by_name (infile, "\005DocumentSummaryInformation");

    // if (src)
    // {
        // process_msole_summary (src, metadata);
        // g_object_unref (G_OBJECT(src));
    // }

    // src = gsf_infile_child_by_name (infile, "SfxDocumentInfo");

    // if (src)
    // {
        // process_msole_SO (src, metadata);
        // g_object_unref (G_OBJECT(src));
    // }

    for (gint i=0; i<gsf_infile_num_children (infile); i++)
    {
        const gchar *name = gsf_infile_name_by_index (infile, i);

        if (!name)
            continue;

        DEBUG('t', "\tname[%i]=%s\n", i, name);

        if (strcmp(name, "\005SummaryInformation")==0 || strcmp(name, "\005DocumentSummaryInformation")==0)
        {
            GsfInput *src = gsf_infile_child_by_index (infile, i);

            if (src)
            {
                process_msole_summary (src, metadata);
                g_object_unref (G_OBJECT (src));
            }

            continue;
        }

        if (strcmp(name, "SfxDocumentInfo")==0)
        {
            GsfInput *src = gsf_infile_child_by_index (infile, i);

            if (src)
            {
                process_msole_SO (src, metadata);
                g_object_unref (G_OBJECT (src));
            }

            continue;
        }
    }
}
#endif


void gcmd_tags_libgsf_init()
{
#ifdef HAVE_GSF
    static struct
    {
        GnomeCmdTag tag;
        const gchar *name;
    }
    gsf_data[] = {
                    {TAG_DOC_BYTECOUNT, GSF_META_NAME_BYTE_COUNT},
                    {TAG_DOC_CASESENSITIVE, GSF_META_NAME_CASE_SENSITIVE},
                    {TAG_DOC_CATEGORY, GSF_META_NAME_CATEGORY},
                    {TAG_DOC_CELLCOUNT, GSF_META_NAME_CELL_COUNT},
                    {TAG_DOC_CHARACTERCOUNT, GSF_META_NAME_CHARACTER_COUNT},
                    {TAG_DOC_CODEPAGE, GSF_META_NAME_CODEPAGE},
                    {TAG_DOC_COMPANY, GSF_META_NAME_COMPANY},
                    {TAG_DOC_CREATOR, GSF_META_NAME_CREATOR},
                    {TAG_DOC_DATECREATED, GSF_META_NAME_DATE_CREATED},
                    {TAG_DOC_DATEMODIFIED, GSF_META_NAME_DATE_MODIFIED},
                    {TAG_DOC_DESCRIPTION, GSF_META_NAME_DESCRIPTION},
                    {TAG_DOC_DICTIONARY, GSF_META_NAME_DICTIONARY},
                    {TAG_DOC_EDITINGDURATION, GSF_META_NAME_EDITING_DURATION},
                    {TAG_DOC_GENERATOR, GSF_META_NAME_GENERATOR},
                    {TAG_DOC_HIDDENSLIDECOUNT, GSF_META_NAME_HIDDEN_SLIDE_COUNT},
                    {TAG_DOC_IMAGECOUNT, GSF_META_NAME_IMAGE_COUNT},
                    {TAG_DOC_INITIALCREATOR, GSF_META_NAME_INITIAL_CREATOR},
                    {TAG_DOC_KEYWORDS, GSF_META_NAME_KEYWORD},
                    {TAG_DOC_KEYWORDS, GSF_META_NAME_KEYWORDS},
                    {TAG_DOC_LANGUAGE, GSF_META_NAME_LANGUAGE},
                    {TAG_DOC_LASTPRINTED, GSF_META_NAME_LAST_PRINTED},
                    {TAG_DOC_LASTSAVEDBY, GSF_META_NAME_LAST_SAVED_BY},
                    {TAG_DOC_LINECOUNT, GSF_META_NAME_LINE_COUNT},
                    {TAG_DOC_LINKSDIRTY, GSF_META_NAME_LINKS_DIRTY},
                    {TAG_DOC_LOCALESYSTEMDEFAULT, GSF_META_NAME_LOCALE_SYSTEM_DEFAULT},
                    {TAG_DOC_MANAGER, GSF_META_NAME_MANAGER},
                    {TAG_DOC_MMCLIPCOUNT, GSF_META_NAME_MM_CLIP_COUNT},
                    {TAG_DOC_NOTECOUNT, GSF_META_NAME_NOTE_COUNT},
                    {TAG_DOC_OBJECTCOUNT, GSF_META_NAME_OBJECT_COUNT},
                    {TAG_DOC_PAGECOUNT, GSF_META_NAME_PAGE_COUNT},
                    {TAG_DOC_PARAGRAPHCOUNT, GSF_META_NAME_PARAGRAPH_COUNT},
                    {TAG_DOC_PRESENTATIONFORMAT, GSF_META_NAME_PRESENTATION_FORMAT},
                    {TAG_DOC_PRINTDATE, GSF_META_NAME_PRINT_DATE},
                    {TAG_DOC_PRINTEDBY, GSF_META_NAME_PRINTED_BY},
                    {TAG_DOC_REVISIONCOUNT, GSF_META_NAME_REVISION_COUNT},
                    {TAG_DOC_SCALE, GSF_META_NAME_SCALE},
                    {TAG_DOC_SECURITY, GSF_META_NAME_SECURITY},
                    {TAG_DOC_SLIDECOUNT, GSF_META_NAME_SLIDE_COUNT},
                    {TAG_DOC_SPREADSHEETCOUNT, GSF_META_NAME_SPREADSHEET_COUNT},
                    {TAG_DOC_SUBJECT, GSF_META_NAME_SUBJECT},
                    {TAG_DOC_TABLECOUNT, GSF_META_NAME_TABLE_COUNT},
                    {TAG_DOC_TEMPLATE, GSF_META_NAME_TEMPLATE},
                    {TAG_DOC_TITLE, GSF_META_NAME_TITLE},
                    {TAG_DOC_WORDCOUNT, GSF_META_NAME_WORD_COUNT}
                };

    load_data (gsf_tags, gsf_data, G_N_ELEMENTS(gsf_data));

    gsf_init();
#endif
}


void gcmd_tags_libgsf_shutdown()
{
#ifdef HAVE_GSF
    gsf_shutdown();
#endif
}


void gcmd_tags_libgsf_load_metadata(GnomeCmdFile *f)
{
    g_return_if_fail (f != NULL);
    g_return_if_fail (f->info != NULL);

#ifdef HAVE_GSF
    if (f->metadata && f->metadata->is_accessed(TAG_DOC))  return;

    if (!f->metadata)
        f->metadata = new GnomeCmdFileMetadata;

    if (!f->metadata)  return;

    f->metadata->mark_as_accessed(TAG_DOC);

    if (!gnome_cmd_file_is_local(f))  return;

    GError *err = NULL;
    gchar *fname = gnome_cmd_file_get_real_path (f);

    DEBUG('t', "Loading doc metadata for '%s'\n", fname);

    GsfInput *input = gsf_input_mmap_new (fname, NULL);
    if (!input)
        input = gsf_input_stdio_new (fname, &err);

    if (!input)
    {
        g_return_if_fail (err != NULL);
        g_warning ("'%s' error: %s", fname, err->message);
        g_error_free (err);
        g_free (fname);
        return;
    }

    g_free (fname);

    GsfInfile *infile = NULL;

    if ((infile = gsf_infile_msole_new (input, NULL)))
        process_msole_infile(infile, f->metadata);
    else
        if ((infile = gsf_infile_zip_new (input, NULL)))
            process_opendoc_infile(infile, f->metadata);

    if (infile)
        g_object_unref (G_OBJECT (infile));

    if (err)
        g_error_free (err);

    g_object_unref (G_OBJECT (input));
#endif
}

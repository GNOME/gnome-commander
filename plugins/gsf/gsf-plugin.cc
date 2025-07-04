/**
 * @file gsf-plugin.cc
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
#include <glib.h>
#include <glib/gi18n.h>
#include <libgcmd.h>
#include <gsf/gsf.h>


extern "C" GObject *create_plugin ();
extern "C" GnomeCmdPluginInfo *get_plugin_info ();


using namespace std;


#define GNOME_CMD_TYPE_GSF_PLUGIN (gnome_cmd_gsf_plugin_get_type ())
G_DECLARE_FINAL_TYPE (GnomeCmdGsfPlugin, gnome_cmd_gsf_plugin, GNOME_CMD, GSF_PLUGIN, GObject)

struct _GnomeCmdGsfPlugin
{
    GObject parent;
};

static void gnome_cmd_file_metadata_extractor_init (GnomeCmdFileMetadataExtractorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GnomeCmdGsfPlugin, gnome_cmd_gsf_plugin, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GNOME_CMD_TYPE_FILE_METADATA_EXTRACTOR, gnome_cmd_file_metadata_extractor_init))


static GStrv supported_tags (GnomeCmdFileMetadataExtractor *);
static GStrv summary_tags (GnomeCmdFileMetadataExtractor *);
static gchar *class_name (GnomeCmdFileMetadataExtractor *, const gchar *);
static gchar *tag_name (GnomeCmdFileMetadataExtractor *, const gchar *);
static gchar *tag_description (GnomeCmdFileMetadataExtractor *, const gchar *);
static void extract_metadata (GnomeCmdFileMetadataExtractor *,
                              GnomeCmdFileDescriptor *,
                              GnomeCmdFileMetadataExtractorAddTag,
                              gpointer);


struct DocTag
{
    const char *id;
    const char *name;
    const char *description;
};

static DocTag TAG_DOC_AUTHOR              = { "Doc.Author",               N_("Author"),                 N_("Name of the author.") };
static DocTag TAG_DOC_CREATOR             = { "Doc.Creator",              N_("Creator"),                N_("An entity primarily responsible for making the content of the resource, typically a person, organization, or service.") };
static DocTag TAG_DOC_TITLE               = { "Doc.Title",                N_("Title"),                  N_("Title of the document.") };
static DocTag TAG_DOC_SUBJECT             = { "Doc.Subject",              N_("Subject"),                N_("Document subject.") };
static DocTag TAG_DOC_DESCRIPTION         = { "Doc.Description",          N_("Description"),            N_("An account of the content of the resource.") };
static DocTag TAG_DOC_CATEGORY            = { "Doc.Category",             N_("Category"),               N_("Category.") };
static DocTag TAG_DOC_KEYWORDS            = { "Doc.Keywords",             N_("Keywords"),               N_("Searchable, indexable keywords.") };
static DocTag TAG_DOC_REVISIONCOUNT       = { "Doc.RevisionCount",        N_("Revision Count"),         N_("Number of revision on the document.") };
static DocTag TAG_DOC_PAGECOUNT           = { "Doc.PageCount",            N_("Page Count"),             N_("Number of pages in the document.") };
static DocTag TAG_DOC_PARAGRAPHCOUNT      = { "Doc.ParagraphCount",       N_("Paragraph Count"),        N_("Number of paragraphs in the document.") };
static DocTag TAG_DOC_LINECOUNT           = { "Doc.LineCount",            N_("Line Count"),             N_("Number of lines in the document.") };
static DocTag TAG_DOC_WORDCOUNT           = { "Doc.WordCount",            N_("Word Count"),             N_("Number of words in the document.") };
static DocTag TAG_DOC_BYTECOUNT           = { "Doc.ByteCount",            N_("Byte Count"),             N_("Number of bytes in the document.") };
static DocTag TAG_DOC_CELLCOUNT           = { "Doc.CellCount",            N_("Cell Count"),             N_("Number of cells in the spreadsheet document.") };
static DocTag TAG_DOC_CHARACTERCOUNT      = { "Doc.CharacterCount",       N_("Character Count"),        N_("Number of characters in the document.") };
static DocTag TAG_DOC_CODEPAGE            = { "Doc.Codepage",             N_("Codepage"),               N_("The MS codepage to encode strings for metadata.") };
static DocTag TAG_DOC_COMMENTS            = { "Doc.Comments",             N_("Comments"),               N_("User definable free text.") };
static DocTag TAG_DOC_COMPANY             = { "Doc.Company",              N_("Company"),                N_("Organization that the <Doc.Creator> entity is associated with.") };
static DocTag TAG_DOC_DATECREATED         = { "Doc.DateCreated",          N_("Date Created"),           N_("Datetime document was originally created.") };
static DocTag TAG_DOC_DATEMODIFIED        = { "Doc.DateModified",         N_("Date Modified"),          N_("The last time the document was saved.") };
static DocTag TAG_DOC_DICTIONARY          = { "Doc.Dictionary",           N_("Dictionary"),             N_("Dictionary.") };
static DocTag TAG_DOC_EDITINGDURATION     = { "Doc.EditingDuration",      N_("Editing Duration"),       N_("The total time taken until the last modification.") };
static DocTag TAG_DOC_GENERATOR           = { "Doc.Generator",            N_("Generator"),              N_("The application that generated this document.") };
static DocTag TAG_DOC_HIDDENSLIDECOUNT    = { "Doc.HiddenSlideCount",     N_("Hidden Slide Count"),     N_("Number of hidden slides in the presentation document.") };
static DocTag TAG_DOC_IMAGECOUNT          = { "Doc.ImageCount",           N_("Image Count"),            N_("Number of images in the document.") };
static DocTag TAG_DOC_INITIALCREATOR      = { "Doc.InitialCreator",       N_("Initial Creator"),        N_("Specifies the name of the person who created the document initially.") };
static DocTag TAG_DOC_LANGUAGE            = { "Doc.Language",             N_("Language"),               N_("The locale language of the intellectual content of the resource.") };
static DocTag TAG_DOC_LASTPRINTED         = { "Doc.LastPrinted",          N_("Last Printed"),           N_("The last time this document was printed.") };
static DocTag TAG_DOC_LASTSAVEDBY         = { "Doc.LastSavedBy",          N_("Last Saved By"),          N_("The entity that made the last change to the document, typically a person, organization, or service.") };
static DocTag TAG_DOC_LOCALESYSTEMDEFAULT = { "Doc.LocaleSystemDefault",  N_("Locale System Default"),  N_("Identifier representing the default system locale.") };
static DocTag TAG_DOC_MMCLIPCOUNT         = { "Doc.MMClipCount",          N_("Multimedia Clip Count"),  N_("Number of multimedia clips in the document.") };
static DocTag TAG_DOC_MANAGER             = { "Doc.Manager",              N_("Manager"),                N_("Name of the manager of <Doc.Creator> entity.") };
static DocTag TAG_DOC_NOTECOUNT           = { "Doc.NoteCount",            N_("Note Count"),             N_("Number of “notes” in the document.") };
static DocTag TAG_DOC_OBJECTCOUNT         = { "Doc.ObjectCount",          N_("Object Count"),           N_("Number of objects (OLE and other graphics) in the document.") };
static DocTag TAG_DOC_PRESENTATIONFORMAT  = { "Doc.PresentationFormat",   N_("Presentation Format"),    N_("Type of presentation, like “On-screen Show”, “SlideView”, etc.") };
static DocTag TAG_DOC_PRINTDATE           = { "Doc.PrintDate",            N_("Print Date"),             N_("Specifies the date and time when the document was last printed.") };
static DocTag TAG_DOC_PRINTEDBY           = { "Doc.PrintedBy",            N_("Printed By"),             N_("Specifies the name of the last person who printed the document.") };
static DocTag TAG_DOC_SCALE               = { "Doc.Scale",                N_("Scale"),                  N_("Scale.") };
static DocTag TAG_DOC_SECURITY            = { "Doc.Security",             N_("Security"),               N_("One of: “Password protected”, “Read-only recommended”, “Read-only enforced” or “Locked for annotations”.") };
static DocTag TAG_DOC_SLIDECOUNT          = { "Doc.SlideCount",           N_("Slide Count"),            N_("Number of slides in the presentation document.") };
static DocTag TAG_DOC_SPREADSHEETCOUNT    = { "Doc.SpreadsheetCount",     N_("Spreadsheet Count"),      N_("Number of pages in the document.") };
static DocTag TAG_DOC_TABLECOUNT          = { "Doc.TableCount",           N_("Table Count"),            N_("Number of tables in the document.") };
static DocTag TAG_DOC_TEMPLATE            = { "Doc.Template",             N_("Template"),               N_("The template file that is been used to generate this document.") };
static DocTag TAG_DOC_CASESENSITIVE       = { "Doc.CaseSensitive",        N_("Case Sensitive"),         N_("Case sensitive.") };
static DocTag TAG_DOC_LINKSDIRTY          = { "Doc.LinksDirty",           N_("Links Dirty"),            N_("Links dirty.") };

static DocTag* DOC_TAGS[] = {
    &TAG_DOC_AUTHOR,
    &TAG_DOC_CREATOR,
    &TAG_DOC_TITLE,
    &TAG_DOC_SUBJECT,
    &TAG_DOC_DESCRIPTION,
    &TAG_DOC_CATEGORY,
    &TAG_DOC_KEYWORDS,
    &TAG_DOC_REVISIONCOUNT,
    &TAG_DOC_PAGECOUNT,
    &TAG_DOC_PARAGRAPHCOUNT,
    &TAG_DOC_LINECOUNT,
    &TAG_DOC_WORDCOUNT,
    &TAG_DOC_BYTECOUNT,
    &TAG_DOC_CELLCOUNT,
    &TAG_DOC_CHARACTERCOUNT,
    &TAG_DOC_CODEPAGE,
    &TAG_DOC_COMMENTS,
    &TAG_DOC_COMPANY,
    &TAG_DOC_DATECREATED,
    &TAG_DOC_DATEMODIFIED,
    &TAG_DOC_DICTIONARY,
    &TAG_DOC_EDITINGDURATION,
    &TAG_DOC_GENERATOR,
    &TAG_DOC_HIDDENSLIDECOUNT,
    &TAG_DOC_IMAGECOUNT,
    &TAG_DOC_INITIALCREATOR,
    &TAG_DOC_LANGUAGE,
    &TAG_DOC_LASTPRINTED,
    &TAG_DOC_LASTSAVEDBY,
    &TAG_DOC_LOCALESYSTEMDEFAULT,
    &TAG_DOC_MMCLIPCOUNT,
    &TAG_DOC_MANAGER,
    &TAG_DOC_NOTECOUNT,
    &TAG_DOC_OBJECTCOUNT,
    &TAG_DOC_PRESENTATIONFORMAT,
    &TAG_DOC_PRINTDATE,
    &TAG_DOC_PRINTEDBY,
    &TAG_DOC_SCALE,
    &TAG_DOC_SECURITY,
    &TAG_DOC_SLIDECOUNT,
    &TAG_DOC_SPREADSHEETCOUNT,
    &TAG_DOC_TABLECOUNT,
    &TAG_DOC_TEMPLATE,
    &TAG_DOC_CASESENSITIVE,
    &TAG_DOC_LINKSDIRTY
};


struct Context
{
    GnomeCmdGsfPlugin *plugin;
    GnomeCmdFileMetadataExtractorAddTag add;
    gpointer user_data;
};


static gchar *clean_value (gchar *value)
{
    g_strdelimit (value, "\t\n\r", ' ');
    g_strstrip (value);
    return value;
}


static gchar *clean_date_value (gchar *value)
{
    g_strcanon (value, "0123456789:-", ' ');
    g_strstrip (value);
    return value;
}


static const char *TAG_FILE_PUBLISHER   = "File.Publisher";
static const char *TAG_FILE_DESCRIPTION = "File.Description";
static const char *TAG_FILE_KEYWORDS    = "File.Keywords";


static void process_metadata(gpointer key, gpointer value, gpointer user_data)
{
    char *type = (char *) key;
    const GsfDocProp *prop = (const GsfDocProp *) value;
    auto context = static_cast<Context*> (user_data);
    auto add = context->add;
    auto add_data = context->user_data;

    if (!key || !value)  return;

    const GValue *gval = gsf_doc_prop_get_val (prop);

    gchar *contents = G_VALUE_TYPE (gval)==G_TYPE_STRING ? g_strdup (g_value_get_string (gval)) : g_strdup_value_contents (gval);

    if (!contents) return;
    else if (!g_strcmp0 (type, GSF_META_NAME_CREATOR))               { clean_value(contents);
                                                                       add(TAG_DOC_CREATOR.id,             contents,                   add_data);
                                                                       add(TAG_FILE_PUBLISHER,             contents,                   add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_TITLE))                 { add(TAG_DOC_TITLE.id,               clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_SUBJECT))               { add(TAG_DOC_SUBJECT.id,             clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_DESCRIPTION))           { clean_value(contents);
                                                                       add(TAG_DOC_DESCRIPTION.id,         contents,                   add_data);
                                                                       add(TAG_FILE_DESCRIPTION,           contents,                   add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_CATEGORY))              { add(TAG_DOC_CATEGORY.id,            clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_KEYWORD))               { clean_value(contents);
                                                                       add(TAG_DOC_KEYWORDS.id,            contents,                   add_data);
                                                                       add(TAG_FILE_KEYWORDS,              contents,                   add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_KEYWORDS))              { clean_value(contents);
                                                                       add(TAG_DOC_KEYWORDS.id,            contents,                   add_data);
                                                                       add(TAG_FILE_KEYWORDS,              contents,                   add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_REVISION_COUNT))        { add(TAG_DOC_REVISIONCOUNT.id,       clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_PAGE_COUNT))            { add(TAG_DOC_PAGECOUNT.id,           clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_PARAGRAPH_COUNT))       { add(TAG_DOC_PARAGRAPHCOUNT.id,      clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_LINE_COUNT))            { add(TAG_DOC_LINECOUNT.id,           clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_WORD_COUNT))            { add(TAG_DOC_WORDCOUNT.id,           clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_BYTE_COUNT))            { add(TAG_DOC_BYTECOUNT.id,           clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_CELL_COUNT))            { add(TAG_DOC_CELLCOUNT.id,           clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_CHARACTER_COUNT))       { add(TAG_DOC_CHARACTERCOUNT.id,      clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_CODEPAGE))              { add(TAG_DOC_CODEPAGE.id,            clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_COMPANY))               { add(TAG_DOC_COMPANY.id,             clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_DATE_CREATED))          { add(TAG_DOC_DATECREATED.id,         clean_date_value(contents), add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_DATE_MODIFIED))         { add(TAG_DOC_DATEMODIFIED.id,        clean_date_value(contents), add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_DICTIONARY))            { add(TAG_DOC_DICTIONARY.id,          clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_EDITING_DURATION))      { add(TAG_DOC_EDITINGDURATION.id,     clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_GENERATOR))             { add(TAG_DOC_GENERATOR.id,           clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_HIDDEN_SLIDE_COUNT))    { add(TAG_DOC_HIDDENSLIDECOUNT.id,    clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_IMAGE_COUNT))           { add(TAG_DOC_IMAGECOUNT.id,          clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_INITIAL_CREATOR))       { add(TAG_DOC_INITIALCREATOR.id,      clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_LANGUAGE))              { add(TAG_DOC_LANGUAGE.id,            clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_LAST_PRINTED))          { add(TAG_DOC_LASTPRINTED.id,         clean_date_value(contents), add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_LAST_SAVED_BY))         { add(TAG_DOC_LASTSAVEDBY.id,         clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_LOCALE_SYSTEM_DEFAULT)) { add(TAG_DOC_LOCALESYSTEMDEFAULT.id, clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_MM_CLIP_COUNT))         { add(TAG_DOC_MMCLIPCOUNT.id,         clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_MANAGER))               { add(TAG_DOC_MANAGER.id,             clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_NOTE_COUNT))            { add(TAG_DOC_NOTECOUNT.id,           clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_OBJECT_COUNT))          { add(TAG_DOC_OBJECTCOUNT.id,         clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_PRESENTATION_FORMAT))   { add(TAG_DOC_PRESENTATIONFORMAT.id,  clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_PRINT_DATE))            { add(TAG_DOC_PRINTDATE.id,           clean_date_value(contents), add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_PRINTED_BY))            { add(TAG_DOC_PRINTEDBY.id,           clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_SCALE))                 { add(TAG_DOC_SCALE.id,               clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_SECURITY))              { add(TAG_DOC_SECURITY.id,            clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_SLIDE_COUNT))           { add(TAG_DOC_SLIDECOUNT.id,          clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_SPREADSHEET_COUNT))     { add(TAG_DOC_SPREADSHEETCOUNT.id,    clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_TABLE_COUNT))           { add(TAG_DOC_TABLECOUNT.id,          clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_TEMPLATE))              { add(TAG_DOC_TEMPLATE.id,            clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_CASE_SENSITIVE))        { add(TAG_DOC_CASESENSITIVE.id,       clean_value(contents),      add_data); }
    else if (!g_strcmp0 (type, GSF_META_NAME_LINKS_DIRTY))           { add(TAG_DOC_LINKSDIRTY.id,          clean_value(contents),      add_data); }

    g_free (contents);
}


inline void process_opendoc_infile(GsfInfile *infile, Context *context)
{
    GsfInput *meta_file = gsf_infile_child_by_name (infile, "meta.xml");

    if (!meta_file)
        return;

    GsfDocMetaData *sections = gsf_doc_meta_data_new ();
    GError         *err = gsf_doc_meta_data_read_from_odf (sections, meta_file);

    if (!err)
        gsf_doc_meta_data_foreach (sections, &process_metadata, context);
    else
        g_error_free (err);

    g_object_unref (sections);
}


inline void process_msole_summary(GsfInput *input, Context *context)
{
    GsfDocMetaData *sections = gsf_doc_meta_data_new ();
    GError         *err = gsf_doc_meta_data_read_from_odf (sections, input);

    if (!err)
        gsf_doc_meta_data_foreach (sections, &process_metadata, context);
    else
        g_error_free (err);

    g_object_unref (sections);
}


inline void process_msole_SO(GsfInput *input, Context *context)
{
    static const gchar SfxDocumentInfo[] = "SfxDocumentInfo";

    off_t size = gsf_input_size (input);

    if (size < 0x374) // == 0x375 ??
        return;

    auto buf = static_cast<gchar*> (g_malloc0(size));
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


inline void process_msole_infile(GsfInfile *infile, Context *context)
{
    // static gchar *names[] = {"\005SummaryInformation", "\005DocumentSummaryInformation", "SfxDocumentInfo", nullptr};

    // GsfInput *src = nullptr;

    // src = gsf_infile_child_by_name (infile, "\005SummaryInformation");

    // if (src)
    // {
        // process_msole_summary (src, metadata);
        // g_object_unref (src);
    // }

    // src = gsf_infile_child_by_name (infile, "\005DocumentSummaryInformation");

    // if (src)
    // {
        // process_msole_summary (src, metadata);
        // g_object_unref (src);
    // }

    // src = gsf_infile_child_by_name (infile, "SfxDocumentInfo");

    // if (src)
    // {
        // process_msole_SO (src, metadata);
        // g_object_unref (src);
    // }

    for (gint i=0; i<gsf_infile_num_children (infile); i++)
    {
        const gchar *name = gsf_infile_name_by_index (infile, i);

        if (!name)
            continue;

        if (strcmp(name, "\005SummaryInformation")==0 || strcmp(name, "\005DocumentSummaryInformation")==0)
        {
            GsfInput *src = gsf_infile_child_by_index (infile, i);

            if (src)
            {
                process_msole_summary (src, context);
                g_object_unref (src);
            }

            continue;
        }

        if (strcmp(name, "SfxDocumentInfo")==0)
        {
            GsfInput *src = gsf_infile_child_by_index (infile, i);

            if (src)
            {
                process_msole_SO (src, context);
                g_object_unref (src);
            }

            continue;
        }
    }
}


GStrv supported_tags (GnomeCmdFileMetadataExtractor *plugin)
{
    GStrvBuilder *builder = g_strv_builder_new ();
    for (auto tag : DOC_TAGS)
        g_strv_builder_add (builder, tag->id);
    GStrv result = g_strv_builder_end (builder);
    g_strv_builder_unref (builder);
    return result;
}


GStrv summary_tags (GnomeCmdFileMetadataExtractor *plugin)
{
    GStrvBuilder *builder = g_strv_builder_new ();
    g_strv_builder_add (builder, TAG_DOC_TITLE.id);
    g_strv_builder_add (builder, TAG_DOC_PAGECOUNT.id);
    GStrv result = g_strv_builder_end (builder);
    g_strv_builder_unref (builder);
    return result;
}


gchar *class_name (GnomeCmdFileMetadataExtractor *plugin, const gchar *cls)
{
    if (!g_strcmp0("Doc", cls))
        return g_strdup(_("Doc"));
    return nullptr;
}


gchar *tag_name (GnomeCmdFileMetadataExtractor *plugin, const gchar *tag_id)
{
    for (auto tag : DOC_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->name));
    return nullptr;
}


gchar *tag_description (GnomeCmdFileMetadataExtractor *plugin, const gchar *tag_id)
{
    for (auto tag : DOC_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->description));
    return nullptr;
}


void extract_metadata (GnomeCmdFileMetadataExtractor *plugin,
    GnomeCmdFileDescriptor *fd,
    GnomeCmdFileMetadataExtractorAddTag add,
    gpointer user_data)
{
    auto file = gnome_cmd_file_descriptor_get_file (fd);

    gchar *fname = g_file_get_path (file);

    GError *err = nullptr;

    GsfInput *input = gsf_input_mmap_new (fname, nullptr);
    if (!input)
        input = gsf_input_stdio_new (fname, &err);

    if (!input)
    {
        g_return_if_fail (err != nullptr);
        g_warning ("'%s' error: %s", fname, err->message);
        g_error_free (err);
        g_free (fname);
        return;
    }

    g_free (fname);

    GsfInfile *infile = nullptr;

    Context context = { GNOME_CMD_GSF_PLUGIN (plugin), add, user_data };

    if ((infile = gsf_infile_msole_new (input, nullptr)))
        process_msole_infile(infile, &context);
    else
        if ((infile = gsf_infile_zip_new (input, nullptr)))
            process_opendoc_infile(infile, &context);

    if (infile)
        g_object_unref (infile);

    if (err)
        g_error_free (err);

    g_object_unref (input);
}


static void finalize (GObject *object)
{
    gsf_shutdown();

    G_OBJECT_CLASS (gnome_cmd_gsf_plugin_parent_class)-> finalize (object);
}


static void gnome_cmd_gsf_plugin_class_init (GnomeCmdGsfPluginClass *klass)
{
    G_OBJECT_CLASS (klass)-> finalize = finalize;
}


static void gnome_cmd_file_metadata_extractor_init (GnomeCmdFileMetadataExtractorInterface *iface)
{
    iface->supported_tags = supported_tags;
    iface->summary_tags = summary_tags;
    iface->class_name = class_name;
    iface->tag_name = tag_name;
    iface->tag_description = tag_description;
    iface->extract_metadata = extract_metadata;
}


static void gnome_cmd_gsf_plugin_init (GnomeCmdGsfPlugin *plugin)
{
    gsf_init();
}


GObject *create_plugin ()
{
    return G_OBJECT (g_object_new (GNOME_CMD_TYPE_GSF_PLUGIN, NULL));
}


GnomeCmdPluginInfo *get_plugin_info ()
{
    static const char *authors[] = {
        "Andrey Kutejko <andy128k@gmail.com>",
        nullptr
    };

    static GnomeCmdPluginInfo info = {
        .plugin_system_version = GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION,
        .name = "GSF",
        .version = PACKAGE_VERSION,
        .copyright = "Copyright \xc2\xa9 2025 Andrey Kutejko",
        .comments = nullptr,
        .authors = (gchar **) authors,
        .documenters = nullptr,
        .translator = nullptr,
        .webpage = "https://gcmd.github.io"
    };

    if (!info.comments)
        info.comments = g_strdup (_("A plugin for extracting metadata from document files using libgsf library."));

    return &info;
}

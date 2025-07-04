/**
 * @file poppler-plugin.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 * @copyright (C) 2024 Andrey Kutejko\n
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
#include <glib.h>
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgcmd.h>
#include <poppler/glib/poppler.h>


extern "C" GObject *create_plugin ();
extern "C" GnomeCmdPluginInfo *get_plugin_info ();


using namespace std;


#define GNOME_CMD_TYPE_POPPLER_PLUGIN (gnome_cmd_poppler_plugin_get_type ())
G_DECLARE_FINAL_TYPE (GnomeCmdPopplerPlugin, gnome_cmd_poppler_plugin, GNOME_CMD, POPPLER_PLUGIN, GObject)

struct _GnomeCmdPopplerPlugin
{
    GObject parent;
};

static void gnome_cmd_file_metadata_extractor_init (GnomeCmdFileMetadataExtractorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GnomeCmdPopplerPlugin, gnome_cmd_poppler_plugin, G_TYPE_OBJECT,
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


static const char *TAG_FILE_PUBLISHER  = "File.Publisher";
static const char *TAG_FILE_KEYWORDS   = "File.Keywords";

struct PdfTag
{
    const char *id;
    const char *name;
    const char *description;
};

static PdfTag TAG_DOC_PAGECOUNT            = { "Doc.PageCount",             N_("Page Count"),                   N_("Number of pages in the document.") };
static PdfTag TAG_DOC_SECURITY             = { "Doc.Security",              N_("Security"),                     N_("One of: “Password protected”, “Read-only recommended”, “Read-only enforced” or “Locked for annotations”.") };
static PdfTag TAG_DOC_TITLE                = { "Doc.Title",                 N_("Title"),                        N_("Title of the document.") };
static PdfTag TAG_DOC_SUBJECT              = { "Doc.Subject",               N_("Subject"),                      N_("Document subject.") };
static PdfTag TAG_DOC_KEYWORDS             = { "Doc.Keywords",              N_("Keywords"),                     N_("Searchable, indexable keywords.") };
static PdfTag TAG_DOC_AUTHOR               = { "Doc.Author",                N_("Author"),                       N_("Name of the author.") };
static PdfTag TAG_DOC_GENERATOR            = { "Doc.Generator",             N_("Generator"),                    N_("The application that generated this document.") };
static PdfTag TAG_DOC_DATECREATED          = { "Doc.DateCreated",           N_("Date Created"),                 N_("Datetime document was originally created.") };
static PdfTag TAG_DOC_DATEMODIFIED         = { "Doc.DateModified",          N_("Date Modified"),                N_("The last time the document was saved.") };

static PdfTag TAG_PDF_PAGESIZE             = { "PDF.PageSize",              N_("Page Size"),                    N_("Page size format.") };
static PdfTag TAG_PDF_PAGEWIDTH            = { "PDF.PageWidth",             N_("Page Width"),                   N_("Page width in mm.") };
static PdfTag TAG_PDF_PAGEHEIGHT           = { "PDF.PageHeight",            N_("Page Height"),                  N_("Page height in mm.") };
static PdfTag TAG_PDF_VERSION              = { "PDF.Version",               N_("PDF Version"),                  N_("The PDF version of the document.") };
static PdfTag TAG_PDF_PRODUCER             = { "PDF.Producer",              N_("Producer"),                     N_("The application that converted the document to PDF.") };
static PdfTag TAG_PDF_EMBEDDEDFILES        = { "PDF.EmbeddedFiles",         N_("Embedded Files"),               N_("Number of embedded files in the document.") };
static PdfTag TAG_PDF_OPTIMIZED            = { "PDF.Optimized",             N_("Fast Web View"),                N_("Set to “1” if optimized for network access.") };
static PdfTag TAG_PDF_PRINTING             = { "PDF.Printing",              N_("Printing"),                     N_("Set to “1” if printing is allowed.") };
static PdfTag TAG_PDF_HIRESPRINTING        = { "PDF.HiResPrinting",         N_("Printing in High Resolution"),  N_("Set to “1” if high resolution printing is allowed.") };
static PdfTag TAG_PDF_COPYING              = { "PDF.Copying",               N_("Copying"),                      N_("Set to “1” if copying the contents is allowed.") };
static PdfTag TAG_PDF_MODIFYING            = { "PDF.Modifying",             N_("Modifying"),                    N_("Set to “1” if modifying the contents is allowed.") };
static PdfTag TAG_PDF_DOCASSEMBLY          = { "PDF.DocAssembly",           N_("Document Assembly"),            N_("Set to “1” if inserting, rotating, or deleting pages and creating navigation elements is allowed.") };
static PdfTag TAG_PDF_COMMENTING           = { "PDF.Commenting",            N_("Commenting"),                   N_("Set to “1” if adding or modifying text annotations is allowed.") };
static PdfTag TAG_PDF_FORMFILLING          = { "PDF.FormFilling",           N_("Form Filling"),                 N_("Set to “1” if filling of form fields is allowed.") };
static PdfTag TAG_PDF_ACCESSIBILITYSUPPORT = { "PDF.AccessibilitySupport",  N_("Accessibility Support"),        N_("Set to “1” if accessibility support (e.g. screen readers) is enabled.") };

static PdfTag *PDF_TAGS[] = {
    &TAG_DOC_PAGECOUNT,
    &TAG_DOC_SECURITY,
    &TAG_DOC_TITLE,
    &TAG_DOC_SUBJECT,
    &TAG_DOC_KEYWORDS,
    &TAG_DOC_AUTHOR,
    &TAG_DOC_GENERATOR,
    &TAG_DOC_DATECREATED,
    &TAG_DOC_DATEMODIFIED,
    &TAG_PDF_PAGESIZE,
    &TAG_PDF_PAGEWIDTH,
    &TAG_PDF_PAGEHEIGHT,
    &TAG_PDF_VERSION,
    &TAG_PDF_PRODUCER,
    &TAG_PDF_EMBEDDEDFILES,
    &TAG_PDF_OPTIMIZED,
    &TAG_PDF_PRINTING,
    &TAG_PDF_HIRESPRINTING,
    &TAG_PDF_COPYING,
    &TAG_PDF_MODIFYING,
    &TAG_PDF_DOCASSEMBLY,
    &TAG_PDF_COMMENTING,
    &TAG_PDF_FORMFILLING,
    &TAG_PDF_ACCESSIBILITYSUPPORT
};


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

inline const gchar *enum_bit_to_01(int enum_value, int enum_bit)
{
    return (enum_value & enum_bit) ? "1" : "0";
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


GStrv supported_tags (GnomeCmdFileMetadataExtractor *plugin)
{
    GStrvBuilder *builder = g_strv_builder_new ();
    for (auto tag : PDF_TAGS)
        g_strv_builder_add (builder, tag->id);
    GStrv result = g_strv_builder_end (builder);
    g_strv_builder_unref (builder);
    return result;
}


GStrv summary_tags (GnomeCmdFileMetadataExtractor *plugin)
{
    return nullptr;
}


gchar *class_name (GnomeCmdFileMetadataExtractor *plugin, const gchar *cls)
{
    if (!g_strcmp0("Doc", cls))
        return g_strdup(_("Doc"));
    return nullptr;
}


gchar *tag_name (GnomeCmdFileMetadataExtractor *plugin, const gchar *tag_id)
{
    for (auto tag : PDF_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->name));
    return nullptr;
}


gchar *tag_description (GnomeCmdFileMetadataExtractor *plugin,
                        const gchar *tag_id)
{
    for (auto tag : PDF_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->description));
    return nullptr;
}


void extract_metadata (GnomeCmdFileMetadataExtractor *plugin,
                       GnomeCmdFileDescriptor *f,
                       GnomeCmdFileMetadataExtractorAddTag add,
                       gpointer user_data)
{
    auto file = gnome_cmd_file_descriptor_get_file (f);
    auto file_info = gnome_cmd_file_descriptor_get_file_info (f);

    // skip non pdf files, as pdf metatags extraction is very expensive...
    if (g_file_info_get_content_type (file_info) == nullptr) return;
    if (!strstr (g_file_info_get_content_type (file_info), "pdf"))  return;

    GError *error = NULL;
    gchar *uri = g_file_get_uri (file);

    PopplerDocument *document = poppler_document_new_from_file(uri, NULL, &error);
    g_free(uri);
    if (error)
    {
        if (error->code == POPPLER_ERROR_ENCRYPTED)
        {
            add(TAG_DOC_SECURITY.id, "1", user_data);
        }
        g_error_free(error);
        return;
    }

    gchar *title, *author, *subject, *keywords, *creator, *producer;
    gchar *str;
    GTime creation_date, mod_date;
    PopplerPermissions permissions;
    guint format_major, format_minor;
    gchar buffer[200];

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

    snprintf(buffer, sizeof(buffer), "%u.%u", format_major, format_minor);
    add(TAG_PDF_VERSION.id, buffer, user_data);

    snprintf(buffer, sizeof(buffer), "%i", poppler_document_get_n_pages(document));
    add(TAG_DOC_PAGECOUNT.id, buffer, user_data);

    snprintf(buffer, sizeof(buffer), "%u", poppler_document_is_linearized(document));
    add(TAG_PDF_OPTIMIZED.id, buffer, user_data);

    add(TAG_DOC_SECURITY.id, "0", user_data);

    add(TAG_PDF_PRINTING.id, enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_PRINT), user_data);
    add(TAG_PDF_MODIFYING.id, enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_MODIFY), user_data);
    add(TAG_PDF_COPYING.id, enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_COPY), user_data);
    add(TAG_PDF_COMMENTING.id, enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_ADD_NOTES), user_data);
    add(TAG_PDF_FORMFILLING.id, enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_FILL_FORM), user_data);
    add(TAG_PDF_HIRESPRINTING.id, enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_PRINT_HIGH_RESOLUTION), user_data);
    add(TAG_PDF_DOCASSEMBLY.id, enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_ASSEMBLE), user_data);
    add(TAG_PDF_ACCESSIBILITYSUPPORT.id, enum_bit_to_01(permissions, POPPLER_PERMISSIONS_OK_TO_EXTRACT_CONTENTS), user_data);

    add(TAG_DOC_TITLE.id, title, user_data);
    g_free(title);

    add(TAG_DOC_SUBJECT.id, subject, user_data);
    g_free(subject);

    // FIXME:  split keywords here
    add(TAG_DOC_KEYWORDS.id, keywords, user_data);
    add(TAG_FILE_KEYWORDS, keywords, user_data);
    g_free(keywords);

    add(TAG_DOC_AUTHOR.id, author, user_data);
    add(TAG_FILE_PUBLISHER, author, user_data);
    g_free(author);

    add(TAG_PDF_PRODUCER.id, creator, user_data);
    g_free(creator);

    add(TAG_DOC_GENERATOR.id, producer, user_data);
    g_free(producer);

    str = pgd_format_date (creation_date);
    add(TAG_DOC_DATECREATED.id, str, user_data);
    g_free (str);

    str = pgd_format_date (mod_date);
    add(TAG_DOC_DATEMODIFIED.id, str, user_data);
    g_free (str);

    if (poppler_document_get_n_pages(document) > 0)
    {
        PopplerPage *page = poppler_document_get_page(document, 0);
        double page_width, page_height;
        poppler_page_get_size(page, &page_width, &page_height);

        double width = page_width/72.0f*25.4f;
        double height = page_height/72.0f*25.4f;

        snprintf(buffer, sizeof(buffer), "%.0f", width);
        add(TAG_PDF_PAGEWIDTH.id, buffer, user_data);

        snprintf(buffer, sizeof(buffer), "%.0f", height);
        add(TAG_PDF_PAGEHEIGHT.id, buffer, user_data);

        gchar *paper_size = paper_name (width, height);

        add(TAG_PDF_PAGESIZE.id, paper_size, user_data);

        g_object_unref(page);
        g_free (paper_size);
    }

    if (poppler_document_has_attachments(document))
    {
        GList *list = poppler_document_get_attachments(document);

        snprintf(buffer, sizeof(buffer), "%u", g_list_length(list));
        add(TAG_PDF_EMBEDDEDFILES.id, buffer, user_data);

        g_list_free_full(list, g_object_unref);
    }
    else
    {
        add(TAG_PDF_EMBEDDEDFILES.id, "0", user_data);
    }

    g_object_unref(document);
}


static void gnome_cmd_poppler_plugin_class_init (GnomeCmdPopplerPluginClass *klass)
{
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


static void gnome_cmd_poppler_plugin_init (GnomeCmdPopplerPlugin *plugin)
{
}


GObject *create_plugin ()
{
    return G_OBJECT (g_object_new (GNOME_CMD_TYPE_POPPLER_PLUGIN, NULL));
}


GnomeCmdPluginInfo *get_plugin_info ()
{
    static const char *authors[] = {
        "Andrey Kutejko <andy128k@gmail.com>",
        nullptr
    };

    static GnomeCmdPluginInfo info = {
        .plugin_system_version = GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION,
        .name = "Poppler",
        .version = PACKAGE_VERSION,
        .copyright = "Copyright \xc2\xa9 2025 Andrey Kutejko",
        .comments = nullptr,
        .authors = (gchar **) authors,
        .documenters = nullptr,
        .translator = nullptr,
        .webpage = "https://gcmd.github.io"
    };

    if (!info.comments)
        info.comments = g_strdup (_("A plugin for extracting metadata from PDF files using Poppler library."));

    return &info;
}

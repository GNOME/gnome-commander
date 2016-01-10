/** 
 * @file gnome-cmd-advrename-profile-component.cc
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

#include <vector>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-advrename-profile-component.h"
#include "gnome-cmd-data.h"
#include "gnome-cmd-convert.h"
#include "gnome-cmd-menu-button.h"
#include "gnome-cmd-treeview.h"
#include "dialogs/gnome-cmd-advrename-regex-dialog.h"
#include "tags/gnome-cmd-tags.h"
#include "utils.h"

using namespace std;


struct GnomeCmdAdvrenameProfileComponentClass
{
    GtkVBoxClass parent_class;

    void (* template_changed) (GnomeCmdAdvrenameProfileComponent *component);
    void (* counter_changed) (GnomeCmdAdvrenameProfileComponent *component);
    void (* regex_changed) (GnomeCmdAdvrenameProfileComponent *component);
};


enum {TEMPLATE_CHANGED, COUNTER_CHANGED, REGEX_CHANGED, LAST_SIGNAL};

static guint signals[LAST_SIGNAL] = { 0 };


struct GnomeCmdAdvrenameProfileComponent::Private
{
    GnomeCmdConvertFunc convert_case;
    GnomeCmdConvertFunc trim_blanks;

    GtkWidget *template_entry;
    GtkWidget *template_combo;

    GtkWidget *counter_start_spin;
    GtkWidget *counter_step_spin;
    GtkWidget *counter_digits_combo;

    GtkTreeModel *regex_model;
    GtkWidget *regex_view;

    GtkWidget *regex_add_button;
    GtkWidget *regex_edit_button;
    GtkWidget *regex_remove_button;
    GtkWidget *regex_remove_all_button;

    GtkWidget *case_combo;
    GtkWidget *trim_combo;

    gchar *sample_fname;

    enum {DIR_MENU, FILE_MENU, COUNTER_MENU, DATE_MENU, METATAG_MENU, NUM_MENUS};

    static GtkItemFactoryEntry dir_items[];
    static GtkItemFactoryEntry name_items[];
    static GtkItemFactoryEntry counter_items[];
    static GtkItemFactoryEntry date_items[];
    static GtkItemFactoryEntry *items[];
    static GnomeCmdTag metatags[];

    GtkWidget *menu_button[NUM_MENUS];

    Private();
    ~Private();

    void fill_regex_model(const GnomeCmdData::AdvrenameConfig::Profile &profile);

    static gchar *translate_menu (const gchar *path, gpointer data);

    GtkWidget *create_placeholder_menu(int menu_type);
    GtkWidget *create_button_with_menu(gchar *label_text, int menu_type);
    void insert_tag(const gchar *text);

    gchar *get_selected_range (GtkWindow *parent, const gchar *title, const gchar *placeholder, const gchar *filename=NULL);

    static void insert_text_tag(GnomeCmdAdvrenameProfileComponent::Private *priv, guint n, GtkWidget *widget);
    static void insert_num_tag(GnomeCmdAdvrenameProfileComponent::Private *priv, guint tag, GtkWidget *widget);
    static void insert_range(Private *priv, guint unused, GtkWidget *widget);

    static void on_template_entry_changed(GtkEntry *entry, GnomeCmdAdvrenameProfileComponent *component);

    static void on_counter_start_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameProfileComponent *component);
    static void on_counter_step_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameProfileComponent *component);
    static void on_counter_digits_combo_value_changed (GtkWidget *spin, GnomeCmdAdvrenameProfileComponent *component);

    static void on_regex_model_row_deleted (GtkTreeModel *treemodel, GtkTreePath *path, GnomeCmdAdvrenameProfileComponent *component);
    static void on_regex_add_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component);
    static void on_regex_edit_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component);
    static void on_regex_remove_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component);
    static void on_regex_remove_all_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component);
    static void on_regex_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameProfileComponent *component);

    static void on_case_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameProfileComponent *component);
    static void on_trim_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameProfileComponent *component);
};


GtkItemFactoryEntry GnomeCmdAdvrenameProfileComponent::Private::dir_items[] =
    {{N_("/Grandparent"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 0},
     {N_("/Parent"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 1}};

GtkItemFactoryEntry GnomeCmdAdvrenameProfileComponent::Private::name_items[] =
    {{N_("/File name"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 2},
     {N_("/File name (range)"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_range, 2},
     {N_("/File name without extension"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 3},
     {N_("/File name without extension (range)"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_range,3},
     {N_("/File extension"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 4}};

GtkItemFactoryEntry GnomeCmdAdvrenameProfileComponent::Private::counter_items[] =
    {{N_("/Counter"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 5},
     {N_("/Counter (width)"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 6},
     {N_("/Counter (auto)"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 27},
     {N_("/Hexadecimal random number (width)"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 7}};

GtkItemFactoryEntry GnomeCmdAdvrenameProfileComponent::Private::date_items[] =
    {{N_("/Date"), NULL, NULL, 0, "<Branch>"},
     {N_("/Date/<locale>"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 8},
     {N_("/Date/yyyy-mm-dd"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 9},
     {N_("/Date/yy-mm-dd"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 10},
     {N_("/Date/yy.mm.dd"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 11},
     {N_("/Date/yymmdd"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 12},
     {N_("/Date/dd.mm.yy"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 13},
     {N_("/Date/mm-dd-yy"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 14},
     {N_("/Date/yyyy"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 15},
     {N_("/Date/yy"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 16},
     {N_("/Date/mm"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 17},
     {N_("/Date/mmm"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 18},
     {N_("/Date/dd"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 19},
     {N_("/Time"), NULL, NULL, 0, "<Branch>"},
     {N_("/Time/<locale>"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 20},
     {N_("/Time/HH.MM.SS"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 21},
     {N_("/Time/HH-MM-SS"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 22},
     {N_("/Time/HHMMSS"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 23},
     {N_("/Time/HH"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 24},
     {N_("/Time/MM"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 25},
     {N_("/Time/SS"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag, 26}};

GtkItemFactoryEntry *GnomeCmdAdvrenameProfileComponent::Private::items[] = {dir_items, name_items, counter_items, date_items};

GnomeCmdTag GnomeCmdAdvrenameProfileComponent::Private::metatags[] =
    {TAG_FILE_NAME, TAG_FILE_PATH,
     TAG_FILE_LINK,
     TAG_FILE_SIZE,
     TAG_FILE_MODIFIED, TAG_FILE_ACCESSED,
     TAG_FILE_PERMISSIONS,
     TAG_FILE_FORMAT,
     TAG_FILE_PUBLISHER, TAG_FILE_DESCRIPTION, TAG_FILE_KEYWORDS, TAG_FILE_RANK,

     TAG_AUDIO_ALBUMARTIST, TAG_AUDIO_ALBUMGAIN, TAG_AUDIO_ALBUMPEAKGAIN,
     TAG_AUDIO_ALBUMTRACKCOUNT, TAG_AUDIO_ALBUM, TAG_AUDIO_ARTIST, TAG_AUDIO_BITRATE,
     TAG_AUDIO_CHANNELS, TAG_AUDIO_CODECVERSION, TAG_AUDIO_CODEC, TAG_AUDIO_COMMENT,
     TAG_AUDIO_COVERALBUMTHUMBNAILPATH, TAG_AUDIO_DISCNO,
     TAG_AUDIO_DURATION, TAG_AUDIO_DURATIONMMSS,
     TAG_AUDIO_GENRE, TAG_AUDIO_ISNEW, TAG_AUDIO_ISRC, TAG_AUDIO_LASTPLAY, TAG_AUDIO_LYRICS,
     TAG_AUDIO_MBALBUMARTISTID, TAG_AUDIO_MBALBUMID, TAG_AUDIO_MBARTISTID,
     TAG_AUDIO_MBTRACKID, TAG_AUDIO_PERFORMER, TAG_AUDIO_PLAYCOUNT,
     TAG_AUDIO_RELEASEDATE, TAG_AUDIO_SAMPLERATE, TAG_AUDIO_TITLE, TAG_AUDIO_TRACKGAIN,
     TAG_AUDIO_TRACKNO, TAG_AUDIO_TRACKPEAKGAIN, TAG_AUDIO_YEAR,
     TAG_AUDIO_MPEG_CHANNELMODE, TAG_AUDIO_MPEG_LAYER, TAG_AUDIO_MPEG_VERSION,

     TAG_DOC_AUTHOR, TAG_DOC_CREATOR, TAG_DOC_TITLE,
     TAG_DOC_SUBJECT, TAG_DOC_DESCRIPTION,
     TAG_DOC_CATEGORY, TAG_DOC_KEYWORDS, TAG_DOC_REVISIONCOUNT,
     TAG_DOC_PAGECOUNT, TAG_DOC_PARAGRAPHCOUNT, TAG_DOC_LINECOUNT,
     TAG_DOC_WORDCOUNT, TAG_DOC_BYTECOUNT,
     TAG_DOC_CELLCOUNT, TAG_DOC_CHARACTERCOUNT,
     TAG_DOC_CODEPAGE, TAG_DOC_COMMENTS, TAG_DOC_COMPANY,
     TAG_DOC_DATECREATED, TAG_DOC_DATEMODIFIED,
     TAG_DOC_DICTIONARY,
     TAG_DOC_EDITINGDURATION, TAG_DOC_GENERATOR,
     TAG_DOC_HIDDENSLIDECOUNT,
     TAG_DOC_IMAGECOUNT, TAG_DOC_INITIALCREATOR,
     TAG_DOC_LANGUAGE,
     TAG_DOC_LASTPRINTED, TAG_DOC_LASTSAVEDBY,
     TAG_DOC_LOCALESYSTEMDEFAULT, TAG_DOC_MMCLIPCOUNT,
     TAG_DOC_MANAGER, TAG_DOC_NOTECOUNT, TAG_DOC_OBJECTCOUNT,
     TAG_DOC_PRESENTATIONFORMAT, TAG_DOC_PRINTDATE,
     TAG_DOC_PRINTEDBY, TAG_DOC_SCALE, TAG_DOC_SECURITY,
     TAG_DOC_SLIDECOUNT, TAG_DOC_SPREADSHEETCOUNT,
     TAG_DOC_TABLECOUNT, TAG_DOC_TEMPLATE,
     TAG_DOC_CASESENSITIVE, TAG_DOC_LINKSDIRTY,

     TAG_IMAGE_ALBUM, TAG_IMAGE_MAKE, TAG_IMAGE_MODEL,
     TAG_IMAGE_COMMENTS, TAG_IMAGE_COPYRIGHT, TAG_IMAGE_CREATOR,
     TAG_IMAGE_DATE, TAG_IMAGE_DESCRIPTION, TAG_IMAGE_EXPOSUREPROGRAM,
     TAG_IMAGE_EXPOSURETIME, TAG_IMAGE_FLASH, TAG_IMAGE_FNUMBER,
     TAG_IMAGE_FOCALLENGTH, TAG_IMAGE_HEIGHT, TAG_IMAGE_ISOSPEED,
     TAG_IMAGE_KEYWORDS, TAG_IMAGE_METERINGMODE, TAG_IMAGE_ORIENTATION,
     TAG_IMAGE_SOFTWARE, TAG_IMAGE_TITLE, TAG_IMAGE_WHITEBALANCE,
     TAG_IMAGE_WIDTH,

     TAG_NONE,

     TAG_ID3_BAND,
     TAG_ID3_CONTENTTYPE,
     TAG_ID3_ALBUMSORTORDER, TAG_ID3_AUDIOCRYPTO,
     TAG_ID3_AUDIOSEEKPOINT,
     TAG_ID3_BPM, TAG_ID3_BUFFERSIZE, TAG_ID3_CDID,
     TAG_ID3_COMMERCIAL, TAG_ID3_COMPOSER, TAG_ID3_CONDUCTOR,
     TAG_ID3_CONTENTGROUP, TAG_ID3_CONTENTTYPE,
     TAG_ID3_COPYRIGHT,
     TAG_ID3_CRYPTOREG, TAG_ID3_DATE,
     TAG_ID3_EMPHASIS, TAG_ID3_ENCODEDBY,
     TAG_ID3_ENCODERSETTINGS, TAG_ID3_ENCODINGTIME, TAG_ID3_EQUALIZATION,
     TAG_ID3_EQUALIZATION2, TAG_ID3_EVENTTIMING, TAG_ID3_FILEOWNER,
     TAG_ID3_FILETYPE, TAG_ID3_FRAMES, TAG_ID3_GENERALOBJECT,
     TAG_ID3_GROUPINGREG, TAG_ID3_INITIALKEY,
     TAG_ID3_INVOLVEDPEOPLE, TAG_ID3_INVOLVEDPEOPLE2,
     TAG_ID3_LANGUAGE, TAG_ID3_LINKEDINFO,
     TAG_ID3_LYRICIST, TAG_ID3_MEDIATYPE, TAG_ID3_MIXARTIST,
     TAG_ID3_MOOD,
     TAG_ID3_MPEGLOOKUP,
     TAG_ID3_MUSICIANCREDITLIST,
     TAG_ID3_NETRADIOOWNER, TAG_ID3_NETRADIOSTATION,
     TAG_ID3_ORIGALBUM, TAG_ID3_ORIGARTIST, TAG_ID3_ORIGFILENAME,
     TAG_ID3_ORIGLYRICIST, TAG_ID3_ORIGRELEASETIME, TAG_ID3_ORIGYEAR,
     TAG_ID3_OWNERSHIP, TAG_ID3_PARTINSET, TAG_ID3_PERFORMERSORTORDER,
     TAG_ID3_PICTURE, TAG_ID3_PLAYCOUNTER, TAG_ID3_PLAYLISTDELAY,
     TAG_ID3_POPULARIMETER, TAG_ID3_POSITIONSYNC, TAG_ID3_PRIVATE,
     TAG_ID3_PRODUCEDNOTICE, TAG_ID3_PUBLISHER, TAG_ID3_RECORDINGDATES,
     TAG_ID3_RECORDINGTIME, TAG_ID3_RELEASETIME, TAG_ID3_REVERB,
     TAG_ID3_SETSUBTITLE, TAG_ID3_SIGNATURE,
     TAG_ID3_SIZE, TAG_ID3_SONGLEN, TAG_ID3_SUBTITLE, TAG_ID3_SYNCEDLYRICS,
     TAG_ID3_SYNCEDTEMPO, TAG_ID3_TAGGINGTIME, TAG_ID3_TERMSOFUSE,
     TAG_ID3_TIME, TAG_ID3_TITLESORTORDER,
     TAG_ID3_UNIQUEFILEID, TAG_ID3_UNSYNCEDLYRICS, TAG_ID3_USERTEXT,
     TAG_ID3_VOLUMEADJ, TAG_ID3_VOLUMEADJ2, TAG_ID3_WWWARTIST,
     TAG_ID3_WWWAUDIOFILE, TAG_ID3_WWWAUDIOSOURCE, TAG_ID3_WWWCOMMERCIALINFO,
     TAG_ID3_WWWCOPYRIGHT, TAG_ID3_WWWPAYMENT, TAG_ID3_WWWPUBLISHER,
     TAG_ID3_WWWRADIOPAGE, TAG_ID3_WWWUSER,

     TAG_VORBIS_CONTACT, TAG_VORBIS_DESCRIPTION,
     TAG_VORBIS_LICENSE, TAG_VORBIS_LOCATION,
     TAG_VORBIS_MAXBITRATE, TAG_VORBIS_MINBITRATE,
     TAG_VORBIS_NOMINALBITRATE, TAG_VORBIS_ORGANIZATION,
     TAG_VORBIS_VENDOR, TAG_VORBIS_VERSION,

     TAG_EXIF_COPYRIGHT, TAG_EXIF_DATETIME,
     TAG_EXIF_EXPOSUREBIASVALUE, TAG_EXIF_EXPOSUREMODE, TAG_EXIF_EXPOSUREPROGRAM,
     TAG_EXIF_FLASH, TAG_EXIF_FLASHENERGY,
     TAG_EXIF_FNUMBER, TAG_EXIF_FOCALLENGTH,
     TAG_EXIF_ISOSPEEDRATINGS, TAG_EXIF_MAXAPERTUREVALUE,
     TAG_EXIF_METERINGMODE, TAG_EXIF_SHUTTERSPEEDVALUE, TAG_EXIF_WHITEBALANCE,
     TAG_EXIF_PIXELXDIMENSION, TAG_EXIF_PIXELYDIMENSION,
     TAG_EXIF_XRESOLUTION, TAG_EXIF_YRESOLUTION,
     TAG_EXIF_IMAGELENGTH, TAG_EXIF_IMAGEWIDTH,
     TAG_EXIF_CUSTOMRENDERED, TAG_EXIF_COLORSPACE,
     TAG_EXIF_DOCUMENTNAME, TAG_EXIF_USERCOMMENT,

     TAG_EXIF_APERTUREVALUE, TAG_EXIF_ARTIST, TAG_EXIF_BATTERYLEVEL,
     TAG_EXIF_BITSPERSAMPLE, TAG_EXIF_BRIGHTNESSVALUE,
     TAG_EXIF_CFAPATTERN, TAG_EXIF_COMPONENTSCONFIGURATION,
     TAG_EXIF_COMPRESSEDBITSPERPIXEL, TAG_EXIF_COMPRESSION, TAG_EXIF_CONTRAST,
     TAG_EXIF_DATETIMEDIGITIZED, TAG_EXIF_DATETIMEORIGINAL,
     TAG_EXIF_DEVICESETTINGDESCRIPTION, TAG_EXIF_DIGITALZOOMRATIO,
     TAG_EXIF_EXIFVERSION,
     TAG_EXIF_EXPOSUREINDEX,
     TAG_EXIF_EXPOSURETIME, TAG_EXIF_FILESOURCE,
     TAG_EXIF_FILLORDER,
     TAG_EXIF_FLASHPIXVERSION,
     TAG_EXIF_FOCALLENGTHIN35MMFILM, TAG_EXIF_FOCALPLANERESOLUTIONUNIT,
     TAG_EXIF_FOCALPLANEXRESOLUTION, TAG_EXIF_FOCALPLANEYRESOLUTION,
     TAG_EXIF_GAINCONTROL, TAG_EXIF_GAMMA, TAG_EXIF_GPSALTITUDE,
     TAG_EXIF_GPSLATITUDE, TAG_EXIF_GPSLONGITUDE,
     TAG_EXIF_GPSVERSIONID, TAG_EXIF_IMAGEDESCRIPTION, TAG_EXIF_IMAGEUNIQUEID,
     TAG_EXIF_INTERCOLORPROFILE, TAG_EXIF_INTEROPERABILITYINDEX, TAG_EXIF_INTEROPERABILITYVERSION,
     TAG_EXIF_IPTCNAA, TAG_EXIF_JPEGINTERCHANGEFORMAT,
     TAG_EXIF_JPEGINTERCHANGEFORMATLENGTH, TAG_EXIF_LIGHTSOURCE,
     TAG_EXIF_MAKE, TAG_EXIF_MAKERNOTE,
     TAG_EXIF_METERINGMODE, TAG_EXIF_MODEL, TAG_EXIF_NEWCFAPATTERN,
     TAG_EXIF_NEWSUBFILETYPE, TAG_EXIF_OECF, TAG_EXIF_ORIENTATION,
     TAG_EXIF_PHOTOMETRICINTERPRETATION, TAG_EXIF_PLANARCONFIGURATION,
     TAG_EXIF_PRIMARYCHROMATICITIES, TAG_EXIF_REFERENCEBLACKWHITE,
     TAG_EXIF_RELATEDIMAGEFILEFORMAT, TAG_EXIF_RELATEDIMAGELENGTH,
     TAG_EXIF_RELATEDIMAGEWIDTH, TAG_EXIF_RELATEDSOUNDFILE, TAG_EXIF_RESOLUTIONUNIT,
     TAG_EXIF_ROWSPERSTRIP, TAG_EXIF_SAMPLESPERPIXEL, TAG_EXIF_SATURATION,
     TAG_EXIF_SCENECAPTURETYPE, TAG_EXIF_SCENETYPE, TAG_EXIF_SENSINGMETHOD,
     TAG_EXIF_SHARPNESS, TAG_EXIF_SHUTTERSPEEDVALUE, TAG_EXIF_SOFTWARE,
     TAG_EXIF_SPATIALFREQUENCYRESPONSE, TAG_EXIF_SPECTRALSENSITIVITY,
     TAG_EXIF_STRIPBYTECOUNTS, TAG_EXIF_STRIPOFFSETS,
     TAG_EXIF_SUBJECTAREA, TAG_EXIF_SUBJECTDISTANCE, TAG_EXIF_SUBJECTDISTANCERANGE,
     TAG_EXIF_SUBJECTLOCATION, TAG_EXIF_SUBSECTIME, TAG_EXIF_SUBSECTIMEDIGITIZED,
     TAG_EXIF_SUBSECTIMEORIGINAL, TAG_EXIF_TIFFEPSTANDARDID, TAG_EXIF_TRANSFERFUNCTION,
     TAG_EXIF_TRANSFERRANGE, TAG_EXIF_WHITEPOINT,
     TAG_EXIF_YCBCRCOEFFICIENTS, TAG_EXIF_YCBCRPOSITIONING,
     TAG_EXIF_YCBCRSUBSAMPLING,

     TAG_IPTC_BYLINE, TAG_IPTC_BYLINETITLE, TAG_IPTC_CAPTION, TAG_IPTC_HEADLINE,
     TAG_IPTC_SUBLOCATION, TAG_IPTC_CITY, TAG_IPTC_PROVINCE,
     TAG_IPTC_COUNTRYCODE, TAG_IPTC_COUNTRYNAME,
     TAG_IPTC_CONTACT, TAG_IPTC_COPYRIGHTNOTICE, TAG_IPTC_CREDIT,
     TAG_IPTC_KEYWORDS,
     TAG_IPTC_DIGITALCREATIONDATE, TAG_IPTC_DIGITALCREATIONTIME,
     TAG_IPTC_IMAGEORIENTATION,
     TAG_IPTC_SPECIALINSTRUCTIONS, TAG_IPTC_URGENCY,

     TAG_IPTC_ACTIONADVISED, TAG_IPTC_ARMID, TAG_IPTC_ARMVERSION,
     TAG_IPTC_AUDIODURATION, TAG_IPTC_AUDIOOUTCUE, TAG_IPTC_AUDIOSAMPLINGRATE,
     TAG_IPTC_AUDIOSAMPLINGRES, TAG_IPTC_AUDIOTYPE,
     TAG_IPTC_CATEGORY, TAG_IPTC_CHARACTERSET, TAG_IPTC_CONFIRMEDDATASIZE,
     TAG_IPTC_CONTENTLOCCODE, TAG_IPTC_CONTENTLOCNAME,
     TAG_IPTC_DATECREATED, TAG_IPTC_DATESENT,
     TAG_IPTC_DESTINATION, TAG_IPTC_EDITORIALUPDATE, TAG_IPTC_EDITSTATUS,
     TAG_IPTC_ENVELOPENUM, TAG_IPTC_ENVELOPEPRIORITY, TAG_IPTC_EXPIRATIONDATE,
     TAG_IPTC_EXPIRATIONTIME, TAG_IPTC_FILEFORMAT, TAG_IPTC_FILEVERSION,
     TAG_IPTC_FIXTUREID, TAG_IPTC_IMAGETYPE, TAG_IPTC_LANGUAGEID,
     TAG_IPTC_MAXOBJECTSIZE, TAG_IPTC_MAXSUBFILESIZE, TAG_IPTC_MODELVERSION,
     TAG_IPTC_OBJECTATTRIBUTE, TAG_IPTC_OBJECTCYCLE, TAG_IPTC_OBJECTNAME,
     TAG_IPTC_OBJECTSIZEANNOUNCED, TAG_IPTC_OBJECTTYPE, TAG_IPTC_ORIGINATINGPROGRAM,
     TAG_IPTC_ORIGTRANSREF, TAG_IPTC_PREVIEWDATA, TAG_IPTC_PREVIEWFORMAT,
     TAG_IPTC_PREVIEWFORMATVER, TAG_IPTC_PRODUCTID, TAG_IPTC_PROGRAMVERSION,
     TAG_IPTC_PROVINCE, TAG_IPTC_RASTERIZEDCAPTION, TAG_IPTC_RECORDVERSION,
     TAG_IPTC_REFERENCEDATE, TAG_IPTC_REFERENCENUMBER, TAG_IPTC_REFERENCESERVICE,
     TAG_IPTC_RELEASEDATE, TAG_IPTC_RELEASETIME, TAG_IPTC_SERVICEID,
     TAG_IPTC_SIZEMODE, TAG_IPTC_SOURCE, TAG_IPTC_SUBFILE, TAG_IPTC_SUBJECTREFERENCE,
     TAG_IPTC_SUPPLCATEGORY, TAG_IPTC_TIMECREATED, TAG_IPTC_TIMESENT, TAG_IPTC_UNO,
     TAG_IPTC_URGENCY, TAG_IPTC_WRITEREDITOR,

     TAG_PDF_PAGESIZE, TAG_PDF_PAGEWIDTH, TAG_PDF_PAGEHEIGHT,
     TAG_PDF_VERSION, TAG_PDF_PRODUCER,
     TAG_PDF_EMBEDDEDFILES,
     TAG_PDF_OPTIMIZED,
     TAG_PDF_PRINTING,
     TAG_PDF_HIRESPRINTING,
     TAG_PDF_COPYING,
     TAG_PDF_MODIFYING,
     TAG_PDF_DOCASSEMBLY,
     TAG_PDF_COMMENTING,
     TAG_PDF_FORMFILLING,
     TAG_PDF_ACCESSIBILITYSUPPORT
    };


inline GtkTreeModel *create_regex_model ();

template <typename T>
inline GtkTreeModel *copy_regex_model(GtkTreeModel *model, guint col_id, vector<T> &v);

inline GtkTreeModel *clear_regex_model(GtkTreeModel *model);

inline GtkWidget *create_regex_view ();


inline GnomeCmdAdvrenameProfileComponent::Private::Private()
{
    memset(menu_button, 0, sizeof(menu_button));
    convert_case = gcmd_convert_unchanged;
    trim_blanks = gcmd_convert_strip;
    regex_model = NULL;
    sample_fname = NULL;
}


inline GnomeCmdAdvrenameProfileComponent::Private::~Private()
{
    g_object_unref (template_entry);

    clear_regex_model(regex_model);

    if (regex_model)  g_object_unref (regex_model);

    g_free (sample_fname);
}


inline gboolean model_is_empty(GtkTreeModel *tree_model)
{
    GtkTreeIter iter;

    return !gtk_tree_model_get_iter_first (tree_model, &iter);
}


void GnomeCmdAdvrenameProfileComponent::Private::fill_regex_model(const GnomeCmdData::AdvrenameConfig::Profile &profile)
{
    if (!regex_model)
        return;

    GtkTreeIter iter;

    for (vector<GnomeCmd::ReplacePattern>::const_iterator r=profile.regexes.begin(); r!=profile.regexes.end(); ++r)
    {
        GnomeCmd::RegexReplace *rx = new GnomeCmd::RegexReplace(r->pattern, r->replacement, r->match_case);

        gtk_list_store_append (GTK_LIST_STORE (regex_model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (regex_model), &iter,
                            COL_REGEX, rx,
                            COL_MALFORMED_REGEX, !*rx,
                            COL_PATTERN, rx->pattern.c_str(),
                            COL_REPLACE, rx->replacement.c_str(),
                            COL_MATCH_CASE, rx->match_case ? _("Yes") : _("No"),
                            -1);
    }
}


gchar *GnomeCmdAdvrenameProfileComponent::Private::translate_menu (const gchar *path, gpointer data)
{
    return _(path);
}


inline GtkWidget *GnomeCmdAdvrenameProfileComponent::Private::create_placeholder_menu(int menu_type)
{
    static guint items_size[] = {G_N_ELEMENTS(dir_items),
                                 G_N_ELEMENTS(name_items),
                                 G_N_ELEMENTS(counter_items),
                                 G_N_ELEMENTS(date_items)};
    switch (menu_type)
    {
        case DIR_MENU:
        case FILE_MENU:
        case COUNTER_MENU:
        case DATE_MENU:
            {
                GtkItemFactory *ifac = gtk_item_factory_new (GTK_TYPE_MENU, "<main>", NULL);

                gtk_item_factory_set_translate_func (ifac, translate_menu, NULL, NULL);
                gtk_item_factory_create_items (ifac, items_size[menu_type], items[menu_type], this);

                return gtk_item_factory_get_widget (ifac, "<main>");
            }

        case METATAG_MENU:
            {
                const int BUFF_SIZE = 2048;
                gchar *s = (gchar *) g_malloc0 (BUFF_SIZE);
                GtkItemFactoryEntry *items = g_try_new0 (GtkItemFactoryEntry, G_N_ELEMENTS(metatags));

                g_return_val_if_fail (items!=NULL, NULL);

                for (guint i=0; i<G_N_ELEMENTS(metatags); ++i)
                {
                    GnomeCmdTag tag = metatags[i];
                    const gchar *class_name = gcmd_tags_get_class_name(tag);
                    GtkItemFactoryEntry *p = items+i;

                    if (!class_name || *class_name==0)
                    {
                        p->path = g_strdup("/");
                        p->item_type = "<Separator>";
                    }
                    else
                    {
                        strncpy (s, gcmd_tags_get_title (tag), BUFF_SIZE-1);

                        for (gchar *i=s; *i; ++i)
                            if (*i=='/')  *i = ' ';

                        p->path = g_strdup_printf ("/%s/%s", gcmd_tags_get_class_name(tag), s);
                        p->callback = (GtkItemFactoryCallback) insert_num_tag;
                        p->callback_action = tag;
                    }
                }

                g_free (s);

                GtkItemFactory *ifac = gtk_item_factory_new (GTK_TYPE_MENU, "<main>", NULL);

                gtk_item_factory_create_items (ifac, G_N_ELEMENTS(metatags), items, this);

                for (guint i=0; i<G_N_ELEMENTS(metatags); ++i)
                    g_free (items[i].path);

                g_free (items);

                return gtk_item_factory_get_widget (ifac, "<main>");
            }

        default:
            return NULL;
    }
}


inline GtkWidget *GnomeCmdAdvrenameProfileComponent::Private::create_button_with_menu(gchar *label_text, int menu_type)
{
    menu_button[menu_type] = gnome_cmd_button_menu_new (label_text, create_placeholder_menu(menu_type));

    return menu_button[menu_type];
}


inline void GnomeCmdAdvrenameProfileComponent::Private::insert_tag(const gchar *text)
{
    GtkEditable *editable = (GtkEditable *) template_entry;
    gint pos = gtk_editable_get_position (editable);

    gtk_editable_insert_text (editable, text, strlen(text), &pos);
    gtk_editable_set_position (editable, pos);
    gtk_widget_grab_focus ((GtkWidget *) editable);
    gtk_editable_select_region (editable, pos, pos);
}


void GnomeCmdAdvrenameProfileComponent::Private::insert_text_tag(GnomeCmdAdvrenameProfileComponent::Private *priv, guint n, GtkWidget *widget)
{
    static const gchar *placeholder[] = {"$g",          //  0
                                         "$p",          //  1
                                         "$N",          //  2
                                         "$n",          //  3
                                         "$e",          //  4
                                         "$c",          //  5
                                         "$c(2)",       //  6
                                         "$x(8)",       //  7
                                         "%x",          //  8
                                         "%Y-%m-%d",    //  9
                                         "%y-%m-%d",    // 10
                                         "%y.%m.%d",    // 11
                                         "%y%m%d",      // 12
                                         "%d.%m.%y",    // 13
                                         "%m-%d-%y",    // 14
                                         "%Y",          // 15
                                         "%y",          // 16
                                         "%m",          // 17
                                         "%b",          // 18
                                         "%d",          // 19
                                         "%X",          // 20
                                         "%H.%M.%S",    // 21
                                         "%H-%M-%S",    // 22
                                         "%H%M%S",      // 23
                                         "%H",          // 24
                                         "%M",          // 25
                                         "%S",          // 26
                                         "$c(a)"};      // 27
    
    g_return_if_fail (n < G_N_ELEMENTS(placeholder));

    priv->insert_tag(placeholder[n]);
}


void GnomeCmdAdvrenameProfileComponent::Private::insert_num_tag(GnomeCmdAdvrenameProfileComponent::Private *priv, guint tag, GtkWidget *widget)
{
    gchar *s = g_strdup_printf ("$T(%s)", gcmd_tags_get_name((GnomeCmdTag) tag));
    priv->insert_tag(s);
    g_free (s);
}


inline gchar *GnomeCmdAdvrenameProfileComponent::Private::get_selected_range (GtkWindow *parent, const gchar *title, const gchar *placeholder, const gchar *filename)
{
    if (!filename)
        filename = "Lorem ipsum dolor sit amet, consectetur adipisici elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. " \
                   "Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. " \
                   "Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. " \
                   "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

    GtkWidget *dialog = gtk_dialog_new_with_buttons (title, parent,
                                                     GtkDialogFlags (GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_OK, GTK_RESPONSE_OK,
                                                     NULL);

    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_widget_set_size_request (dialog, 480, -1);
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

    // HIG defaults
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);
    gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->action_area), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->action_area),6);

    GtkWidget *hbox, *label, *entry, *option;

    hbox = gtk_hbox_new (FALSE, 12);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic (_("_Select range:"));
    gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

    entry = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (entry), filename);
    gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
    g_object_set_data (G_OBJECT (dialog), "filename", entry);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
    gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

    option = gtk_check_button_new_with_mnemonic (_("_Inverse selection"));
    g_object_set_data (G_OBJECT (dialog), "inverse", option);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), option, FALSE, FALSE, 0);

    gtk_widget_show_all (GTK_DIALOG (dialog)->vbox);

    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

    gint result = gtk_dialog_run (GTK_DIALOG (dialog));

    gchar *range = NULL;

    if (result==GTK_RESPONSE_OK)
    {
        gint beg, end;
        GtkWidget *entry = lookup_widget (GTK_WIDGET (dialog), "filename");

        gboolean inversed = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lookup_widget (GTK_WIDGET (dialog), "inverse")));

        if (gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &beg, &end))
        {
#if GTK_CHECK_VERSION (2, 16, 0)
            guint16 len = gtk_entry_get_text_length (GTK_ENTRY (entry));
#else
            guint16 len = (guint16) g_utf8_strlen (gtk_entry_get_text (GTK_ENTRY (entry)), -1);
#endif
            if (!inversed)
                range = end==len ? g_strdup_printf ("%s(%i:)", placeholder, beg) :
                                   g_strdup_printf ("%s(%i:%i)", placeholder, beg, end);
            else
            {
                if (end!=len)
                    range = beg ? g_strdup_printf ("%s(:%i)%s(%i:)", placeholder, beg, placeholder, end) :
                                  g_strdup_printf ("%s(%i:)", placeholder, end);
                else
                    if (beg)
                        range = g_strdup_printf ("%s(:%i)", placeholder, beg);
            }
        }
        else
            if (inversed)
                range = g_strdup (placeholder);
    }

    gtk_widget_destroy (dialog);

    return range;
}


void GnomeCmdAdvrenameProfileComponent::Private::insert_range(GnomeCmdAdvrenameProfileComponent::Private *priv, guint n, GtkWidget *widget)
{
    static const gchar *placeholder[] = {"$g",          //  0
                                         "$p",          //  1
                                         "$N",          //  2
                                         "$n",          //  3
                                         "$e"};         //  4

    gchar *fname = NULL;

    if (n==3 && priv->sample_fname)
    {
        fname = g_strdup (priv->sample_fname);
        gchar *ext = g_utf8_strrchr (fname, -1, '.');
        if (ext)
            *ext = 0;
    }

    gchar *tag = priv->get_selected_range (NULL, _("Range Selection"), placeholder[n], fname ? fname : priv->sample_fname);

    if (tag)
        priv->insert_tag(tag);

    g_free (tag);
    g_free (fname);
}


G_DEFINE_TYPE (GnomeCmdAdvrenameProfileComponent, gnome_cmd_advrename_profile_component, GTK_TYPE_VBOX)


void GnomeCmdAdvrenameProfileComponent::Private::on_template_entry_changed(GtkEntry *entry, GnomeCmdAdvrenameProfileComponent *component)
{
    g_signal_emit (component, signals[TEMPLATE_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_counter_start_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameProfileComponent *component)
{
    component->profile.counter_start = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
    g_signal_emit (component, signals[COUNTER_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_counter_step_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameProfileComponent *component)
{
    component->profile.counter_step = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
    g_signal_emit (component, signals[COUNTER_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_counter_digits_combo_value_changed (GtkWidget *combo, GnomeCmdAdvrenameProfileComponent *component)
{
    component->profile.counter_width = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
    component->profile.counter_width = CLAMP(component->profile.counter_width, 0, 16);
    g_signal_emit (component, signals[COUNTER_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_model_row_deleted (GtkTreeModel *treemodel, GtkTreePath *path, GnomeCmdAdvrenameProfileComponent *component)
{
    g_signal_emit (component, signals[REGEX_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_add_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component)
{
    GnomeCmd::RegexReplace *rx = new GnomeCmd::RegexReplace;

    if (gnome_cmd_advrename_regex_dialog_new (_("Add Rule"), GTK_WINDOW (gtk_widget_get_toplevel (*component)), rx))
    {
        GtkTreeIter i;

        gtk_list_store_append (GTK_LIST_STORE (component->priv->regex_model), &i);
        gtk_list_store_set (GTK_LIST_STORE (component->priv->regex_model), &i,
                            COL_REGEX, rx,
                            COL_MALFORMED_REGEX, !*rx,
                            COL_PATTERN, rx->pattern.c_str(),
                            COL_REPLACE, rx->replacement.c_str(),
                            COL_MATCH_CASE, rx->match_case ? _("Yes") : _("No"),
                            -1);

        g_signal_emit (component, signals[REGEX_CHANGED], 0);

        gtk_widget_set_sensitive (component->priv->regex_edit_button, TRUE);
        gtk_widget_set_sensitive (component->priv->regex_remove_button, TRUE);
        gtk_widget_set_sensitive (component->priv->regex_remove_all_button, TRUE);
    }
    else
        delete rx;
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_edit_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (component->priv->regex_view);
    GtkTreeIter i;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (tree_view), NULL, &i))
    {
        GnomeCmd::RegexReplace *rx = NULL;

        gtk_tree_model_get (component->priv->regex_model, &i, COL_REGEX, &rx, -1);

        if (gnome_cmd_advrename_regex_dialog_new (_("Edit Rule"), GTK_WINDOW (gtk_widget_get_toplevel (*component)), rx))
        {
            gtk_list_store_set (GTK_LIST_STORE (component->priv->regex_model), &i,
                                COL_REGEX, rx,
                                COL_MALFORMED_REGEX, !*rx,
                                COL_PATTERN, rx->pattern.c_str(),
                                COL_REPLACE, rx->replacement.c_str(),
                                COL_MATCH_CASE, rx->match_case ? _("Yes") : _("No"),
                                -1);

            g_signal_emit (component, signals[REGEX_CHANGED], 0);
        }
    }
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_remove_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (component->priv->regex_view);
    GtkTreeIter i;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (tree_view), NULL, &i))
    {
        GnomeCmd::RegexReplace *rx = NULL;

        gtk_tree_model_get (component->priv->regex_model, &i, COL_REGEX, &rx, -1);
        gtk_list_store_remove (GTK_LIST_STORE (component->priv->regex_model), &i);
        delete rx;

        if (model_is_empty (component->priv->regex_model))
        {
            gtk_widget_set_sensitive (component->priv->regex_edit_button, FALSE);
            gtk_widget_set_sensitive (component->priv->regex_remove_button, FALSE);
            gtk_widget_set_sensitive (component->priv->regex_remove_all_button, FALSE);
        }
    }
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_remove_all_btn_clicked (GtkButton *button, GnomeCmdAdvrenameProfileComponent *component)
{
    clear_regex_model(component->priv->regex_model);

    g_signal_handlers_block_by_func (component->priv->regex_model, gpointer (on_regex_model_row_deleted), component);
    gtk_list_store_clear (GTK_LIST_STORE (component->priv->regex_model));
    g_signal_handlers_unblock_by_func (component->priv->regex_model, gpointer (on_regex_model_row_deleted), component);

    g_signal_emit (component, signals[REGEX_CHANGED], 0);

    gtk_widget_set_sensitive (component->priv->regex_edit_button, FALSE);
    gtk_widget_set_sensitive (component->priv->regex_remove_button, FALSE);
    gtk_widget_set_sensitive (component->priv->regex_remove_all_button, FALSE);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_regex_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameProfileComponent *component)
{
    on_regex_edit_btn_clicked (NULL, component);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_case_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameProfileComponent *component)
{
    gint item = gtk_combo_box_get_active (combo);

    switch (item)
    {
        case 0: component->priv->convert_case = gcmd_convert_unchanged; break;
        case 1: component->priv->convert_case = gcmd_convert_lowercase; break;
        case 2: component->priv->convert_case = gcmd_convert_uppercase; break;
        case 3: component->priv->convert_case = gcmd_convert_sentence_case; break;
        case 4: component->priv->convert_case = gcmd_convert_initial_caps; break;
        case 5: component->priv->convert_case = gcmd_convert_toggle_case; break;

        default:
            return;
    }

    component->profile.case_conversion = item;
    g_signal_emit (component, signals[REGEX_CHANGED], 0);
}


void GnomeCmdAdvrenameProfileComponent::Private::on_trim_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameProfileComponent *component)
{
    gint item = gtk_combo_box_get_active (combo);

    switch (item)
    {
        case 0: component->priv->trim_blanks = gcmd_convert_unchanged; break;
        case 1: component->priv->trim_blanks = gcmd_convert_ltrim; break;
        case 2: component->priv->trim_blanks = gcmd_convert_rtrim; break;
        case 3: component->priv->trim_blanks = gcmd_convert_strip; break;

        default:
            return;
    }

    component->profile.trim_blanks = item;
    g_signal_emit (component, signals[REGEX_CHANGED], 0);
}


static void gnome_cmd_advrename_profile_component_init (GnomeCmdAdvrenameProfileComponent *component)
{
    component->priv = new GnomeCmdAdvrenameProfileComponent::Private;

    GtkWidget *align;
    GtkWidget *label;
    GtkWidget *table;
    GtkWidget *combo;
    GtkWidget *hbox;
    GtkWidget *bbox;
    GtkWidget *spin;
    GtkWidget *button;

    gchar *str;

    hbox = gtk_hbox_new (FALSE, 18);
    gtk_box_pack_start (GTK_BOX (component), hbox, FALSE, FALSE, 0);


    // Template
    {
        GtkWidget *vbox = gtk_vbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

        str = g_strdup_printf ("<b>%s</b>", _("_Template"));
        label = gtk_label_new_with_mnemonic (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
        gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 12, 0);
        gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);

        {
            GtkWidget *vbox = gtk_vbox_new (FALSE, 6);
            gtk_container_add (GTK_CONTAINER (align), vbox);

            component->priv->template_combo = combo = gtk_combo_box_entry_new_text ();
            component->priv->template_entry = gtk_bin_get_child (GTK_BIN (component->priv->template_combo));
            gtk_entry_set_activates_default (GTK_ENTRY (component->priv->template_entry), TRUE);
            gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
            gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, FALSE, 0);
            g_object_ref (component->priv->template_entry);

            GtkWidget *bbox = gtk_hbutton_box_new ();
            gtk_box_pack_start (GTK_BOX (vbox), bbox, TRUE, FALSE, 0);

            gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);
            gtk_box_set_spacing (GTK_BOX (bbox), 6);
            gtk_box_pack_start (GTK_BOX (bbox), component->priv->create_button_with_menu (_("Directory"), GnomeCmdAdvrenameProfileComponent::Private::DIR_MENU), FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (bbox), component->priv->create_button_with_menu (_("File"), GnomeCmdAdvrenameProfileComponent::Private::FILE_MENU), FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (bbox), component->priv->create_button_with_menu (_("Counter"), GnomeCmdAdvrenameProfileComponent::Private::COUNTER_MENU), FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (bbox), component->priv->create_button_with_menu (_("Date"), GnomeCmdAdvrenameProfileComponent::Private::DATE_MENU), FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (bbox), component->priv->create_button_with_menu (_("Metatag"), GnomeCmdAdvrenameProfileComponent::Private::METATAG_MENU), FALSE, FALSE, 0);
        }
    }


    // Counter
    {
        GtkWidget *vbox = gtk_vbox_new (FALSE, 6);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

        str = g_strdup_printf ("<b>%s</b>", _("Counter"));
        label = gtk_label_new_with_mnemonic (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
        gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 12, 0);
        gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);

        table = gtk_table_new (3, 2, FALSE);
        gtk_table_set_row_spacings (GTK_TABLE (table), 6);
        gtk_table_set_col_spacings (GTK_TABLE (table), 12);
        gtk_container_add (GTK_CONTAINER (align), table);

        label = gtk_label_new_with_mnemonic (_("_Start:"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        component->priv->counter_start_spin = spin = gtk_spin_button_new_with_range (0, 1000000, 1);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
        gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
        gtk_table_attach_defaults (GTK_TABLE (table), spin, 1, 2, 0, 1);

        label = gtk_label_new_with_mnemonic (_("Ste_p:"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        component->priv->counter_step_spin = spin = gtk_spin_button_new_with_range (-1000, 1000, 1);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
        gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
        gtk_table_attach_defaults (GTK_TABLE (table), spin, 1, 2, 1, 2);

        label = gtk_label_new_with_mnemonic (_("Di_gits:"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        component->priv->counter_digits_combo = combo = gtk_combo_box_new_text ();

        static const char *digit_widths[] = {N_("auto"),"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16",NULL};

        for (const char **i=digit_widths; *i; ++i)
            gtk_combo_box_append_text (GTK_COMBO_BOX (combo), *i);

        gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
        gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
        gtk_table_attach_defaults (GTK_TABLE (table), combo, 1, 2, 2, 3);
    }


    // Regex
    {
        str = g_strdup_printf ("<b>%s</b>", _("Regex replacing"));
        label = gtk_label_new (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (component), label, FALSE, FALSE, 0);

        align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
        gtk_alignment_set_padding (GTK_ALIGNMENT (align), 6, 12, 12, 0);
        gtk_box_pack_start (GTK_BOX (component), align, FALSE, FALSE, 0);

        table = gtk_table_new (2, 1, FALSE);
        gtk_table_set_row_spacings (GTK_TABLE (table), 6);
        gtk_table_set_col_spacings (GTK_TABLE (table), 12);
        gtk_container_add (GTK_CONTAINER (align), table);

        GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
        gtk_table_attach (GTK_TABLE (table), scrolled_window, 0, 1, 0, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL), GTK_FILL, 0, 0);

        component->priv->regex_view = create_regex_view ();
        gtk_container_add (GTK_CONTAINER (scrolled_window), component->priv->regex_view);

        bbox = gtk_vbutton_box_new ();
        gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
        gtk_box_set_spacing (GTK_BOX (bbox), 12);
        gtk_table_attach (GTK_TABLE (table), bbox, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

        component->priv->regex_add_button = button = gtk_button_new_from_stock (GTK_STOCK_ADD);
        gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

        component->priv->regex_edit_button = button = gtk_button_new_from_stock (GTK_STOCK_EDIT);
        gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

        component->priv->regex_remove_button = button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
        gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

        component->priv->regex_remove_all_button = button = gtk_button_new_with_mnemonic (_("Remove A_ll"));
        gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
    }


    align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 18, 0, 0);
    gtk_box_pack_start (GTK_BOX (component), align, FALSE, FALSE, 0);
    hbox = gtk_hbox_new (FALSE, 12);
    gtk_container_add (GTK_CONTAINER (align), hbox);

    // Case conversion & blank triming
    {
        str = g_strdup_printf ("<b>%s</b>", _("Case"));
        label = gtk_label_new_with_mnemonic (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

        component->priv->case_combo = combo = gtk_combo_box_new_text ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

        gchar *case_modes[] = {
                                _("<unchanged>"),
                                _("lowercase"),
                                _("UPPERCASE"),
                                NULL,
                                _("Sentence case"),       //  FIXME
                                _("Initial Caps"),        //  FIXME
                                _("tOGGLE cASE"),         //  FIXME
                                NULL
                              };

        for (gchar **items=case_modes; *items; ++items)
            gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _(*items));

        gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);


        str = g_strdup_printf ("<b>%s</b>", _("Trim blanks"));
        label = gtk_label_new_with_mnemonic (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

        component->priv->trim_combo = combo = gtk_combo_box_new_text ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);

        gchar *trim_modes[] = {
                                  _("<none>"),
                                  _("leading"),
                                  _("trailing"),
                                  _("leading and trailing"),
                                  NULL
                              };

        for (gchar **items=trim_modes; *items; ++items)
            gtk_combo_box_append_text (GTK_COMBO_BOX (combo), _(*items));

        gtk_box_pack_start (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
    }
}


static void gnome_cmd_advrename_profile_component_finalize (GObject *object)
{
    GnomeCmdAdvrenameProfileComponent *component = GNOME_CMD_ADVRENAME_PROFILE_COMPONENT (object);

    delete component->priv;

    G_OBJECT_CLASS (gnome_cmd_advrename_profile_component_parent_class)->finalize (object);
}


static void gnome_cmd_advrename_profile_component_class_init (GnomeCmdAdvrenameProfileComponentClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_advrename_profile_component_finalize;

    signals[TEMPLATE_CHANGED] =
        g_signal_new ("template-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdAdvrenameProfileComponentClass, template_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[COUNTER_CHANGED] =
        g_signal_new ("counter-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdAdvrenameProfileComponentClass, counter_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    signals[REGEX_CHANGED] =
        g_signal_new ("regex-changed",
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            G_STRUCT_OFFSET (GnomeCmdAdvrenameProfileComponentClass, regex_changed),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);
}


GnomeCmdAdvrenameProfileComponent::GnomeCmdAdvrenameProfileComponent(GnomeCmdData::AdvrenameConfig::Profile &p): profile(p)
{
    // Template
    gtk_widget_grab_focus (priv->template_entry);
    g_signal_connect (priv->template_combo, "changed", G_CALLBACK (Private::on_template_entry_changed), this);

    // Counter
    g_signal_connect (priv->counter_start_spin, "value-changed", G_CALLBACK (Private::on_counter_start_spin_value_changed), this);
    g_signal_connect (priv->counter_step_spin, "value-changed", G_CALLBACK (Private::on_counter_step_spin_value_changed), this);
    g_signal_connect (priv->counter_digits_combo, "changed", G_CALLBACK (Private::on_counter_digits_combo_value_changed), this);

    // Regex
    priv->regex_model = create_regex_model ();
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->regex_view), priv->regex_model);

    g_signal_connect (priv->regex_model, "row-deleted", G_CALLBACK (Private::on_regex_model_row_deleted), this);
    g_signal_connect (priv->regex_view, "row-activated", G_CALLBACK (Private::on_regex_view_row_activated), this);
    g_signal_connect (priv->regex_add_button, "clicked", G_CALLBACK (Private::on_regex_add_btn_clicked), this);
    g_signal_connect (priv->regex_edit_button, "clicked", G_CALLBACK (Private::on_regex_edit_btn_clicked), this);
    g_signal_connect (priv->regex_remove_button, "clicked", G_CALLBACK (Private::on_regex_remove_btn_clicked), this);
    g_signal_connect (priv->regex_remove_all_button, "clicked", G_CALLBACK (Private::on_regex_remove_all_btn_clicked), this);

    // Case conversion & blank triming
    g_signal_connect (priv->case_combo, "changed", G_CALLBACK (Private::on_case_combo_changed), this);
    g_signal_connect (priv->trim_combo, "changed", G_CALLBACK (Private::on_trim_combo_changed), this);
}


inline GtkTreeModel *create_regex_model ()
{
    GtkTreeModel *model = GTK_TREE_MODEL (gtk_list_store_new (GnomeCmdAdvrenameProfileComponent::NUM_REGEX_COLS,
                                                              G_TYPE_POINTER,
                                                              G_TYPE_BOOLEAN,
                                                              G_TYPE_STRING,
                                                              G_TYPE_STRING,
                                                              G_TYPE_STRING));
    return model;
}


template <typename T>
inline GtkTreeModel *copy_regex_model(GtkTreeModel *model, guint col_id, vector<T> &v)
{
    v.clear();

    GtkTreeIter i;

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (model, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (model, &i))
    {
        T *rx;

        gtk_tree_model_get (model, &i,
                            col_id, &rx,
                            -1);
        if (rx)                            //  ignore null regex patterns
            v.push_back(*rx);
    }

    return model;
}


inline GtkTreeModel *clear_regex_model(GtkTreeModel *model)
{
    GtkTreeIter i;

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (model, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (model, &i))
    {
        GnomeCmd::RegexReplace *rx;

        gtk_tree_model_get (model, &i,
                            GnomeCmdAdvrenameProfileComponent::COL_REGEX, &rx,
                            -1);
        delete rx;
    }

    return model;
}


inline GtkWidget *create_regex_view ()
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "rules-hint", TRUE,
                  "reorderable", TRUE,
                  "enable-search", FALSE,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    GtkTooltips *tips = gtk_tooltips_new ();

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameProfileComponent::COL_PATTERN, _("Search for"));
    g_object_set (renderer, "foreground", "red", NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameProfileComponent::COL_MALFORMED_REGEX);
    gtk_tooltips_set_tip (tips, col->button, _("Regex pattern"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameProfileComponent::COL_REPLACE, _("Replace with"));
    g_object_set (renderer, "foreground", "red", NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameProfileComponent::COL_MALFORMED_REGEX);
    gtk_tooltips_set_tip (tips, col->button, _("Replacement"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameProfileComponent::COL_MATCH_CASE, _("Match case"));
    g_object_set (renderer, "foreground", "red", NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameProfileComponent::COL_MALFORMED_REGEX);
    gtk_tooltips_set_tip (tips, col->button, _("Case sensitive matching"), NULL);

    g_object_set (renderer,
                  "xalign", 0.0,
                  NULL);

    return view;
}


void GnomeCmdAdvrenameProfileComponent::update()
{
    set_template_history(gnome_cmd_data.advrename_defaults.templates.ents);

    gtk_entry_set_text (GTK_ENTRY (priv->template_entry), profile.template_string.empty() ? "$N" : profile.template_string.c_str());
    gtk_editable_set_position (GTK_EDITABLE (priv->template_entry), -1);
    gtk_editable_select_region (GTK_EDITABLE (priv->template_entry), -1, -1);

    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->counter_start_spin), profile.counter_start);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->counter_step_spin), profile.counter_step);
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->counter_digits_combo), profile.counter_width);

    if (!model_is_empty(priv->regex_model))
    {
        clear_regex_model(priv->regex_model);

        g_signal_handlers_block_by_func (priv->regex_model, gpointer (Private::on_regex_model_row_deleted), this);
        gtk_list_store_clear (GTK_LIST_STORE (priv->regex_model));
        g_signal_handlers_unblock_by_func (priv->regex_model, gpointer (Private::on_regex_model_row_deleted), this);
    }

    priv->fill_regex_model(profile);

    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->case_combo), profile.case_conversion);
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->trim_combo), profile.trim_blanks);

    g_signal_emit (this, signals[REGEX_CHANGED], 0);
}


const gchar *GnomeCmdAdvrenameProfileComponent::get_template_entry() const
{
    return gtk_entry_get_text (GTK_ENTRY (priv->template_entry));
}


void GnomeCmdAdvrenameProfileComponent::set_template_history(GList *history)
{
    GtkTreeModel *store = gtk_combo_box_get_model (GTK_COMBO_BOX (priv->template_combo));
    gtk_list_store_clear (GTK_LIST_STORE (store));

    for (GList *i=history; i; i=i->next)
        gtk_combo_box_append_text (GTK_COMBO_BOX (priv->template_combo), (const gchar *) i->data);
}


void GnomeCmdAdvrenameProfileComponent::set_sample_fname(const gchar *fname)
{
    g_free (priv->sample_fname);
    priv->sample_fname = g_strdup (fname);
}


GtkTreeModel *GnomeCmdAdvrenameProfileComponent::get_regex_model() const
{
    return priv ? priv->regex_model : NULL;
}


void GnomeCmdAdvrenameProfileComponent::copy()
{
    const char *template_entry = get_template_entry();

    profile.template_string =  template_entry && *template_entry ? template_entry : "$N";
    copy_regex_model(priv->regex_model, COL_REGEX, profile.regexes);
}


void GnomeCmdAdvrenameProfileComponent::copy(GnomeCmdData::AdvrenameConfig::Profile &p)
{
    const char *template_entry = get_template_entry();

    profile.template_string =  template_entry && *template_entry ? template_entry : "$N";
    copy_regex_model(priv->regex_model, COL_REGEX, p.regexes);

    if (&p==&profile)
        return;

    p.name.clear();
    p.template_string = profile.template_string;
    p.counter_start = profile.counter_start;
    p.counter_width = profile.counter_width;
    p.counter_step = profile.counter_step;
    p.case_conversion = profile.case_conversion;
    p.trim_blanks = profile.trim_blanks;
}


gchar *GnomeCmdAdvrenameProfileComponent::convert_case(gchar *string)
{
    return priv->convert_case(string);
}


gchar *GnomeCmdAdvrenameProfileComponent::trim_blanks(gchar *string)
{
    return priv->trim_blanks(string);
}

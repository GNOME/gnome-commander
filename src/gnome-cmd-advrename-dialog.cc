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

#include <config.h>
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>
#include <errno.h>
#include <gtk/gtkdialog.h>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-convert.h"
#include "gnome-cmd-advrename-dialog.h"
#include "dialogs/gnome-cmd-advrename-regex-dialog.h"
#include "gnome-cmd-advrename-lexer.h"
#include "gnome-cmd-file.h"
#include "gnome-cmd-treeview.h"
#include "gnome-cmd-data.h"
#include "tags/gnome-cmd-tags.h"
#include "utils.h"

using namespace std;


struct GnomeCmdAdvrenameDialogClass
{
    GtkDialogClass parent_class;
};


struct GnomeCmdAdvrenameDialog::Private
{
    GnomeCmdConvertFunc convert_case;
    GnomeCmdConvertFunc trim_blanks;

    GtkWidget *template_combo;
    GtkWidget *template_entry;

    gboolean template_has_counters;

    GtkWidget *counter_start_spin;
    GtkWidget *counter_step_spin;
    GtkWidget *counter_digits_spin;

    GtkWidget *regex_view;

    GtkWidget *regex_add_button;
    GtkWidget *regex_edit_button;
    GtkWidget *regex_remove_button;
    GtkWidget *regex_remove_all_button;

    GtkWidget *case_combo;
    GtkWidget *trim_combo;

    GtkWidget *files_view;

    enum {DIR_MENU, FILE_MENU, COUNTER_MENU, DATE_MENU, METATAG_MENU, NUM_MENUS};

    GtkWidget *menu[NUM_MENUS];

    static GtkItemFactoryEntry dir_items[];
    static GtkItemFactoryEntry name_items[];
    static GtkItemFactoryEntry counter_items[];
    static GtkItemFactoryEntry date_items[];
    static GtkItemFactoryEntry *items[];
    static GnomeCmdTag metatags[];

    Private();
    ~Private();

    GtkWidget *create_placeholder_menu(int menu_type);
    GtkWidget *create_button_with_menu(gchar *label_text, int menu_type);
    void insert_tag(const gchar *text);

    static void insert_text_tag(GnomeCmdAdvrenameDialog::Private *priv, guint n, GtkWidget *widget);
    static void insert_num_tag(GnomeCmdAdvrenameDialog::Private *priv, guint tag, GtkWidget *widget);

    void files_view_popup_menu (GtkWidget *treeview, GnomeCmdAdvrenameDialog *dialog, GdkEventButton *event=NULL);

    static void on_template_entry_changed(GtkEntry *entry, GnomeCmdAdvrenameDialog *dialog);
    static void on_menu_button_clicked(GtkButton *widget, GtkWidget *menu);

    static void on_counter_start_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameDialog *dialog);
    static void on_counter_step_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameDialog *dialog);
    static void on_counter_digits_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameDialog *dialog);

    static void on_regex_model_row_deleted (GtkTreeModel *treemodel, GtkTreePath *path, GnomeCmdAdvrenameDialog *dialog);
    static void on_regex_add_btn_clicked (GtkButton *button, GnomeCmdAdvrenameDialog *dialog);
    static void on_regex_edit_btn_clicked (GtkButton *button, GnomeCmdAdvrenameDialog *dialog);
    static void on_regex_remove_btn_clicked (GtkButton *button, GnomeCmdAdvrenameDialog *dialog);
    static void on_regex_remove_all_btn_clicked (GtkButton *button, GnomeCmdAdvrenameDialog *dialog);
    static void on_regex_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameDialog *dialog);

    static void on_case_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameDialog *dialog);
    static void on_trim_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameDialog *dialog);

    static void on_files_model_row_deleted (GtkTreeModel *files, GtkTreePath *path, GnomeCmdAdvrenameDialog *dialog);
    static void on_files_view_popup_menu__remove (GtkWidget *menuitem, GtkTreeView *treeview);
    static void on_files_view_popup_menu__show_properties (GtkWidget *menuitem, GtkTreeView *treeview);
    static void on_files_view_popup_menu__update_files (GtkWidget *menuitem, GnomeCmdAdvrenameDialog *dialog);
    static gboolean on_files_view_button_pressed (GtkWidget *treeview, GdkEventButton *event, GnomeCmdAdvrenameDialog *dialog);
    static gboolean on_files_view_popup_menu (GtkWidget *treeview, GnomeCmdAdvrenameDialog *dialog);
    static void on_files_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameDialog *dialog);

    static gboolean on_dialog_delete (GtkWidget *widget, GdkEvent *event, GnomeCmdAdvrenameDialog *dialog);
    static void on_dialog_size_allocate (GtkWidget *widget, GtkAllocation *allocation, GnomeCmdAdvrenameDialog *dialog);
    static void on_dialog_response (GnomeCmdAdvrenameDialog *dialog, int response_id, gpointer data);
};


GtkItemFactoryEntry GnomeCmdAdvrenameDialog::Private::dir_items[] =
    {{_("/Grandparent"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 0},
     {_("/Parent"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 1}};

GtkItemFactoryEntry GnomeCmdAdvrenameDialog::Private::name_items[] =
    {{_("/File name"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 2},
     {_("/File name without extension"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 3},
     {_("/File extension"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 4}};

GtkItemFactoryEntry GnomeCmdAdvrenameDialog::Private::counter_items[] =
    {{_("/Counter"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 5},
     {_("/Counter (width)"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 6},
     {_("/Hexadecimal random number (width)"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 7}};

GtkItemFactoryEntry GnomeCmdAdvrenameDialog::Private::date_items[] =
    {{_("/Date/<locale>"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 8},
     {_("/Date/yyyy-mm-dd"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 9},
     {_("/Date/yy-mm-dd"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 10},
     {_("/Date/yy.mm.dd"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 11},
     {_("/Date/yymmdd"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 12},
     {_("/Date/dd.mm.yy"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 13},
     {_("/Date/mm-dd-yy"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 14},
     {_("/Date/yyyy"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 15},
     {_("/Date/yy"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 16},
     {_("/Date/mm"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 17},
     {_("/Date/mmm"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 18},
     {_("/Date/dd"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 19},
     {_("/Time/<locale>"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 20},
     {_("/Time/HH.MM.SS"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 21},
     {_("/Time/HH-MM-SS"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 22},
     {_("/Time/HHMMSS"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 23},
     {_("/Time/HH"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 24},
     {_("/Time/MM"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 25},
     {_("/Time/SS"), NULL, (GtkItemFactoryCallback) GnomeCmdAdvrenameDialog::Private::insert_text_tag, 26}};

GtkItemFactoryEntry *GnomeCmdAdvrenameDialog::Private::items[] = {dir_items, name_items, counter_items, date_items};

GnomeCmdTag GnomeCmdAdvrenameDialog::Private::metatags[] =
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


inline GnomeCmdAdvrenameDialog::Private::Private()
{
    convert_case = gcmd_convert_unchanged;
    trim_blanks = gcmd_convert_strip;
    template_has_counters = FALSE;
}


inline GnomeCmdAdvrenameDialog::Private::~Private()
{
}


inline GtkWidget *GnomeCmdAdvrenameDialog::Private::create_placeholder_menu(int menu_type)
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

                gtk_item_factory_create_items (ifac, items_size[menu_type], items[menu_type], this);

                return gtk_item_factory_get_widget (ifac, "<main>");
            }

        case METATAG_MENU:
            {
                GtkItemFactoryEntry *items = g_try_new0(GtkItemFactoryEntry, G_N_ELEMENTS(metatags));

                g_return_val_if_fail (items != NULL, NULL);

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
                        p->path = g_strdup_printf ("/%s/%s", gcmd_tags_get_class_name(tag), gcmd_tags_get_title(tag));
                        p->callback = (GtkItemFactoryCallback) insert_num_tag;
                        p->callback_action = tag;
                    }
                }

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


inline GtkWidget *GnomeCmdAdvrenameDialog::Private::create_button_with_menu(gchar *label_text, int menu_type)
{
    GtkWidget *arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
    GtkWidget *label = gtk_label_new (label_text);
    GtkWidget *hbox = gtk_hbox_new (FALSE, 3);

    gtk_box_pack_start(GTK_BOX (hbox), label, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX (hbox), arrow, FALSE, FALSE, 0);

    GtkWidget *button = gtk_button_new ();
    gtk_container_add (GTK_CONTAINER (button), hbox);

    menu[menu_type] = create_placeholder_menu(menu_type);

    gtk_widget_set_events (button, GDK_BUTTON_PRESS_MASK);
    g_signal_connect (G_OBJECT(button), "clicked", G_CALLBACK (on_menu_button_clicked), menu[menu_type]);

    gtk_widget_show_all(button);

    return button;
}


inline void GnomeCmdAdvrenameDialog::Private::insert_tag(const gchar *text)
{
    GtkEditable *editable = (GtkEditable *) template_entry;
    gint pos = gtk_editable_get_position (editable);

    gtk_editable_insert_text (editable, text, strlen(text), &pos);
    gtk_editable_set_position (editable, pos);
    gtk_widget_grab_focus ((GtkWidget *) editable);
    gtk_editable_select_region (editable, pos, pos);
}


void GnomeCmdAdvrenameDialog::Private::insert_text_tag(GnomeCmdAdvrenameDialog::Private *priv, guint n, GtkWidget *widget)
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
                                         "%S"};         // 26

    g_return_if_fail (n < G_N_ELEMENTS(placeholder));

    priv->insert_tag(placeholder[n]);
}


void GnomeCmdAdvrenameDialog::Private::insert_num_tag(GnomeCmdAdvrenameDialog::Private *priv, guint tag, GtkWidget *widget)
{
    gchar *s = g_strdup_printf ("$T(%s)", gcmd_tags_get_name((GnomeCmdTag) tag));
    priv->insert_tag(s);
    g_free (s);
}


void GnomeCmdAdvrenameDialog::Private::on_menu_button_clicked(GtkButton *widget, GtkWidget *menu)
{
    GdkEventButton *event = (GdkEventButton *) gtk_get_current_event();

    if (event == NULL)
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 1, gtk_get_current_event_time());
    else
        if (event->button == 1)
            gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
}


inline GtkTreeModel *create_and_fill_regex_model (GnomeCmdData::AdvrenameDefaults &defaults);
inline GtkWidget *create_regex_view ();

inline GtkTreeModel *create_files_model ();
inline GtkWidget *create_files_view ();


inline gboolean model_is_empty (GtkTreeModel *tree_model)
{
    GtkTreeIter iter;

    return !gtk_tree_model_get_iter_first (tree_model, &iter);
}


G_DEFINE_TYPE (GnomeCmdAdvrenameDialog, gnome_cmd_advrename_dialog, GTK_TYPE_DIALOG)


void GnomeCmdAdvrenameDialog::Private::on_template_entry_changed(GtkEntry *entry, GnomeCmdAdvrenameDialog *dialog)
{
    gnome_cmd_advrename_parse_template (gtk_entry_get_text (GTK_ENTRY (dialog->priv->template_entry)), dialog->priv->template_has_counters);
    dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_counter_start_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameDialog *dialog)
{
    dialog->defaults.counter_start = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
    if (dialog->priv->template_has_counters)
        dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_counter_step_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameDialog *dialog)
{
    dialog->defaults.counter_increment = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
    if (dialog->priv->template_has_counters)
        dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_counter_digits_spin_value_changed (GtkWidget *spin, GnomeCmdAdvrenameDialog *dialog)
{
    dialog->defaults.counter_precision = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
    if (dialog->priv->template_has_counters)
        dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_regex_model_row_deleted (GtkTreeModel *treemodel, GtkTreePath *path, GnomeCmdAdvrenameDialog *dialog)
{
    dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_regex_add_btn_clicked (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    Regex *rx = new Regex;

    if (gnome_cmd_advrename_regex_dialog_new (_("Add Rule"), GTK_WINDOW (dialog), rx))
    {
        GtkTreeIter i;

        gtk_list_store_append (GTK_LIST_STORE (dialog->defaults.regexes), &i);
        gtk_list_store_set (GTK_LIST_STORE (dialog->defaults.regexes), &i,
                            COL_REGEX, rx,
                            COL_MALFORMED_REGEX, !*rx,
                            COL_PATTERN, rx->from.c_str(),
                            COL_REPLACE, rx->to.c_str(),
                            COL_MATCH_CASE, rx->case_sensitive ? _("Yes") : _("No"),
                            -1);

        dialog->update_new_filenames();

        gtk_widget_set_sensitive (dialog->priv->regex_edit_button, TRUE);
        gtk_widget_set_sensitive (dialog->priv->regex_remove_button, TRUE);
        gtk_widget_set_sensitive (dialog->priv->regex_remove_all_button, TRUE);
    }
    else
        delete rx;
}


void GnomeCmdAdvrenameDialog::Private::on_regex_edit_btn_clicked (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (dialog->priv->regex_view);
    GtkTreeIter i;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (tree_view), NULL, &i))
    {
        Regex *rx = NULL;

        gtk_tree_model_get (dialog->defaults.regexes, &i, COL_REGEX, &rx, -1);

        if (gnome_cmd_advrename_regex_dialog_new (_("Edit Rule"), GTK_WINDOW (dialog), rx))
        {
            gtk_list_store_set (GTK_LIST_STORE (dialog->defaults.regexes), &i,
                                COL_REGEX, rx,
                                COL_MALFORMED_REGEX, !*rx,
                                COL_PATTERN, rx->from.c_str(),
                                COL_REPLACE, rx->to.c_str(),
                                COL_MATCH_CASE, rx->case_sensitive ? _("Yes") : _("No"),
                                -1);

            dialog->update_new_filenames();
        }
    }
}


void GnomeCmdAdvrenameDialog::Private::on_regex_remove_btn_clicked (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    GtkTreeView *tree_view = GTK_TREE_VIEW (dialog->priv->regex_view);
    GtkTreeIter i;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (tree_view), NULL, &i))
    {
        Regex *rx = NULL;

        gtk_tree_model_get (dialog->defaults.regexes, &i, COL_REGEX, &rx, -1);
        gtk_list_store_remove (GTK_LIST_STORE (dialog->defaults.regexes), &i);
        delete rx;

        if (model_is_empty (dialog->defaults.regexes))
        {
            gtk_widget_set_sensitive (dialog->priv->regex_edit_button, FALSE);
            gtk_widget_set_sensitive (dialog->priv->regex_remove_button, FALSE);
            gtk_widget_set_sensitive (dialog->priv->regex_remove_all_button, FALSE);
        }
    }
}


void GnomeCmdAdvrenameDialog::Private::on_regex_remove_all_btn_clicked (GtkButton *button, GnomeCmdAdvrenameDialog *dialog)
{
    GtkTreeIter i;

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (dialog->defaults.regexes, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (dialog->defaults.regexes, &i))
    {
        Regex *rx = NULL;

        gtk_tree_model_get (dialog->defaults.regexes, &i, COL_REGEX, &rx, -1);
        delete rx;
    }

    g_signal_handlers_block_by_func (dialog->defaults.regexes, gpointer (on_regex_model_row_deleted), dialog);
    gtk_list_store_clear (GTK_LIST_STORE (dialog->defaults.regexes));
    g_signal_handlers_unblock_by_func (dialog->defaults.regexes, gpointer (on_regex_model_row_deleted), dialog);

    dialog->update_new_filenames();

    gtk_widget_set_sensitive (dialog->priv->regex_edit_button, FALSE);
    gtk_widget_set_sensitive (dialog->priv->regex_remove_button, FALSE);
    gtk_widget_set_sensitive (dialog->priv->regex_remove_all_button, FALSE);
}


void GnomeCmdAdvrenameDialog::Private::on_regex_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameDialog *dialog)
{
    on_regex_edit_btn_clicked (NULL, dialog);
}


void GnomeCmdAdvrenameDialog::Private::on_case_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameDialog *dialog)
{
    switch (gtk_combo_box_get_active (combo))
    {
        case 0: dialog->priv->convert_case = gcmd_convert_unchanged; break;
        case 1: dialog->priv->convert_case = gcmd_convert_lowercase; break;
        case 2: dialog->priv->convert_case = gcmd_convert_uppercase; break;
        case 3: dialog->priv->convert_case = gcmd_convert_sentence_case; break;
        case 4: dialog->priv->convert_case = gcmd_convert_initial_caps; break;
        case 5: dialog->priv->convert_case = gcmd_convert_toggle_case; break;

        default:
            return;
    }

    dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_trim_combo_changed (GtkComboBox *combo, GnomeCmdAdvrenameDialog *dialog)
{
    switch (gtk_combo_box_get_active (combo))
    {
        case 0: dialog->priv->trim_blanks = gcmd_convert_unchanged; break;
        case 1: dialog->priv->trim_blanks = gcmd_convert_ltrim; break;
        case 2: dialog->priv->trim_blanks = gcmd_convert_rtrim; break;
        case 3: dialog->priv->trim_blanks = gcmd_convert_strip; break;

        default:
            return;
    }

    dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_files_model_row_deleted (GtkTreeModel *files, GtkTreePath *path, GnomeCmdAdvrenameDialog *dialog)
{
    if (dialog->priv->template_has_counters)
        dialog->update_new_filenames();
}


void GnomeCmdAdvrenameDialog::Private::on_files_view_popup_menu__remove (GtkWidget *menuitem, GtkTreeView *treeview)
{
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (treeview);

        GnomeCmdFile *f;

        gtk_tree_model_get (model, &iter, COL_FILE, &f, -1);
        gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

        gnome_cmd_file_unref (f);
    }
}


void GnomeCmdAdvrenameDialog::Private::on_files_view_popup_menu__show_properties (GtkWidget *menuitem, GtkTreeView *treeview)
{
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), NULL, &iter))
    {
        GtkTreeModel *model = gtk_tree_view_get_model (treeview);
        GnomeCmdFile *f;

        gtk_tree_model_get (model, &iter, COL_FILE, &f, -1);

        if (f)
            gnome_cmd_file_show_properties (f);
    }
}


void GnomeCmdAdvrenameDialog::Private::on_files_view_popup_menu__update_files (GtkWidget *menuitem, GnomeCmdAdvrenameDialog *dialog)
{
    GtkTreeIter i;
    GnomeCmdFile *f;

    //  re-read file attributes, as they could be changed...
    for (gboolean valid_iter=gtk_tree_model_get_iter_first (dialog->files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (dialog->files, &i))
    {
        gtk_tree_model_get (dialog->files, &i,
                            COL_FILE, &f,
                            -1);

        gtk_list_store_set (GTK_LIST_STORE (dialog->files), &i,
                            COL_NAME, gnome_cmd_file_get_name (f),
                            COL_SIZE, gnome_cmd_file_get_size (f),
                            COL_DATE, gnome_cmd_file_get_mdate (f, FALSE),
                            COL_RENAME_FAILED, FALSE,
                            -1);
    }

    gnome_cmd_advrename_parse_template (gtk_entry_get_text (GTK_ENTRY (dialog->priv->template_entry)), dialog->priv->template_has_counters);
    dialog->update_new_filenames();
}


inline void GnomeCmdAdvrenameDialog::Private::files_view_popup_menu (GtkWidget *treeview, GnomeCmdAdvrenameDialog *dialog, GdkEventButton *event)
{
    GtkWidget *menu = gtk_menu_new ();
    GtkWidget *menuitem;

    menuitem = gtk_menu_item_new_with_label (_("Remove from file list"));
    g_signal_connect (menuitem, "activate", G_CALLBACK (on_files_view_popup_menu__remove), treeview);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    menuitem = gtk_menu_item_new_with_label (_("File properties"));
    g_signal_connect (menuitem, "activate", G_CALLBACK (on_files_view_popup_menu__show_properties), treeview);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

    menuitem = gtk_menu_item_new_with_label (_("Update file list"));
    g_signal_connect (menuitem, "activate", G_CALLBACK (on_files_view_popup_menu__update_files), dialog);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    gtk_widget_show_all (menu);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
                    (event != NULL) ? event->button : 0, gdk_event_get_time ((GdkEvent*) event));
}


gboolean GnomeCmdAdvrenameDialog::Private::on_files_view_button_pressed (GtkWidget *treeview, GdkEventButton *event, GnomeCmdAdvrenameDialog *dialog)
{
    if (event->type==GDK_BUTTON_PRESS && event->button==3)
    {
        // optional: select row if no row is selected or only one other row is selected
        // (will only do something if you set a tree selection mode
        // as described later in the tutorial)
        if (1)
        {
            GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
            if (gtk_tree_selection_count_selected_rows (selection) <= 1)
            {
                GtkTreePath *path;

                if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview),
                                                   (gint) event->x, (gint) event->y,
                                                   &path,
                                                   NULL, NULL, NULL))
                {
                    gtk_tree_selection_unselect_all (selection);
                    gtk_tree_selection_select_path (selection, path);
                    gtk_tree_path_free (path);
                }
            }
        }
        dialog->priv->files_view_popup_menu (treeview, dialog, event);

        return TRUE;
    }

    return FALSE;
}


gboolean GnomeCmdAdvrenameDialog::Private::on_files_view_popup_menu (GtkWidget *treeview, GnomeCmdAdvrenameDialog *dialog)
{
    dialog->priv->files_view_popup_menu (treeview, dialog);

    return TRUE;
}


void GnomeCmdAdvrenameDialog::Private::on_files_view_row_activated (GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *col, GnomeCmdAdvrenameDialog *dialog)
{
    on_files_view_popup_menu__show_properties (NULL, view);
}


gboolean GnomeCmdAdvrenameDialog::Private::on_dialog_delete (GtkWidget *widget, GdkEvent *event, GnomeCmdAdvrenameDialog *dialog)
{
    return event->type==GDK_DELETE;
}


void GnomeCmdAdvrenameDialog::Private::on_dialog_size_allocate (GtkWidget *widget, GtkAllocation *allocation, GnomeCmdAdvrenameDialog *dialog)
{
    dialog->defaults.width  = allocation->width;
    dialog->defaults.height = allocation->height;
}


void GnomeCmdAdvrenameDialog::Private::on_dialog_response (GnomeCmdAdvrenameDialog *dialog, int response_id, gpointer unused)
{
    GtkTreeIter i;

    switch (response_id)
    {
        case GTK_RESPONSE_OK:
        case GTK_RESPONSE_APPLY:
            for (gboolean valid_iter=gtk_tree_model_get_iter_first (dialog->files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (dialog->files, &i))
            {
                GnomeCmdFile *f;
                gchar *new_name;

                gtk_tree_model_get (dialog->files, &i,
                                    COL_FILE, &f,
                                    COL_NEW_NAME, &new_name,
                                    -1);

                GnomeVFSResult result = GNOME_VFS_OK;

                if (strcmp (f->info->name, new_name) != 0)
                    result = gnome_cmd_file_rename (f, new_name);

                gtk_list_store_set (GTK_LIST_STORE (dialog->files), &i,
                                    COL_NAME, gnome_cmd_file_get_name (f),
                                    COL_RENAME_FAILED, result!=GNOME_VFS_OK,
                                    -1);

                g_free (new_name);
            }
            dialog->update_new_filenames();
            dialog->defaults.templates->add(gtk_entry_get_text (GTK_ENTRY (dialog->priv->template_entry)));
            break;


        case GTK_RESPONSE_NONE:
        case GTK_RESPONSE_DELETE_EVENT:
        case GTK_RESPONSE_CANCEL:
        case GTK_RESPONSE_CLOSE:
            dialog->defaults.templates->add(gtk_entry_get_text (GTK_ENTRY (dialog->priv->template_entry)));
            gtk_widget_hide (*dialog);
            dialog->unset();
            g_signal_stop_emission_by_name (dialog, "response");        //  FIXME:  ???
            break;

        case GTK_RESPONSE_HELP:
            gnome_cmd_help_display ("gnome-commander.xml", "gnome-commander-advanced-rename");
            g_signal_stop_emission_by_name (dialog, "response");
            break;

        case GCMD_RESPONSE_RESET:
            gtk_entry_set_text (GTK_ENTRY (dialog->priv->template_entry), "$N");
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->counter_start_spin), 1);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->counter_step_spin), 1);
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->priv->counter_digits_spin), 1);
            gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->case_combo), 0);
            gtk_combo_box_set_active (GTK_COMBO_BOX (dialog->priv->trim_combo), 3);
            on_regex_remove_all_btn_clicked (NULL, dialog);
            break;

        default :
            g_assert_not_reached ();
    }
}


static void gnome_cmd_advrename_dialog_init (GnomeCmdAdvrenameDialog *dialog)
{
    dialog->priv = new GnomeCmdAdvrenameDialog::Private;

    gtk_window_set_title (*dialog, _("Advanced Rename Tool"));
    gtk_window_set_resizable (*dialog, TRUE);
    gtk_dialog_set_has_separator (*dialog, FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
    gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 2);

    GtkWidget *align;
    GtkWidget *label;
    GtkWidget *table;
    GtkWidget *combo;
    GtkWidget *hbox;
    GtkWidget *vbox;
    GtkWidget *bbox;
    GtkWidget *spin;
    GtkWidget *button;

    gchar *str;

    vbox = gtk_vbox_new (FALSE, 6);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);

    hbox = gtk_hbox_new (FALSE, 18);
    gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

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

            dialog->priv->template_combo = combo = gtk_combo_box_entry_new_text ();
            dialog->priv->template_entry = gtk_bin_get_child (GTK_BIN (dialog->priv->template_combo));
            gtk_entry_set_activates_default (GTK_ENTRY (dialog->priv->template_entry), TRUE);
            gtk_label_set_mnemonic_widget (GTK_LABEL (label), combo);
            gtk_box_pack_start (GTK_BOX (vbox), combo, FALSE, FALSE, 0);

            GtkWidget *bbox = gtk_hbutton_box_new ();
            gtk_box_pack_start (GTK_BOX (vbox), bbox, TRUE, FALSE, 0);

            gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_START);
            gtk_box_set_spacing (GTK_BOX (bbox), 6);
            gtk_box_pack_start (GTK_BOX (bbox), dialog->priv->create_button_with_menu (_("Directory"), GnomeCmdAdvrenameDialog::Private::DIR_MENU), FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (bbox), dialog->priv->create_button_with_menu (_("File"), GnomeCmdAdvrenameDialog::Private::FILE_MENU), FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (bbox), dialog->priv->create_button_with_menu (_("Counter"), GnomeCmdAdvrenameDialog::Private::COUNTER_MENU), FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (bbox), dialog->priv->create_button_with_menu (_("Date"), GnomeCmdAdvrenameDialog::Private::DATE_MENU), FALSE, FALSE, 0);
            gtk_box_pack_start (GTK_BOX (bbox), dialog->priv->create_button_with_menu (_("Metatag"), GnomeCmdAdvrenameDialog::Private::METATAG_MENU), FALSE, FALSE, 0);
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
        dialog->priv->counter_start_spin = spin = gtk_spin_button_new_with_range (0, 1000000, 1);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
        gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
        gtk_table_attach_defaults (GTK_TABLE (table), spin, 1, 2, 0, 1);

        label = gtk_label_new_with_mnemonic (_("Ste_p:"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        dialog->priv->counter_step_spin = spin = gtk_spin_button_new_with_range (-1000, 1000, 1);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
        gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
        gtk_table_attach_defaults (GTK_TABLE (table), spin, 1, 2, 1, 2);

        label = gtk_label_new_with_mnemonic (_("Di_gits:"));
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        dialog->priv->counter_digits_spin = spin = gtk_spin_button_new_with_range (1, 16, 1);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), spin);
        gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
        gtk_table_attach_defaults (GTK_TABLE (table), spin, 1, 2, 2, 3);
    }


    // Regex
    {
        str = g_strdup_printf ("<b>%s</b>", _("Regex replacing"));
        label = gtk_label_new (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
        gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 18, 12, 0);
        gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);

        table = gtk_table_new (2, 1, FALSE);
        gtk_table_set_row_spacings (GTK_TABLE (table), 6);
        gtk_table_set_col_spacings (GTK_TABLE (table), 12);
        gtk_container_add (GTK_CONTAINER (align), table);

        GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
        gtk_table_attach (GTK_TABLE (table), scrolled_window, 0, 1, 0, 1, (GtkAttachOptions) (GTK_EXPAND|GTK_FILL), GTK_FILL, 0, 0);

        dialog->priv->regex_view = create_regex_view ();
        gtk_container_add (GTK_CONTAINER (scrolled_window), dialog->priv->regex_view);

        bbox = gtk_vbutton_box_new ();
        gtk_button_box_set_layout (GTK_BUTTON_BOX (bbox), GTK_BUTTONBOX_END);
        gtk_box_set_spacing (GTK_BOX (bbox), 12);
        gtk_table_attach (GTK_TABLE (table), bbox, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 0, 0);

        dialog->priv->regex_add_button = button = gtk_button_new_from_stock (GTK_STOCK_ADD);
        gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

        dialog->priv->regex_edit_button = button = gtk_button_new_from_stock (GTK_STOCK_EDIT);
        gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

        dialog->priv->regex_remove_button = button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
        gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);

        dialog->priv->regex_remove_all_button = button = gtk_button_new_with_mnemonic ("Remove A_ll");
        gtk_box_pack_start (GTK_BOX (bbox), button, FALSE, FALSE, 0);
    }


    align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 18, 0, 0);
    gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);
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

        dialog->priv->case_combo = combo = gtk_combo_box_new_text ();
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

        dialog->priv->trim_combo = combo = gtk_combo_box_new_text ();
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

    // Results
    {
        str = g_strdup_printf ("<b>%s</b>", _("Results"));
        label = gtk_label_new_with_mnemonic (str);
        g_free (str);

        gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
        gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

        align = gtk_alignment_new (0.0, 0.0, 1.0, 1.0);
        gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 6, 12, 0);
        gtk_container_add (GTK_CONTAINER (vbox), align);

        GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
        gtk_container_add (GTK_CONTAINER (align), scrolled_window);

        dialog->priv->files_view = create_files_view ();
        gtk_container_add (GTK_CONTAINER (scrolled_window), dialog->priv->files_view);

        GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->priv->files_view));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
    }

    gtk_dialog_add_buttons (*dialog,
                            GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                            _("Reset"), GnomeCmdAdvrenameDialog::GCMD_RESPONSE_RESET,
                            GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                            GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
                            NULL);

    gtk_dialog_set_default_response (*dialog, GTK_RESPONSE_APPLY);
}


static void gnome_cmd_advrename_dialog_finalize (GObject *object)
{
    GnomeCmdAdvrenameDialog *dialog = GNOME_CMD_ADVRENAME_DIALOG (object);

    delete dialog->priv;

    G_OBJECT_CLASS (gnome_cmd_advrename_dialog_parent_class)->finalize (object);
}


static void gnome_cmd_advrename_dialog_class_init (GnomeCmdAdvrenameDialogClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = gnome_cmd_advrename_dialog_finalize;
}


inline GtkWidget *create_regex_view ()
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "rules-hint", TRUE,
                  "reorderable", TRUE,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    GtkTooltips *tips = gtk_tooltips_new ();

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameDialog::COL_PATTERN, _("Search for"));
    g_object_set (renderer, "foreground", "red", NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_MALFORMED_REGEX);
    gtk_tooltips_set_tip (tips, col->button, _("Regex pattern"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameDialog::COL_REPLACE, _("Replace with"));
    g_object_set (renderer, "foreground", "red", NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_MALFORMED_REGEX);
    gtk_tooltips_set_tip (tips, col->button, _("Replacement"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameDialog::COL_MATCH_CASE, _("Match case"));
    g_object_set (renderer, "foreground", "red", NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_MALFORMED_REGEX);
    gtk_tooltips_set_tip (tips, col->button, _("Case sensitive matching"), NULL);

    g_object_set (renderer,
                  "xalign", 0.0,
                  NULL);

    return view;
}


inline GtkTreeModel *create_files_model ()
{
    GtkListStore *store = gtk_list_store_new (GnomeCmdAdvrenameDialog::NUM_FILE_COLS,
                                              G_TYPE_POINTER,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING,
                                              G_TYPE_BOOLEAN);

    return GTK_TREE_MODEL (store);
}


inline GtkWidget *create_files_view ()
{
    GtkWidget *view = gtk_tree_view_new ();

    g_object_set (view,
                  "rules-hint", TRUE,
                  "reorderable", TRUE,
                  "enable-search", TRUE,
                  "search-column", GnomeCmdAdvrenameDialog::COL_NAME,
                  NULL);

    GtkCellRenderer *renderer = NULL;
    GtkTreeViewColumn *col = NULL;

    GtkTooltips *tips = gtk_tooltips_new ();

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameDialog::COL_NAME, _("Old name"));
    g_object_set (renderer, "foreground", "DarkGray", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tooltips_set_tip (tips, col->button, _("Current file name"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameDialog::COL_NEW_NAME, _("New name"));
    g_object_set (renderer, "foreground", "DarkGray", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tooltips_set_tip (tips, col->button, _("New file name"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), renderer, GnomeCmdAdvrenameDialog::COL_SIZE, _("Size"));
    g_object_set (renderer, "xalign", 1.0, "foreground", "DarkGray", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tooltips_set_tip (tips, col->button, _("File size"), NULL);

    col = gnome_cmd_treeview_create_new_text_column (GTK_TREE_VIEW (view), GnomeCmdAdvrenameDialog::COL_DATE, _("Date"));
    g_object_set (renderer, "foreground", "DarkGray", "style", PANGO_STYLE_ITALIC, NULL);
    gtk_tree_view_column_add_attribute (col, renderer, "foreground-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tree_view_column_add_attribute (col, renderer, "style-set", GnomeCmdAdvrenameDialog::COL_RENAME_FAILED);
    gtk_tooltips_set_tip (tips, col->button, _("File modification date"), NULL);

    return view;
}


void GnomeCmdAdvrenameDialog::update_new_filenames()
{
    gnome_cmd_advrename_reset_counter (defaults.counter_start, defaults.counter_precision, defaults.counter_increment);

    char buff[256];
    GtkTreeIter i;

    vector<Regex *> rx;

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (defaults.regexes, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (defaults.regexes, &i))
    {
        Regex *r;

        gtk_tree_model_get (defaults.regexes, &i,
                            COL_REGEX, &r,
                            -1);
        if (r && *r)                            //  ignore regex pattern if it can't be retrieved or if it is malformed
            rx.push_back(r);
    }

#if !GLIB_CHECK_VERSION (2, 14, 0)
    vector<pair<int,int> > match;
#endif

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (files, &i))
    {
        GnomeCmdFile *f;

        gtk_tree_model_get (files, &i,
                            COL_FILE, &f,
                            -1);
        if (!f)
            continue;

        gnome_cmd_advrename_gen_fname (buff, sizeof(buff), f);

        gchar *fname = g_strdup (buff);

        for (vector<Regex *>::iterator j=rx.begin(); j!=rx.end(); ++j)
        {
            Regex *&r = *j;

            gchar *prev_fname = fname;

#if GLIB_CHECK_VERSION (2, 14, 0)
            fname = r->replace(prev_fname);
#else
            match.clear();

            for (gchar *s=prev_fname; *s && r->match(s); s+=r->end())
                if (r->length()>0)
                    match.push_back(make_pair(r->start(), r->end()));

            gchar *src = prev_fname;
            gchar *dest = fname = (gchar *) g_malloc (strlen(prev_fname) + match.size()*r->to.size() + 1);    // allocate new fname big enough to hold all data

            for (vector<pair<int,int> >::const_iterator i=match.begin(); i!=match.end(); ++i)
            {
                memcpy(dest, src, i->first);
                dest += i->first;
                src += i->second;
                memcpy(dest, r->to.c_str(), r->to.size());
                dest += r->to.size();
            }

            strcpy(dest, src);
#endif
            g_free (prev_fname);
        }

        fname = priv->trim_blanks (priv->convert_case (fname));
        gtk_list_store_set (GTK_LIST_STORE (files), &i,
                            COL_NEW_NAME, fname,
                            -1);
        g_free (fname);
    }
}


GnomeCmdAdvrenameDialog::GnomeCmdAdvrenameDialog(GnomeCmdData::AdvrenameDefaults &cfg): defaults(cfg)
{
    gtk_window_set_default_size (*this, defaults.width, defaults.height);

    // Template
    for (GList *i=defaults.templates->ents; i; i=i->next)
        gtk_combo_box_append_text (GTK_COMBO_BOX (priv->template_combo), (const gchar *) i->data);

    if (defaults.templates->ents)
        gtk_entry_set_text (GTK_ENTRY (priv->template_entry), (const gchar *) defaults.templates->ents->data);
    else
        gtk_entry_set_text (GTK_ENTRY (priv->template_entry), "$N");

    gtk_editable_set_position (GTK_EDITABLE (priv->template_entry), -1);
    gtk_widget_grab_focus (priv->template_entry);
    gtk_entry_select_region (GTK_ENTRY (priv->template_entry), -1, -1);

    g_signal_connect (priv->template_combo, "changed", G_CALLBACK (Private::on_template_entry_changed), this);

    // Counter
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->counter_start_spin), defaults.counter_start);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->counter_step_spin), defaults.counter_increment);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->counter_digits_spin), defaults.counter_precision);
    g_signal_connect (priv->counter_start_spin, "value-changed", G_CALLBACK (Private::on_counter_start_spin_value_changed), this);
    g_signal_connect (priv->counter_step_spin, "value-changed", G_CALLBACK (Private::on_counter_step_spin_value_changed), this);
    g_signal_connect (priv->counter_digits_spin, "value-changed", G_CALLBACK (Private::on_counter_digits_spin_value_changed), this);

    // Regex
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->regex_view), defaults.regexes);

    g_signal_connect (defaults.regexes, "row-deleted", G_CALLBACK (Private::on_regex_model_row_deleted), this);
    g_signal_connect (priv->regex_view, "row-activated", G_CALLBACK (Private::on_regex_view_row_activated), this);
    g_signal_connect (priv->regex_add_button, "clicked", G_CALLBACK (Private::on_regex_add_btn_clicked), this);
    g_signal_connect (priv->regex_edit_button, "clicked", G_CALLBACK (Private::on_regex_edit_btn_clicked), this);
    g_signal_connect (priv->regex_remove_button, "clicked", G_CALLBACK (Private::on_regex_remove_btn_clicked), this);
    g_signal_connect (priv->regex_remove_all_button, "clicked", G_CALLBACK (Private::on_regex_remove_all_btn_clicked), this);

    // Case conversion & blank triming
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->case_combo), 0);
    gtk_combo_box_set_active (GTK_COMBO_BOX (priv->trim_combo), 3);
    g_signal_connect (priv->case_combo, "changed", G_CALLBACK (Private::on_case_combo_changed), this);
    g_signal_connect (priv->trim_combo, "changed", G_CALLBACK (Private::on_trim_combo_changed), this);

    // Results
    files = create_files_model ();

    g_signal_connect (files, "row-deleted", G_CALLBACK (Private::on_files_model_row_deleted), this);
    g_signal_connect (priv->files_view, "button-press-event", G_CALLBACK (Private::on_files_view_button_pressed), this);
    g_signal_connect (priv->files_view, "popup-menu", G_CALLBACK (Private::on_files_view_popup_menu), this);
    g_signal_connect (priv->files_view, "row-activated", G_CALLBACK (Private::on_files_view_row_activated), this);

    g_signal_connect (this, "delete-event", G_CALLBACK (Private::on_dialog_delete), this);
    g_signal_connect (this, "size-allocate", G_CALLBACK (Private::on_dialog_size_allocate), this);
    g_signal_connect (this, "response", G_CALLBACK (Private::on_dialog_response), this);

    gnome_cmd_advrename_parse_template (gtk_entry_get_text (GTK_ENTRY (priv->template_entry)), priv->template_has_counters);
}


void GnomeCmdAdvrenameDialog::set(GList *file_list)
{
    for (GtkTreeIter iter; file_list; file_list=file_list->next)
    {
        GnomeCmdFile *f = (GnomeCmdFile *) file_list->data;

        gtk_list_store_append (GTK_LIST_STORE (files), &iter);
        gtk_list_store_set (GTK_LIST_STORE (files), &iter,
                            COL_FILE, gnome_cmd_file_ref (f),
                            COL_NAME, gnome_cmd_file_get_name (f),
                            COL_SIZE, gnome_cmd_file_get_size (f),
                            COL_DATE, gnome_cmd_file_get_mdate (f, FALSE),
                            -1);
    }

    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->files_view), files);
    // g_object_unref (files);          // destroy model automatically with view

    update_new_filenames();
}


void GnomeCmdAdvrenameDialog::unset()
{
    gtk_tree_view_set_model (GTK_TREE_VIEW (priv->files_view), NULL);       // unset the model

    GnomeCmdFile *f;
    GtkTreeIter i;

    for (gboolean valid_iter=gtk_tree_model_get_iter_first (files, &i); valid_iter; valid_iter=gtk_tree_model_iter_next (files, &i))
    {
        gtk_tree_model_get (files, &i,
                            COL_FILE, &f,
                            -1);

        gnome_cmd_file_unref (f);
    }

    g_signal_handlers_block_by_func (files, gpointer (Private::on_files_model_row_deleted), this);
    gtk_list_store_clear (GTK_LIST_STORE (files));
    g_signal_handlers_unblock_by_func (files, gpointer (Private::on_files_model_row_deleted), this);
}

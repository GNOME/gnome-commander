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
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#include <config.h>

#include <stdio.h>
#include <stdarg.h>

#include <vector>

#include "gnome-cmd-includes.h"
#include "gnome-cmd-tags.h"
#include "gnome-cmd-tags-file.h"
#include "gnome-cmd-tags-exiv2.h"
#include "gnome-cmd-tags-taglib.h"
#include "gnome-cmd-tags-doc.h"
#include "gnome-cmd-tags-poppler.h"
#include "utils.h"
#include "dict.h"

using namespace std;


struct GnomeCmdTagName
{
    const gchar *name;
    GnomeCmdTagClass tag_class;
    const gchar *title;
    const gchar *description;
};


inline bool operator < (const GnomeCmdTagName &t1, const GnomeCmdTagName &t2)
{
    return strcasecmp(t1.name,t2.name)<0;
}


static GnomeCmdTagName TAG_NONE_NAME = {"", TAG_NONE_CLASS, "", ""};

static DICT<GnomeCmdTag,GnomeCmdTagName> metatags(TAG_NONE, TAG_NONE_NAME);


static char empty_string[] = "";

#ifndef HAVE_EXIV2
static char no_support_for_exiv2_tags_string[] = N_("<Exif and IPTC tags not supported>");
#endif

#ifndef HAVE_ID3
static char no_support_for_taglib_tags_string[] = N_("<ID3, APE, FLAC and Vorbis tags not supported>");
#endif

#ifndef HAVE_GSF
static char no_support_for_libgsf_tags_string[] = N_("<OLE2 and ODF tags not supported>");
#endif

#ifndef HAVE_PDF
static char no_support_for_poppler_tags_string[] = N_("<PDF tags not supported>");
#endif


void GnomeCmdFileMetadata::addf(const GnomeCmdTag tag, const gchar *fmt, ...)
{
    static vector<char> buff(16);

    va_list args;

    while (TRUE)
    {
        // Try to print in the allocated space
        va_start (args, fmt);
        int n = vsnprintf (&buff[0], buff.size(), fmt, args);
        va_end (args);

        // If that worked, return the string
        if (n > -1 && n < buff.size())
          break;

        buff.resize(n > -1 ? n+1 : buff.size()*2);
    }

    add(tag,&buff[0]);
}


void gcmd_tags_init()
{
    static struct
    {
        GnomeCmdTag tag;
        GnomeCmdTagName name;
    }
    metatags_data[] = {
                       {TAG_AUDIO_ALBUM, {"Audio.Album", TAG_AUDIO, N_("Album"), N_("Name of the album.")}},
                       {TAG_AUDIO_ALBUMARTIST, {"Audio.AlbumArtist", TAG_AUDIO, N_("Album Artist"), N_("Artist of the album.")}},
                       {TAG_AUDIO_ALBUMGAIN, {"Audio.AlbumGain", TAG_AUDIO, N_("Album Gain"), N_("Gain adjustment of the album.")}},
                       {TAG_AUDIO_ALBUMPEAKGAIN, {"Audio.AlbumPeakGain", TAG_AUDIO, N_("Album Peak Gain"), N_("Peak gain adjustment of album.")}},
                       {TAG_AUDIO_ALBUMTRACKCOUNT, {"Audio.AlbumTrackCount", TAG_AUDIO, N_("Album Track Count"), N_("Total number of tracks on the album.")}},
                       {TAG_AUDIO_ARTIST, {"Audio.Artist", TAG_AUDIO, N_("Artist"), N_("Artist of the track.")}},
                       {TAG_AUDIO_BITRATE, {"Audio.Bitrate", TAG_AUDIO, N_("Bitrate"), N_("Bitrate in kbps.")}},
                       {TAG_AUDIO_CHANNELS, {"Audio.Channels", TAG_AUDIO, N_("Channels"), N_("Number of channels in the audio (2 = stereo).")}},
                       {TAG_AUDIO_CODEC, {"Audio.Codec", TAG_AUDIO, N_("Codec"), N_("Codec encoding description.")}},
                       {TAG_AUDIO_CODECVERSION, {"Audio.CodecVersion", TAG_AUDIO, N_("Codec Version"), N_("Codec version.")}},
                       {TAG_AUDIO_COMMENT, {"Audio.Comment", TAG_AUDIO, N_("Comment"), N_("Comments on the track.")}},
                       {TAG_AUDIO_COPYRIGHT, {"Audio.Copyright", TAG_AUDIO, N_("Copyright"), N_("Copyright message.")}},
                       {TAG_AUDIO_COVERALBUMTHUMBNAILPATH, {"Audio.CoverAlbumThumbnailPath", TAG_AUDIO, N_("Cover Album Thumbnail Path"), N_("File path to thumbnail image of the cover album.")}},
                       {TAG_AUDIO_DISCNO, {"Audio.DiscNo", TAG_AUDIO, N_("Disc Number"), N_("Specifies which disc the track is on.")}},
                       {TAG_AUDIO_DURATION, {"Audio.Duration", TAG_AUDIO, N_("Duration"), N_("Duration of track in seconds.")}},
                       {TAG_AUDIO_DURATIONMMSS, {"Audio.Duration.MMSS", TAG_AUDIO, N_("Duration [MM:SS]"), N_("Duration of track as MM:SS.")}},
                       {TAG_AUDIO_GENRE, {"Audio.Genre", TAG_AUDIO, N_("Genre"), N_("Type of music classification for the track as defined in ID3 spec.")}},
                       {TAG_AUDIO_ISNEW, {"Audio.IsNew", TAG_AUDIO, N_("Is New"), N_("Set to \"1\" if track is new to the user (default \"0\").")}},
                       {TAG_AUDIO_ISRC, {"Audio.ISRC", TAG_AUDIO, N_("ISRC"), N_("ISRC (international standard recording code).")}},
                       {TAG_AUDIO_LASTPLAY, {"Audio.LastPlay", TAG_AUDIO, N_("Last Play"), N_("When track was last played.")}},
                       {TAG_AUDIO_LYRICS, {"Audio.Lyrics", TAG_AUDIO, N_("Lyrics"), N_("Lyrics of the track.")}},
                       {TAG_AUDIO_MBALBUMARTISTID, {"Audio.MBAlbumArtistID", TAG_AUDIO, N_("MB album artist ID"), N_("MusicBrainz album artist ID in UUID format.")}},
                       {TAG_AUDIO_MBALBUMID, {"Audio.MBAlbumID", TAG_AUDIO, N_("MB Album ID"), N_("MusicBrainz album ID in UUID format.")}},
                       {TAG_AUDIO_MBARTISTID, {"Audio.MBArtistID", TAG_AUDIO, N_("MB Artist ID"), N_("MusicBrainz artist ID in UUID format.")}},
                       {TAG_AUDIO_MBTRACKID, {"Audio.MBTrackID", TAG_AUDIO, N_("MB Track ID"), N_("MusicBrainz track ID in UUID format.")}},
                       {TAG_AUDIO_MPEG_CHANNELMODE, {"Audio.MPEG.ChannelMode", TAG_AUDIO, N_("Channel Mode"), N_("MPEG channel mode.")}},
                       {TAG_AUDIO_MPEG_COPYRIGHTED, {"Audio.MPEG.Copyrighted", TAG_AUDIO, N_("Copyrighted"), N_("\"1\" if the copyrighted bit is set.")}},
                       {TAG_AUDIO_MPEG_LAYER, {"Audio.MPEG.Layer", TAG_AUDIO, N_("Layer"), N_("MPEG layer.")}},
                       {TAG_AUDIO_MPEG_ORIGINAL, {"Audio.MPEG.Original", TAG_AUDIO, N_("Original Audio"), N_("\"1\" if the \"original\" bit is set.")}},
                       {TAG_AUDIO_MPEG_VERSION, {"Audio.MPEG.Version", TAG_AUDIO, N_("MPEG Version"), N_("MPEG version.")}},
                       {TAG_AUDIO_PERFORMER, {"Audio.Performer", TAG_AUDIO, N_("Performer"), N_("Name of the performer/conductor of the music.")}},
                       {TAG_AUDIO_PLAYCOUNT, {"Audio.PlayCount", TAG_AUDIO, N_("Play Count"), N_("Number of times the track has been played.")}},
                       {TAG_AUDIO_RELEASEDATE, {"Audio.ReleaseDate", TAG_AUDIO, N_("Release Date"), N_("Date track was released.")}},
                       {TAG_AUDIO_SAMPLERATE, {"Audio.SampleRate", TAG_AUDIO, N_("Sample Rate"), N_("Sample rate in Hz.")}},
                       {TAG_AUDIO_TITLE, {"Audio.Title", TAG_AUDIO, N_("Title"), N_("Title of the track.")}},
                       {TAG_AUDIO_TRACKGAIN, {"Audio.TrackGain", TAG_AUDIO, N_("Track Gain"), N_("Gain adjustment of the track.")}},
                       {TAG_AUDIO_TRACKNO, {"Audio.TrackNo", TAG_AUDIO, N_("Track Number"), N_("Position of track on the album.")}},
                       {TAG_AUDIO_TRACKPEAKGAIN, {"Audio.TrackPeakGain", TAG_AUDIO, N_("Track Peak Gain"), N_("Peak gain adjustment of track.")}},
                       {TAG_AUDIO_YEAR, {"Audio.Year", TAG_AUDIO, N_("Year"), N_("Year.")}},
                       {TAG_DOC_AUTHOR, {"Doc.Author", TAG_DOC, N_("Author"), N_("Name of the author.")}},
                       {TAG_DOC_BYTECOUNT, {"Doc.ByteCount", TAG_DOC, N_("Byte Count"), N_("Number of bytes in the document.")}},
                       {TAG_DOC_CASESENSITIVE, {"Doc.CaseSensitive", TAG_DOC, N_("Case Sensitive"), N_("Case sensitive.")}},
                       {TAG_DOC_CATEGORY, {"Doc.Category", TAG_DOC, N_("Category"), N_("Category.")}},
                       {TAG_DOC_CELLCOUNT, {"Doc.CellCount", TAG_DOC, N_("Cell Count"), N_("Number of cells in the spreadsheet document.")}},
                       {TAG_DOC_CHARACTERCOUNT, {"Doc.CharacterCount", TAG_DOC, N_("Character Count"), N_("Number of characters in the document.")}},
                       {TAG_DOC_CODEPAGE, {"Doc.Codepage", TAG_DOC, N_("Codepage"), N_("The MS codepage to encode strings for metadata.")}},
                       {TAG_DOC_COMMENTS, {"Doc.Comments", TAG_DOC, N_("Comments"), N_("User definable free text.")}},
                       {TAG_DOC_COMPANY, {"Doc.Company", TAG_DOC, N_("Company"), N_("Organization that the <Doc.Creator> entity is associated with.")}},
                       {TAG_DOC_CREATOR, {"Doc.Author", TAG_DOC, N_("Author"), N_("Name of the author.")}},
                       {TAG_DOC_CREATOR, {"Doc.Creator", TAG_DOC, N_("Creator"), N_("An entity primarily responsible for making the content of the resource, typically a person, organization, or service.")}},
                       {TAG_DOC_DATECREATED, {"Doc.Created", TAG_DOC, N_("Created"), N_("Datetime document was originally created.")}},
                       {TAG_DOC_DATECREATED, {"Doc.DateCreated", TAG_DOC, N_("Date Created"), N_("Date associated with an event in the life cycle of the resource (creation/publication date).")}},
                       {TAG_DOC_DATEMODIFIED, {"Doc.DateModified", TAG_DOC, N_("Date Modified"), N_("The last time the document was saved.")}},
                       {TAG_DOC_DESCRIPTION, {"Doc.Description", TAG_DOC, N_("Description"), N_("An account of the content of the resource.")}},
                       {TAG_DOC_DICTIONARY, {"Doc.Dictionary", TAG_DOC, N_("Dictionary"), N_("Dictionary.")}},
                       {TAG_DOC_EDITINGDURATION, {"Doc.EditingDuration", TAG_DOC, N_("Editing Duration"), N_("The total time taken until the last modification.")}},
                       {TAG_DOC_GENERATOR, {"Doc.Generator", TAG_DOC, N_("Generator"), N_("The application that generated this document.")}},
                       {TAG_DOC_HIDDENSLIDECOUNT, {"Doc.HiddenSlideCount", TAG_DOC, N_("Hidden Slide Count"), N_("Number of hidden slides in the presentation document.")}},
                       {TAG_DOC_IMAGECOUNT, {"Doc.ImageCount", TAG_DOC, N_("Image Count"), N_("Number of images in the document.")}},
                       {TAG_DOC_INITIALCREATOR, {"Doc.InitialCreator", TAG_DOC, N_("Initial Creator"), N_("Specifies the name of the person who created the document initially.")}},
                       {TAG_DOC_KEYWORDS, {"Doc.Keywords", TAG_DOC, N_("Keywords"), N_("Searchable, indexable keywords.")}},
                       {TAG_DOC_LANGUAGE, {"Doc.Language", TAG_DOC, N_("Language"), N_("The locale language of the intellectual content of the resource.")}},
                       {TAG_DOC_LASTPRINTED, {"Doc.LastPrinted", TAG_DOC, N_("Last Printed"), N_("The last time this document was printed.")}},
                       {TAG_DOC_LASTSAVEDBY, {"Doc.LastSavedBy", TAG_DOC, N_("Last Saved By"), N_("The entity that made the last change to the document, typically a person, organization, or service.")}},
                       {TAG_DOC_LINECOUNT, {"Doc.LineCount", TAG_DOC, N_("Line Count"), N_("Number of lines in the document.")}},
                       {TAG_DOC_LINKSDIRTY, {"Doc.LinksDirty", TAG_DOC, N_("Links Dirty"), N_("Links dirty.")}},
                       {TAG_DOC_LOCALESYSTEMDEFAULT, {"Doc.LocaleSystemDefault", TAG_DOC, N_("Locale System Default"), N_("Identifier representing the default system locale.")}},
                       {TAG_DOC_MANAGER, {"Doc.Manager", TAG_DOC, N_("Manager"), N_("Name of the manager of <Doc.Creator> entity.")}},
                       {TAG_DOC_MMCLIPCOUNT, {"Doc.MMClipCount", TAG_DOC, N_("Multimedia Clip Count"), N_("Number of multimedia clips in the document.")}},
                       {TAG_DOC_NOTECOUNT, {"Doc.NoteCount", TAG_DOC, N_("Note Count"), N_("Number of \"notes\" in the document.")}},
                       {TAG_DOC_OBJECTCOUNT, {"Doc.ObjectCount", TAG_DOC, N_("Object Count"), N_("Number of objects (OLE and other graphics) in the document.")}},
                       {TAG_DOC_PAGECOUNT, {"Doc.PageCount", TAG_DOC, N_("Page Count"), N_("Number of pages in the document.")}},
                       {TAG_DOC_PARAGRAPHCOUNT, {"Doc.ParagraphCount", TAG_DOC, N_("Paragraph Count"), N_("Number of paragraphs in the document.")}},
                       {TAG_DOC_PRESENTATIONFORMAT, {"Doc.PresentationFormat", TAG_DOC, N_("Presentation Format"), N_("Type of presentation, like \"On-screen Show\", \"SlideView\", etc.")}},
                       {TAG_DOC_PRINTDATE, {"Doc.PrintDate", TAG_DOC, N_("Print Date"), N_("Specifies the date and time when the document was last printed.")}},
                       {TAG_DOC_PRINTEDBY, {"Doc.PrintedBy", TAG_DOC, N_("Printed By"), N_("Specifies the name of the last person who printed the document.")}},
                       {TAG_DOC_REVISIONCOUNT, {"Doc.RevisionCount", TAG_DOC, N_("Revision Count"), N_("Number of revision on the document.")}},
                       {TAG_DOC_SCALE, {"Doc.Scale", TAG_DOC, N_("Scale"), N_("Scale.")}},
                       {TAG_DOC_SECURITY, {"Doc.Security", TAG_DOC, N_("Security"), N_("One of: \"Password protected\", \"Read-only recommended\", \"Read-only enforced\" or \"Locked for annotations\".")}},
                       {TAG_DOC_SLIDECOUNT, {"Doc.SlideCount", TAG_DOC, N_("Slide Count"), N_("Number of slides in the presentation document.")}},
                       {TAG_DOC_SPREADSHEETCOUNT, {"Doc.SpreadsheetCount", TAG_DOC, N_("Spreadsheet Count"), N_("Number of pages in the document.")}},
                       {TAG_DOC_SUBJECT, {"Doc.Subject", TAG_DOC, N_("Subject"), N_("Document subject.")}},
                       {TAG_DOC_TABLECOUNT, {"Doc.TableCount", TAG_DOC, N_("Table Count"), N_("Number of tables in the document.")}},
                       {TAG_DOC_TEMPLATE, {"Doc.Template", TAG_DOC, N_("Template"), N_("The template file that is been used to generate this document.")}},
                       {TAG_DOC_TITLE, {"Doc.Title", TAG_DOC, N_("Title"), N_("Title of the document.")}},
                       {TAG_DOC_WORDCOUNT, {"Doc.WordCount", TAG_DOC, N_("Word Count"), N_("Number of words in the document.")}},
                       {TAG_EXIF_APERTUREVALUE, {"Exif.ApertureValue", TAG_EXIF, N_("Aperture"), N_("The lens aperture. The unit is the APEX value.")}},
                       {TAG_EXIF_ARTIST, {"Exif.Artist", TAG_EXIF, N_("Artist"), N_("Name of the camera owner, photographer or image creator. The detailed format is not specified, but it is recommended that the information be written for ease of Interoperability. When the field is left blank, it is treated as unknown.")}},
                       {TAG_EXIF_BATTERYLEVEL, {"Exif.BatteryLevel", TAG_EXIF, N_("Battery Level"), N_("Battery level.")}},
                       {TAG_EXIF_BITSPERSAMPLE, {"Exif.BitsPerSample", TAG_EXIF, N_("Bits per Sample"), N_("The number of bits per image component. Each component of the image is 8 bits, so the value for this tag is 8. In JPEG compressed data a JPEG marker is used instead of this tag.")}},
                       {TAG_EXIF_BRIGHTNESSVALUE, {"Exif.BrightnessValue", TAG_EXIF, N_("Brightness"), N_("The value of brightness. The unit is the APEX value. Ordinarily it is given in the range of -99.99 to 99.99.")}},
                       {TAG_EXIF_CFAPATTERN, {"Exif.CFAPattern", TAG_EXIF, N_("CFA Pattern"), N_("The color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used. It does not apply to all sensing methods.")}},
                       {TAG_EXIF_CFAREPEATPATTERNDIM, {"Exif.CFARepeatPatternDim", TAG_EXIF, N_("CFA Repeat Pattern Dim"), N_("CFA Repeat Pattern Dim.")}},
                       {TAG_EXIF_COLORSPACE, {"Exif.ColorSpace", TAG_EXIF, N_("Color Space"), N_("The color space information tag is always recorded as the color space specifier. Normally sRGB is used to define the color space based on the PC monitor conditions and environment. If a color space other than sRGB is used, Uncalibrated is set. Image data recorded as Uncalibrated can be treated as sRGB when it is converted to FlashPix.")}},
                       {TAG_EXIF_COMPONENTSCONFIGURATION, {"Exif.ComponentsConfiguration", TAG_EXIF, N_("Components Configuration"), N_("Information specific to compressed data. The channels of each component are arranged in order from the 1st component to the 4th. For uncompressed data the data arrangement is given in the <Exif.PhotometricInterpretation> tag. However, since <Exif.PhotometricInterpretation> can only express the order of Y, Cb and Cr, this tag is provided for cases when compressed data uses components other than Y, Cb, and Cr and to enable support of other sequences.")}},
                       {TAG_EXIF_COMPRESSEDBITSPERPIXEL, {"Exif.CompressedBitsPerPixel", TAG_EXIF, N_("Compressed Bits per Pixel"), N_("Information specific to compressed data. The compression mode used for a compressed image is indicated in unit bits per pixel.")}},
                       {TAG_EXIF_COMPRESSION, {"Exif.Compression", TAG_EXIF, N_("Compression"), N_("The compression scheme used for the image data. When a primary image is JPEG compressed, this designation is not necessary and is omitted. When thumbnails use JPEG compression, this tag value is set to 6.")}},
                       {TAG_EXIF_CONTRAST, {"Exif.Contrast", TAG_EXIF, N_("Contrast"), N_("The direction of contrast processing applied by the camera when the image was shot.")}},
                       {TAG_EXIF_COPYRIGHT, {"Exif.Copyright", TAG_EXIF, N_("Copyright"), N_("Copyright information. The tag is used to indicate both the photographer and editor copyrights. It is the copyright notice of the person or organization claiming rights to the image. The Interoperability copyright statement including date and rights should be written in this field; e.g., \"Copyright, John Smith, 19xx. All rights reserved.\". The field records both the photographer and editor copyrights, with each recorded in a separate part of the statement. When there is a clear distinction between the photographer and editor copyrights, these are to be written in the order of photographer followed by editor copyright, separated by NULL (in this case, since the statement also ends with a NULL, there are two NULL codes) (see example 1). When only the photographer is given, it is terminated by one NULL code. When only the editor copyright is given, the photographer copyright part consists of one space followed by a terminating NULL code, then the editor copyright is given. When the field is left blank, it is treated as unknown.")}},
                       {TAG_EXIF_CUSTOMRENDERED, {"Exif.CustomRendered", TAG_EXIF, N_("Custom Rendered"), N_("The use of special processing on image data, such as rendering geared to output. When special processing is performed, the reader is expected to disable or minimize any further processing.")}},
                       {TAG_EXIF_DATETIME, {"Exif.DateTime", TAG_EXIF, N_("Date and Time"), N_("The date and time of image creation.")}},
                       {TAG_EXIF_DATETIMEDIGITIZED, {"Exif.DateTimeDigitized", TAG_EXIF, N_("Date and Time (digitized)"), N_("The date and time when the image was stored as digital data.")}},
                       {TAG_EXIF_DATETIMEORIGINAL, {"Exif.DateTimeOriginal", TAG_EXIF, N_("Date and Time (original)"), N_("The date and time when the original image data was generated. For a digital still camera the date and time the picture was taken are recorded.")}},
                       {TAG_EXIF_DEVICESETTINGDESCRIPTION, {"Exif.DeviceSettingDescription", TAG_EXIF, N_("Device Setting Description"), N_("Information on the picture-taking conditions of a particular camera model. The tag is used only to indicate the picture-taking conditions in the reader.")}},
                       {TAG_EXIF_DIGITALZOOMRATIO, {"Exif.DigitalZoomRatio", TAG_EXIF, N_("Digital Zoom Ratio"), N_("The digital zoom ratio when the image was shot. If the numerator of the recorded value is 0, this indicates that digital zoom was not used.")}},
                       {TAG_EXIF_DOCUMENTNAME, {"Exif.DocumentName", TAG_EXIF, N_("Document Name"), N_("Document name.")}},
                       {TAG_EXIF_EXIFIFDPOINTER, {"Exif.ExifIfdPointer", TAG_EXIF, N_("Exif IFD Pointer"), N_("A pointer to the Exif IFD. Interoperability, Exif IFD has the same structure as that of the IFD specified in TIFF.")}},
                       {TAG_EXIF_EXIFVERSION, {"Exif.ExifVersion", TAG_EXIF, N_("Exif Version"), N_("The version of Exif standard supported. Nonexistence of this field is taken to mean nonconformance to the standard.")}},
                       {TAG_EXIF_EXPOSUREBIASVALUE, {"Exif.ExposureBiasValue", TAG_EXIF, N_("Exposure Bias"), N_("The exposure bias. The units is the APEX value. Ordinarily it is given in the range of -99.99 to 99.99.")}},
                       {TAG_EXIF_EXPOSUREINDEX, {"Exif.ExposureIndex", TAG_EXIF, N_("Exposure Index"), N_("The exposure index selected on the camera or input device at the time the image is captured.")}},
                       {TAG_EXIF_EXPOSUREMODE, {"Exif.ExposureMode", TAG_EXIF, N_("Exposure Mode"), N_("The exposure mode set when the image was shot. In auto-bracketing mode, the camera shoots a series of frames of the same scene at different exposure settings.")}},
                       {TAG_EXIF_EXPOSUREPROGRAM, {"Exif.ExposureProgram", TAG_EXIF, N_("Exposure Program"), N_("The class of the program used by the camera to set exposure when the picture is taken.")}},
                       {TAG_EXIF_EXPOSURETIME, {"Exif.ExposureTime", TAG_EXIF, N_("Exposure Time"), N_("Exposure time, given in seconds.")}},
                       {TAG_EXIF_FILESOURCE, {"Exif.FileSource", TAG_EXIF, N_("File Source"), N_("Indicates the image source. If a DSC recorded the image, this tag value of this tag always be set to 3, indicating that the image was recorded on a DSC.")}},
                       {TAG_EXIF_FILLORDER, {"Exif.FillOrder", TAG_EXIF, N_("Fill Order"), N_("Fill order.")}},
                       {TAG_EXIF_FLASH, {"Exif.Flash", TAG_EXIF, N_("Flash"), N_("This tag is recorded when an image is taken using a strobe light (flash).")}},
                       {TAG_EXIF_FLASHENERGY, {"Exif.FlashEnergy", TAG_EXIF, N_("Flash Energy"), N_("The strobe energy at the time the image is captured, as measured in Beam Candle Power Seconds (BCPS).")}},
                       {TAG_EXIF_FLASHPIXVERSION, {"Exif.FlashPixVersion", TAG_EXIF, N_("FlashPix Version"), N_("The FlashPix format version supported by a FPXR file.")}},
                       {TAG_EXIF_FNUMBER, {"Exif.FNumber", TAG_EXIF, N_("F Number"), N_("Diameter of the aperture relative to the effective focal length of the lens.")}},
                       {TAG_EXIF_FOCALLENGTH, {"Exif.FocalLength", TAG_EXIF, N_("Focal Length"), N_("The actual focal length of the lens, in mm. Conversion is not made to the focal length of a 35 mm film camera.")}},
                       {TAG_EXIF_FOCALLENGTHIN35MMFILM, {"Exif.FocalLengthIn35mmFilm", TAG_EXIF, N_("Focal Length In 35mm Film"), N_("The equivalent focal length assuming a 35mm film camera, in mm. A value of 0 means the focal length is unknown. Note that this tag differs from the <Exif.FocalLength> tag.")}},
                       {TAG_EXIF_FOCALPLANERESOLUTIONUNIT, {"Exif.FocalPlaneResolutionUnit", TAG_EXIF, N_("Focal Plane Resolution Unit"), N_("The unit for measuring <Exif.FocalPlaneXResolution> and <Exif.FocalPlaneYResolution>. This value is the same as the <Exif.ResolutionUnit>.")}},
                       {TAG_EXIF_FOCALPLANEXRESOLUTION, {"Exif.FocalPlaneXResolution", TAG_EXIF, N_("Focal Plane x-Resolution"), N_("The number of pixels in the image width (X) direction per <Exif.FocalPlaneResolutionUnit> on the camera focal plane.")}},
                       {TAG_EXIF_FOCALPLANEYRESOLUTION, {"Exif.FocalPlaneYResolution", TAG_EXIF, N_("Focal Plane y-Resolution"), N_("The number of pixels in the image height (Y) direction per <Exif.FocalPlaneResolutionUnit> on the camera focal plane.")}},
                       {TAG_EXIF_GAINCONTROL, {"Exif.GainControl", TAG_EXIF, N_("Gain Control"), N_("This tag indicates the degree of overall image gain adjustment.")}},
                       {TAG_EXIF_GAMMA, {"Exif.Gamma", TAG_EXIF, N_("Gamma"), N_("Indicates the value of coefficient gamma.")}},
                       {TAG_EXIF_GPSALTITUDE, {"Exif.GPS.Altitude", TAG_EXIF, N_("Altitude"), N_("Indicates the altitude based on the reference in <Exif.GPS.AltitudeRef>. The reference unit is meters.")}},
                       {TAG_EXIF_GPSALTITUDEREF, {"Exif.GPS.AltitudeRef", TAG_EXIF, N_("Altitude Reference"), N_("Indicates the altitude used as the reference altitude. If the reference is sea level and the altitude is above sea level, 0 is given. If the altitude is below sea level, a value of 1 is given and the altitude is indicated as an absolute value in the <Exif.GPS.Altitude> tag. The reference unit is meters.")}},
                       {TAG_EXIF_GPSINFOIFDPOINTER, {"Exif.GPS.InfoIFDPointer", TAG_EXIF, N_("GPS Info IFDPointer"), N_("A pointer to the GPS Info IFD. The Interoperability structure of the GPS Info IFD, like that of Exif IFD, has no image data.")}},
                       {TAG_EXIF_GPSLATITUDE, {"Exif.GPS.Latitude", TAG_EXIF, N_("Latitude"), N_("Indicates the latitude. The latitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is dd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is dd/1,mmmm/100,0/1.")}},
                       {TAG_EXIF_GPSLATITUDEREF, {"Exif.GPS.LatitudeRef", TAG_EXIF, N_("North or South Latitude"), N_("Indicates whether the latitude is north or south latitude. The ASCII value 'N' indicates north latitude, and 'S' is south latitude.")}},
                       {TAG_EXIF_GPSLONGITUDE, {"Exif.GPS.Longitude", TAG_EXIF, N_("Longitude"), N_("Indicates the longitude. The longitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is ddd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is ddd/1,mmmm/100,0/1.")}},
                       {TAG_EXIF_GPSLONGITUDEREF, {"Exif.GPS.LongitudeRef", TAG_EXIF, N_("East or West Longitude"), N_("Indicates whether the longitude is east or west longitude. ASCII 'E' indicates east longitude, and 'W' is west longitude.")}},
                       {TAG_EXIF_GPSVERSIONID, {"Exif.GPS.VersionID", TAG_EXIF, N_("GPS Tag Version"), N_("Indicates the version of <Exif.GPS.InfoIFD>. This tag is mandatory when <Exif.GPS.Info> tag is present.")}},
                       {TAG_EXIF_IMAGEDESCRIPTION, {"Exif.ImageDescription", TAG_EXIF, N_("Image Description"), N_("A character string giving the title of the image. Two-bytes character codes cannot be used. When a 2-bytes code is necessary, the Exif Private tag <Exif.UserComment> is to be used.")}},
                       {TAG_EXIF_IMAGELENGTH, {"Exif.ImageLength", TAG_EXIF, N_("Image Length"), N_("The number of rows of image data. In JPEG compressed data a JPEG marker is used instead of this tag.")}},
                       {TAG_EXIF_IMAGERESOURCES, {"Exif.ImageResources", TAG_EXIF, N_("Image Resources Block"), N_("Image Resources Block.")}},
                       {TAG_EXIF_IMAGEUNIQUEID, {"Exif.ImageUniqueID", TAG_EXIF, N_("Image Unique ID"), N_("This tag indicates an identifier assigned uniquely to each image. It is recorded as an ASCII string equivalent to hexadecimal notation and 128-bit fixed length.")}},
                       {TAG_EXIF_IMAGEWIDTH, {"Exif.ImageWidth", TAG_EXIF, N_("Image Width"), N_("The number of columns of image data, equal to the number of pixels per row. In JPEG compressed data a JPEG marker is used instead of this tag.")}},
                       {TAG_EXIF_INTERCOLORPROFILE, {"Exif.InterColorProfile", TAG_EXIF, N_("Inter Color Profile"), N_("Inter Color Profile.")}},
                       {TAG_EXIF_INTEROPERABILITYIFDPOINTER, {"Exif.InteroperabilityIFDPointer", TAG_EXIF, N_("Interoperability IFD Pointer"), N_("Interoperability IFD is composed of tags which stores the information to ensure the Interoperability and pointed by the following tag located in Exif IFD. The Interoperability structure of Interoperability IFD is the same as TIFF defined IFD structure but does not contain the image data characteristically compared with normal TIFF IFD.")}},
                       {TAG_EXIF_INTEROPERABILITYINDEX, {"Exif.InteroperabilityIndex", TAG_EXIF, N_("Interoperability Index"), N_("Indicates the identification of the Interoperability rule. Use \"R98\" for stating ExifR98 Rules.")}},
                       {TAG_EXIF_INTEROPERABILITYVERSION, {"Exif.InteroperabilityVersion", TAG_EXIF, N_("Interoperability Version"), N_("Interoperability version.")}},
                       {TAG_EXIF_IPTCNAA, {"Exif.IPTC_NAA", TAG_EXIF, "IPTC/NAA", N_("An IPTC/NAA record.")}},
                       {TAG_EXIF_ISOSPEEDRATINGS, {"Exif.ISOSpeedRatings", TAG_EXIF, N_("ISO Speed Ratings"), N_("The ISO Speed and ISO Latitude of the camera or input device as specified in ISO 12232.")}},
                       {TAG_EXIF_JPEGINTERCHANGEFORMAT, {"Exif.JPEGInterchangeFormat", TAG_EXIF, N_("JPEG Interchange Format"), N_("The offset to the start byte (SOI) of JPEG compressed thumbnail data. This is not used for primary image JPEG data.")}},
                       {TAG_EXIF_JPEGINTERCHANGEFORMATLENGTH, {"Exif.JPEGInterchangeFormatLength", TAG_EXIF, N_("JPEG Interchange Format Length"), N_("The number of bytes of JPEG compressed thumbnail data. This is not used for primary image JPEG data. JPEG thumbnails are not divided but are recorded as a continuous JPEG bitstream from SOI to EOI. Appn and COM markers should not be recorded. Compressed thumbnails must be recorded in no more than 64 Kbytes, including all other data to be recorded in APP1.")}},
                       {TAG_EXIF_JPEGPROC, {"Exif.JPEGProc", TAG_EXIF, N_("JPEG Procedure"), N_("JPEG procedure.")}},
                       {TAG_EXIF_LIGHTSOURCE, {"Exif.LightSource", TAG_EXIF, N_("Light Source"), N_("The kind of light source.")}},
                       {TAG_EXIF_MAKE, {"Exif.Make", TAG_EXIF, N_("Manufacturer"), N_("The manufacturer of the recording equipment. This is the manufacturer of the DSC, scanner, video digitizer or other equipment that generated the image. When the field is left blank, it is treated as unknown.")}},
                       {TAG_EXIF_MAKERNOTE, {"Exif.MakerNote", TAG_EXIF, N_("Maker Note"), N_("A tag for manufacturers of Exif writers to record any desired information. The contents are up to the manufacturer.")}},
                       {TAG_EXIF_MAXAPERTUREVALUE, {"Exif.MaxApertureValue", TAG_EXIF, N_("Max Aperture Value"), N_("The smallest F number of the lens. The unit is the APEX value. Ordinarily it is given in the range of 00.00 to 99.99, but it is not limited to this range.")}},
                       {TAG_EXIF_METERINGMODE, {"Exif.MeteringMode", TAG_EXIF, N_("Metering Mode"), N_("The metering mode.")}},
                       {TAG_EXIF_MODEL, {"Exif.Model", TAG_EXIF, N_("Model"), N_("The model name or model number of the equipment. This is the model name or number of the DSC, scanner, video digitizer or other equipment that generated the image. When the field is left blank, it is treated as unknown.")}},
                       {TAG_EXIF_NEWCFAPATTERN, {"Exif.CFAPattern", TAG_EXIF, N_("CFA Pattern"), N_("The color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used. It does not apply to all sensing methods.")}},
                       {TAG_EXIF_NEWSUBFILETYPE, {"Exif.NewSubfileType", TAG_EXIF, N_("New Subfile Type"), N_("A general indication of the kind of data contained in this subfile.")}},
                       {TAG_EXIF_OECF, {"Exif.OECF", TAG_EXIF, N_("OECF"), N_("The Opto-Electronic Conversion Function (OECF) specified in ISO 14524. <Exif.OECF> is the relationship between the camera optical input and the image values.")}},
                       {TAG_EXIF_ORIENTATION, {"Exif.Orientation", TAG_EXIF, N_("Orientation"), N_("The image orientation viewed in terms of rows and columns.")}},
                       {TAG_EXIF_PHOTOMETRICINTERPRETATION, {"Exif.PhotometricInterpretation", TAG_EXIF, N_("Photometric Interpretation"), N_("The pixel composition. In JPEG compressed data a JPEG marker is used instead of this tag.")}},
                       {TAG_EXIF_PIXELXDIMENSION, {"Exif.PixelXDimension", TAG_EXIF, N_("Pixel X Dimension"), N_("Information specific to compressed data. When a compressed file is recorded, the valid width of the meaningful image must be recorded in this tag, whether or not there is padding data or a restart marker. This tag should not exist in an uncompressed file.")}},
                       {TAG_EXIF_PIXELYDIMENSION, {"Exif.PixelYDimension", TAG_EXIF, N_("Pixel Y Dimension"), N_("Information specific to compressed data. When a compressed file is recorded, the valid height of the meaningful image must be recorded in this tag, whether or not there is padding data or a restart marker. This tag should not exist in an uncompressed file. Since data padding is unnecessary in the vertical direction, the number of lines recorded in this valid image height tag will in fact be the same as that recorded in the SOF.")}},
                       {TAG_EXIF_PLANARCONFIGURATION, {"Exif.PlanarConfiguration", TAG_EXIF, N_("Planar Configuration"), N_("Indicates whether pixel components are recorded in a chunky or planar format. In JPEG compressed files a JPEG marker is used instead of this tag. If this field does not exist, the TIFF default of 1 (chunky) is assumed.")}},
                       {TAG_EXIF_PRIMARYCHROMATICITIES, {"Exif.PrimaryChromaticities", TAG_EXIF, N_("Primary Chromaticities"), N_("The chromaticity of the three primary colors of the image. Normally this tag is not necessary, since colorspace is specified in <Exif.ColorSpace> tag.")}},
                       {TAG_EXIF_REFERENCEBLACKWHITE, {"Exif.ReferenceBlackWhite", TAG_EXIF, N_("Reference Black/White"), N_("The reference black point value and reference white point value. No defaults are given in TIFF, but the values below are given as defaults here. The color space is declared in a color space information tag, with the default being the value that gives the optimal image characteristics Interoperability these conditions.")}},
                       {TAG_EXIF_RELATEDIMAGEFILEFORMAT, {"Exif.RelatedImageFileFormat", TAG_EXIF, N_("Related Image File Format"), N_("Related image file format.")}},
                       {TAG_EXIF_RELATEDIMAGELENGTH, {"Exif.RelatedImageLength", TAG_EXIF, N_("Related Image Length"), N_("Related image length.")}},
                       {TAG_EXIF_RELATEDIMAGEWIDTH, {"Exif.RelatedImageWidth", TAG_EXIF, N_("Related Image Width"), N_("Related image width.")}},
                       {TAG_EXIF_RELATEDSOUNDFILE, {"Exif.RelatedSoundFile", TAG_EXIF, N_("Related Sound File"), N_("This tag is used to record the name of an audio file related to the image data. The only relational information recorded here is the Exif audio file name and extension (an ASCII string consisting of 8 characters + '.' + 3 characters). The path is not recorded. When using this tag, audio files must be recorded in conformance to the Exif audio format. Writers are also allowed to store the data such as Audio within APP2 as FlashPix extension stream data. Audio files must be recorded in conformance to the Exif audio format. If multiple files are mapped to one file, the above format is used to record just one audio file name. If there are multiple audio files, the first recorded file is given. When there are three Exif audio files \"SND00001.WAV\", \"SND00002.WAV\" and \"SND00003.WAV\", the Exif image file name for each of them, \"DSC00001.JPG\", is indicated. By combining multiple relational information, a variety of playback possibilities can be supported. The method of using relational information is left to the implementation on the playback side. Since this information is an ASCII character string, it is terminated by NULL. When this tag is used to map audio files, the relation of the audio file to image data must also be indicated on the audio file end.")}},
                       {TAG_EXIF_RESOLUTIONUNIT, {"Exif.ResolutionUnit", TAG_EXIF, N_("Resolution Unit"), N_("The unit for measuring <Exif.XResolution> and <Exif.YResolution>. The same unit is used for both <Exif.XResolution> and <Exif.YResolution>. If the image resolution is unknown, 2 (inches) is designated.")}},
                       {TAG_EXIF_ROWSPERSTRIP, {"Exif.RowsPerStrip", TAG_EXIF, N_("Rows per Strip"), N_("The number of rows per strip. This is the number of rows in the image of one strip when an image is divided into strips. With JPEG compressed data this designation is not needed and is omitted.")}},
                       {TAG_EXIF_SAMPLESPERPIXEL, {"Exif.SamplesPerPixel", TAG_EXIF, N_("Samples per Pixel"), N_("The number of components per pixel. Since this standard applies to RGB and YCbCr images, the value set for this tag is 3. In JPEG compressed data a JPEG marker is used instead of this tag.")}},
                       {TAG_EXIF_SATURATION, {"Exif.Saturation", TAG_EXIF, N_("Saturation"), N_("The direction of saturation processing applied by the camera when the image was shot.")}},
                       {TAG_EXIF_SCENECAPTURETYPE, {"Exif.SceneCaptureType", TAG_EXIF, N_("Scene Capture Type"), N_("The type of scene that was shot. It can also be used to record the mode in which the image was shot. Note that this differs from <Exif.SceneType> tag.")}},
                       {TAG_EXIF_SCENETYPE, {"Exif.SceneType", TAG_EXIF, N_("Scene Type"), N_("The type of scene. If a DSC recorded the image, this tag value must always be set to 1, indicating that the image was directly photographed.")}},
                       {TAG_EXIF_SENSINGMETHOD, {"Exif.SensingMethod", TAG_EXIF, N_("Sensing Method"), N_("The image sensor type on the camera or input device.")}},
                       {TAG_EXIF_SHARPNESS, {"Exif.Sharpness", TAG_EXIF, N_("Sharpness"), N_("The direction of sharpness processing applied by the camera when the image was shot.")}},
                       {TAG_EXIF_SHUTTERSPEEDVALUE, {"Exif.ShutterSpeedValue", TAG_EXIF, N_("Shutter Speed"), N_("Shutter speed. The unit is the APEX (Additive System of Photographic Exposure) setting.")}},
                       {TAG_EXIF_SOFTWARE, {"Exif.Software", TAG_EXIF, N_("Software"), N_("This tag records the name and version of the software or firmware of the camera or image input device used to generate the image. When the field is left blank, it is treated as unknown.")}},
                       {TAG_EXIF_SPATIALFREQUENCYRESPONSE, {"Exif.SpatialFrequencyResponse", TAG_EXIF, N_("Spatial Frequency Response"), N_("This tag records the camera or input device spatial frequency table and SFR values in the direction of image width, image height, and diagonal direction, as specified in ISO 12233.")}},
                       {TAG_EXIF_SPECTRALSENSITIVITY, {"Exif.SpectralSensitivity", TAG_EXIF, N_("Spectral Sensitivity"), N_("The spectral sensitivity of each channel of the camera used.")}},
                       {TAG_EXIF_STRIPBYTECOUNTS, {"Exif.StripByteCounts", TAG_EXIF, N_("Strip Byte Count"), N_("The total number of bytes in each strip. With JPEG compressed data this designation is not needed and is omitted.")}},
                       {TAG_EXIF_STRIPOFFSETS, {"Exif.StripOffsets", TAG_EXIF, N_("Strip Offsets"), N_("For each strip, the byte offset of that strip. It is recommended that this be selected so the number of strip bytes does not exceed 64 Kbytes. With JPEG compressed data this designation is not needed and is omitted.")}},
                       {TAG_EXIF_SUBIFDS, {"Exif.SubIFDs", TAG_EXIF, N_("Sub IFD Offsets"), N_("Defined by Adobe Corporation to enable TIFF Trees within a TIFF file.")}},
                       {TAG_EXIF_SUBJECTAREA, {"Exif.SubjectArea", TAG_EXIF, N_("Subject Area"), N_("The location and area of the main subject in the overall scene.")}},
                       {TAG_EXIF_SUBJECTDISTANCE, {"Exif.SubjectDistance", TAG_EXIF, N_("Subject Distance"), N_("The distance to the subject, given in meters.")}},
                       {TAG_EXIF_SUBJECTDISTANCERANGE, {"Exif.SubjectDistanceRange", TAG_EXIF, N_("Subject Distance Range"), N_("The distance to the subject.")}},
                       {TAG_EXIF_SUBJECTLOCATION, {"Exif.SubjectLocation", TAG_EXIF, N_("Subject Location"), N_("The location of the main subject in the scene. The value of this tag represents the pixel at the center of the main subject relative to the left edge, prior to rotation processing as per the <Exif.Rotation> tag. The first value indicates the X column number and second indicates the Y row number.")}},
                       {TAG_EXIF_SUBSECTIME, {"Exif.SubsecTime", TAG_EXIF, N_("Subsec Time"), N_("Fractions of seconds for the <Exif.DateTime> tag.")}},
                       {TAG_EXIF_SUBSECTIMEDIGITIZED, {"Exif.SubSecTimeDigitized", TAG_EXIF, N_("Subsec Time Digitized"), N_("Fractions of seconds for the <Exif.DateTimeDigitized> tag.")}},
                       {TAG_EXIF_SUBSECTIMEORIGINAL, {"Exif.SubSecTimeOriginal", TAG_EXIF, N_("Subsec Time Original"), N_("Fractions of seconds for the <Exif.DateTimeOriginal> tag.")}},
                       {TAG_EXIF_TIFFEPSTANDARDID, {"Exif.TIFF/EPStandardID", TAG_EXIF, N_("TIFF/EP Standard ID"), N_("TIFF/EP Standard ID.")}},
                       {TAG_EXIF_TRANSFERFUNCTION, {"Exif.TransferFunction", TAG_EXIF, N_("Transfer Function"), N_("A transfer function for the image, described in tabular style. Normally this tag is not necessary, since color space is specified in <Exif.ColorSpace> tag.")}},
                       {TAG_EXIF_TRANSFERRANGE, {"Exif.TransferRange", TAG_EXIF, N_("Transfer Range"), N_("Transfer range.")}},
                       {TAG_EXIF_USERCOMMENT, {"Exif.UserComment", TAG_EXIF, N_("User Comment"), N_("A tag for Exif users to write keywords or comments on the image besides those in <Exif.ImageDescription>, and without the character code limitations of the <Exif.ImageDescription> tag. The character code used in the <Exif.UserComment> tag is identified based on an ID code in a fixed 8-byte area at the start of the tag data area. The unused portion of the area is padded with NULL (\"00.h\"). ID codes are assigned by means of registration. The value of CountN is determinated based on the 8 bytes in the character code area and the number of bytes in the user comment part. Since the TYPE is not ASCII, NULL termination is not necessary. The ID code for the <Exif.UserComment> area may be a Defined code such as JIS or ASCII, or may be Undefined. The Undefined name is UndefinedText, and the ID code is filled with 8 bytes of all \"NULL\" (\"00.H\"). An Exif reader that reads the <Exif.UserComment> tag must have a function for determining the ID code. This function is not required in Exif readers that do not use the <Exif.UserComment> tag. When a <Exif.UserComment> area is set aside, it is recommended that the ID code be ASCII and that the following user comment part be filled with blank characters [20.H].")}},
                       {TAG_EXIF_WHITEBALANCE, {"Exif.WhiteBalance", TAG_EXIF, N_("White Balance"), N_("The white balance mode set when the image was shot.")}},
                       {TAG_EXIF_WHITEPOINT, {"Exif.WhitePoint", TAG_EXIF, N_("White Point"), N_("The chromaticity of the white point of the image. Normally this tag is not necessary, since color space is specified in <Exif.ColorSpace> tag.")}},
                       {TAG_EXIF_XMLPACKET, {"Exif.XMLPacket", TAG_EXIF, N_("XML Packet"), N_("XMP metadata.")}},
                       {TAG_EXIF_XRESOLUTION, {"Exif.XResolution", TAG_EXIF, N_("x Resolution"), N_("The number of pixels per <Exif.ResolutionUnit> in the <Exif.ImageWidth> direction. When the image resolution is unknown, 72 [dpi] is designated.")}},
                       {TAG_EXIF_YCBCRCOEFFICIENTS, {"Exif.YCbCrCoefficients", TAG_EXIF, N_("YCbCr Coefficients"), N_("The matrix coefficients for transformation from RGB to YCbCr image data. No default is given in TIFF; but here \"Color Space Guidelines\" is used as the default. The color space is declared in a color space information tag, with the default being the value that gives the optimal image characteristics Interoperability this condition.")}},
                       {TAG_EXIF_YCBCRPOSITIONING, {"Exif.YCbCrPositioning", TAG_EXIF, N_("YCbCr Positioning"), N_("The position of chrominance components in relation to the luminance component. This field is designated only for JPEG compressed data or uncompressed YCbCr data. The TIFF default is 1 (centered); but when Y:Cb:Cr = 4:2:2 it is recommended that 2 (co-sited) be used to record data, in order to improve the image quality when viewed on TV systems. When this field does not exist, the reader shall assume the TIFF default. In the case of Y:Cb:Cr = 4:2:0, the TIFF default (centered) is recommended. If the reader does not have the capability of supporting both kinds of <Exif.YCbCrPositioning>, it shall follow the TIFF default regardless of the value in this field. It is preferable that readers be able to support both centered and co-sited positioning.")}},
                       {TAG_EXIF_YCBCRSUBSAMPLING, {"Exif.YCbCrSubSampling", TAG_EXIF, N_("YCbCr Sub-Sampling"), N_("The sampling ratio of chrominance components in relation to the luminance component. In JPEG compressed data a JPEG marker is used instead of this tag.")}},
                       {TAG_EXIF_YRESOLUTION, {"Exif.YResolution", TAG_EXIF, N_("y Resolution"), N_("The number of pixels per <Exif.ResolutionUnit> in the <Exif.ImageLength> direction. The same value as <Exif.XResolution> is designated.")}},
                       {TAG_FILE_ACCESSED, {"File.Accessed", TAG_FILE, N_("Accessed"), N_("Last access datetime.")}},
                       {TAG_FILE_CONTENT, {"File.Content", TAG_FILE, N_("Content"), N_("File's contents filtered as plain text.")}},
                       {TAG_FILE_DESCRIPTION, {"File.Description", TAG_FILE, N_("Description"), N_("Editable free text/notes.")}},
                       {TAG_FILE_FORMAT, {"File.Format", TAG_FILE, N_("Format"), N_("MIME type of the file or if a directory it should contain value \"Folder\".")}},
                       {TAG_FILE_KEYWORDS, {"File.Keywords", TAG_FILE, N_("Keywords"), N_("Editable array of keywords.")}},
                       {TAG_FILE_LINK, {"File.Link", TAG_FILE, N_("Link"), N_("URI of link target.")}},
                       {TAG_FILE_MODIFIED, {"File.Modified", TAG_FILE, N_("Modified"), N_("Last modified datetime.")}},
                       {TAG_FILE_NAME, {"File.Name", TAG_FILE, N_("Name"), N_("File name excluding path but including the file extension.")}},
                       {TAG_FILE_PATH, {"File.Path", TAG_FILE, N_("Path"), N_("Full file path of file excluding the file name.")}},
                       {TAG_FILE_PERMISSIONS, {"File.Permissions", TAG_FILE, N_("Permissions"), N_("Permission string in unix format eg \"-rw-r--r--\".")}},
                       {TAG_FILE_PUBLISHER, {"File.Publisher", TAG_FILE, N_("Publisher"), N_("Editable DC type for the name of the publisher of the file (EG dc:publisher field in RSS feed).")}},
                       {TAG_FILE_RANK, {"File.Rank", TAG_FILE, N_("Rank"), N_("Editable file rank for grading favourites. Value should be in the range 1..10.")}},
                       {TAG_FILE_SIZE, {"File.Size", TAG_FILE, N_("Size"), N_("Size of the file in bytes or if a directory number of items it contains.")}},
                       {TAG_ID3_ALBUMSORTORDER, {"ID3.AlbumSortOrder", TAG_ID3, N_("Album Sort Order"), N_("String which should be used instead of the album name for sorting purposes.")}},
                       {TAG_ID3_AUDIOCRYPTO, {"ID3.AudioCrypto", TAG_ID3, N_("Audio Encryption"), N_("Frame indicates if the audio stream is encrypted, and by whom.")}},
                       {TAG_ID3_AUDIOSEEKPOINT, {"ID3.AudioSeekPoint", TAG_ID3, N_("Audio Seek Point"), N_("Fractional offset within the audio data, providing a starting point from which to find an appropriate point to start decoding.")}},
                       {TAG_ID3_BAND, {"ID3.Band", TAG_ID3, N_("Band"), N_("Additional information about the performers in the recording.")}},
                       {TAG_ID3_BPM, {"ID3.BPM", TAG_ID3, N_("BPM"), N_("BPM (beats per minute).")}},
                       {TAG_ID3_BUFFERSIZE, {"ID3.BufferSize", TAG_ID3, N_("Buffer Size"), N_("Recommended buffer size.")}},
                       {TAG_ID3_CDID, {"ID3.CDID", TAG_ID3, N_("CD ID"), N_("Music CD identifier.")}},
                       {TAG_ID3_COMMERCIAL, {"ID3.Commercial", TAG_ID3, N_("Commercial"), N_("Commercial frame.")}},
                       {TAG_ID3_COMPOSER, {"ID3.Composer", TAG_ID3, N_("Composer"), N_("Composer.")}},
                       {TAG_ID3_CONDUCTOR, {"ID3.Conductor", TAG_ID3, N_("Conductor"), N_("Conductor.")}},
                       {TAG_ID3_CONTENTGROUP, {"ID3.ContentGroup", TAG_ID3, N_("Content Group"), N_("Content group description.")}},
                       {TAG_ID3_CONTENTTYPE, {"ID3.ContentType", TAG_ID3, N_("Content Type"), N_("Type of music classification for the track as defined in ID3 spec.")}},
                       {TAG_ID3_COPYRIGHT, {"ID3.Copyright", TAG_ID3, N_("Copyright"), N_("Copyright message.")}},
                       {TAG_ID3_CRYPTOREG, {"ID3.CryptoReg", TAG_ID3, N_("Encryption Registration"), N_("Encryption method registration.")}},
                       {TAG_ID3_DATE, {"ID3.Date", TAG_ID3, N_("Date"), N_("Date.")}},
                       {TAG_ID3_EMPHASIS, {"ID3.Emphasis", TAG_ID3, N_("Emphasis"), N_("Emphasis.")}},
                       {TAG_ID3_ENCODEDBY, {"ID3.EncodedBy", TAG_ID3, N_("Encoded By"), N_("Person or organisation that encoded the audio file. This field may contain a copyright message, if the audio file also is copyrighted by the encoder.")}},
                       {TAG_ID3_ENCODERSETTINGS, {"ID3.EncoderSettings", TAG_ID3, N_("Encoder Settings"), N_("Software.")}},
                       {TAG_ID3_ENCODINGTIME, {"ID3.EncodingTime", TAG_ID3, N_("Encoding Time"), N_("Encoding time.")}},
                       {TAG_ID3_EQUALIZATION, {"ID3.Equalization", TAG_ID3, N_("Equalization"), N_("Equalization.")}},
                       {TAG_ID3_EQUALIZATION2, {"ID3.Equalization2", TAG_ID3, N_("Equalization 2"), N_("Equalisation curve predefine within the audio file.")}},
                       {TAG_ID3_EVENTTIMING, {"ID3.EventTiming", TAG_ID3, N_("Event Timing"), N_("Event timing codes.")}},
                       {TAG_ID3_FILEOWNER, {"ID3.FileOwner", TAG_ID3, N_("File Owner"), N_("File owner.")}},
                       {TAG_ID3_FILETYPE, {"ID3.FileType", TAG_ID3, N_("File Type"), N_("File type.")}},
                       {TAG_ID3_FRAMES, {"ID3.Frames", TAG_ID3, N_("Frames"), N_("Number of frames.")}},
                       {TAG_ID3_GENERALOBJECT, {"ID3.GeneralObject", TAG_ID3, N_("General Object"), N_("General encapsulated object.")}},
                       {TAG_ID3_GROUPINGREG, {"ID3.GroupingReg", TAG_ID3, N_("Grouping Registration"), N_("Group identification registration.")}},
                       {TAG_ID3_INITIALKEY, {"ID3.InitialKey", TAG_ID3, N_("Initial Key"), N_("Initial key.")}},
                       {TAG_ID3_INVOLVEDPEOPLE, {"ID3.InvolvedPeople", TAG_ID3, N_("Involved People"), N_("Involved people list.")}},
                       {TAG_ID3_INVOLVEDPEOPLE2, {"ID3.InvolvedPeople2", TAG_ID3, N_("InvolvedPeople2"), N_("Involved people list.")}},
                       {TAG_ID3_LANGUAGE, {"ID3.Language", TAG_ID3, N_("Language"), N_("Language.")}},
                       {TAG_ID3_LINKEDINFO, {"ID3.LinkedInfo", TAG_ID3, N_("Linked Info"), N_("Linked information.")}},
                       {TAG_ID3_LYRICIST, {"ID3.Lyricist", TAG_ID3, N_("Lyricist"), N_("Lyricist.")}},
                       {TAG_ID3_MEDIATYPE, {"ID3.MediaType", TAG_ID3, N_("Media Type"), N_("Media type.")}},
                       {TAG_ID3_MIXARTIST, {"ID3.MixArtist", TAG_ID3, N_("Mix Artist"), N_("Interpreted, remixed, or otherwise modified by.")}},
                       {TAG_ID3_MOOD, {"ID3.Mood", TAG_ID3, N_("Mood"), N_("Mood.")}},
                       {TAG_ID3_MPEGLOOKUP, {"ID3.MPEG.Lookup", TAG_ID3, N_("MPEG Lookup"), N_("MPEG location lookup table.")}},
                       {TAG_ID3_MUSICIANCREDITLIST, {"ID3.MusicianCreditList", TAG_ID3, N_("Musician Credit List"), N_("Musician credits list.")}},
                       {TAG_ID3_NETRADIOOWNER, {"ID3.NetRadioOwner", TAG_ID3, N_("Net Radio Owner"), N_("Internet radio station owner.")}},
                       {TAG_ID3_NETRADIOSTATION, {"ID3.NetRadiostation", TAG_ID3, N_("Net Radiostation"), N_("Internet radio station name.")}},
                       {TAG_ID3_ORIGALBUM, {"ID3.OriginalAlbum", TAG_ID3, N_("Original Album"), N_("Original album.")}},
                       {TAG_ID3_ORIGARTIST, {"ID3.OriginalArtist", TAG_ID3, N_("Original Artist"), N_("Original artist.")}},
                       {TAG_ID3_ORIGFILENAME, {"ID3.OriginalFileName", TAG_ID3, N_("Original File Name"), N_("Original file name.")}},
                       {TAG_ID3_ORIGLYRICIST, {"ID3.OriginalLyricist", TAG_ID3, N_("Original Lyricist"), N_("Original lyricist.")}},
                       {TAG_ID3_ORIGRELEASETIME, {"ID3.OriginalReleaseTime", TAG_ID3, N_("Original Release Time"), N_("Original release time.")}},
                       {TAG_ID3_ORIGYEAR, {"ID3.OriginalYear", TAG_ID3, N_("Original Year"), N_("Original release year.")}},
                       {TAG_ID3_OWNERSHIP, {"ID3.Ownership", TAG_ID3, N_("Ownership"), N_("Ownership frame.")}},
                       {TAG_ID3_PARTINSET, {"ID3.PartInSet", TAG_ID3, N_("Part of a Set"), N_("Part of a set the audio came from.")}},
                       {TAG_ID3_PERFORMERSORTORDER, {"ID3.PerformerSortOrder", TAG_ID3, N_("Performer Sort Order"), N_("Performer sort order.")}},
                       {TAG_ID3_PICTURE, {"ID3.Picture", TAG_ID3, N_("Picture"), N_("Attached picture.")}},
                       {TAG_ID3_PLAYCOUNTER, {"ID3.PlayCounter", TAG_ID3, N_("Play Counter"), N_("Number of times a file has been played.")}},
                       {TAG_ID3_PLAYLISTDELAY, {"ID3.PlaylistDelay", TAG_ID3, N_("Playlist Delay"), N_("Playlist delay.")}},
                       {TAG_ID3_POPULARIMETER, {"ID3.Popularimeter", TAG_ID3, N_("Popularimeter"), N_("Rating of the audio file.")}},
                       {TAG_ID3_POSITIONSYNC, {"ID3.PositionSync", TAG_ID3, N_("Position Sync"), N_("Position synchronisation frame.")}},
                       {TAG_ID3_PRIVATE, {"ID3.Private", TAG_ID3, N_("Private"), N_("Private frame.")}},
                       {TAG_ID3_PRODUCEDNOTICE, {"ID3.ProducedNotice", TAG_ID3, N_("Produced Notice"), N_("Produced notice.")}},
                       {TAG_ID3_PUBLISHER, {"ID3.Publisher", TAG_ID3, N_("Publisher"), N_("Publisher.")}},
                       {TAG_ID3_RECORDINGDATES, {"ID3.RecordingDates", TAG_ID3, N_("Recording Dates"), N_("Recording dates.")}},
                       {TAG_ID3_RECORDINGTIME, {"ID3.RecordingTime", TAG_ID3, N_("Recording Time"), N_("Recording time.")}},
                       {TAG_ID3_RELEASETIME, {"ID3.ReleaseTime", TAG_ID3, N_("Release Time"), N_("Release time.")}},
                       {TAG_ID3_REVERB, {"ID3.Reverb", TAG_ID3, N_("Reverb"), N_("Reverb.")}},
                       {TAG_ID3_SETSUBTITLE, {"ID3.SetSubtitle", TAG_ID3, N_("Set Subtitle"), N_("Subtitle of the part of a set this track belongs to.")}},
                       {TAG_ID3_SIGNATURE, {"ID3.Signature", TAG_ID3, N_("Signature"), N_("Signature frame.")}},
                       {TAG_ID3_SIZE, {"ID3.Size", TAG_ID3, N_("Size"), N_("Size of the audio file in bytes, excluding the ID3 tag.")}},
                       {TAG_ID3_SONGLEN, {"ID3.SongLength", TAG_ID3, N_("Song length"), N_("Length of the song in milliseconds.")}},
                       {TAG_ID3_SUBTITLE, {"ID3.Subtitle", TAG_ID3, N_("Subtitle"), N_("Subtitle.")}},
                       {TAG_ID3_SYNCEDLYRICS, {"ID3.Syncedlyrics", TAG_ID3, N_("Synchronized Lyrics"), N_("Synchronized lyric.")}},
                       {TAG_ID3_SYNCEDTEMPO, {"ID3.SyncedTempo", TAG_ID3, N_("Synchronized Tempo"), N_("Synchronized tempo codes.")}},
                       {TAG_ID3_TAGGINGTIME, {"ID3.TaggingTime", TAG_ID3, N_("Tagging Time"), N_("Tagging time.")}},
                       {TAG_ID3_TERMSOFUSE, {"ID3.TermsOfUse", TAG_ID3, N_("Terms of Use"), N_("Terms of use.")}},
                       {TAG_ID3_TIME, {"ID3.Time", TAG_ID3, N_("Time"), N_("Time.")}},
                       {TAG_ID3_TITLESORTORDER, {"ID3.TitleSortOrder", TAG_ID3, N_("Title Sort Order"), N_("Title sort order.")}},
                       {TAG_ID3_UNIQUEFILEID, {"ID3.UniqueFileID", TAG_ID3, N_("Unique File ID"), N_("Unique file identifier.")}},
                       {TAG_ID3_UNSYNCEDLYRICS, {"ID3.UnsyncedLyrics", TAG_ID3, N_("Unsynchronized Lyrics"), N_("Unsynchronized lyric.")}},
                       {TAG_ID3_USERTEXT, {"ID3.UserText", TAG_ID3, N_("User Text"), N_("User defined text information.")}},
                       {TAG_ID3_VOLUMEADJ, {"ID3.VolumeAdj", TAG_ID3, N_("Volume Adjustment"), N_("Relative volume adjustment.")}},
                       {TAG_ID3_VOLUMEADJ2, {"ID3.VolumeAdj2", TAG_ID3, N_("Volume Adjustment 2"), N_("Relative volume adjustment.")}},
                       {TAG_ID3_WWWARTIST, {"ID3.WWWArtist", TAG_ID3, N_("WWW Artist"), N_("Official artist.")}},
                       {TAG_ID3_WWWAUDIOFILE, {"ID3.WWWAudioFile", TAG_ID3, N_("WWW Audio File"), N_("Official audio file webpage.")}},
                       {TAG_ID3_WWWAUDIOSOURCE, {"ID3.WWWAudioSource", TAG_ID3, N_("WWW Audio Source"), N_("Official audio source webpage.")}},
                       {TAG_ID3_WWWCOMMERCIALINFO, {"ID3.WWWCommercialInfo", TAG_ID3, N_("WWW Commercial Info"), N_("URL pointing at a webpage containing commercial information.")}},
                       {TAG_ID3_WWWCOPYRIGHT, {"ID3.WWWCopyright", TAG_ID3, N_("WWW Copyright"), N_("URL pointing at a webpage that holds copyright.")}},
                       {TAG_ID3_WWWPAYMENT, {"ID3.WWWPayment", TAG_ID3, N_("WWW Payment"), N_("URL pointing at a webpage that will handle the process of paying for this file.")}},
                       {TAG_ID3_WWWPUBLISHER, {"ID3.WWWPublisher", TAG_ID3, N_("WWW Publisher"), N_("URL pointing at the official webpage for the publisher.")}},
                       {TAG_ID3_WWWRADIOPAGE, {"ID3.WWWRadioPage", TAG_ID3, N_("WWW Radio Page"), N_("Official internet radio station homepage.")}},
                       {TAG_ID3_WWWUSER, {"ID3.WWWUser", TAG_ID3, N_("WWW User"), N_("User defined URL link.")}},
                       {TAG_IMAGE_ALBUM, {"Image.Album", TAG_IMAGE, N_("Album"), N_("Name of an album the image belongs to.")}},
                       {TAG_IMAGE_COMMENTS, {"Image.Comments", TAG_IMAGE, N_("Comments"), N_("User definable free text.")}},
                       {TAG_IMAGE_COPYRIGHT, {"Image.Copyright", TAG_IMAGE, N_("Copyright"), N_("Embedded copyright message.")}},
                       {TAG_IMAGE_CREATOR, {"Image.Creator", TAG_IMAGE, N_("Creator"), N_("Name of the author.")}},
                       {TAG_IMAGE_DATE, {"Image.Date", TAG_IMAGE, N_("Date"), N_("Datetime image was originally created.")}},
                       {TAG_IMAGE_DESCRIPTION, {"Image.Description", TAG_IMAGE, N_("Description"), N_("Description of the image.")}},
                       {TAG_IMAGE_EXPOSUREPROGRAM, {"Image.ExposureProgram", TAG_IMAGE, N_("Exposure Program"), N_("The program used by the camera to set exposure when the picture is taken. EG Manual, Normal, Aperture priority etc.")}},
                       {TAG_IMAGE_EXPOSURETIME, {"Image.ExposureTime", TAG_IMAGE, N_("Exposure Time"), N_("Exposure time used to capture the photo in seconds.")}},
                       {TAG_IMAGE_FLASH, {"Image.Flash", TAG_IMAGE, N_("Flash"), N_("Set to \"1\" if flash was fired.")}},
                       {TAG_IMAGE_FNUMBER, {"Image.Fnumber", TAG_IMAGE, N_("F Number"), N_("Diameter of the aperture relative to the effective focal length of the lens.")}},
                       {TAG_IMAGE_FOCALLENGTH, {"Image.FocalLength", TAG_IMAGE, N_("Focal Length"), N_("Focal length of lens in mm.")}},
                       {TAG_IMAGE_HEIGHT, {"Image.Height", TAG_IMAGE, N_("Height"), N_("Height in pixels.")}},
                       {TAG_IMAGE_ISOSPEED, {"Image.ISOSpeed", TAG_IMAGE, N_("ISO Speed"), N_("ISO speed used to acquire the document contents. For example, 100, 200, 400, etc.")}},
                       {TAG_IMAGE_KEYWORDS, {"Image.Keywords", TAG_IMAGE, N_("Keywords"), N_("String of keywords.")}},
                       {TAG_IMAGE_MAKE, {"Image.Make", TAG_IMAGE, N_("Make"), N_("Make of camera used to take the image.")}},
                       {TAG_IMAGE_METERINGMODE, {"Image.MeteringMode", TAG_IMAGE, N_("Metering Mode"), N_("Metering mode used to acquire the image (IE Unknown, Average, CenterWeightedAverage, Spot, MultiSpot, Pattern, Partial).")}},
                       {TAG_IMAGE_MODEL, {"Image.Model", TAG_IMAGE, N_("Model"), N_("Model of camera used to take the image.")}},
                       {TAG_IMAGE_ORIENTATION, {"Image.Orientation", TAG_IMAGE, N_("Orientation"), N_("Represents the orientation of the image wrt camera (IE \"top,left\" or \"bottom,right\").")}},
                       {TAG_IMAGE_SOFTWARE, {"Image.Software", TAG_IMAGE, N_("Software"), N_("Software used to produce/enhance the image.")}},
                       {TAG_IMAGE_TITLE, {"Image.Title", TAG_IMAGE, N_("Title"), N_("Title of image.")}},
                       {TAG_IMAGE_WHITEBALANCE, {"Image.WhiteBalance", TAG_IMAGE, N_("White Balance"), N_("White balance setting of the camera when the picture was taken (auto or manual).")}},
                       {TAG_IMAGE_WIDTH, {"Image.Width", TAG_IMAGE, N_("Width"), N_("Width in pixels.")}},
                       {TAG_IPTC_ACTIONADVISED, {"IPTC.ActionAdvised", TAG_IPTC, N_("Action Advised"), N_("The type of action that this object provides to a previous object. '01' Object Kill, '02' Object Replace, '03' Object Append, '04' Object Reference.")}},
                       {TAG_IPTC_ARMID, {"IPTC.ARMID", TAG_IPTC, N_("ARM Identifier"), N_("Identifies the Abstract Relationship Method (ARM).")}},
                       {TAG_IPTC_ARMVERSION, {"IPTC.ARMVersion", TAG_IPTC, N_("ARM Version"), N_("Identifies the version of the Abstract Relationship Method (ARM).")}},
                       {TAG_IPTC_AUDIODURATION, {"IPTC.AudioDuration", TAG_IPTC, N_("Audio Duration"), N_("The running time of the audio data in the form HHMMSS.")}},
                       {TAG_IPTC_AUDIOOUTCUE, {"IPTC.AudioOutcue", TAG_IPTC, N_("Audio Outcue"), N_("The content at the end of the audio data.")}},
                       {TAG_IPTC_AUDIOSAMPLINGRATE, {"IPTC.AudioSamplingRate", TAG_IPTC, N_("Audio Sampling Rate"), N_("The sampling rate in Hz of the audio data.")}},
                       {TAG_IPTC_AUDIOSAMPLINGRES, {"IPTC.AudioSamplingRes", TAG_IPTC, N_("Audio Sampling Resolution"), N_("The number of bits in each audio sample.")}},
                       {TAG_IPTC_AUDIOTYPE, {"IPTC.AudioType", TAG_IPTC, N_("Audio Type"), N_("The number of channels and type of audio (music, text, etc.) in the object.")}},
                       {TAG_IPTC_BYLINE, {"IPTC.Byline", TAG_IPTC, N_("By-line"), N_("Name of the creator of the object, e.g. writer, photographer or graphic artist (multiple values allowed).")}},
                       {TAG_IPTC_BYLINETITLE, {"IPTC.BylineTitle", TAG_IPTC, N_("By-line Title"), N_("Title of the creator or creators of the object.")}},
                       {TAG_IPTC_CAPTION, {"IPTC.Caption", TAG_IPTC, N_("Caption, Abstract"), N_("A textual description of the data.")}},
                       {TAG_IPTC_CATEGORY, {"IPTC.Category", TAG_IPTC, N_("Category"), N_("Identifies the subject of the object in the opinion of the provider (Deprecated).")}},
                       {TAG_IPTC_CHARACTERSET, {"IPTC.CharacterSet", TAG_IPTC, N_("Coded Character Set"), N_("Control functions used for the announcement, invocation or designation of coded character sets.")}},
                       {TAG_IPTC_CITY, {"IPTC.City", TAG_IPTC, N_("City"), N_("City of object origin.")}},
                       {TAG_IPTC_CONFIRMEDDATASIZE, {"IPTC.ConfirmedDataSize", TAG_IPTC, N_("Confirmed Data Size"), N_("Total size of the object data.")}},
                       {TAG_IPTC_CONTACT, {"IPTC.Contact", TAG_IPTC, N_("Contact"), N_("The person or organization which can provide further background information on the object (multiple values allowed).")}},
                       {TAG_IPTC_CONTENTLOCCODE, {"IPTC.ContentLocCode", TAG_IPTC, N_("Content Location Code"), N_("The code of a country/geographical location referenced by the content of the object (multiple values allowed).")}},
                       {TAG_IPTC_CONTENTLOCNAME, {"IPTC.ContentLocName", TAG_IPTC, N_("Content Location Name"), N_("A full, publishable name of a country/geographical location referenced by the content of the object (multiple values allowed).")}},
                       {TAG_IPTC_COPYRIGHTNOTICE, {"IPTC.CopyrightNotice", TAG_IPTC, N_("Copyright Notice"), N_("Any necessary copyright notice.")}},
                       {TAG_IPTC_COUNTRYCODE, {"IPTC.CountryCode", TAG_IPTC, N_("Country Code"), N_("The code of the country/primary location where the object was created.")}},
                       {TAG_IPTC_COUNTRYNAME, {"IPTC.CountryName", TAG_IPTC, N_("Country Name"), N_("The name of the country/primary location where the object was created.")}},
                       {TAG_IPTC_CREDIT, {"IPTC.Credit", TAG_IPTC, N_("Credit"), N_("Identifies the provider of the object, not necessarily the owner/creator.")}},
                       {TAG_IPTC_DATECREATED, {"IPTC.DateCreated", TAG_IPTC, N_("Date Created"), N_("The date the intellectual content of the object was created rather than the date of the creation of the physical representation.")}},
                       {TAG_IPTC_DATESENT, {"IPTC.DateSent", TAG_IPTC, N_("Date Sent"), N_("The day the service sent the material.")}},
                       {TAG_IPTC_DESTINATION, {"IPTC.Destination", TAG_IPTC, N_("Destination"), N_("Routing information.")}},
                       {TAG_IPTC_DIGITALCREATIONDATE, {"IPTC.DigitalCreationDate", TAG_IPTC, N_("Digital Creation Date"), N_("The date the digital representation of the object was created.")}},
                       {TAG_IPTC_DIGITALCREATIONTIME, {"IPTC.DigitalCreationTime", TAG_IPTC, N_("Digital Creation Time"), N_("The time the digital representation of the object was created.")}},
                       {TAG_IPTC_EDITORIALUPDATE, {"IPTC.EditorialUpdate", TAG_IPTC, N_("Editorial Update"), N_("The type of update this object provides to a previous object. The link to the previous object is made using the ARM. '01' indicates an additional language.")}},
                       {TAG_IPTC_EDITSTATUS, {"IPTC.EditStatus", TAG_IPTC, N_("Edit Status"), N_("Status of the object, according to the practice of the provider.")}},
                       {TAG_IPTC_ENVELOPENUM, {"IPTC.EnvelopeNum", TAG_IPTC, N_("Envelope Number"), N_("A number unique for the date and the service ID.")}},
                       {TAG_IPTC_ENVELOPEPRIORITY, {"IPTC.EnvelopePriority", TAG_IPTC, N_("Envelope Priority"), N_("Specifies the envelope handling priority and not the editorial urgency. '1' for most urgent, '5' for normal, and '8' for least urgent. '9' is user-defined.")}},
                       {TAG_IPTC_EXPIRATIONDATE, {"IPTC.ExpirationDate", TAG_IPTC, N_("Expiration Date"), N_("Designates the latest date the provider intends the object to be used.")}},
                       {TAG_IPTC_EXPIRATIONTIME, {"IPTC.ExpirationTime", TAG_IPTC, N_("Expiration Time"), N_("Designates the latest time the provider intends the object to be used.")}},
                       {TAG_IPTC_FILEFORMAT, {"IPTC.FileFormat", TAG_IPTC, N_("File Format"), N_("File format of the data described by this metadata.")}},
                       {TAG_IPTC_FILEVERSION, {"IPTC.FileVersion", TAG_IPTC, N_("File Version"), N_("Version of the file format.")}},
                       {TAG_IPTC_FIXTUREID, {"IPTC.FixtureID", TAG_IPTC, N_("Fixture Identifier"), N_("Identifies objects that recur often and predictably, enabling users to immediately find or recall such an object.")}},
                       {TAG_IPTC_HEADLINE, {"IPTC.Headline", TAG_IPTC, N_("Headline"), N_("A publishable entry providing a synopsis of the contents of the object.")}},
                       {TAG_IPTC_IMAGEORIENTATION, {"IPTC.ImageOrientation", TAG_IPTC, N_("Image Orientation"), N_("The layout of the image area: 'P' for portrait, 'L' for landscape, and 'S' for square.")}},
                       {TAG_IPTC_IMAGETYPE, {"IPTC.ImageType", TAG_IPTC, N_("Image Type"), N_("The data format of the image object.")}},
                       {TAG_IPTC_KEYWORDS, {"IPTC.Keywords", TAG_IPTC, N_("Keywords"), N_("Used to indicate specific information retrieval words (multiple values allowed).")}},
                       {TAG_IPTC_LANGUAGEID, {"IPTC.LanguageID", TAG_IPTC, N_("Language Identifier"), N_("The major national language of the object, according to the 2-letter codes of ISO 639:1988.")}},
                       {TAG_IPTC_MAXOBJECTSIZE, {"IPTC.MaxObjectSize", TAG_IPTC, N_("Maximum Object Size"), N_("The largest possible size of the object if the size is not known.")}},
                       {TAG_IPTC_MAXSUBFILESIZE, {"IPTC.MaxSubfileSize", TAG_IPTC, N_("Max Subfile Size"), N_("The maximum size for a subfile dataset containing a portion of the object data.")}},
                       {TAG_IPTC_MODELVERSION, {"IPTC.ModelVersion", TAG_IPTC, N_("Model Version"), N_("Version of IIM part 1.")}},
                       {TAG_IPTC_OBJECTATTRIBUTE, {"IPTC.ObjectAttribute", TAG_IPTC, N_("Object Attribute Reference"), N_("Defines the nature of the object independent of the subject (multiple values allowed).")}},
                       {TAG_IPTC_OBJECTCYCLE, {"IPTC.ObjectCycle", TAG_IPTC, N_("Object Cycle"), N_("Where 'a' is morning, 'p' is evening, 'b' is both.")}},
                       {TAG_IPTC_OBJECTNAME, {"IPTC.ObjectName", TAG_IPTC, N_("Object Name"), N_("A shorthand reference for the object.")}},
                       {TAG_IPTC_OBJECTSIZEANNOUNCED, {"IPTC.ObjectSizeAnnounced", TAG_IPTC, N_("Object Size Announced"), N_("The total size of the object data if it is known.")}},
                       {TAG_IPTC_OBJECTTYPE, {"IPTC.ObjectType", TAG_IPTC, N_("Object Type Reference"), N_("Distinguishes between different types of objects within the IIM.")}},
                       {TAG_IPTC_ORIGINATINGPROGRAM, {"IPTC.OriginatingProgram", TAG_IPTC, N_("Originating Program"), N_("The type of program used to originate the object.")}},
                       {TAG_IPTC_ORIGTRANSREF, {"IPTC.OrigTransRef", TAG_IPTC, N_("Original Transmission Reference"), N_("A code representing the location of original transmission.")}},
                       {TAG_IPTC_PREVIEWDATA, {"IPTC.PreviewData", TAG_IPTC, N_("Preview Data"), N_("The object preview data.")}},
                       {TAG_IPTC_PREVIEWFORMAT, {"IPTC.PreviewFileFormat", TAG_IPTC, N_("Preview File Format"), N_("Binary value indicating the file format of the object preview data.")}},
                       {TAG_IPTC_PREVIEWFORMATVER, {"IPTC.PreviewFileFormatVer", TAG_IPTC, N_("Preview File Format Version"), N_("The version of the preview file format.")}},
                       {TAG_IPTC_PRODUCTID, {"IPTC.ProductID", TAG_IPTC, N_("Product ID"), N_("Allows a provider to identify subsets of its overall service.")}},
                       {TAG_IPTC_PROGRAMVERSION, {"IPTC.ProgramVersion", TAG_IPTC, N_("Program Version"), N_("The version of the originating program.")}},
                       {TAG_IPTC_PROVINCE, {"IPTC.Province", TAG_IPTC, N_("Province, State"), N_("The Province/State where the object originates.")}},
                       {TAG_IPTC_RASTERIZEDCAPTION, {"IPTC.RasterizedCaption", TAG_IPTC, N_("Rasterized Caption"), N_("Contains rasterized object description and is used where characters that have not been coded are required for the caption.")}},
                       {TAG_IPTC_RECORDVERSION, {"IPTC.RecordVersion", TAG_IPTC, N_("Record Version"), N_("Identifies the version of the IIM, Part 2.")}},
                       {TAG_IPTC_REFERENCEDATE, {"IPTC.RefDate", TAG_IPTC, N_("Reference Date"), N_("The date of a prior envelope to which the current object refers.")}},
                       {TAG_IPTC_REFERENCENUMBER, {"IPTC.RefNumber", TAG_IPTC, N_("Reference Number"), N_("The Envelope Number of a prior envelope to which the current object refers.")}},
                       {TAG_IPTC_REFERENCESERVICE, {"IPTC.RefService", TAG_IPTC, N_("Reference Service"), N_("The Service Identifier of a prior envelope to which the current object refers.")}},
                       {TAG_IPTC_RELEASEDATE, {"IPTC.ReleaseDate", TAG_IPTC, N_("Release Date"), N_("Designates the earliest date the provider intends the object to be used.")}},
                       {TAG_IPTC_RELEASETIME, {"IPTC.ReleaseTime", TAG_IPTC, N_("Release Time"), N_("Designates the earliest time the provider intends the object to be used.")}},
                       {TAG_IPTC_SERVICEID, {"IPTC.ServiceID", TAG_IPTC, N_("Service Identifier"), N_("Identifies the provider and product.")}},
                       {TAG_IPTC_SIZEMODE, {"IPTC.SizeMode", TAG_IPTC, N_("Size Mode"), N_("Set to 0 if the size of the object is known and 1 if not known.")}},
                       {TAG_IPTC_SOURCE, {"IPTC.Source", TAG_IPTC, N_("Source"), N_("The original owner of the intellectual content of the object.")}},
                       {TAG_IPTC_SPECIALINSTRUCTIONS, {"IPTC.SpecialInstructions", TAG_IPTC, N_("Special Instructions"), N_("Other editorial instructions concerning the use of the object.")}},
                       {TAG_IPTC_STATE, {"IPTC.State", TAG_IPTC, N_("Province, State"), N_("The Province/State where the object originates.")}},
                       {TAG_IPTC_SUBFILE, {"IPTC.Subfile", TAG_IPTC, N_("Subfile"), N_("The object data itself. Subfiles must be sequential so that the subfiles may be reassembled.")}},
                       {TAG_IPTC_SUBJECTREFERENCE, {"IPTC.SubjectRef", TAG_IPTC, N_("Subject Reference"), N_("A structured definition of the subject matter. It must contain an IPR, an 8 digit Subject Reference Number and an optional Subject Name, Subject Matter Name, and Subject Detail Name each separated by a colon (:).")}},
                       {TAG_IPTC_SUBLOCATION, {"IPTC.Sublocation", TAG_IPTC, N_("Sub-location"), N_("The location within a city from which the object originates.")}},
                       {TAG_IPTC_SUPPLCATEGORY, {"IPTC.SupplCategory", TAG_IPTC, N_("Supplemental Category"), N_("Further refines the subject of the object (Deprecated).")}},
                       {TAG_IPTC_TIMECREATED, {"IPTC.TimeCreated", TAG_IPTC, N_("Time Created"), N_("The time the intellectual content of the object was created rather than the date of the creation of the physical representation (multiple values allowed).")}},
                       {TAG_IPTC_TIMESENT, {"IPTC.TimeSent", TAG_IPTC, N_("Time Sent"), N_("The time the service sent the material.")}},
                       {TAG_IPTC_UNO, {"IPTC.UNO", TAG_IPTC, N_("Unique Name of Object"), N_("An eternal, globally unique identification for the object, independent of provider and for any media form.")}},
                       {TAG_IPTC_URGENCY, {"IPTC.Urgency", TAG_IPTC, N_("Urgency"), N_("Specifies the editorial urgency of content and not necessarily the envelope handling priority. '1' is most urgent, '5' normal, and '8' least urgent.")}},
                       {TAG_IPTC_WRITEREDITOR, {"IPTC.WriterEditor", TAG_IPTC, N_("Writer/Editor"), N_("The name of the person involved in the writing, editing or correcting the object or caption/abstract (multiple values allowed).")}},
                       {TAG_PDF_PAGESIZE, {"PDF.PageSize", TAG_PDF, N_("Page Size"), N_("Page size format.")}},
                       {TAG_PDF_PAGEWIDTH, {"PDF.PageWidth", TAG_PDF, N_("Page Width"), N_("Page width in mm.")}},
                       {TAG_PDF_PAGEHEIGHT, {"PDF.PageHeight", TAG_PDF, N_("Page Height"), N_("Page height in mm.")}},
                       {TAG_PDF_VERSION, {"PDF.Version", TAG_PDF, N_("PDF Version"), N_("The PDF version of the document.")}},
                       {TAG_PDF_PRODUCER, {"PDF.Producer", TAG_PDF, N_("Producer"), N_("The application that converted the document to PDF.")}},
                       {TAG_PDF_EMBEDDEDFILES, {"PDF.EmbeddedFiles", TAG_PDF, N_("Embedded Files"), N_("Number of embedded files in the document.")}},
                       {TAG_PDF_OPTIMIZED, {"PDF.Optimized", TAG_PDF, N_("Fast Web View"), N_("Set to \"1\" if optimized for network access.")}},
                       {TAG_PDF_PRINTING, {"PDF.Printing", TAG_PDF, N_("Printing"), N_("Set to \"1\" if printing is allowed.")}},
                       {TAG_PDF_HIRESPRINTING, {"PDF.HiResPrinting", TAG_PDF, N_("Printing in High Resolution"), N_("Set to \"1\" if high resolution printing is allowed.")}},
                       {TAG_PDF_COPYING, {"PDF.Copying", TAG_PDF, N_("Copying"), N_("Set to \"1\" if copying the contents is allowed.")}},
                       {TAG_PDF_MODIFYING, {"PDF.Modifying", TAG_PDF, N_("Modifying"), N_("Set to \"1\" if modifying the contents is allowed.")}},
                       {TAG_PDF_DOCASSEMBLY, {"PDF.DocAssembly", TAG_PDF, N_("Document Assembly"), N_("Set to \"1\" if inserting, rotating, or deleting pages and creating navigation elements is allowed.")}},
                       {TAG_PDF_COMMENTING, {"PDF.Commenting", TAG_PDF, N_("Commenting"), N_("Set to \"1\" if adding or modifying text annotations is allowed.")}},
                       {TAG_PDF_FORMFILLING, {"PDF.FormFilling", TAG_PDF, N_("Form Filling"), N_("Set to \"1\" if filling of form fields is allowed.")}},
                       {TAG_PDF_ACCESSIBILITYSUPPORT, {"PDF.AccessibilitySupport", TAG_PDF, N_("Accessibility Support"), N_("Set to \"1\" if accessibility support (eg. screen readers) is enabled.")}},
                       {TAG_VORBIS_CONTACT, {"Vorbis.Contact", TAG_VORBIS, N_("Contact"), N_("Contact information for the creators or distributors of the track.")}},
                       {TAG_VORBIS_DESCRIPTION, {"Vorbis.Description", TAG_VORBIS, N_("Description"), N_("A textual description of the data.")}},
                       {TAG_VORBIS_LICENSE, {"Vorbis.License", TAG_VORBIS, N_("License"), N_("License information.")}},
                       {TAG_VORBIS_LOCATION, {"Vorbis.Location", TAG_VORBIS, N_("Location"), N_("Location where track was recorded.")}},
                       {TAG_VORBIS_MAXBITRATE, {"Vorbis.Contact", TAG_VORBIS, N_("Maximum bitrate"), N_("Maximum bitrate in kbps.")}},
                       {TAG_VORBIS_MINBITRATE, {"Vorbis.Contact", TAG_VORBIS, N_("Minimum bitrate"), N_("Minimum bitrate in kbps.")}},
                       {TAG_VORBIS_NOMINALBITRATE, {"Vorbis.Contact", TAG_VORBIS, N_("Nominal bitrate"), N_("Nominal bitrate in kbps.")}},
                       {TAG_VORBIS_ORGANIZATION, {"Vorbis.Organization", TAG_VORBIS, N_("Organization"), N_("Organization producing the track.")}},
                       {TAG_VORBIS_VENDOR, {"Vorbis.Vendor", TAG_VORBIS, N_("Vendor"), N_("Vorbis vendor ID.")}},
                       {TAG_VORBIS_VERSION, {"Vorbis.Version", TAG_VORBIS, N_("Vorbis Version"), N_("Vorbis version.")}}
                      };

    load_data (metatags, metatags_data, G_N_ELEMENTS(metatags_data));

    gcmd_tags_exiv2_init();
    gcmd_tags_taglib_init();
    gcmd_tags_libgsf_init();
    gcmd_tags_poppler_init();
}


void gcmd_tags_shutdown()
{
    gcmd_tags_exiv2_shutdown();
    gcmd_tags_taglib_shutdown();
    gcmd_tags_libgsf_shutdown();
    gcmd_tags_poppler_shutdown();
}


GnomeCmdFileMetadata *gcmd_tags_bulk_load(GnomeCmdFile *f)
{
    g_return_val_if_fail (f != NULL, NULL);

    gcmd_tags_file_load_metadata(f);
    gcmd_tags_exiv2_load_metadata(f);
    gcmd_tags_taglib_load_metadata(f);
    gcmd_tags_libgsf_load_metadata(f);
    gcmd_tags_poppler_load_metadata(f);

    return f->metadata;
}


GnomeCmdTag gcmd_tags_get_tag_by_name(const gchar *tag_name, const GnomeCmdTagClass tag_class)
{
    GnomeCmdTagName t;

    if (tag_class!=TAG_NONE_CLASS)
    {
        switch (tag_class)
        {
            case TAG_APE:
                t.name = "APE.";
                break;

            case TAG_AUDIO:
                t.name = "Audio.";
                break;

            case TAG_CHM:
                t.name = "CHM.";
                break;

            case TAG_DOC:
                t.name = "Doc.";
                break;

            case TAG_EXIF:
                t.name = "Exif.";
                break;

            case TAG_FILE:
                t.name = "File.";
                break;

            case TAG_FLAC:
                t.name = "FLAC";
                break;

            case TAG_ICC:
                t.name = "ICC.";
                break;

            case TAG_ID3:
                t.name = "ID3.";
                break;

            case TAG_IMAGE:
                t.name = "Image.";
                break;

            case TAG_IPTC:
                t.name = "IPTC.";
                break;

            case TAG_PDF:
                t.name = "PDF.";
                break;

            case TAG_RPM:
                t.name = "RPM.";
                break;

            case TAG_VORBIS:
                t.name = "Vorbis.";
                break;

            default:
                t.name = empty_string;
                break;
        }

        t.name = g_strconcat(t.name,tag_name,NULL);
    }
    else
        t.name = tag_name;

    GnomeCmdTag tag = metatags[t];

    if (tag_class!=TAG_NONE_CLASS)
        g_free ((gpointer) t.name);

    return tag;
}


const gchar *gcmd_tags_get_name(const GnomeCmdTag tag)
{
    return metatags[tag].name;;
}


const GnomeCmdTagClass gcmd_tags_get_class(const GnomeCmdTag tag)
{
    return metatags[tag].tag_class;
}


const gchar *gcmd_tags_get_class_name(const GnomeCmdTag tag)
{
    switch (metatags[tag].tag_class)
    {
        case TAG_APE:
            return "APE";

        case TAG_AUDIO:
            return _("Audio");

        case TAG_CHM:
            return "CHM";

        case TAG_DOC:
            return _("Doc");

        case TAG_EXIF:
            return "Exif";

        case TAG_FILE:
            return _("File");

        case TAG_FLAC:
            return "FLAC";

        case TAG_ICC:
            return "ICC";

        case TAG_ID3:
            return "ID3";

        case TAG_IMAGE:
            return _("Image");

        case TAG_IPTC:
            return "IPTC";

        case TAG_PDF:
            return "PDF";

        case TAG_RPM:
            return "RPM";

        case TAG_VORBIS:
            return "Vorbis";

        default:
            break;
    }

    return empty_string;
}


const gchar *gcmd_tags_get_value(GnomeCmdFile *f, const GnomeCmdTag tag)
{
    g_return_val_if_fail (f != NULL, empty_string);

    const gchar *ret_val = empty_string;

    switch (metatags[tag].tag_class)
    {
        case TAG_IMAGE:
        case TAG_EXIF :
        case TAG_IPTC :
#ifndef HAVE_EXIV2
                        return _(no_support_for_exiv2_tags_string);
#endif
                        gcmd_tags_exiv2_load_metadata(f);
                        ret_val = f->metadata->operator [] (tag).c_str();
                        break;

        case TAG_AUDIO:
        case TAG_APE  :
        case TAG_FLAC :
        case TAG_ID3  :
        case TAG_VORBIS:
#ifndef HAVE_ID3
                        return _(no_support_for_taglib_tags_string);
#endif
                        gcmd_tags_taglib_load_metadata(f);
                        ret_val = f->metadata->operator [] (tag).c_str();
                        break;

        case TAG_DOC  :
#ifndef HAVE_GSF
#ifndef HAVE_PDF
                        return _(no_support_for_libgsf_tags_string);
#endif
#endif
                        gcmd_tags_libgsf_load_metadata(f);
                        gcmd_tags_poppler_load_metadata(f);
                        ret_val = f->metadata->operator [] (tag).c_str();
                        break;

        case TAG_PDF  :
#ifndef HAVE_PDF
                        return _(no_support_for_poppler_tags_string);
#endif
                        gcmd_tags_poppler_load_metadata(f);
                        ret_val = f->metadata->operator [] (tag).c_str();
                        break;

        case TAG_CHM  : break;

        case TAG_FILE : gcmd_tags_file_load_metadata(f);
                        ret_val = f->metadata->operator [] (tag).c_str();
                        break;

        case TAG_RPM  : break;

        default:        break;
    }

    return ret_val ? ret_val : empty_string;
}


const gchar *gcmd_tags_get_title(const GnomeCmdTag tag)
{
    return _(metatags[tag].title);
}


const gchar *gcmd_tags_get_description(const GnomeCmdTag tag)
{
    return _(metatags[tag].description);
}

/*
    GNOME Commander - A GNOME based file manager
    Copyright (C) 2001-2006 Marcus Bjurman

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
#include "utils.h"

using namespace std;


struct GnomeCmdTagName
{
    const gchar *name;
    GnomeCmdTagClass tag_class;
    GnomeCmdTag tag;
    const gchar *title;
    const gchar *description;
};


static GnomeCmdTagName metatags[NUMBER_OF_TAGS] = {{"", TAG_NONE_CLASS, TAG_NONE, "", ""},
                                                   {"Audio.AlbumArtist", TAG_AUDIO, TAG_AUDIO_ALBUMARTIST, N_("Album Artist"), N_("Artist of the album.")},
                                                   {"Audio.AlbumGain", TAG_AUDIO, TAG_AUDIO_ALBUMGAIN, N_("Album Gain"), N_("Gain adjustment of the album.")},
                                                   {"Audio.AlbumPeakGain", TAG_AUDIO, TAG_AUDIO_ALBUMPEAKGAIN, N_("Album Peak Gain"), N_("Peak gain adjustment of album.")},
                                                   {"Audio.AlbumTrackCount", TAG_AUDIO, TAG_AUDIO_ALBUMTRACKCOUNT, N_("Album Track Count"), N_("Total number of tracks on the album.")},
                                                   {"Audio.Album", TAG_AUDIO, TAG_AUDIO_ALBUM, N_("Album"), N_("Name of the album.")},
                                                   {"Audio.Artist", TAG_AUDIO, TAG_AUDIO_ARTIST, N_("Artist"), N_("Artist of the track.")},
                                                   {"Audio.Bitrate", TAG_AUDIO, TAG_AUDIO_BITRATE, N_("Bitrate"), N_("Bitrate in kbps.")},
                                                   {"Audio.Channels", TAG_AUDIO, TAG_AUDIO_CHANNELS, N_("Channels"), N_("Number of channels in the audio (2 = stereo).")},
                                                   {"Audio.CodecVersion", TAG_AUDIO, TAG_AUDIO_CODECVERSION, N_("Codec Version"), N_("Codec version.")},
                                                   {"Audio.Codec", TAG_AUDIO, TAG_AUDIO_CODEC, N_("Codec"), N_("Codec encoding description.")},
                                                   {"Audio.Comment", TAG_AUDIO, TAG_AUDIO_COMMENT, N_("Comment"), N_("Comments on the track.")},
                                                   {"Audio.Copyright", TAG_AUDIO, TAG_AUDIO_COPYRIGHT, N_("Copyright"), N_("Copyright message.")},
                                                   {"Audio.CoverAlbumThumbnailPath", TAG_AUDIO, TAG_AUDIO_COVERALBUMTHUMBNAILPATH, N_("Cover album thumbnail path"), N_("File path to thumbnail image of the cover album.")},
                                                   {"Audio.DiscNo", TAG_AUDIO, TAG_AUDIO_DISCNO, N_("Disc Number"), N_("Specifies which disc the track is on.")},
                                                   {"Audio.Duration", TAG_AUDIO, TAG_AUDIO_DURATION, N_("Duration"), N_("Duration of track in seconds.")},
                                                   {"Audio.Duration.MMSS", TAG_AUDIO, TAG_AUDIO_DURATIONMMSS, N_("Duration [MM:SS]"), N_("Duration of track as MM:SS.")},
                                                   {"Audio.Genre", TAG_AUDIO, TAG_AUDIO_GENRE, N_("Genre"), N_("Type of music classification for the track as defined in ID3 spec.")},
                                                   {"Audio.IsNew", TAG_AUDIO, TAG_AUDIO_ISNEW, N_("Is new"), N_("Set to \"1\" if track is new to the user (default \"0\").")},
                                                   {"Audio.ISRC", TAG_AUDIO, TAG_AUDIO_ISRC, N_("ISRC"), N_("ISRC (international standard recording code).")},
                                                   {"Audio.LastPlay", TAG_AUDIO, TAG_AUDIO_LASTPLAY, N_("Last Play"), N_("When track was last played.")},
                                                   {"Audio.Lyrics", TAG_AUDIO, TAG_AUDIO_LYRICS, N_("Lyrics"), N_("Lyrics of the track.")},
                                                   {"Audio.MBAlbumArtistID", TAG_AUDIO, TAG_AUDIO_MBALBUMARTISTID, N_("MB album artist ID"), N_("MusicBrainz album artist ID in UUID format.")},
                                                   {"Audio.MBAlbumID", TAG_AUDIO, TAG_AUDIO_MBALBUMID, N_("MB Album ID"), N_("MusicBrainz album ID in UUID format.")},
                                                   {"Audio.MBArtistID", TAG_AUDIO, TAG_AUDIO_MBARTISTID, N_("MB Artist ID"), N_("MusicBrainz artist ID in UUID format.")},
                                                   {"Audio.MBTrackID", TAG_AUDIO, TAG_AUDIO_MBTRACKID, N_("MB Track ID"), N_("MusicBrainz track ID in UUID format.")},
                                                   {"Audio.MPEG.ChannelMode", TAG_AUDIO, TAG_AUDIO_MPEG_CHANNELMODE, N_("Channel mode"), N_("MPEG channel mode.")},
                                                   {"Audio.MPEG.Copyrighted", TAG_AUDIO, TAG_AUDIO_MPEG_COPYRIGHTED, N_("Copyrighted"), N_("\"1\" if the copyrighted bit is set.")},
                                                   {"Audio.MPEG.Layer", TAG_AUDIO, TAG_AUDIO_MPEG_LAYER, N_("Layer"), N_("MPEG layer.")},
                                                   {"Audio.MPEG.Original", TAG_AUDIO, TAG_AUDIO_MPEG_ORIGINAL, N_("Original audio"), N_("\"1\" if the \"original\" bit is set.")},
                                                   {"Audio.MPEG.Version", TAG_AUDIO, TAG_AUDIO_MPEG_VERSION, N_("MPEG Version"), N_("MPEG version.")},
                                                   {"Audio.Performer", TAG_AUDIO, TAG_AUDIO_PERFORMER, N_("Performer"), N_("Name of the performer/conductor of the music.")},
                                                   {"Audio.PlayCount", TAG_AUDIO, TAG_AUDIO_PLAYCOUNT, N_("Play Count"), N_("Number of times the track has been played.")},
                                                   {"Audio.ReleaseDate", TAG_AUDIO, TAG_AUDIO_RELEASEDATE, N_("Release Date"), N_("Date track was released.")},
                                                   {"Audio.Samplerate", TAG_AUDIO, TAG_AUDIO_SAMPLERATE, N_("Sample Rate"), N_("Sample rate in Hz.")},
                                                   {"Audio.Title", TAG_AUDIO, TAG_AUDIO_TITLE, N_("Title"), N_("Title of the track.")},
                                                   {"Audio.TrackGain", TAG_AUDIO, TAG_AUDIO_TRACKGAIN, N_("Track Gain"), N_("Gain adjustment of the track.")},
                                                   {"Audio.TrackNo", TAG_AUDIO, TAG_AUDIO_TRACKNO, N_("Track Number"), N_("Position of track on the album.")},
                                                   {"Audio.TrackPeakGain", TAG_AUDIO, TAG_AUDIO_TRACKPEAKGAIN, N_("Track Peak Gain"), N_("Peak gain adjustment of track.")},
                                                   {"Audio.Year", TAG_AUDIO, TAG_AUDIO_YEAR, N_("Year"), N_("Year.")},
                                                   {"Doc.Author", TAG_DOC, TAG_DOC_CREATOR, N_("Author"), N_("Name of the author.")},
                                                   {"Doc.ByteCount", TAG_DOC, TAG_DOC_BYTECOUNT, N_("Byte Count"), N_("Number of bytes in the document.")},
                                                   {"Doc.CaseSensitive", TAG_DOC, TAG_DOC_CASESENSITIVE, N_("Case Sensitive"), N_("Case sensitive.")},
                                                   {"Doc.Category", TAG_DOC, TAG_DOC_CATEGORY, N_("Category"), N_("Category.")},
                                                   {"Doc.CellCount", TAG_DOC, TAG_DOC_CELLCOUNT, N_("Cell Count"), N_("Number of cells in the spreadsheet document.")},
                                                   {"Doc.CharacterCount", TAG_DOC, TAG_DOC_CHARACTERCOUNT, N_("Character Count"), N_("Number of characters in the document.")},
                                                   {"Doc.Codepage", TAG_DOC, TAG_DOC_CODEPAGE, N_("Codepage"), N_("The MS codepage to encode strings for metadata.")},
                                                   {"Doc.Comments", TAG_DOC, TAG_DOC_COMMENTS, N_("Comments"), N_("User definable free text.")},
                                                   {"Doc.Company", TAG_DOC, TAG_DOC_COMPANY, N_("Company"), N_("Organization that the <Doc.Creator> entity is associated with.")},
                                                   {"Doc.Created", TAG_DOC, TAG_DOC_DATECREATED, N_("Created"), N_("Datetime document was originally created.")},
                                                   {"Doc.Creator", TAG_DOC, TAG_DOC_CREATOR, N_("Creator"), N_("An entity primarily responsible for making the content of the resource, typically a person, organization, or service.")},
                                                   {"Doc.DateCreated", TAG_DOC, TAG_DOC_DATECREATED, N_("Date Created"), N_("Date associated with an event in the life cycle of the resource (creation/publication date).")},
                                                   {"Doc.DateModified", TAG_DOC, TAG_DOC_DATEMODIFIED, N_("Date Modified"), N_("The last time the document was saved.")},
                                                   {"Doc.Description", TAG_DOC, TAG_DOC_DESCRIPTION, N_("Description"), N_("An acccount of the content of the resource.")},
                                                   {"Doc.Dictionary", TAG_DOC, TAG_DOC_DICTIONARY, N_("Dictionary"), N_("Dictionary.")},
                                                   {"Doc.EditingDuration", TAG_DOC, TAG_DOC_EDITINGDURATION, N_("Editing Duration"), N_("The total time taken until the last modification.")},
                                                   {"Doc.Generator", TAG_DOC, TAG_DOC_GENERATOR, N_("Generator"), N_("The application that generated this document.")},
                                                   {"Doc.HiddenSlideCount", TAG_DOC, TAG_DOC_HIDDENSLIDECOUNT, N_("Hidden Slide Count"), N_("Number of hidden slides in the presentation document.")},
                                                   {"Doc.ImageCount", TAG_DOC, TAG_DOC_IMAGECOUNT, N_("Image Count"), N_("Number of images in the document.")},
                                                   {"Doc.InitialCreator", TAG_DOC, TAG_DOC_INITIALCREATOR, N_("Initial Creator"), N_("Specifies the name of the person who created the document initially.")},
                                                   {"Doc.Keywords", TAG_DOC, TAG_DOC_KEYWORDS, N_("Keywords"), N_("Searchable, indexable keywords.")},
                                                   {"Doc.Language", TAG_DOC, TAG_DOC_LANGUAGE, N_("Language"), N_("The locale language of the intellectual content of the resource.")},
                                                   {"Doc.LastPrinted", TAG_DOC, TAG_DOC_LASTPRINTED, N_("Last Printed"), N_("The last time this document was printed.")},
                                                   {"Doc.LastSavedBy", TAG_DOC, TAG_DOC_LASTSAVEDBY, N_("Last Saved By"), N_("The entity that made the last change to the document, typically a person, organization, or service.")},
                                                   {"Doc.LineCount", TAG_DOC, TAG_DOC_LINECOUNT, N_("Line Count"), N_("Number of lines in the document.")},
                                                   {"Doc.LinksDirty", TAG_DOC, TAG_DOC_LINKSDIRTY, N_("Links Dirty"), N_("Links dirty.")},
                                                   {"Doc.LocaleSystemDefault", TAG_DOC, TAG_DOC_LOCALESYSTEMDEFAULT, N_("Locale System Default"), N_("Identifier representing the default system locale.")},
                                                   {"Doc.MMClipCount", TAG_DOC, TAG_DOC_MMCLIPCOUNT, N_("Multimedia Clip Count"), N_("Number of multimedia clips in the document.")},
                                                   {"Doc.Manager", TAG_DOC, TAG_DOC_MANAGER, N_("Manager"), N_("Name of the manager of <Doc.Creator> entity.")},
                                                   {"Doc.NoteCount", TAG_DOC, TAG_DOC_NOTECOUNT, N_("Note Count"), N_("Number of \"notes\" in the document.")},
                                                   {"Doc.ObjectCount", TAG_DOC, TAG_DOC_OBJECTCOUNT, N_("Object Count"), N_("Number of objects (OLE and other graphics) in the document.")},
                                                   {"Doc.PageCount", TAG_DOC, TAG_DOC_PAGECOUNT, N_("Page Count"), N_("Number of pages in the document.")},
                                                   {"Doc.ParagraphCount", TAG_DOC, TAG_DOC_PARAGRAPHCOUNT, N_("Paragraph Count"), N_("Number of paragraphs in the document.")},
                                                   {"Doc.PresentationFormat", TAG_DOC, TAG_DOC_PRESENTATIONFORMAT, N_("Presentation Format"), N_("Type of presentation, like \"On-screen Show\", \"SlideView\", etc.")},
                                                   {"Doc.PrintDate", TAG_DOC, TAG_DOC_PRINTDATE, N_("Print Date"), N_("Specifies the date and time when the document was last printed.")},
                                                   {"Doc.PrintedBy", TAG_DOC, TAG_DOC_PRINTEDBY, N_("Printed By"), N_("Specifies the name of the last person who printed the document.")},
                                                   {"Doc.RevisionCount", TAG_DOC, TAG_DOC_REVISIONCOUNT, N_("Revision Count"), N_("Number of revision on the document.")},
                                                   {"Doc.Scale", TAG_DOC, TAG_DOC_SCALE, N_("Scale"), N_("Scale.")},
                                                   {"Doc.Security", TAG_DOC, TAG_DOC_SECURITY, N_("Security"), N_("One of: \"Password protected\", \"Read-only recommended\", \"Read-only enforced\" or \"Locked for annotations\".")},
                                                   {"Doc.SlideCount", TAG_DOC, TAG_DOC_SLIDECOUNT, N_("Slide Count"), N_("Number of slides in the presentation document.")},
                                                   {"Doc.SpreadsheetCount", TAG_DOC, TAG_DOC_SPREADSHEETCOUNT, N_("Spreadsheet Count"), N_("Number of pages in the document.")},
                                                   {"Doc.Subject", TAG_DOC, TAG_DOC_SUBJECT, N_("Subject"), N_("Document subject.")},
                                                   {"Doc.TableCount", TAG_DOC, TAG_DOC_TABLECOUNT, N_("Table Count"), N_("Number of tables in the document.")},
                                                   {"Doc.Template", TAG_DOC, TAG_DOC_TEMPLATE, N_("Template"), N_("The template file that is been used to generate this document.")},
                                                   {"Doc.Title", TAG_DOC, TAG_DOC_TITLE, N_("Title"), N_("Title of the document.")},
                                                   {"Doc.WordCount", TAG_DOC, TAG_DOC_WORDCOUNT, N_("Word Count"), N_("Number of words in the document.")},
                                                   {"Exif.ApertureValue", TAG_EXIF, TAG_EXIF_APERTUREVALUE, N_("Aperture"), N_("The lens aperture. The unit is the APEX value.")},
                                                   {"Exif.Artist", TAG_EXIF, TAG_EXIF_ARTIST, N_("Artist"), N_("Name of the camera owner, photographer or image creator. The detailed format is not specified, but it is recommended that the information be written for ease of Interoperability. When the field is left blank, it is treated as unknown.")},
                                                   {"Exif.BatteryLevel", TAG_EXIF, TAG_EXIF_BATTERYLEVEL, N_("Battery Level"), N_("Battery Level.")},
                                                   {"Exif.BitsPerSample", TAG_EXIF, TAG_EXIF_BITSPERSAMPLE, N_("Bits per Sample"), N_("The number of bits per image component. Each component of the image is 8 bits, so the value for this tag is 8. In JPEG compressed data a JPEG marker is used instead of this tag.")},
                                                   {"Exif.BrightnessValue", TAG_EXIF, TAG_EXIF_BRIGHTNESSVALUE, N_("Brightness"), N_("The value of brightness. The unit is the APEX value. Ordinarily it is given in the range of -99.99 to 99.99.")},
                                                   {"Exif.CFAPattern", TAG_EXIF, TAG_EXIF_CFAPATTERN, N_("CFA Pattern"), N_("The color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used. It does not apply to all sensing methods.")},
                                                   {"Exif.CFARepeatPatternDim", TAG_EXIF, TAG_EXIF_CFAREPEATPATTERNDIM, N_("CFA Repeat Pattern Dim"), N_("CFA Repeat Pattern Dim.")},
                                                   {"Exif.ColorSpace", TAG_EXIF, TAG_EXIF_COLORSPACE, N_("Color Space"), N_("The color space information tag is always recorded as the color space specifier. Normally sRGB (=1) is used to define the color space based on the PC monitor conditions and environment. If a color space other than sRGB is used, Uncalibrated (=FFFF.H) is set. Image data recorded as Uncalibrated can be treated as sRGB when it is converted to FlashPix.")},
                                                   {"Exif.ComponentsConfiguration", TAG_EXIF, TAG_EXIF_COMPONENTSCONFIGURATION, N_("Components Configuration"), N_("Information specific to compressed data. The channels of each component are arranged in order from the 1st component to the 4th. For uncompressed data the data arrangement is given in the <Exif.PhotometricInterpretation> tag. However, since <Exif.PhotometricInterpretation> can only express the order of Y, Cb and Cr, this tag is provided for cases when compressed data uses components other than Y, Cb, and Cr and to enable support of other sequences.")},
                                                   {"Exif.CompressedBitsPerPixel", TAG_EXIF, TAG_EXIF_COMPRESSEDBITSPERPIXEL, N_("Compressed Bits per Pixel"), N_("Information specific to compressed data. The compression mode used for a compressed image is indicated in unit bits per pixel.")},
                                                   {"Exif.Compression", TAG_EXIF, TAG_EXIF_COMPRESSION, N_("Compression"), N_("The compression scheme used for the image data. When a primary image is JPEG compressed, this designation is not necessary and is omitted. When thumbnails use JPEG compression, this tag value is set to 6.")},
                                                   {"Exif.Contrast", TAG_EXIF, TAG_EXIF_CONTRAST, N_("Contrast"), N_("The direction of contrast processing applied by the camera when the image was shot.")},
                                                   {"Exif.Copyright", TAG_EXIF, TAG_EXIF_COPYRIGHT, N_("Copyright"), N_("Copyright information. The tag is used to indicate both the photographer and editor copyrights. It is the copyright notice of the person or organization claiming rights to the image. The Interoperability copyright statement including date and rights should be written in this field; e.g., \"Copyright, John Smith, 19xx. All rights reserved.\". The field records both the photographer and editor copyrights, with each recorded in a separate part of the statement. When there is a clear distinction between the photographer and editor copyrights, these are to be written in the order of photographer followed by editor copyright, separated by NULL (in this case, since the statement also ends with a NULL, there are two NULL codes) (see example 1). When only the photographer is given, it is terminated by one NULL code. When only the editor copyright is given, the photographer copyright part consists of one space followed by a terminating NULL code, then the editor copyright is given. When the field is left blank, it is treated as unknown.")},
                                                   {"Exif.CustomRendered", TAG_EXIF, TAG_EXIF_CUSTOMRENDERED, N_("Custom Rendered"), N_("The use of special processing on image data, such as rendering geared to output. When special processing is performed, the reader is expected to disable or minimize any further processing.")},
                                                   {"Exif.DateTime", TAG_EXIF, TAG_EXIF_DATETIME, N_("Date and Time"), N_("The date and time of image creation.")},
                                                   {"Exif.DateTimeDigitized", TAG_EXIF, TAG_EXIF_DATETIMEDIGITIZED, N_("Date and Time (digitized)"), N_("The date and time when the image was stored as digital data.")},
                                                   {"Exif.DateTimeOriginal", TAG_EXIF, TAG_EXIF_DATETIMEORIGINAL, N_("Date and Time (original)"), N_("The date and time when the original image data was generated. For a digital still camera the date and time the picture was taken are recorded.")},
                                                   {"Exif.DeviceSettingDescription", TAG_EXIF, TAG_EXIF_DEVICESETTINGDESCRIPTION, N_("Device Setting Description"), N_("Information on the picture-taking conditions of a particular camera model. The tag is used only to indicate the picture-taking conditions in the reader.")},
                                                   {"Exif.DigitalZoomRatio", TAG_EXIF, TAG_EXIF_DIGITALZOOMRATIO, N_("Digital Zoom Ratio"), N_("The digital zoom ratio when the image was shot. If the numerator of the recorded value is 0, this indicates that digital zoom was not used.")},
                                                   {"Exif.DocumentName", TAG_EXIF, TAG_EXIF_DOCUMENTNAME, N_("Document Name"), N_("Document Name.")},
                                                   {"Exif.ExifIfdPointer", TAG_EXIF, TAG_EXIF_EXIFIFDPOINTER, N_("Exif IFD Pointer"), N_("A pointer to the Exif IFD. Interoperability, Exif IFD has the same structure as that of the IFD specified in TIFF.")},
                                                   {"Exif.ExifVersion", TAG_EXIF, TAG_EXIF_EXIFVERSION, N_("Exif Version"), N_("The version of Exif standard supported. Nonexistence of this field is taken to mean nonconformance to the standard.")},
                                                   {"Exif.ExposureBiasValue", TAG_EXIF, TAG_EXIF_EXPOSUREBIASVALUE, N_("Exposure Bias"), N_("The exposure bias. The units is the APEX value. Ordinarily it is given in the range of -99.99 to 99.99.")},
                                                   {"Exif.ExposureIndex", TAG_EXIF, TAG_EXIF_EXPOSUREINDEX, N_("Exposure Index"), N_("The exposure index selected on the camera or input device at the time the image is captured.")},
                                                   {"Exif.ExposureMode", TAG_EXIF, TAG_EXIF_EXPOSUREMODE, N_("Exposure Mode"), N_("This tag indicates the exposure mode set when the image was shot. In auto-bracketing mode, the camera shoots a series of frames of the same scene at different exposure settings.")},
                                                   {"Exif.ExposureProgram", TAG_EXIF, TAG_EXIF_EXPOSUREPROGRAM, N_("Exposure Program"), N_("The class of the program used by the camera to set exposure when the picture is taken.")},
                                                   {"Exif.ExposureTime", TAG_EXIF, TAG_EXIF_EXPOSURETIME, N_("Exposure Time"), N_("Exposure time, given in seconds.")},
                                                   {"Exif.FileSource", TAG_EXIF, TAG_EXIF_FILESOURCE, N_("File Source"), N_("Indicates the image source. If a DSC recorded the image, this tag value of this tag always be set to 3, indicating that the image was recorded on a DSC.")},
                                                   {"Exif.FillOrder", TAG_EXIF, TAG_EXIF_FILLORDER, N_("Fill Order"), N_("Fill Order.")},
                                                   {"Exif.Flash", TAG_EXIF, TAG_EXIF_FLASH, N_("Flash"), N_("This tag is recorded when an image is taken using a strobe light (flash).")},
                                                   {"Exif.FlashEnergy", TAG_EXIF, TAG_EXIF_FLASHENERGY, N_("Flash Energy"), N_("Indicates the strobe energy at the time the image is captured, as measured in Beam Candle Power Seconds (BCPS).")},
                                                   {"Exif.FlashPixVersion", TAG_EXIF, TAG_EXIF_FLASHPIXVERSION, N_("FlashPixVersion"), N_("The FlashPix format version supported by a FPXR file.")},
                                                   {"Exif.FNumber", TAG_EXIF, TAG_EXIF_FNUMBER, N_("F Number"), N_("The F number.")},
                                                   {"Exif.FocalLength", TAG_EXIF, TAG_EXIF_FOCALLENGTH, N_("Focal Length"), N_("The actual focal length of the lens, in mm. Conversion is not made to the focal length of a 35 mm film camera.")},
                                                   {"Exif.FocalLengthIn35mmFilm", TAG_EXIF, TAG_EXIF_FOCALLENGTHIN35MMFILM, N_("Focal Length In 35mm Film"), N_("This tag indicates the equivalent focal length assuming a 35mm film camera, in mm. A value of 0 means the focal length is unknown. Note that this tag differs from the <Exif.FocalLength> tag.")},
                                                   {"Exif.FocalPlaneResolutionUnit", TAG_EXIF, TAG_EXIF_FOCALPLANERESOLUTIONUNIT, N_("Focal Plane Resolution Unit"), N_("Indicates the unit for measuring <Exif.FocalPlaneXResolution> and <Exif.FocalPlaneYResolution>. This value is the same as the <Exif.ResolutionUnit>.")},
                                                   {"Exif.FocalPlaneXResolution", TAG_EXIF, TAG_EXIF_FOCALPLANEXRESOLUTION, N_("Focal Plane x-Resolution"), N_("Indicates the number of pixels in the image width (X) direction per <Exif.FocalPlaneResolutionUnit> on the camera focal plane.")},
                                                   {"Exif.FocalPlaneYResolution", TAG_EXIF, TAG_EXIF_FOCALPLANEYRESOLUTION, N_("Focal Plane y-Resolution"), N_("Indicates the number of pixels in the image height (V) direction per <Exif.FocalPlaneResolutionUnit> on the camera focal plane.")},
                                                   {"Exif.GainControl", TAG_EXIF, TAG_EXIF_GAINCONTROL, N_("Gain Control"), N_("This tag indicates the degree of overall image gain adjustment.")},
                                                   {"Exif.Gamma", TAG_EXIF, TAG_EXIF_GAMMA, N_("Gamma"), N_("Indicates the value of coefficient gamma.")},
                                                   {"Exif.GPS.Altitude", TAG_EXIF, TAG_EXIF_GPSALTITUDE, N_("Altitude"), N_("Indicates the altitude based on the reference in <Exif.GPS.AltitudeRef>. The reference unit is meters.")},
                                                   {"Exif.GPS.AltitudeRef", TAG_EXIF, TAG_EXIF_GPSALTITUDEREF, N_("Altitude Reference"), N_("Indicates the altitude used as the reference altitude. If the reference is sea level and the altitude is above sea level, 0 is given. If the altitude is below sea level, a value of 1 is given and the altitude is indicated as an absolute value in the <Exif.GPS.Altitude> tag. The reference unit is meters.")},
                                                   {"Exif.GPS.InfoIFDPointer", TAG_EXIF, TAG_EXIF_GPSINFOIFDPOINTER, N_("GPS Info IFDPointer"), N_("A pointer to the GPS Info IFD. The Interoperability structure of the GPS Info IFD, like that of Exif IFD, has no image data.")},
                                                   {"Exif.GPS.Latitude", TAG_EXIF, TAG_EXIF_GPSLATITUDE, N_("Latitude"), N_("Indicates the latitude. The latitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is dd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is dd/1,mmmm/100,0/1.")},
                                                   {"Exif.GPS.LatitudeRef", TAG_EXIF, TAG_EXIF_GPSLATITUDEREF, N_("North or South Latitude"), N_("Indicates whether the latitude is north or south latitude. The ASCII value 'N' indicates north latitude, and 'S' is south latitude.")},
                                                   {"Exif.GPS.Longitude", TAG_EXIF, TAG_EXIF_GPSLONGITUDE, N_("Longitude"), N_("Indicates the longitude. The longitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is ddd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is ddd/1,mmmm/100,0/1.")},
                                                   {"Exif.GPS.LongitudeRef", TAG_EXIF, TAG_EXIF_GPSLONGITUDEREF, N_("East or West Longitude"), N_("Indicates whether the longitude is east or west longitude. ASCII 'E' indicates east longitude, and 'W' is west longitude.")},
                                                   {"Exif.GPS.VersionID", TAG_EXIF, TAG_EXIF_GPSVERSIONID, N_("GPS tag version"), N_("Indicates the version of <Exif.GPS.InfoIFD>. This tag is mandatory when <Exif.GPS.Info> tag is present.")},
                                                   {"Exif.ImageDescription", TAG_EXIF, TAG_EXIF_IMAGEDESCRIPTION, N_("Image Description"), N_("A character string giving the title of the image. Two-bytes character codes cannot be used. When a 2-bytes code is necessary, the Exif Private tag <Exif.UserComment> is to be used.")},
                                                   {"Exif.ImageLength", TAG_EXIF, TAG_EXIF_IMAGELENGTH, N_("Image Length"), N_("The number of rows of image data. In JPEG compressed data a JPEG marker is used instead of this tag.")},
                                                   {"Exif.ImageResources", TAG_EXIF, TAG_EXIF_IMAGERESOURCES, N_("Image Resources Block"), N_("Image Resources Block.")},
                                                   {"Exif.ImageUniqueID", TAG_EXIF, TAG_EXIF_IMAGEUNIQUEID, N_("Image Unique ID"), N_("This tag indicates an identifier assigned uniquely to each image. It is recorded as an ASCII string equivalent to hexadecimal notation and 128-bit fixed length.")},
                                                   {"Exif.ImageWidth", TAG_EXIF, TAG_EXIF_IMAGEWIDTH, N_("Image Width"), N_("The number of columns of image data, equal to the number of pixels per row. In JPEG compressed data a JPEG marker is used instead of this tag.")},
                                                   {"Exif.InterColorProfile", TAG_EXIF, TAG_EXIF_INTERCOLORPROFILE, N_("Inter Color Profile"), N_("Inter Color Profile.")},
                                                   {"Exif.InteroperabilityIFDPointer", TAG_EXIF, TAG_EXIF_INTEROPERABILITYIFDPOINTER, N_("Interoperability IFD Pointer"), N_("Interoperability IFD is composed of tags which stores the information to ensure the Interoperability and pointed by the following tag located in Exif IFD. The Interoperability structure of Interoperability IFD is the same as TIFF defined IFD structure but does not contain the image data characteristically compared with normal TIFF IFD.")},
                                                   {"Exif.InteroperabilityIndex", TAG_EXIF, TAG_EXIF_INTEROPERABILITYINDEX, N_("Interoperability Index"), N_("Indicates the identification of the Interoperability rule. Use \"R98\" for stating ExifR98 Rules. Four bytes used including the termination code (NULL).")},
                                                   {"Exif.InteroperabilityVersion", TAG_EXIF, TAG_EXIF_INTEROPERABILITYVERSION, N_("Interoperability Version"), N_("Interoperability Version.")},
                                                   {"Exif.IPTC_NAA", TAG_EXIF, TAG_EXIF_IPTCNAA, N_("IPTC/NAA"), N_("IPTC/NAA")},
                                                   {"Exif.ISOSpeedRatings", TAG_EXIF, TAG_EXIF_ISOSPEEDRATINGS, N_("ISO Speed Ratings"), N_("Indicates the ISO Speed and ISO Latitude of the camera or input device as specified in ISO 12232.")},
                                                   {"Exif.JPEGInterchangeFormat", TAG_EXIF, TAG_EXIF_JPEGINTERCHANGEFORMAT, N_("JPEG Interchange Format"), N_("The offset to the start byte (SOI) of JPEG compressed thumbnail data. This is not used for primary image JPEG data.")},
                                                   {"Exif.JPEGInterchangeFormatLength", TAG_EXIF, TAG_EXIF_JPEGINTERCHANGEFORMATLENGTH, N_("JPEG Interchange Format Length"), N_("The number of bytes of JPEG compressed thumbnail data. This is not used for primary image JPEG data. JPEG thumbnails are not divided but are recorded as a continuous JPEG bitstream from SOI to EOI. Appn and COM markers should not be recorded. Compressed thumbnails must be recorded in no more than 64 Kbytes, including all other data to be recorded in APP1.")},
                                                   {"Exif.JPEGProc", TAG_EXIF, TAG_EXIF_JPEGPROC, N_("JPEG Proc"), N_("JPEG Procedure")},
                                                   {"Exif.LightSource", TAG_EXIF, TAG_EXIF_LIGHTSOURCE, N_("Light Source"), N_("The kind of light source.")},
                                                   {"Exif.Make", TAG_EXIF, TAG_EXIF_MAKE, N_("Manufacturer"), N_("The manufacturer of the recording equipment. This is the manufacturer of the DSC, scanner, video digitizer or other equipment that generated the image. When the field is left blank, it is treated as unknown.")},
                                                   {"Exif.MakerNote", TAG_EXIF, TAG_EXIF_MAKERNOTE, N_("Maker Note"), N_("A tag for manufacturers of Exif writers to record any desired information. The contents are up to the manufacturer.")},
                                                   {"Exif.MaxApertureValue", TAG_EXIF, TAG_EXIF_MAXAPERTUREVALUE, N_("Max Aperture Value"), N_("The smallest F number of the lens. The unit is the APEX value. Ordinarily it is given in the range of 00.00 to 99.99, but it is not limited to this range.")},
                                                   {"Exif.MeteringMode", TAG_EXIF, TAG_EXIF_METERINGMODE, N_("Metering Mode"), N_("The metering mode.")},
                                                   {"Exif.Model", TAG_EXIF, TAG_EXIF_MODEL, N_("Model"), N_("The model name or model number of the equipment. This is the model name or number of the DSC, scanner, video digitizer or other equipment that generated the image. When the field is left blank, it is treated as unknown.")},
                                                   {"Exif.CFAPattern", TAG_EXIF, TAG_EXIF_NEWCFAPATTERN, N_("CFA Pattern"), N_("Indicates the color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used. It does not apply to all sensing methods.")},
                                                   {"Exif.NewSubfileType", TAG_EXIF, TAG_EXIF_NEWSUBFILETYPE, N_("New Subfile Type"), N_("A general indication of the kind of data contained in this subfile.")},
                                                   {"Exif.OECF", TAG_EXIF, TAG_EXIF_OECF, N_("OECF"), N_("Indicates the Opto-Electoric Conversion Function (OECF) specified in ISO 14524. <Exif.OECF> is the relationship between the camera optical input and the image values.")},
                                                   {"Exif.Orientation", TAG_EXIF, TAG_EXIF_ORIENTATION, N_("Orientation"), N_("The image orientation viewed in terms of rows and columns.")},
                                                   {"Exif.PhotometricInterpretation", TAG_EXIF, TAG_EXIF_PHOTOMETRICINTERPRETATION, N_("Photometric Interpretation"), N_("The pixel composition. In JPEG compressed data a JPEG marker is used instead of this tag.")},
                                                   {"Exif.PixelXDimension", TAG_EXIF, TAG_EXIF_PIXELXDIMENSION, N_("Pixel X Dimension"), N_("Information specific to compressed data. When a compressed file is recorded, the valid width of the meaningful image must be recorded in this tag, whether or not there is padding data or a restart marker. This tag should not exist in an uncompressed file.")},
                                                   {"Exif.PixelYDimension", TAG_EXIF, TAG_EXIF_PIXELYDIMENSION, N_("Pixel Y Dimension"), N_("Information specific to compressed data. When a compressed file is recorded, the valid height of the meaningful image must be recorded in this tag, whether or not there is padding data or a restart marker. This tag should not exist in an uncompressed file. Since data padding is unnecessary in the vertical direction, the number of lines recorded in this valid image height tag will in fact be the same as that recorded in the SOF.")},
                                                   {"Exif.PlanarConfiguration", TAG_EXIF, TAG_EXIF_PLANARCONFIGURATION, N_("Planar Configuration"), N_("Indicates whether pixel components are recorded in a chunky or planar format. In JPEG compressed files a JPEG marker is used instead of this tag. If this field does not exist, the TIFF default of 1 (chunky) is assumed.")},
                                                   {"Exif.PrimaryChromaticities", TAG_EXIF, TAG_EXIF_PRIMARYCHROMATICITIES, N_("Primary Chromaticities"), N_("The chromaticity of the three primary colors of the image. Normally this tag is not necessary, since colorspace is specified in <Exif.ColorSpace> tag.")},
                                                   {"Exif.ReferenceBlackWhite", TAG_EXIF, TAG_EXIF_REFERENCEBLACKWHITE, N_("Reference Black/White"), N_("The reference black point value and reference white point value. No defaults are given in TIFF, but the values below are given as defaults here. The color space is declared in a color space information tag, with the default being the value that gives the optimal image characteristics Interoperability these conditions.")},
                                                   {"Exif.RelatedImageFileFormat", TAG_EXIF, TAG_EXIF_RELATEDIMAGEFILEFORMAT, N_("Related Image File Format"), N_("Related Image File Format.")},
                                                   {"Exif.RelatedImageLength", TAG_EXIF, TAG_EXIF_RELATEDIMAGELENGTH, N_("Related Image Length"), N_("Related Image Length.")},
                                                   {"Exif.RelatedImageWidth", TAG_EXIF, TAG_EXIF_RELATEDIMAGEWIDTH, N_("Related Image Width"), N_("Related Image Width.")},
                                                   {"Exif.RelatedSoundFile", TAG_EXIF, TAG_EXIF_RELATEDSOUNDFILE, N_("Related Sound File"), N_("This tag is used to record the name of an audio file related to the image data. The only relational information recorded here is the Exif audio file name and extension (an ASCII string consisting of 8 characters + '.' + 3 characters). The path is not recorded. When using this tag, audio files must be recorded in conformance to the Exif audio format. Writers are also allowed to store the data such as Audio within APP2 as FlashPix extension stream data. Audio files must be recorded in conformance to the Exif audio format. If multiple files are mapped to one file, the above format is used to record just one audio file name. If there are multiple audio files, the first recorded file is given. When there are three Exif audio files \"SND00001.WAV\", \"SND00002.WAV\" and \"SND00003.WAV\", the Exif image file name for each of them, \"DSC00001.JPG\", is indicated. By combining multiple relational information, a variety of playback possibilities can be supported. The method of using relational information is left to the implementation on the playback side. Since this information is an ASCII character string, it is terminated by NULL. When this tag is used to map audio files, the relation of the audio file to image data must also be indicated on the audio file end.")},
                                                   {"Exif.ResolutionUnit", TAG_EXIF, TAG_EXIF_RESOLUTIONUNIT, N_("Resolution Unit"), N_("The unit for measuring <Exif.XResolution> and <Exif.YResolution>. The same unit is used for both <Exif.XResolution> and <Exif.YResolution>. If the image resolution is unknown, 2 (inches) is designated.")},
                                                   {"Exif.RowsPerStrip", TAG_EXIF, TAG_EXIF_ROWSPERSTRIP, N_("Rows per Strip"), N_("The number of rows per strip. This is the number of rows in the image of one strip when an image is divided into strips. With JPEG compressed data this designation is not needed and is omitted.")},
                                                   {"Exif.SamplesPerPixel", TAG_EXIF, TAG_EXIF_SAMPLESPERPIXEL, N_("Samples per Pixel"), N_("The number of components per pixel. Since this standard applies to RGB and YCbCr images, the value set for this tag is 3. In JPEG compressed data a JPEG marker is used instead of this tag.")},
                                                   {"Exif.Saturation", TAG_EXIF, TAG_EXIF_SATURATION, N_("Saturation"), N_("This tag indicates the direction of saturation processing applied by the camera when the image was shot.")},
                                                   {"Exif.SceneCaptureType", TAG_EXIF, TAG_EXIF_SCENECAPTURETYPE, N_("Scene Capture Type"), N_("This tag indicates the type of scene that was shot. It can also be used to record the mode in which the image was shot. Note that this differs from <Exif.SceneType> tag.")},
                                                   {"Exif.SceneType", TAG_EXIF, TAG_EXIF_SCENETYPE, N_("Scene Type"), N_("Indicates the type of scene. If a DSC recorded the image, this tag value must always be set to 1, indicating that the image was directly photographed.")},
                                                   {"Exif.SensingMethod", TAG_EXIF, TAG_EXIF_SENSINGMETHOD, N_("Sensing Method"), N_("Indicates the image sensor type on the camera or input device.")},
                                                   {"Exif.Sharpness", TAG_EXIF, TAG_EXIF_SHARPNESS, N_("Sharpness"), N_("This tag indicates the direction of sharpness processing applied by the camera when the image was shot.")},
                                                   {"Exif.ShutterSpeedValue", TAG_EXIF, TAG_EXIF_SHUTTERSPEEDVALUE, N_("Shutter speed"), N_("Shutter speed. The unit is the APEX (Additive System of Photographic Exposure) setting.")},
                                                   {"Exif.Software", TAG_EXIF, TAG_EXIF_SOFTWARE, N_("Software"), N_("This tag records the name and version of the software or firmware of the camera or image input device used to generate the image. When the field is left blank, it is treated as unknown.")},
                                                   {"Exif.SpatialFrequencyResponse", TAG_EXIF, TAG_EXIF_SPATIALFREQUENCYRESPONSE, N_("Spatial Frequency Response"), N_("This tag records the camera or input device spatial frequency table and SFR values in the direction of image width, image height, and diagonal direction, as specified in ISO 12233.")},
                                                   {"Exif.SpectralSensitivity", TAG_EXIF, TAG_EXIF_SPECTRALSENSITIVITY, N_("Spectral Sensitivity"), N_("Indicates the spectral sensitivity of each channel of the camera used.")},
                                                   {"Exif.StripByteCounts", TAG_EXIF, TAG_EXIF_STRIPBYTECOUNTS, N_("Strip Byte Count"), N_("The total number of bytes in each strip. With JPEG compressed data this designation is not needed and is omitted.")},
                                                   {"Exif.StripOffsets", TAG_EXIF, TAG_EXIF_STRIPOFFSETS, N_("Strip Offsets"), N_("For each strip, the byte offset of that strip. It is recommended that this be selected so the number of strip bytes does not exceed 64 Kbytes. With JPEG compressed data this designation is not needed and is omitted.")},
                                                   {"Exif.SubIFDs", TAG_EXIF, TAG_EXIF_SUBIFDS, N_("Sub IFD Offsets"), N_("Defined by Adobe Corporation to enable TIFF Trees within a TIFF file.")},
                                                   {"Exif.SubjectArea", TAG_EXIF, TAG_EXIF_SUBJECTAREA, N_("Subject Area"), N_("This tag indicates the location and area of the main subject in the overall scene.")},
                                                   {"Exif.SubjectDistance", TAG_EXIF, TAG_EXIF_SUBJECTDISTANCE, N_("Subject Distance"), N_("The distance to the subject, given in meters.")},
                                                   {"Exif.SubjectDistanceRange", TAG_EXIF, TAG_EXIF_SUBJECTDISTANCERANGE, N_("Subject Distance Range"), N_("This tag indicates the distance to the subject.")},
                                                   {"Exif.SubjectLocation", TAG_EXIF, TAG_EXIF_SUBJECTLOCATION, N_("Subject Location"), N_("Indicates the location of the main subject in the scene. The value of this tag represents the pixel at the center of the main subject relative to the left edge, prior to rotation processing as per the <Exif.Rotation> tag. The first value indicates the X column number and second indicates the Y row number.")},
                                                   {"Exif.SubsecTime", TAG_EXIF, TAG_EXIF_SUBSECTIME, N_("Subsec Time"), N_("A tag used to record fractions of seconds for the <Exif.DateTime> tag.")},
                                                   {"Exif.SubSecTimeDigitized", TAG_EXIF, TAG_EXIF_SUBSECTIMEDIGITIZED, N_("Subsec Time Digitized"), N_("A tag used to record fractions of seconds for the <Exif.DateTimeDigitized> tag.")},
                                                   {"Exif.SubSecTimeOriginal", TAG_EXIF, TAG_EXIF_SUBSECTIMEORIGINAL, N_("Subsec Time Original"), N_("A tag used to record fractions of seconds for the <Exif.DateTimeOriginal> tag.")},
                                                   {"Exif.TIFF/EPStandardID", TAG_EXIF, TAG_EXIF_TIFFEPSTANDARDID, N_("TIFF/EP Standard ID"), N_("TIFF/EP Standard ID.")},
                                                   {"Exif.TransferFunction", TAG_EXIF, TAG_EXIF_TRANSFERFUNCTION, N_("Transfer Function"), N_("A transfer function for the image, described in tabular style. Normally this tag is not necessary, since color space is specified in <Exif.ColorSpace> tag.")},
                                                   {"Exif.TransferRange", TAG_EXIF, TAG_EXIF_TRANSFERRANGE, N_("Transfer Range"), N_("Transfer Range.")},
                                                   {"Exif.UserComment", TAG_EXIF, TAG_EXIF_USERCOMMENT, N_("User Comment"), N_("A tag for Exif users to write keywords or comments on the image besides those in <Exif.ImageDescription>, and without the character code limitations of the <Exif.ImageDescription> tag. The character code used in the <Exif.UserComment> tag is identified based on an ID code in a fixed 8-byte area at the start of the tag data area. The unused portion of the area is padded with NULL (\"00.h\"). ID codes are assigned by means of registration. The value of CountN is determinated based on the 8 bytes in the character code area and the number of bytes in the user comment part. Since the TYPE is not ASCII, NULL termination is not necessary. The ID code for the <Exif.UserComment> area may be a Defined code such as JIS or ASCII, or may be Undefined. The Undefined name is UndefinedText, and the ID code is filled with 8 bytes of all \"NULL\" (\"00.H\"). An Exif reader that reads the <Exif.UserComment> tag must have a function for determining the ID code. This function is not required in Exif readers that do not use the <Exif.UserComment> tag. When a <Exif.UserComment> area is set aside, it is recommended that the ID code be ASCII and that the following user comment part be filled with blank characters [20.H].")},
                                                   {"Exif.WhiteBalance", TAG_EXIF, TAG_EXIF_WHITEBALANCE, N_("White Balance"), N_("The white balance mode set when the image was shot.")},
                                                   {"Exif.WhitePoint", TAG_EXIF, TAG_EXIF_WHITEPOINT, N_("White Point"), N_("The chromaticity of the white point of the image. Normally this tag is not necessary, since color space is specified in <Exif.ColorSpace> tag.")},
                                                   {"Exif.XMLPacket", TAG_EXIF, TAG_EXIF_XMLPACKET, N_("XML Packet"), N_("XMP Metadata.")},
                                                   {"Exif.XResolution", TAG_EXIF, TAG_EXIF_XRESOLUTION, N_("x Resolution"), N_("The number of pixels per <Exif.ResolutionUnit> in the <Exif.ImageWidth> direction. When the image resolution is unknown, 72 [dpi] is designated.")},
                                                   {"Exif.YCbCrCoefficients", TAG_EXIF, TAG_EXIF_YCBCRCOEFFICIENTS, N_("YCbCr Coefficients"), N_("The matrix coefficients for transformation from RGB to YCbCr image data. No default is given in TIFF; but here \"Color Space Guidelines\" is used as the default. The color space is declared in a color space information tag, with the default being the value that gives the optimal image characteristics Interoperability this condition.")},
                                                   {"Exif.YCbCrPositioning", TAG_EXIF, TAG_EXIF_YCBCRPOSITIONING, N_("YCbCr Positioning"), N_("The position of chrominance components in relation to the luminance component. This field is designated only for JPEG compressed data or uncompressed YCbCr data. The TIFF default is 1 (centered); but when Y:Cb:Cr = 4:2:2 it is recommended that 2 (co-sited) be used to record data, in order to improve the image quality when viewed on TV systems. When this field does not exist, the reader shall assume the TIFF default. In the case of Y:Cb:Cr = 4:2:0, the TIFF default (centered) is recommended. If the reader does not have the capability of supporting both kinds of <Exif.YCbCrPositioning>, it shall follow the TIFF default regardless of the value in this field. It is preferable that readers be able to support both centered and co-sited positioning.")},
                                                   {"Exif.YCbCrSubSampling", TAG_EXIF, TAG_EXIF_YCBCRSUBSAMPLING, N_("YCbCr Sub-Sampling"), N_("The sampling ratio of chrominance components in relation to the luminance component. In JPEG compressed data a JPEG marker is used instead of this tag.")},
                                                   {"Exif.YResolution", TAG_EXIF, TAG_EXIF_YRESOLUTION, N_("y Resolution"), N_("The number of pixels per <Exif.ResolutionUnit> in the <Exif.ImageLength> direction. The same value as <Exif.XResolution> is designated.")},
                                                   {"File.Accessed", TAG_FILE, TAG_FILE_ACCESSED, N_("Accessed"), N_("Last access datetime.")},
                                                   {"File.Content", TAG_FILE, TAG_FILE_CONTENT, N_("Content"), N_("File's contents filtered as plain text.")},
                                                   {"File.Description", TAG_FILE, TAG_FILE_DESCRIPTION, N_("Description"), N_("Editable free text/notes.")},
                                                   {"File.Format", TAG_FILE, TAG_FILE_FORMAT, N_("Format"), N_("MIME type of the file or if a directory it should contain value \"Folder\"")},
                                                   {"File.Keywords", TAG_FILE, TAG_FILE_KEYWORDS, N_("Keywords"), N_("Editable array of keywords.")},
                                                   {"File.Link", TAG_FILE, TAG_FILE_LINK, N_("Link"), N_("URI of link target.")},
                                                   {"File.Modified", TAG_FILE, TAG_FILE_MODIFIED, N_("Modified"), N_("Last modified datetime.")},
                                                   {"File.Name", TAG_FILE, TAG_FILE_NAME, N_("Name"), N_("File name excluding path but including the file extension.")},
                                                   {"File.Path", TAG_FILE, TAG_FILE_PATH, N_("Path"), N_("Full file path of file excluding the filename.")},
                                                   {"File.Permissions", TAG_FILE, TAG_FILE_PERMISSIONS, N_("Permissions"), N_("Permission string in unix format eg \"-rw-r--r--\".")},
                                                   {"File.Publisher", TAG_FILE, TAG_FILE_PUBLISHER, N_("Publisher"), N_("Editable DC type for the name of the publisher of the file (EG dc:publisher field in RSS feed).")},
                                                   {"File.Rank", TAG_FILE, TAG_FILE_RANK, N_("Rank"), N_("Editable file rank for grading favourites. Value should be in the range 1..10.")},
                                                   {"File.Size", TAG_FILE, TAG_FILE_SIZE, N_("Size"), N_("Size of the file in bytes or if a directory number of items it contains.")},
                                                   {"ID3.AlbumSortOrder", TAG_ID3, TAG_ID3_ALBUMSORTORDER, N_("Album Sort Order"), N_("String which should be used instead of the album name for sorting purposes.")},
                                                   {"ID3.AudioCrypto", TAG_ID3, TAG_ID3_AUDIOCRYPTO, N_("Audio Encryption"), N_("Frame indicates if the audio stream is encrypted, and by whom.")},
                                                   {"ID3.AudioSeekPoint", TAG_ID3, TAG_ID3_AUDIOSEEKPOINT, N_("Audio Seek Point"), N_("Fractional offset within the audio data, providing a starting point from which to find an appropriate point to start decoding.")},
                                                   {"ID3.Band", TAG_ID3, TAG_ID3_BAND, N_("Band"), N_("Additional information about the performers in the recording.")},
                                                   {"ID3.BPM", TAG_ID3, TAG_ID3_BPM, N_("BPM"), N_("BPM (beats per minute).")},
                                                   {"ID3.BufferSize", TAG_ID3, TAG_ID3_BUFFERSIZE, N_("Buffer Size"), N_("Recommended buffer size.")},
                                                   {"ID3.CDID", TAG_ID3, TAG_ID3_CDID, N_("CD ID"), N_("Music CD identifier.")},
                                                   {"ID3.Commercial", TAG_ID3, TAG_ID3_COMMERCIAL, N_("Commercial"), N_("Commercial frame.")},
                                                   {"ID3.Composer", TAG_ID3, TAG_ID3_COMPOSER, N_("Composer"), N_("Composer.")},
                                                   {"ID3.Conductor", TAG_ID3, TAG_ID3_CONDUCTOR, N_("Conductor"), N_("Conductor.")},
                                                   {"ID3.ContentGroup", TAG_ID3, TAG_ID3_CONTENTGROUP, N_("Content Group"), N_("Content group description.")},
                                                   {"ID3.ContentType", TAG_ID3, TAG_ID3_CONTENTTYPE, N_("Content Type"), N_("Type of music classification for the track as defined in ID3 spec.")},
                                                   {"ID3.Copyright", TAG_ID3, TAG_ID3_COPYRIGHT, N_("Copyright"), N_("Copyright message.")},
                                                   {"ID3.CryptoReg", TAG_ID3, TAG_ID3_CRYPTOREG, N_("Encryption Registration"), N_("Encryption method registration.")},
                                                   {"ID3.Date", TAG_ID3, TAG_ID3_DATE, N_("Date"), N_("Date.")},
                                                   {"ID3.Emphasis", TAG_ID3, TAG_ID3_EMPHASIS, N_("Emphasis"), N_("Emphasis.")},
                                                   {"ID3.EncodedBy", TAG_ID3, TAG_ID3_ENCODEDBY, N_("Encoded By"), N_("Person or organisation that encoded the audio file. This field may contain a copyright message, if the audio file also is copyrighted by the encoder.")},
                                                   {"ID3.EncoderSettings", TAG_ID3, TAG_ID3_ENCODERSETTINGS, N_("Encoder Settings"), N_("Software.")},
                                                   {"ID3.EncodingTime", TAG_ID3, TAG_ID3_ENCODINGTIME, N_("Encoding Time"), N_("Encoding time.")},
                                                   {"ID3.Equalization", TAG_ID3, TAG_ID3_EQUALIZATION, N_("Equalization"), N_("Equalization.")},
                                                   {"ID3.Equalization2", TAG_ID3, TAG_ID3_EQUALIZATION2, N_("Equalization 2"), N_("Equalisation curve predifine within the audio file.")},
                                                   {"ID3.EventTiming", TAG_ID3, TAG_ID3_EVENTTIMING, N_("Event Timing"), N_("Event timing codes.")},
                                                   {"ID3.FileOwner", TAG_ID3, TAG_ID3_FILEOWNER, N_("File Owner"), N_("File owner.")},
                                                   {"ID3.FileType", TAG_ID3, TAG_ID3_FILETYPE, N_("File Type"), N_("File type.")},
                                                   {"ID3.Frames", TAG_ID3, TAG_ID3_FRAMES, N_("Frames"), N_("Number of frames.")},
                                                   {"ID3.GeneralObject", TAG_ID3, TAG_ID3_GENERALOBJECT, N_("General Object"), N_("General encapsulated object.")},
                                                   {"ID3.GroupingReg", TAG_ID3, TAG_ID3_GROUPINGREG, N_("Grouping Registration"), N_("Group identification registration.")},
                                                   {"ID3.InitialKey", TAG_ID3, TAG_ID3_INITIALKEY, N_("Initial Key"), N_("Initial key.")},
                                                   {"ID3.InvolvedPeople", TAG_ID3, TAG_ID3_INVOLVEDPEOPLE, N_("Involved People"), N_("Involved people list.")},
                                                   {"ID3.InvolvedPeople2", TAG_ID3, TAG_ID3_INVOLVEDPEOPLE2, N_("InvolvedPeople2"), N_("Involved people list.")},
                                                   {"ID3.Language", TAG_ID3, TAG_ID3_LANGUAGE, N_("Language"), N_("Language.")},
                                                   {"ID3.LinkedInfo", TAG_ID3, TAG_ID3_LINKEDINFO, N_("Linked Info"), N_("Linked information.")},
                                                   {"ID3.Lyricist", TAG_ID3, TAG_ID3_LYRICIST, N_("Lyricist"), N_("Lyricist.")},
                                                   {"ID3.MediaType", TAG_ID3, TAG_ID3_MEDIATYPE, N_("Media Type"), N_("Media type.")},
                                                   {"ID3.MixArtist", TAG_ID3, TAG_ID3_MIXARTIST, N_("Mix Artist"), N_("Interpreted, remixed, or otherwise modified by.")},
                                                   {"ID3.Mood", TAG_ID3, TAG_ID3_MOOD, N_("Mood"), N_("Mood.")},
                                                   {"ID3.MPEG.Lookup", TAG_ID3, TAG_ID3_MPEGLOOKUP, N_("MPEG Lookup"), N_("MPEG location lookup table.")},
                                                   {"ID3.MusicianCreditList", TAG_ID3, TAG_ID3_MUSICIANCREDITLIST, N_("Musician Credit List"), N_("Musician credits list.")},
                                                   {"ID3.NetRadioOwner", TAG_ID3, TAG_ID3_NETRADIOOWNER, N_("Net Radio Owner"), N_("Internet radio station owner.")},
                                                   {"ID3.NetRadiostation", TAG_ID3, TAG_ID3_NETRADIOSTATION, N_("Net Radiostation"), N_("Internet radio station name.")},
                                                   {"ID3.OriginalAlbum", TAG_ID3, TAG_ID3_ORIGALBUM, N_("Original Album"), N_("Original album.")},
                                                   {"ID3.OriginalArtist", TAG_ID3, TAG_ID3_ORIGARTIST, N_("Original Artist"), N_("Original artist.")},
                                                   {"ID3.OriginalFileName", TAG_ID3, TAG_ID3_ORIGFILENAME, N_("Original File Name"), N_("Original filename.")},
                                                   {"ID3.OriginalLyricist", TAG_ID3, TAG_ID3_ORIGLYRICIST, N_("Original Lyricist"), N_("Original lyricist.")},
                                                   {"ID3.OriginalReleaseTime", TAG_ID3, TAG_ID3_ORIGRELEASETIME, N_("Original Release Time"), N_("Original release time.")},
                                                   {"ID3.OriginalYear", TAG_ID3, TAG_ID3_ORIGYEAR, N_("Original Year"), N_("Original release year.")},
                                                   {"ID3.Ownership", TAG_ID3, TAG_ID3_OWNERSHIP, N_("Ownership"), N_("Ownership frame.")},
                                                   {"ID3.PartInSet", TAG_ID3, TAG_ID3_PARTINSET, N_("Part of a Set"), N_("Part of a set the audio came from.")},
                                                   {"ID3.PerformerSortOrder", TAG_ID3, TAG_ID3_PERFORMERSORTORDER, N_("Performer Sort Order"), N_("Performer sort order.")},
                                                   {"ID3.Picture", TAG_ID3, TAG_ID3_PICTURE, N_("Picture"), N_("Attached picture.")},
                                                   {"ID3.PlayCounter", TAG_ID3, TAG_ID3_PLAYCOUNTER, N_("Play Counter"), N_("Number of times a file has been played.")},
                                                   {"ID3.PlaylistDelay", TAG_ID3, TAG_ID3_PLAYLISTDELAY, N_("Playlist Delay"), N_("Playlist delay.")},
                                                   {"ID3.Popularimeter", TAG_ID3, TAG_ID3_POPULARIMETER, N_("Popularimeter"), N_("Rating of the audio file.")},
                                                   {"ID3.PositionSync", TAG_ID3, TAG_ID3_POSITIONSYNC, N_("Position Sync"), N_("Position synchronisation frame.")},
                                                   {"ID3.Private", TAG_ID3, TAG_ID3_PRIVATE, N_("Private"), N_("Private frame.")},
                                                   {"ID3.ProducedNotice", TAG_ID3, TAG_ID3_PRODUCEDNOTICE, N_("Produced Notice"), N_("Produced notice.")},
                                                   {"ID3.Publisher", TAG_ID3, TAG_ID3_PUBLISHER, N_("Publisher"), N_("Publisher.")},
                                                   {"ID3.RecordingDates", TAG_ID3, TAG_ID3_RECORDINGDATES, N_("Recording Dates"), N_("Recording dates.")},
                                                   {"ID3.RecordingTime", TAG_ID3, TAG_ID3_RECORDINGTIME, N_("Recording Time"), N_("Recording time.")},
                                                   {"ID3.ReleaseTime", TAG_ID3, TAG_ID3_RELEASETIME, N_("Release Time"), N_("Release time.")},
                                                   {"ID3.Reverb", TAG_ID3, TAG_ID3_REVERB, N_("Reverb"), N_("Reverb.")},
                                                   {"ID3.SetSubtitle", TAG_ID3, TAG_ID3_SETSUBTITLE, N_("Set Subtitle"), N_("Subtitle of the part of a set this track belongs to.")},
                                                   {"ID3.Signature", TAG_ID3, TAG_ID3_SIGNATURE, N_("Signature"), N_("Signature frame.")},
                                                   {"ID3.Size", TAG_ID3, TAG_ID3_SIZE, N_("Size"), N_("Size of the audio file in bytes, excluding the ID3 tag.")},
                                                   {"ID3.SongLength", TAG_ID3, TAG_ID3_SONGLEN, N_("Song length"), N_("Length of the song in milliseconds.")},
                                                   {"ID3.Subtitle", TAG_ID3, TAG_ID3_SUBTITLE, N_("Subtitle"), N_("Subtitle.")},
                                                   {"ID3.Syncedlyrics", TAG_ID3, TAG_ID3_SYNCEDLYRICS, N_("Syncedlyrics"), N_("Synchronized lyric.")},
                                                   {"ID3.SyncedTempo", TAG_ID3, TAG_ID3_SYNCEDTEMPO, N_("Synchronized Tempo"), N_("Synchronized tempo codes.")},
                                                   {"ID3.TaggingTime", TAG_ID3, TAG_ID3_TAGGINGTIME, N_("Tagging Time"), N_("Tagging time.")},
                                                   {"ID3.TermsOfUse", TAG_ID3, TAG_ID3_TERMSOFUSE, N_("Terms Of Use"), N_("Terms of use.")},
                                                   {"ID3.Time", TAG_ID3, TAG_ID3_TIME, N_("Time"), N_("Time.")},
                                                   {"ID3.Titlesortorder", TAG_ID3, TAG_ID3_TITLESORTORDER, N_("Titlesortorder"), N_("Title sort order.")},
                                                   {"ID3.UniqueFileID", TAG_ID3, TAG_ID3_UNIQUEFILEID, N_("Unique File ID"), N_("Unique file identifier.")},
                                                   {"ID3.UnsyncedLyrics", TAG_ID3, TAG_ID3_UNSYNCEDLYRICS, N_("Unsynchronized Lyrics"), N_("Unsynchronized lyric.")},
                                                   {"ID3.UserText", TAG_ID3, TAG_ID3_USERTEXT, N_("User Text"), N_("User defined text information.")},
                                                   {"ID3.VolumeAdj", TAG_ID3, TAG_ID3_VOLUMEADJ, N_("Volume Adjustment"), N_("Relative volume adjustment.")},
                                                   {"ID3.VolumeAdj2", TAG_ID3, TAG_ID3_VOLUMEADJ2, N_("Volume Adjustment 2"), N_("Relative volume adjustment.")},
                                                   {"ID3.WWWArtist", TAG_ID3, TAG_ID3_WWWARTIST, N_("WWW Artist"), N_("Official artist.")},
                                                   {"ID3.WWWAudioFile", TAG_ID3, TAG_ID3_WWWAUDIOFILE, N_("WWW Audio File"), N_("Official audio file webpage.")},
                                                   {"ID3.WWWAudioSource", TAG_ID3, TAG_ID3_WWWAUDIOSOURCE, N_("WWW Audio Source"), N_("Official audio source webpage.")},
                                                   {"ID3.WWWCommercialInfo", TAG_ID3, TAG_ID3_WWWCOMMERCIALINFO, N_("WWW Commercial Info"), N_("URL pointing at a webpage containing commercial information.")},
                                                   {"ID3.WWWCopyright", TAG_ID3, TAG_ID3_WWWCOPYRIGHT, N_("WWW Copyright"), N_("URL pointing at a webpage that holds copyright.")},
                                                   {"ID3.WWWPayment", TAG_ID3, TAG_ID3_WWWPAYMENT, N_("WWW Payment"), N_("URL pointing at a webpage that will handle the process of paying for this file.")},
                                                   {"ID3.WWWPublisher", TAG_ID3, TAG_ID3_WWWPUBLISHER, N_("WWW Publisher"), N_("URL pointing at the official webpage for the publisher.")},
                                                   {"ID3.WWWRadioPage", TAG_ID3, TAG_ID3_WWWRADIOPAGE, N_("WWW Radio Page"), N_("Official internet radio station homepage.")},
                                                   {"ID3.WWWUser", TAG_ID3, TAG_ID3_WWWUSER, N_("WWW User"), N_("User defined URL link.")},
                                                   {"Image.Album", TAG_IMAGE, TAG_IMAGE_ALBUM, N_("Album"), N_("Name of an album the image belongs to.")},
                                                   {"Image.CameraMake", TAG_IMAGE, TAG_IMAGE_CAMERAMAKE, N_("Camera Make"), N_("Make of camera used to take the image.")},
                                                   {"Image.CameraModel", TAG_IMAGE, TAG_IMAGE_CAMERAMODEL, N_("Camera Model"), N_("Model of camera used to take the image.")},
                                                   {"Image.Comments", TAG_IMAGE, TAG_IMAGE_COMMENTS, N_("Comments"), N_("User definable free text.")},
                                                   {"Image.Copyright", TAG_IMAGE, TAG_IMAGE_COPYRIGHT, N_("Copyright"), N_("Embedded copyright message.")},
                                                   {"Image.Creator", TAG_IMAGE, TAG_IMAGE_CREATOR, N_("Creator"), N_("Name of the author.")},
                                                   {"Image.Date", TAG_IMAGE, TAG_IMAGE_DATE, N_("Date"), N_("Datetime image was originally created.")},
                                                   {"Image.Description", TAG_IMAGE, TAG_IMAGE_DESCRIPTION, N_("Description"), N_("Description of the image.")},
                                                   {"Image.ExposureProgram", TAG_IMAGE, TAG_IMAGE_EXPOSUREPROGRAM, N_("Exposure Program"), N_("The program used by the camera to set exposure when the picture is taken. EG Manual, Normal, Aperture priority etc.")},
                                                   {"Image.ExposureTime", TAG_IMAGE, TAG_IMAGE_EXPOSURETIME, N_("Exposure Time"), N_("Exposure time used to capture the photo in seconds.")},
                                                   {"Image.Flash", TAG_IMAGE, TAG_IMAGE_FLASH, N_("Flash"), N_("Set to \"1\" if flash was fired.")},
                                                   {"Image.Fnumber", TAG_IMAGE, TAG_IMAGE_FNUMBER, N_("F number"), N_("Diameter of the aperture relative to the effective focal length of the lens.")},
                                                   {"Image.FocalLength", TAG_IMAGE, TAG_IMAGE_FOCALLENGTH, N_("Focal Length"), N_("Focal length of lens in mm.")},
                                                   {"Image.Height", TAG_IMAGE, TAG_IMAGE_HEIGHT, N_("Height"), N_("Height in pixels.")},
                                                   {"Image.ISOSpeed", TAG_IMAGE, TAG_IMAGE_ISOSPEED, N_("ISO Speed"), N_("ISO speed used to acquire the document contents. For example, 100, 200, 400, etc.")},
                                                   {"Image.Keywords", TAG_IMAGE, TAG_IMAGE_KEYWORDS, N_("Keywords"), N_("String of keywords.")},
                                                   {"Image.MeteringMode", TAG_IMAGE, TAG_IMAGE_METERINGMODE, N_("Metering Mode"), N_("Metering mode used to acquire the image (IE Unknown, Average, CenterWeightedAverage, Spot, MultiSpot, Pattern, Partial).")},
                                                   {"Image.Orientation", TAG_IMAGE, TAG_IMAGE_ORIENTATION, N_("Orientation"), N_("Represents the orientation of the image wrt camera (IE \"top,left\" or \"bottom,right\").")},
                                                   {"Image.Software", TAG_IMAGE, TAG_IMAGE_SOFTWARE, N_("Software"), N_("Software used to produce/enhance the image.")},
                                                   {"Image.Title", TAG_IMAGE, TAG_IMAGE_TITLE, N_("Title"), N_("Title of image.")},
                                                   {"Image.WhiteBalance", TAG_IMAGE, TAG_IMAGE_WHITEBALANCE, N_("White Balance"), N_("White balance setting of the camera when the picture was taken (auto or manual).")},
                                                   {"Image.Width", TAG_IMAGE, TAG_IMAGE_WIDTH, N_("Width"), N_("Width in pixels.")},
                                                   {"IPTC.ActionAdvised", TAG_IPTC, TAG_IPTC_ACTIONADVISED, N_("Action Advised"), N_("The type of action that this object provides to a previous object. '01' Object Kill, '02' Object Replace, '03' Object Append, '04' Object Reference.")},
                                                   {"IPTC.ARMID", TAG_IPTC, TAG_IPTC_ARMID, N_("ARM Identifier"), N_("Identifies the Abstract Relationship Method (ARM).")},
                                                   {"IPTC.ARMVersion", TAG_IPTC, TAG_IPTC_ARMVERSION, N_("ARM Version"), N_("Identifies the version of the Abstract Relationship Method (ARM).")},
                                                   {"IPTC.AudioDuration", TAG_IPTC, TAG_IPTC_AUDIODURATION, N_("Audio Duration"), N_("The running time of the audio data in the form HHMMSS.")},
                                                   {"IPTC.AudioOutcue", TAG_IPTC, TAG_IPTC_AUDIOOUTCUE, N_("Audio Outcue"), N_("The content at the end of the audio data.")},
                                                   {"IPTC.AudioSamplingRate", TAG_IPTC, TAG_IPTC_AUDIOSAMPLINGRATE, N_("Audio Sampling Rate"), N_("The sampling rate in Hz of the audio data.")},
                                                   {"IPTC.AudioSamplingRes", TAG_IPTC, TAG_IPTC_AUDIOSAMPLINGRES, N_("Audio Sampling Resolution"), N_("The number of bits in each audio sample.")},
                                                   {"IPTC.AudioType", TAG_IPTC, TAG_IPTC_AUDIOTYPE, N_("Audio Type"), N_("The number of channels and type of audio (music, text, etc.) in the object.")},
                                                   {"IPTC.Byline", TAG_IPTC, TAG_IPTC_BYLINE, N_("By-line"), N_("Name of the creator of the object, e.g. writer, photographer or graphic artist (multiple values allowed).")},
                                                   {"IPTC.BylineTitle", TAG_IPTC, TAG_IPTC_BYLINETITLE, N_("By-line Title"), N_("Title of the creator or creators of the object.")},
                                                   {"IPTC.Caption", TAG_IPTC, TAG_IPTC_CAPTION, N_("Caption, Abstract"), N_("A textual description of the data")},
                                                   {"IPTC.Category", TAG_IPTC, TAG_IPTC_CATEGORY, N_("Category"), N_("Identifies the subject of the object in the opinion of the provider (Deprecated).")},
                                                   {"IPTC.CharacterSet", TAG_IPTC, TAG_IPTC_CHARACTERSET, N_("Coded Character Set"), N_("Control functions used for the announcement, invocation or designation of coded character sets.")},
                                                   {"IPTC.City", TAG_IPTC, TAG_IPTC_CITY, N_("City"), N_("City of object origin.")},
                                                   {"IPTC.ConfirmedDataSize", TAG_IPTC, TAG_IPTC_CONFIRMEDDATASIZE, N_("Confirmed Data Size"), N_("Total size of the object data.")},
                                                   {"IPTC.Contact", TAG_IPTC, TAG_IPTC_CONTACT, N_("Contact"), N_("The person or organization which can provide further background information on the object (multiple values allowed).")},
                                                   {"IPTC.ContentLocCode", TAG_IPTC, TAG_IPTC_CONTENTLOCCODE, N_("Content Location Code"), N_("Indicates the code of a country/geographical location referenced by the content of the object (multiple values allowed).")},
                                                   {"IPTC.ContentLocName", TAG_IPTC, TAG_IPTC_CONTENTLOCNAME, N_("Content Location Name"), N_("A full, publishable name of a country/geographical location referenced by the content of the object (multiple values allowed).")},
                                                   {"IPTC.CopyrightNotice", TAG_IPTC, TAG_IPTC_COPYRIGHTNOTICE, N_("Copyright Notice"), N_("Any necessary copyright notice.")},
                                                   {"IPTC.CountryCode", TAG_IPTC, TAG_IPTC_COUNTRYCODE, N_("Country Code"), N_("The code of the country/primary location where the object was created.")},
                                                   {"IPTC.CountryName", TAG_IPTC, TAG_IPTC_COUNTRYNAME, N_("Country Name"), N_("The name of the country/primary location where the object was created.")},
                                                   {"IPTC.Credit", TAG_IPTC, TAG_IPTC_CREDIT, N_("Credit"), N_("Identifies the provider of the object, not necessarily the owner/creator.")},
                                                   {"IPTC.DateCreated", TAG_IPTC, TAG_IPTC_DATECREATED, N_("Date Created"), N_("The date the intellectual content of the object was created rather than the date of the creation of the physical representation.")},
                                                   {"IPTC.DateSent", TAG_IPTC, TAG_IPTC_DATESENT, N_("Date Sent"), N_("The day the service sent the material.")},
                                                   {"IPTC.Destination", TAG_IPTC, TAG_IPTC_DESTINATION, N_("Destination"), N_("Routing information.")},
                                                   {"IPTC.DigitalCreationDate", TAG_IPTC, TAG_IPTC_DIGITALCREATIONDATE, N_("Digital Creation Date"), N_("The date the digital representation of the object was created.")},
                                                   {"IPTC.DigitalCreationTime", TAG_IPTC, TAG_IPTC_DIGITALCREATIONTIME, N_("Digital Creation Time"), N_("The time the digital representation of the object was created.")},
                                                   {"IPTC.EditorialUpdate", TAG_IPTC, TAG_IPTC_EDITORIALUPDATE, N_("Editorial Update"), N_("Indicates the type of update this object provides to a previous object. The link to the previous object is made using the ARM. '01' indicates an additional language.")},
                                                   {"IPTC.EditStatus", TAG_IPTC, TAG_IPTC_EDITSTATUS, N_("Edit Status"), N_("Status of the object, according to the practice of the provider.")},
                                                   {"IPTC.EnvelopeNum", TAG_IPTC, TAG_IPTC_ENVELOPENUM, N_("Envelope Number"), N_("A number unique for the date in 1:70 and the service ID in 1:30.")},
                                                   {"IPTC.EnvelopePriority", TAG_IPTC, TAG_IPTC_ENVELOPEPRIORITY, N_("Envelope Priority"), N_("Specifies the envelope handling priority and not the editorial urgency. '1' for most urgent, '5' for normal, and '8' for least urgent. '9' is user-defined.")},
                                                   {"IPTC.ExpirationDate", TAG_IPTC, TAG_IPTC_EXPIRATIONDATE, N_("Expiration Date"), N_("Designates the latest date the provider intends the object to be used.")},
                                                   {"IPTC.ExpirationTime", TAG_IPTC, TAG_IPTC_EXPIRATIONTIME, N_("Expiration Time"), N_("Designates the latest time the provider intends the object to be used.")},
                                                   {"IPTC.FileFormat", TAG_IPTC, TAG_IPTC_FILEFORMAT, N_("File Format"), N_("File format of the data described by this metadata.")},
                                                   {"IPTC.FileVersion", TAG_IPTC, TAG_IPTC_FILEVERSION, N_("File Version"), N_("Version of the file format.")},
                                                   {"IPTC.FixtureID", TAG_IPTC, TAG_IPTC_FIXTUREID, N_("Fixture Identifier"), N_("Identifies objects that recur often and predictably, enabling users to immediately find or recall such an object.")},
                                                   {"IPTC.Headline", TAG_IPTC, TAG_IPTC_HEADLINE, N_("Headline"), N_("A publishable entry providing a synopsis of the contents of the object.")},
                                                   {"IPTC.ImageOrientation", TAG_IPTC, TAG_IPTC_IMAGEORIENTATION, N_("Image Orientation"), N_("The layout of the image area: 'P' for portrait, 'L' for landscape, and 'S' for square.")},
                                                   {"IPTC.ImageType", TAG_IPTC, TAG_IPTC_IMAGETYPE, N_("Image Type"), N_("Indicates the data format of the image object.")},
                                                   {"IPTC.Keywords", TAG_IPTC, TAG_IPTC_KEYWORDS, N_("Keywords"), N_("Used to indicate specific information retrieval words (multiple values allowed).")},
                                                   {"IPTC.LanguageID", TAG_IPTC, TAG_IPTC_LANGUAGEID, N_("Language Identifier"), N_("The major national language of the object, according to the 2-letter codes of ISO 639:1988.")},
                                                   {"IPTC.MaxObjectSize", TAG_IPTC, TAG_IPTC_MAXOBJECTSIZE, N_("Maximum Object Size"), N_("The largest possible size of the object if the size is not known.")},
                                                   {"IPTC.MaxSubfileSize", TAG_IPTC, TAG_IPTC_MAXSUBFILESIZE, N_("Max Subfile Size"), N_("The maximum size for a subfile dataset (8:10) containing a portion of the object data.")},
                                                   {"IPTC.ModelVersion", TAG_IPTC, TAG_IPTC_MODELVERSION, N_("Model Version"), N_("Version of IIM part 1.")},
                                                   {"IPTC.ObjectAttribute", TAG_IPTC, TAG_IPTC_OBJECTATTRIBUTE, N_("Object Attribute Reference"), N_("Defines the nature of the object independent of the subject (multiple values allowed).")},
                                                   {"IPTC.ObjectCycle", TAG_IPTC, TAG_IPTC_OBJECTCYCLE, N_("Object Cycle"), N_("Where 'a' is morning, 'p' is evening, 'b' is both.")},
                                                   {"IPTC.ObjectName", TAG_IPTC, TAG_IPTC_OBJECTNAME, N_("Object Name"), N_("A shorthand reference for the object.")},
                                                   {"IPTC.ObjectSizeAnnounced", TAG_IPTC, TAG_IPTC_OBJECTSIZEANNOUNCED, N_("Object Size Announced"), N_("The total size of the object data if it is known.")},
                                                   {"IPTC.ObjectType", TAG_IPTC, TAG_IPTC_OBJECTTYPE, N_("Object Type Reference"), N_("Distinguishes between different types of objects within the IIM.")},
                                                   {"IPTC.OriginatingProgram", TAG_IPTC, TAG_IPTC_ORIGINATINGPROGRAM, N_("Originating Program"), N_("The type of program used to originate the object.")},
                                                   {"IPTC.OrigTransRef", TAG_IPTC, TAG_IPTC_ORIGTRANSREF, N_("Original Transmission Reference"), N_("A code representing the location of original transmission.")},
                                                   {"IPTC.PreviewData", TAG_IPTC, TAG_IPTC_PREVIEWDATA, N_("Preview Data"), N_("The object preview data")},
                                                   {"IPTC.PreviewFileFormat", TAG_IPTC, TAG_IPTC_PREVIEWFORMAT, N_("Preview File Format"), N_("Binary value indicating the file format of the object preview data in dataset 2:202.")},
                                                   {"IPTC.PreviewFileFormatVer", TAG_IPTC, TAG_IPTC_PREVIEWFORMATVER, N_("Preview File Format Version"), N_("The version of the preview file format specified in 2:200.")},
                                                   {"IPTC.ProductID", TAG_IPTC, TAG_IPTC_PRODUCTID, N_("Product ID"), N_("Allows a provider to identify subsets of its overall service.")},
                                                   {"IPTC.ProgramVersion", TAG_IPTC, TAG_IPTC_PROGRAMVERSION, N_("Program Version"), N_("The version of the originating program.")},
                                                   {"IPTC.Province", TAG_IPTC, TAG_IPTC_PROVINCE, N_("Province, State"), N_("The Province/State where the object originates.")},
                                                   {"IPTC.RasterizedCaption", TAG_IPTC, TAG_IPTC_RASTERIZEDCAPTION, N_("Rasterized Caption"), N_("Contains rasterized object description and is used where characters that have not been coded are required for the caption.")},
                                                   {"IPTC.RecordVersion", TAG_IPTC, TAG_IPTC_RECORDVERSION, N_("Record Version"), N_("Identifies the version of the IIM, Part 2")},
                                                   {"IPTC.RefDate", TAG_IPTC, TAG_IPTC_REFERENCEDATE, N_("Reference Date"), N_("The date of a prior envelope to which the current object refers.")},
                                                   {"IPTC.RefNumber", TAG_IPTC, TAG_IPTC_REFERENCENUMBER, N_("Reference Number"), N_("The Envelope Number of a prior envelope to which the current object refers.")},
                                                   {"IPTC.RefService", TAG_IPTC, TAG_IPTC_REFERENCESERVICE, N_("Reference Service"), N_("The Service Identifier of a prior envelope to which the current object refers.")},
                                                   {"IPTC.ReleaseDate", TAG_IPTC, TAG_IPTC_RELEASEDATE, N_("Release Date"), N_("Designates the earliest date the provider intends the object to be used.")},
                                                   {"IPTC.ReleaseTime", TAG_IPTC, TAG_IPTC_RELEASETIME, N_("Release Time"), N_("Designates the earliest time the provider intends the object to be used.")},
                                                   {"IPTC.ServiceID", TAG_IPTC, TAG_IPTC_SERVICEID, N_("Service Identifier"), N_("Identifies the provider and product.")},
                                                   {"IPTC.SizeMode", TAG_IPTC, TAG_IPTC_SIZEMODE, N_("Size Mode"), N_("Set to 0 if the size of the object is known and 1 if not known.")},
                                                   {"IPTC.Source", TAG_IPTC, TAG_IPTC_SOURCE, N_("Source"), N_("The original owner of the intellectual content of the object.")},
                                                   {"IPTC.SpecialInstructions", TAG_IPTC, TAG_IPTC_SPECIALINSTRUCTIONS, N_("Special Instructions"), N_("Other editorial instructions concerning the use of the object.")},
                                                   {"IPTC.State", TAG_IPTC, TAG_IPTC_STATE, N_("Province, State"), N_("The Province/State where the object originates.")},
                                                   {"IPTC.Subfile", TAG_IPTC, TAG_IPTC_SUBFILE, N_("Subfile"), N_("The object data itself. Subfiles must be sequential so that the subfiles may be reassembled.")},
                                                   {"IPTC.SubjectRef", TAG_IPTC, TAG_IPTC_SUBJECTREFERENCE, N_("Subject Reference"), N_("A structured definition of the subject matter. It must contain an IPR, an 8 digit Subject Reference Number and an optional Subject Name, Subject Matter Name, and Subject Detail Name each separated by a colon (:).")},
                                                   {"IPTC.Sublocation", TAG_IPTC, TAG_IPTC_SUBLOCATION, N_("Sub-location"), N_("The location within a city from which the object originates.")},
                                                   {"IPTC.SupplCategory", TAG_IPTC, TAG_IPTC_SUPPLCATEGORY, N_("Supplemental Category"), N_("Further refines the subject of the object (Deprecated).")},
                                                   {"IPTC.TimeCreated", TAG_IPTC, TAG_IPTC_TIMECREATED, N_("Time Created"), N_("The time the intellectual content of the object was created rather than the date of the creation of the physical representation (multiple values allowed).")},
                                                   {"IPTC.TimeSent", TAG_IPTC, TAG_IPTC_TIMESENT, N_("Time Sent"), N_("The time the service sent the material.")},
                                                   {"IPTC.UNO", TAG_IPTC, TAG_IPTC_UNO, N_("Unique Name of Object"), N_("An eternal, globally unique identification for the object, independent of provider and for any media form.")},
                                                   {"IPTC.Urgency", TAG_IPTC, TAG_IPTC_URGENCY, N_("Urgency"), N_("Specifies the editorial urgency of content and not necessarily the envelope handling priority. '1' is most urgent, '5' normal, and '8' least urgent.")},
                                                   {"IPTC.WriterEditor", TAG_IPTC, TAG_IPTC_WRITEREDITOR, N_("Writer/Editor"), N_("The name of the person involved in the writing, editing or correcting the object or caption/abstract (multiple values allowed)")},
                                                   {"Vorbis.Contact", TAG_VORBIS, TAG_VORBIS_CONTACT, N_("Contact"), N_("Contact information for the creators or distributors of the track.")},
                                                   {"Vorbis.Description", TAG_VORBIS, TAG_VORBIS_DESCRIPTION, N_("Description"), N_("A textual description of the data.")},
                                                   {"Vorbis.License", TAG_VORBIS, TAG_VORBIS_LICENSE, N_("License"), N_("License information.")},
                                                   {"Vorbis.Location", TAG_VORBIS, TAG_VORBIS_LOCATION, N_("Location"), N_("Location where track was recorded.")},
                                                   {"Vorbis.Contact", TAG_VORBIS, TAG_VORBIS_MAXBITRATE, N_("Maximum bitrate"), N_("Maximum bitrate in kbps.")},
                                                   {"Vorbis.Contact", TAG_VORBIS, TAG_VORBIS_MINBITRATE, N_("Minimum bitrate"), N_("Minimum bitrate in kbps.")},
                                                   {"Vorbis.Contact", TAG_VORBIS, TAG_VORBIS_NOMINALBITRATE, N_("Nominal bitrate"), N_("Nominal bitrate in kbps.")},
                                                   {"Vorbis.Organization", TAG_VORBIS, TAG_VORBIS_ORGANIZATION, N_("Organization"), N_("Organization producing the track.")},
                                                   {"Vorbis.Vendor", TAG_VORBIS, TAG_VORBIS_VENDOR, N_("Vendor"), N_("Vorbis vendor ID.")},
                                                   {"Vorbis.Version", TAG_VORBIS, TAG_VORBIS_VERSION, N_("Vorbis Version"), N_("Vorbis version.")}
                                                  };


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


static int tagcmp(const void *t1, const void *t2)
{
    return strcasecmp(((GnomeCmdTagName *) t1)->name,((GnomeCmdTagName *) t2)->name);
}


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
    gcmd_tags_exiv2_init();
    gcmd_tags_taglib_init();
    gcmd_tags_libgsf_init();
}


void gcmd_tags_shutdown()
{
    gcmd_tags_exiv2_shutdown();
    gcmd_tags_taglib_shutdown();
    gcmd_tags_libgsf_shutdown();
}


GnomeCmdFileMetadata *gcmd_tags_bulk_load(GnomeCmdFile *finfo)
{
    g_return_val_if_fail (finfo != NULL, NULL);

    gcmd_tags_file_load_metadata(finfo);
    gcmd_tags_exiv2_load_metadata(finfo);
    gcmd_tags_taglib_load_metadata(finfo);
    gcmd_tags_libgsf_load_metadata(finfo);

    return finfo->metadata;
}


GnomeCmdTag gcmd_tags_get_tag_by_name(const gchar *tag_name, const GnomeCmdTagClass tag_class)
{
    GnomeCmdTagName tag;

    if (tag_class==TAG_NONE_CLASS)
    {
        switch (tag_class)
        {
            case TAG_APE:
                tag.name = "APE.";
                break;

            case TAG_AUDIO:
                tag.name = "Audio.";
                break;

            case TAG_CHM:
                tag.name = "CHM.";
                break;

            case TAG_DOC:
                tag.name = "Doc.";
                break;

            case TAG_EXIF:
                tag.name = "Exif.";
                break;

            case TAG_FILE:
                tag.name = "File.";
                break;

            case TAG_FLAC:
                tag.name = "FLAC";
                break;

            case TAG_ICC:
                tag.name = "ICC.";
                break;

            case TAG_ID3:
                tag.name = "ID3.";
                break;

            case TAG_IMAGE:
                tag.name = "Image.";
                break;

            case TAG_IPTC:
                tag.name = "IPTC.";
                break;

            case TAG_RPM:
                tag.name = "RPM.";
                break;

            case TAG_VORBIS:
                tag.name = "Vorbis.";
                break;

            default:
                tag.name = empty_string;
                break;
        }

        tag.name = g_strconcat(tag.name,tag_name,NULL);
    }
    else
        tag.name = tag_name;

    GnomeCmdTagName *elem = (GnomeCmdTagName *) bsearch(&tag,metatags,NUMBER_OF_TAGS,sizeof(GnomeCmdTagName),tagcmp);

    if (tag_class==TAG_NONE_CLASS)
        g_free((gpointer) tag.name);

    return elem ? elem->tag : TAG_NONE;
}


const gchar *gcmd_tags_get_name(const GnomeCmdTag tag)
{
    return metatags[ tag<NUMBER_OF_TAGS ? tag : TAG_NONE ].name;
}


const GnomeCmdTagClass gcmd_tags_get_class(const GnomeCmdTag tag)
{
    return metatags[ tag<NUMBER_OF_TAGS ? tag : TAG_NONE ].tag_class;
}


const gchar *gcmd_tags_get_class_name(const GnomeCmdTag tag)
{
    switch (metatags[ tag<NUMBER_OF_TAGS ? tag : TAG_NONE ].tag_class)
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

        case TAG_RPM:
            return "RPM";

        case TAG_VORBIS:
            return "Vorbis";

        default:
            break;
    }

    return empty_string;
}


const gchar *gcmd_tags_get_value(GnomeCmdFile *finfo, const GnomeCmdTag tag)
{
    g_return_val_if_fail (finfo != NULL, empty_string);

    const gchar *ret_val = empty_string;

    if (tag>=NUMBER_OF_TAGS)
        return ret_val;

    switch (metatags[tag].tag_class)
    {
        case TAG_IMAGE:
        case TAG_EXIF :
        case TAG_IPTC :
#ifndef HAVE_GSF
                        return _(no_support_for_exiv2_tags_string);
#endif
                        gcmd_tags_exiv2_load_metadata(finfo);
                        ret_val = finfo->metadata->operator [] (tag).c_str();
                        break;

        case TAG_AUDIO:
        case TAG_APE  :
        case TAG_FLAC :
        case TAG_ID3  :
        case TAG_VORBIS:
#ifndef HAVE_ID3
                        return _(no_support_for_taglib_tags_string);
#endif
                        gcmd_tags_taglib_load_metadata(finfo);
                        ret_val = finfo->metadata->operator [] (tag).c_str();
                        break;

        case TAG_CHM  : break;

        case TAG_DOC  :
#ifndef HAVE_GSF
                        return _(no_support_for_libgsf_tags_string);
#endif
                        gcmd_tags_libgsf_load_metadata(finfo);
                        ret_val = finfo->metadata->operator [] (tag).c_str();
                        break;

        case TAG_FILE : gcmd_tags_file_load_metadata(finfo);
                        ret_val = finfo->metadata->operator [] (tag).c_str();
                        break;

        case TAG_RPM  : break;

        default:        break;
    }

    return ret_val ? ret_val : empty_string;
}


const gchar *gcmd_tags_get_title(const GnomeCmdTag tag)
{
    return _(metatags[ tag<NUMBER_OF_TAGS ? tag : TAG_NONE ].title);
}


const gchar *gcmd_tags_get_description(const GnomeCmdTag tag)
{
    return _(metatags[ tag<NUMBER_OF_TAGS ? tag : TAG_NONE ].description);
}

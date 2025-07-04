/**
 * @file exiv2-plugin.cc
 * @copyright (C) 2001-2006 Marcus Bjurman\n
 * @copyright (C) 2007-2012 Piotr Eljasiak\n
 * @copyright (C) 2013-2024 Uwe Scholz\n
 * @copyright (C) 2025 Andrey Kutejko\n
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
#include <libgcmd.h>
#include <exiv2/exiv2.hpp>


extern "C" GObject *create_plugin ();
extern "C" GnomeCmdPluginInfo *get_plugin_info ();


using namespace std;
using namespace Exiv2;


#define GNOME_CMD_TYPE_EXIV2_PLUGIN (gnome_cmd_exiv2_plugin_get_type ())
G_DECLARE_FINAL_TYPE (GnomeCmdExiv2Plugin, gnome_cmd_exiv2_plugin, GNOME_CMD, EXIV2_PLUGIN, GObject)

struct _GnomeCmdExiv2Plugin
{
    GObject parent;
};

struct GnomeCmdExiv2PluginPrivate
{
    GHashTable *exiv2_tags;
};

static void gnome_cmd_file_metadata_extractor_init (GnomeCmdFileMetadataExtractorInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GnomeCmdExiv2Plugin, gnome_cmd_exiv2_plugin, G_TYPE_OBJECT,
    G_ADD_PRIVATE (GnomeCmdExiv2Plugin)
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


static const char *TAG_FILE_DESCRIPTION    = "File.Description";
static const char *TAG_FILE_KEYWORDS       = "File.Keywords";
static const char *TAG_FILE_PUBLISHER      = "File.Publisher";


struct ImageTag
{
    const char *id;
    const char *name;
    const char *description;
};

static ImageTag TAG_IMAGE_ALBUM            = { "Image.Album",              N_("Album"),            N_("Name of an album the image belongs to.") };
static ImageTag TAG_IMAGE_MAKE             = { "Image.Make",               N_("Make"),             N_("Make of camera used to take the image.") };
static ImageTag TAG_IMAGE_MODEL            = { "Image.Model",              N_("Model"),            N_("Model of camera used to take the image.") };
static ImageTag TAG_IMAGE_COMMENTS         = { "Image.Comments",           N_("Comments"),         N_("User definable free text.") };
static ImageTag TAG_IMAGE_COPYRIGHT        = { "Image.Copyright",          N_("Copyright"),        N_("Embedded copyright message.") };
static ImageTag TAG_IMAGE_CREATOR          = { "Image.Creator",            N_("Creator"),          N_("Name of the author.") };
static ImageTag TAG_IMAGE_DATE             = { "Image.Date",               N_("Date"),             N_("Datetime image was originally created.") };
static ImageTag TAG_IMAGE_DESCRIPTION      = { "Image.Description",        N_("Description"),      N_("Description of the image.") };
static ImageTag TAG_IMAGE_EXPOSUREPROGRAM  = { "Image.ExposureProgram",    N_("Exposure Program"), N_("The program used by the camera to set exposure when the picture is taken. EG Manual, Normal, Aperture priority etc.") };
static ImageTag TAG_IMAGE_EXPOSURETIME     = { "Image.ExposureTime",       N_("Exposure Time"),    N_("Exposure time used to capture the photo in seconds.") };
static ImageTag TAG_IMAGE_FLASH            = { "Image.Flash",              N_("Flash"),            N_("Set to “1” if flash was fired.") };
static ImageTag TAG_IMAGE_FNUMBER          = { "Image.Fnumber",            N_("F Number"),         N_("Diameter of the aperture relative to the effective focal length of the lens.") };
static ImageTag TAG_IMAGE_FOCALLENGTH      = { "Image.FocalLength",        N_("Focal Length"),     N_("Focal length of lens in mm.") };
static ImageTag TAG_IMAGE_ISOSPEED         = { "Image.ISOSpeed",           N_("ISO Speed"),        N_("ISO speed used to acquire the document contents. For example, 100, 200, 400, etc.") };
static ImageTag TAG_IMAGE_KEYWORDS         = { "Image.Keywords",           N_("Keywords"),         N_("String of keywords.") };
static ImageTag TAG_IMAGE_METERINGMODE     = { "Image.MeteringMode",       N_("Metering Mode"),    N_("Metering mode used to acquire the image (IE Unknown, Average, CenterWeightedAverage, Spot, MultiSpot, Pattern, Partial).") };
static ImageTag TAG_IMAGE_ORIENTATION      = { "Image.Orientation",        N_("Orientation"),      N_("Represents the orientation of the image wrt camera (IE “top,left” or “bottom,right”).") };
static ImageTag TAG_IMAGE_SOFTWARE         = { "Image.Software",           N_("Software"),         N_("Software used to produce/enhance the image.") };
static ImageTag TAG_IMAGE_TITLE            = { "Image.Title",              N_("Title"),            N_("Title of image.") };
static ImageTag TAG_IMAGE_WHITEBALANCE     = { "Image.WhiteBalance",       N_("White Balance"),    N_("White balance setting of the camera when the picture was taken (auto or manual).") };
static ImageTag TAG_IMAGE_WIDTH            = { "Image.Width",              N_("Width"),            N_("Width in pixels.") };
static ImageTag TAG_IMAGE_HEIGHT           = { "Image.Height",             N_("Height"),           N_("Height in pixels.") };

static ImageTag TAG_EXIF_COPYRIGHT                    = { "Exif.Copyright",                    N_("Copyright"),                      N_("Copyright information. The tag is used to indicate both the photographer and editor copyrights. It is the copyright notice of the person or organization claiming rights to the image. The Interoperability copyright statement including date and rights should be written in this field; e.g., “Copyright, John Smith, 19xx. All rights reserved.”. The field records both the photographer and editor copyrights, with each recorded in a separate part of the statement. When there is a clear distinction between the photographer and editor copyrights, these are to be written in the order of photographer followed by editor copyright, separated by NULL (in this case, since the statement also ends with a NULL, there are two NULL codes) (see example 1). When only the photographer is given, it is terminated by one NULL code. When only the editor copyright is given, the photographer copyright part consists of one space followed by a terminating NULL code, then the editor copyright is given. When the field is left blank, it is treated as unknown.") };
static ImageTag TAG_EXIF_DATETIME                     = { "Exif.DateTime",                     N_("Date and Time"),                  N_("The date and time of image creation.") };
static ImageTag TAG_EXIF_EXPOSUREBIASVALUE            = { "Exif.ExposureBiasValue",            N_("Exposure Bias"),                  N_("The exposure bias. The units is the APEX value. Ordinarily it is given in the range of -99.99 to 99.99.") };
static ImageTag TAG_EXIF_EXPOSUREMODE                 = { "Exif.ExposureMode",                 N_("Exposure Mode"),                  N_("The exposure mode set when the image was shot. In auto-bracketing mode, the camera shoots a series of frames of the same scene at different exposure settings.") };
static ImageTag TAG_EXIF_EXPOSUREPROGRAM              = { "Exif.ExposureProgram",              N_("Exposure Program"),               N_("The class of the program used by the camera to set exposure when the picture is taken.") };
static ImageTag TAG_EXIF_FLASH                        = { "Exif.Flash",                        N_("Flash"),                          N_("This tag is recorded when an image is taken using a strobe light (flash).") };
static ImageTag TAG_EXIF_FLASHENERGY                  = { "Exif.FlashEnergy",                  N_("Flash Energy"),                   N_("The strobe energy at the time the image is captured, as measured in Beam Candle Power Seconds (BCPS).") };
static ImageTag TAG_EXIF_FNUMBER                      = { "Exif.FNumber",                      N_("F Number"),                       N_("Diameter of the aperture relative to the effective focal length of the lens.") };
static ImageTag TAG_EXIF_FOCALLENGTH                  = { "Exif.FocalLength",                  N_("Focal Length"),                   N_("The actual focal length of the lens, in mm. Conversion is not made to the focal length of a 35 mm film camera.") };
static ImageTag TAG_EXIF_ISOSPEEDRATINGS              = { "Exif.ISOSpeedRatings",              N_("ISO Speed Ratings"),              N_("The ISO Speed and ISO Latitude of the camera or input device as specified in ISO 12232.") };
static ImageTag TAG_EXIF_MAXAPERTUREVALUE             = { "Exif.MaxApertureValue",             N_("Max Aperture Value"),             N_("The smallest F number of the lens. The unit is the APEX value. Ordinarily it is given in the range of 00.00 to 99.99, but it is not limited to this range.") };
static ImageTag TAG_EXIF_METERINGMODE                 = { "Exif.MeteringMode",                 N_("Metering Mode"),                  N_("The metering mode.") };
static ImageTag TAG_EXIF_SHUTTERSPEEDVALUE            = { "Exif.ShutterSpeedValue",            N_("Shutter Speed"),                  N_("Shutter speed. The unit is the APEX (Additive System of Photographic Exposure) setting.") };
static ImageTag TAG_EXIF_WHITEBALANCE                 = { "Exif.WhiteBalance",                 N_("White Balance"),                  N_("The white balance mode set when the image was shot.") };
static ImageTag TAG_EXIF_PIXELXDIMENSION              = { "Exif.PixelXDimension",              N_("Pixel X Dimension"),              N_("Information specific to compressed data. When a compressed file is recorded, the valid width of the meaningful image must be recorded in this tag, whether or not there is padding data or a restart marker. This tag should not exist in an uncompressed file.") };
static ImageTag TAG_EXIF_PIXELYDIMENSION              = { "Exif.PixelYDimension",              N_("Pixel Y Dimension"),              N_("Information specific to compressed data. When a compressed file is recorded, the valid height of the meaningful image must be recorded in this tag, whether or not there is padding data or a restart marker. This tag should not exist in an uncompressed file. Since data padding is unnecessary in the vertical direction, the number of lines recorded in this valid image height tag will in fact be the same as that recorded in the SOF.") };
static ImageTag TAG_EXIF_XRESOLUTION                  = { "Exif.XResolution",                  N_("x Resolution"),                   N_("The number of pixels per <Exif.ResolutionUnit> in the <Exif.ImageWidth> direction. When the image resolution is unknown, 72 [dpi] is designated.") };
static ImageTag TAG_EXIF_YRESOLUTION                  = { "Exif.YResolution",                  N_("y Resolution"),                   N_("The number of pixels per <Exif.ResolutionUnit> in the <Exif.ImageLength> direction. The same value as <Exif.XResolution> is designated.") };
static ImageTag TAG_EXIF_IMAGELENGTH                  = { "Exif.ImageLength",                  N_("Image Length"),                   N_("The number of rows of image data. In JPEG compressed data a JPEG marker is used instead of this tag.") };
static ImageTag TAG_EXIF_IMAGEWIDTH                   = { "Exif.ImageWidth",                   N_("Image Width"),                    N_("The number of columns of image data, equal to the number of pixels per row. In JPEG compressed data a JPEG marker is used instead of this tag.") };
static ImageTag TAG_EXIF_CUSTOMRENDERED               = { "Exif.CustomRendered",               N_("Custom Rendered"),                N_("The use of special processing on image data, such as rendering geared to output. When special processing is performed, the reader is expected to disable or minimize any further processing.") };
static ImageTag TAG_EXIF_COLORSPACE                   = { "Exif.ColorSpace",                   N_("Color Space"),                    N_("The color space information tag is always recorded as the color space specifier. Normally sRGB is used to define the color space based on the PC monitor conditions and environment. If a color space other than sRGB is used, Uncalibrated is set. Image data recorded as Uncalibrated can be treated as sRGB when it is converted to FlashPix.") };
static ImageTag TAG_EXIF_DOCUMENTNAME                 = { "Exif.DocumentName",                 N_("Document Name"),                  N_("Document name.") };
static ImageTag TAG_EXIF_USERCOMMENT                  = { "Exif.UserComment",                  N_("User Comment"),                   N_("A tag for Exif users to write keywords or comments on the image besides those in <Exif.ImageDescription>, and without the character code limitations of the <Exif.ImageDescription> tag. The character code used in the <Exif.UserComment> tag is identified based on an ID code in a fixed 8-byte area at the start of the tag data area. The unused portion of the area is padded with NULL (“00.h”). ID codes are assigned by means of registration. The value of CountN is determinated based on the 8 bytes in the character code area and the number of bytes in the user comment part. Since the TYPE is not ASCII, NULL termination is not necessary. The ID code for the <Exif.UserComment> area may be a Defined code such as JIS or ASCII, or may be Undefined. The Undefined name is UndefinedText, and the ID code is filled with 8 bytes of all “NULL” (“00.H”). An Exif reader that reads the <Exif.UserComment> tag must have a function for determining the ID code. This function is not required in Exif readers that do not use the <Exif.UserComment> tag. When a <Exif.UserComment> area is set aside, it is recommended that the ID code be ASCII and that the following user comment part be filled with blank characters [20.H].") };
static ImageTag TAG_EXIF_APERTUREVALUE                = { "Exif.ApertureValue",                N_("Aperture"),                       N_("The lens aperture. The unit is the APEX value.") };
static ImageTag TAG_EXIF_ARTIST                       = { "Exif.Artist",                       N_("Artist"),                         N_("Name of the camera owner, photographer or image creator. The detailed format is not specified, but it is recommended that the information be written for ease of Interoperability. When the field is left blank, it is treated as unknown.") };
static ImageTag TAG_EXIF_BATTERYLEVEL                 = { "Exif.BatteryLevel",                 N_("Battery Level"),                  N_("Battery level.") };
static ImageTag TAG_EXIF_BITSPERSAMPLE                = { "Exif.BitsPerSample",                N_("Bits per Sample"),                N_("The number of bits per image component. Each component of the image is 8 bits, so the value for this tag is 8. In JPEG compressed data a JPEG marker is used instead of this tag.") };
static ImageTag TAG_EXIF_BRIGHTNESSVALUE              = { "Exif.BrightnessValue",              N_("Brightness"),                     N_("The value of brightness. The unit is the APEX value. Ordinarily it is given in the range of -99.99 to 99.99.") };
static ImageTag TAG_EXIF_CFAPATTERN                   = { "Exif.CFAPattern",                   N_("CFA Pattern"),                    N_("The color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used. It does not apply to all sensing methods.") };
static ImageTag TAG_EXIF_CFAREPEATPATTERNDIM          = { "Exif.CFARepeatPatternDim",          N_("CFA Repeat Pattern Dim"),         N_("CFA Repeat Pattern Dim.") };
static ImageTag TAG_EXIF_COMPONENTSCONFIGURATION      = { "Exif.ComponentsConfiguration",      N_("Components Configuration"),       N_("Information specific to compressed data. The channels of each component are arranged in order from the 1st component to the 4th. For uncompressed data the data arrangement is given in the <Exif.PhotometricInterpretation> tag. However, since <Exif.PhotometricInterpretation> can only express the order of Y, Cb and Cr, this tag is provided for cases when compressed data uses components other than Y, Cb, and Cr and to enable support of other sequences.") };
static ImageTag TAG_EXIF_COMPRESSEDBITSPERPIXEL       = { "Exif.CompressedBitsPerPixel",       N_("Compressed Bits per Pixel"),      N_("Information specific to compressed data. The compression mode used for a compressed image is indicated in unit bits per pixel.") };
static ImageTag TAG_EXIF_COMPRESSION                  = { "Exif.Compression",                  N_("Compression"),                    N_("The compression scheme used for the image data. When a primary image is JPEG compressed, this designation is not necessary and is omitted. When thumbnails use JPEG compression, this tag value is set to 6.") };
static ImageTag TAG_EXIF_CONTRAST                     = { "Exif.Contrast",                     N_("Contrast"),                       N_("The direction of contrast processing applied by the camera when the image was shot.") };
static ImageTag TAG_EXIF_DATETIMEDIGITIZED            = { "Exif.DateTimeDigitized",            N_("Date and Time (digitized)"),      N_("The date and time when the image was stored as digital data.") };
static ImageTag TAG_EXIF_DATETIMEORIGINAL             = { "Exif.DateTimeOriginal",             N_("Date and Time (original)"),       N_("The date and time when the original image data was generated. For a digital still camera the date and time the picture was taken are recorded.") };
static ImageTag TAG_EXIF_DEVICESETTINGDESCRIPTION     = { "Exif.DeviceSettingDescription",     N_("Device Setting Description"),     N_("Information on the picture-taking conditions of a particular camera model. The tag is used only to indicate the picture-taking conditions in the reader.") };
static ImageTag TAG_EXIF_DIGITALZOOMRATIO             = { "Exif.DigitalZoomRatio",             N_("Digital Zoom Ratio"),             N_("The digital zoom ratio when the image was shot. If the numerator of the recorded value is 0, this indicates that digital zoom was not used.") };
static ImageTag TAG_EXIF_EXIFVERSION                  = { "Exif.ExifVersion",                  N_("Exif Version"),                   N_("The version of Exif standard supported. Nonexistence of this field is taken to mean nonconformance to the standard.") };
static ImageTag TAG_EXIF_EXPOSUREINDEX                = { "Exif.ExposureIndex",                N_("Exposure Index"),                 N_("The exposure index selected on the camera or input device at the time the image is captured.") };
static ImageTag TAG_EXIF_EXPOSURETIME                 = { "Exif.ExposureTime",                 N_("Exposure Time"),                  N_("Exposure time, given in seconds.") };
static ImageTag TAG_EXIF_FILESOURCE                   = { "Exif.FileSource",                   N_("File Source"),                    N_("Indicates the image source. If a DSC recorded the image, this tag value of this tag always be set to 3, indicating that the image was recorded on a DSC.") };
static ImageTag TAG_EXIF_FILLORDER                    = { "Exif.FillOrder",                    N_("Fill Order"),                     N_("Fill order.") };
static ImageTag TAG_EXIF_FLASHPIXVERSION              = { "Exif.FlashPixVersion",              N_("FlashPix Version"),               N_("The FlashPix format version supported by a FPXR file.") };
static ImageTag TAG_EXIF_FOCALLENGTHIN35MMFILM        = { "Exif.FocalLengthIn35mmFilm",        N_("Focal Length In 35mm Film"),      N_("The equivalent focal length assuming a 35mm film camera, in mm. A value of 0 means the focal length is unknown. Note that this tag differs from the <Exif.FocalLength> tag.") };
static ImageTag TAG_EXIF_FOCALPLANERESOLUTIONUNIT     = { "Exif.FocalPlaneResolutionUnit",     N_("Focal Plane Resolution Unit"),    N_("The unit for measuring <Exif.FocalPlaneXResolution> and <Exif.FocalPlaneYResolution>. This value is the same as the <Exif.ResolutionUnit>.") };
static ImageTag TAG_EXIF_FOCALPLANEXRESOLUTION        = { "Exif.FocalPlaneXResolution",        N_("Focal Plane x-Resolution"),       N_("The number of pixels in the image width (X) direction per <Exif.FocalPlaneResolutionUnit> on the camera focal plane.") };
static ImageTag TAG_EXIF_FOCALPLANEYRESOLUTION        = { "Exif.FocalPlaneYResolution",        N_("Focal Plane y-Resolution"),       N_("The number of pixels in the image height (Y) direction per <Exif.FocalPlaneResolutionUnit> on the camera focal plane.") };
static ImageTag TAG_EXIF_GAINCONTROL                  = { "Exif.GainControl",                  N_("Gain Control"),                   N_("This tag indicates the degree of overall image gain adjustment.") };
static ImageTag TAG_EXIF_GAMMA                        = { "Exif.Gamma",                        N_("Gamma"),                          N_("Indicates the value of coefficient gamma.") };
static ImageTag TAG_EXIF_GPSALTITUDE                  = { "Exif.GPS.Altitude",                 N_("Altitude"),                       N_("Indicates the altitude based on the reference in <Exif.GPS.AltitudeRef>. The reference unit is meters.") };
static ImageTag TAG_EXIF_GPSALTITUDEREF               = { "Exif.GPS.AltitudeRef",              N_("Altitude Reference"),             N_("Indicates the altitude used as the reference altitude. If the reference is sea level and the altitude is above sea level, 0 is given. If the altitude is below sea level, a value of 1 is given and the altitude is indicated as an absolute value in the <Exif.GPS.Altitude> tag. The reference unit is meters.") };
static ImageTag TAG_EXIF_GPSINFOIFDPOINTER            = { "Exif.GPS.InfoIFDPointer",           N_("GPS Info IFDPointer"),            N_("A pointer to the GPS Info IFD. The Interoperability structure of the GPS Info IFD, like that of Exif IFD, has no image data.") };
static ImageTag TAG_EXIF_GPSLATITUDE                  = { "Exif.GPS.Latitude",                 N_("Latitude"),                       N_("Indicates the latitude. The latitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is dd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is dd/1,mmmm/100,0/1.") };
static ImageTag TAG_EXIF_GPSLATITUDEREF               = { "Exif.GPS.LatitudeRef",              N_("North or South Latitude"),        N_("Indicates whether the latitude is north or south latitude. The ASCII value “N” indicates north latitude, and “S” is south latitude.") };
static ImageTag TAG_EXIF_GPSLONGITUDE                 = { "Exif.GPS.Longitude",                N_("Longitude"),                      N_("Indicates the longitude. The longitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is ddd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is ddd/1,mmmm/100,0/1.") };
static ImageTag TAG_EXIF_GPSLONGITUDEREF              = { "Exif.GPS.LongitudeRef",             N_("East or West Longitude"),         N_("Indicates whether the longitude is east or west longitude. ASCII “E” indicates east longitude, and “W” is west longitude.") };
static ImageTag TAG_EXIF_GPSVERSIONID                 = { "Exif.GPS.VersionID",                N_("GPS Tag Version"),                N_("Indicates the version of <Exif.GPS.InfoIFD>. This tag is mandatory when <Exif.GPS.Info> tag is present.") };
static ImageTag TAG_EXIF_IMAGEDESCRIPTION             = { "Exif.ImageDescription",             N_("Image Description"),              N_("A character string giving the title of the image. Two-bytes character codes cannot be used. When a 2-bytes code is necessary, the Exif Private tag <Exif.UserComment> is to be used.") };
static ImageTag TAG_EXIF_IMAGERESOURCES               = { "Exif.ImageResources",               N_("Image Resources Block"),          N_("Image Resources Block.") };
static ImageTag TAG_EXIF_IMAGEUNIQUEID                = { "Exif.ImageUniqueID",                N_("Image Unique ID"),                N_("This tag indicates an identifier assigned uniquely to each image. It is recorded as an ASCII string equivalent to hexadecimal notation and 128-bit fixed length.") };
static ImageTag TAG_EXIF_INTERCOLORPROFILE            = { "Exif.InterColorProfile",            N_("Inter Color Profile"),            N_("Inter Color Profile.") };
static ImageTag TAG_EXIF_INTEROPERABILITYINDEX        = { "Exif.InteroperabilityIndex",        N_("Interoperability Index"),         N_("Indicates the identification of the Interoperability rule. Use “R98” for stating ExifR98 Rules.") };
static ImageTag TAG_EXIF_INTEROPERABILITYVERSION      = { "Exif.InteroperabilityVersion",      N_("Interoperability Version"),       N_("Interoperability version.") };
static ImageTag TAG_EXIF_IPTCNAA                      = { "Exif.IPTC_NAA",                     N_("IPTC/NAA"),                       N_("An IPTC/NAA record.") };
static ImageTag TAG_EXIF_JPEGINTERCHANGEFORMAT        = { "Exif.JPEGInterchangeFormat",        N_("JPEG Interchange Format"),        N_("The offset to the start byte (SOI) of JPEG compressed thumbnail data. This is not used for primary image JPEG data.") };
static ImageTag TAG_EXIF_JPEGINTERCHANGEFORMATLENGTH  = { "Exif.JPEGInterchangeFormatLength",  N_("JPEG Interchange Format Length"), N_("The number of bytes of JPEG compressed thumbnail data. This is not used for primary image JPEG data. JPEG thumbnails are not divided but are recorded as a continuous JPEG bitstream from SOI to EOI. Appn and COM markers should not be recorded. Compressed thumbnails must be recorded in no more than 64 Kbytes, including all other data to be recorded in APP1.") };
static ImageTag TAG_EXIF_JPEGPROC                     = { "Exif.JPEGProc",                     N_("JPEG Procedure"),                 N_("JPEG procedure.") };
static ImageTag TAG_EXIF_LIGHTSOURCE                  = { "Exif.LightSource",                  N_("Light Source"),                   N_("The kind of light source.") };
static ImageTag TAG_EXIF_MAKE                         = { "Exif.Make",                         N_("Manufacturer"),                   N_("The manufacturer of the recording equipment. This is the manufacturer of the DSC, scanner, video digitizer or other equipment that generated the image. When the field is left blank, it is treated as unknown.") };
static ImageTag TAG_EXIF_MAKERNOTE                    = { "Exif.MakerNote",                    N_("Maker Note"),                     N_("A tag for manufacturers of Exif writers to record any desired information. The contents are up to the manufacturer.") };
static ImageTag TAG_EXIF_MODEL                        = { "Exif.Model",                        N_("Model"),                          N_("The model name or model number of the equipment. This is the model name or number of the DSC, scanner, video digitizer or other equipment that generated the image. When the field is left blank, it is treated as unknown.") };
static ImageTag TAG_EXIF_NEWCFAPATTERN                = { "Exif.CFAPattern",                   N_("CFA Pattern"),                    N_("The color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used. It does not apply to all sensing methods.") };
static ImageTag TAG_EXIF_NEWSUBFILETYPE               = { "Exif.NewSubfileType",               N_("New Subfile Type"),               N_("A general indication of the kind of data contained in this subfile.") };
static ImageTag TAG_EXIF_OECF                         = { "Exif.OECF",                         N_("OECF"),                           N_("The Opto-Electronic Conversion Function (OECF) specified in ISO 14524. <Exif.OECF> is the relationship between the camera optical input and the image values.") };
static ImageTag TAG_EXIF_ORIENTATION                  = { "Exif.Orientation",                  N_("Orientation"),                    N_("The image orientation viewed in terms of rows and columns.") };
static ImageTag TAG_EXIF_PHOTOMETRICINTERPRETATION    = { "Exif.PhotometricInterpretation",    N_("Photometric Interpretation"),     N_("The pixel composition. In JPEG compressed data a JPEG marker is used instead of this tag.") };
static ImageTag TAG_EXIF_PLANARCONFIGURATION          = { "Exif.PlanarConfiguration",          N_("Planar Configuration"),           N_("Indicates whether pixel components are recorded in a chunky or planar format. In JPEG compressed files a JPEG marker is used instead of this tag. If this field does not exist, the TIFF default of 1 (chunky) is assumed.") };
static ImageTag TAG_EXIF_PRIMARYCHROMATICITIES        = { "Exif.PrimaryChromaticities",        N_("Primary Chromaticities"),         N_("The chromaticity of the three primary colors of the image. Normally this tag is not necessary, since colorspace is specified in <Exif.ColorSpace> tag.") };
static ImageTag TAG_EXIF_REFERENCEBLACKWHITE          = { "Exif.ReferenceBlackWhite",          N_("Reference Black/White"),          N_("The reference black point value and reference white point value. No defaults are given in TIFF, but the values below are given as defaults here. The color space is declared in a color space information tag, with the default being the value that gives the optimal image characteristics under these conditions.") };
static ImageTag TAG_EXIF_RELATEDIMAGEFILEFORMAT       = { "Exif.RelatedImageFileFormat",       N_("Related Image File Format"),      N_("Related image file format.") };
static ImageTag TAG_EXIF_RELATEDIMAGELENGTH           = { "Exif.RelatedImageLength",           N_("Related Image Length"),           N_("Related image length.") };
static ImageTag TAG_EXIF_RELATEDIMAGEWIDTH            = { "Exif.RelatedImageWidth",            N_("Related Image Width"),            N_("Related image width.") };
static ImageTag TAG_EXIF_RELATEDSOUNDFILE             = { "Exif.RelatedSoundFile",             N_("Related Sound File"),             N_("This tag is used to record the name of an audio file related to the image data. The only relational information recorded here is the Exif audio file name and extension (an ASCII string consisting of 8 characters + “.” + 3 characters). The path is not recorded. When using this tag, audio files must be recorded in conformance to the Exif audio format. Writers are also allowed to store the data such as Audio within APP2 as FlashPix extension stream data. Audio files must be recorded in conformance to the Exif audio format. If multiple files are mapped to one file, the above format is used to record just one audio file name. If there are multiple audio files, the first recorded file is given. When there are three Exif audio files “SND00001.WAV”, “SND00002.WAV” and “SND00003.WAV”, the Exif image file name for each of them, “DSC00001.JPG”, is indicated. By combining multiple relational information, a variety of playback possibilities can be supported. The method of using relational information is left to the implementation on the playback side. Since this information is an ASCII character string, it is terminated by NULL. When this tag is used to map audio files, the relation of the audio file to image data must also be indicated on the audio file end.") };
static ImageTag TAG_EXIF_RESOLUTIONUNIT               = { "Exif.ResolutionUnit",               N_("Resolution Unit"),                N_("The unit for measuring <Exif.XResolution> and <Exif.YResolution>. The same unit is used for both <Exif.XResolution> and <Exif.YResolution>. If the image resolution is unknown, 2 (inches) is designated.") };
static ImageTag TAG_EXIF_ROWSPERSTRIP                 = { "Exif.RowsPerStrip",                 N_("Rows per Strip"),                 N_("The number of rows per strip. This is the number of rows in the image of one strip when an image is divided into strips. With JPEG compressed data this designation is not needed and is omitted.") };
static ImageTag TAG_EXIF_SAMPLESPERPIXEL              = { "Exif.SamplesPerPixel",              N_("Samples per Pixel"),              N_("The number of components per pixel. Since this standard applies to RGB and YCbCr images, the value set for this tag is 3. In JPEG compressed data a JPEG marker is used instead of this tag.") };
static ImageTag TAG_EXIF_SATURATION                   = { "Exif.Saturation",                   N_("Saturation"),                     N_("The direction of saturation processing applied by the camera when the image was shot.") };
static ImageTag TAG_EXIF_SCENECAPTURETYPE             = { "Exif.SceneCaptureType",             N_("Scene Capture Type"),             N_("The type of scene that was shot. It can also be used to record the mode in which the image was shot. Note that this differs from <Exif.SceneType> tag.") };
static ImageTag TAG_EXIF_SCENETYPE                    = { "Exif.SceneType",                    N_("Scene Type"),                     N_("The type of scene. If a DSC recorded the image, this tag value must always be set to 1, indicating that the image was directly photographed.") };
static ImageTag TAG_EXIF_SENSINGMETHOD                = { "Exif.SensingMethod",                N_("Sensing Method"),                 N_("The image sensor type on the camera or input device.") };
static ImageTag TAG_EXIF_SHARPNESS                    = { "Exif.Sharpness",                    N_("Sharpness"),                      N_("The direction of sharpness processing applied by the camera when the image was shot.") };
static ImageTag TAG_EXIF_SOFTWARE                     = { "Exif.Software",                     N_("Software"),                       N_("This tag records the name and version of the software or firmware of the camera or image input device used to generate the image. When the field is left blank, it is treated as unknown.") };
static ImageTag TAG_EXIF_SPATIALFREQUENCYRESPONSE     = { "Exif.SpatialFrequencyResponse",     N_("Spatial Frequency Response"),     N_("This tag records the camera or input device spatial frequency table and SFR values in the direction of image width, image height, and diagonal direction, as specified in ISO 12233.") };
static ImageTag TAG_EXIF_SPECTRALSENSITIVITY          = { "Exif.SpectralSensitivity",          N_("Spectral Sensitivity"),           N_("The spectral sensitivity of each channel of the camera used.") };
static ImageTag TAG_EXIF_STRIPBYTECOUNTS              = { "Exif.StripByteCounts",              N_("Strip Byte Count"),               N_("The total number of bytes in each strip. With JPEG compressed data this designation is not needed and is omitted.") };
static ImageTag TAG_EXIF_STRIPOFFSETS                 = { "Exif.StripOffsets",                 N_("Strip Offsets"),                  N_("For each strip, the byte offset of that strip. It is recommended that this be selected so the number of strip bytes does not exceed 64 Kbytes. With JPEG compressed data this designation is not needed and is omitted.") };
static ImageTag TAG_EXIF_SUBIFDS                      = { "Exif.SubIFDs",                      N_("Sub IFD Offsets"),                N_("Defined by Adobe Corporation to enable TIFF Trees within a TIFF file.") };
static ImageTag TAG_EXIF_SUBJECTAREA                  = { "Exif.SubjectArea",                  N_("Subject Area"),                   N_("The location and area of the main subject in the overall scene.") };
static ImageTag TAG_EXIF_SUBJECTDISTANCE              = { "Exif.SubjectDistance",              N_("Subject Distance"),               N_("The distance to the subject, given in meters.") };
static ImageTag TAG_EXIF_SUBJECTDISTANCERANGE         = { "Exif.SubjectDistanceRange",         N_("Subject Distance Range"),         N_("The distance to the subject.") };
static ImageTag TAG_EXIF_SUBJECTLOCATION              = { "Exif.SubjectLocation",              N_("Subject Location"),               N_("The location of the main subject in the scene. The value of this tag represents the pixel at the center of the main subject relative to the left edge, prior to rotation processing as per the <Exif.Rotation> tag. The first value indicates the X column number and second indicates the Y row number.") };
static ImageTag TAG_EXIF_SUBSECTIME                   = { "Exif.SubsecTime",                   N_("Subsec Time"),                    N_("Fractions of seconds for the <Exif.DateTime> tag.") };
static ImageTag TAG_EXIF_SUBSECTIMEDIGITIZED          = { "Exif.SubSecTimeDigitized",          N_("Subsec Time Digitized"),          N_("Fractions of seconds for the <Exif.DateTimeDigitized> tag.") };
static ImageTag TAG_EXIF_SUBSECTIMEORIGINAL           = { "Exif.SubSecTimeOriginal",           N_("Subsec Time Original"),           N_("Fractions of seconds for the <Exif.DateTimeOriginal> tag.") };
static ImageTag TAG_EXIF_TIFFEPSTANDARDID             = { "Exif.TIFF_EPStandardID",            N_("TIFF/EP Standard ID"),            N_("TIFF/EP Standard ID.") };
static ImageTag TAG_EXIF_TRANSFERFUNCTION             = { "Exif.TransferFunction",             N_("Transfer Function"),              N_("A transfer function for the image, described in tabular style. Normally this tag is not necessary, since color space is specified in <Exif.ColorSpace> tag.") };
static ImageTag TAG_EXIF_TRANSFERRANGE                = { "Exif.TransferRange",                N_("Transfer Range"),                 N_("Transfer range.") };
static ImageTag TAG_EXIF_WHITEPOINT                   = { "Exif.WhitePoint",                   N_("White Point"),                    N_("The chromaticity of the white point of the image. Normally this tag is not necessary, since color space is specified in <Exif.ColorSpace> tag.") };
static ImageTag TAG_EXIF_XMLPACKET                    = { "Exif.XMLPacket",                    N_("XML Packet"),                     N_("XMP metadata.") };
static ImageTag TAG_EXIF_YCBCRCOEFFICIENTS            = { "Exif.YCbCrCoefficients",            N_("YCbCr Coefficients"),             N_("The matrix coefficients for transformation from RGB to YCbCr image data. No default is given in TIFF; but here “Color Space Guidelines” is used as the default. The color space is declared in a color space information tag, with the default being the value that gives the optimal image characteristics under this condition.") };
static ImageTag TAG_EXIF_YCBCRPOSITIONING             = { "Exif.YCbCrPositioning",             N_("YCbCr Positioning"),              N_("The position of chrominance components in relation to the luminance component. This field is designated only for JPEG compressed data or uncompressed YCbCr data. The TIFF default is 1 (centered); but when Y:Cb:Cr = 4:2:2 it is recommended that 2 (co-sited) be used to record data, in order to improve the image quality when viewed on TV systems. When this field does not exist, the reader shall assume the TIFF default. In the case of Y:Cb:Cr = 4:2:0, the TIFF default (centered) is recommended. If the reader does not have the capability of supporting both kinds of <Exif.YCbCrPositioning>, it shall follow the TIFF default regardless of the value in this field. It is preferable that readers be able to support both centered and co-sited positioning.") };
static ImageTag TAG_EXIF_YCBCRSUBSAMPLING             = { "Exif.YCbCrSubSampling",             N_("YCbCr Sub-Sampling"),             N_("The sampling ratio of chrominance components in relation to the luminance component. In JPEG compressed data a JPEG marker is used instead of this tag.") };

static ImageTag TAG_IPTC_BYLINE              = { "IPTC.Byline",                N_("By-line"),                          N_("Name of the creator of the object, e.g. writer, photographer or graphic artist (multiple values allowed).") };
static ImageTag TAG_IPTC_BYLINETITLE         = { "IPTC.BylineTitle",           N_("By-line Title"),                    N_("Title of the creator or creators of the object.") };
static ImageTag TAG_IPTC_CAPTION             = { "IPTC.Caption",               N_("Caption, Abstract"),                N_("A textual description of the data.") };
static ImageTag TAG_IPTC_HEADLINE            = { "IPTC.Headline",              N_("Headline"),                         N_("A publishable entry providing a synopsis of the contents of the object.") };
static ImageTag TAG_IPTC_SUBLOCATION         = { "IPTC.Sublocation",           N_("Sub-location"),                     N_("The location within a city from which the object originates.") };
static ImageTag TAG_IPTC_CITY                = { "IPTC.City",                  N_("City"),                             N_("City of object origin.") };
static ImageTag TAG_IPTC_PROVINCE            = { "IPTC.Province",              N_("Province, State"),                  N_("The Province/State where the object originates.") };
static ImageTag TAG_IPTC_COUNTRYCODE         = { "IPTC.CountryCode",           N_("Country Code"),                     N_("The code of the country/primary location where the object was created.") };
static ImageTag TAG_IPTC_COUNTRYNAME         = { "IPTC.CountryName",           N_("Country Name"),                     N_("The name of the country/primary location where the object was created.") };
static ImageTag TAG_IPTC_CONTACT             = { "IPTC.Contact",               N_("Contact"),                          N_("The person or organization which can provide further background information on the object (multiple values allowed).") };
static ImageTag TAG_IPTC_COPYRIGHTNOTICE     = { "IPTC.CopyrightNotice",       N_("Copyright Notice"),                 N_("Any necessary copyright notice.") };
static ImageTag TAG_IPTC_CREDIT              = { "IPTC.Credit",                N_("Credit"),                           N_("Identifies the provider of the object, not necessarily the owner/creator.") };
static ImageTag TAG_IPTC_KEYWORDS            = { "IPTC.Keywords",              N_("Keywords"),                         N_("Used to indicate specific information retrieval words (multiple values allowed).") };
static ImageTag TAG_IPTC_DIGITALCREATIONDATE = { "IPTC.DigitalCreationDate",   N_("Digital Creation Date"),            N_("The date the digital representation of the object was created.") };
static ImageTag TAG_IPTC_DIGITALCREATIONTIME = { "IPTC.DigitalCreationTime",   N_("Digital Creation Time"),            N_("The time the digital representation of the object was created.") };
static ImageTag TAG_IPTC_IMAGEORIENTATION    = { "IPTC.ImageOrientation",      N_("Image Orientation"),                N_("The layout of the image area: “P” for portrait, “L” for landscape, and “S” for square.") };
static ImageTag TAG_IPTC_SPECIALINSTRUCTIONS = { "IPTC.SpecialInstructions",   N_("Special Instructions"),             N_("Other editorial instructions concerning the use of the object.") };
static ImageTag TAG_IPTC_URGENCY             = { "IPTC.Urgency",               N_("Urgency"),                          N_("Specifies the editorial urgency of content and not necessarily the envelope handling priority. “1” is most urgent, “5” normal, and “8” least urgent.") };
static ImageTag TAG_IPTC_ACTIONADVISED       = { "IPTC.ActionAdvised",         N_("Action Advised"),                   N_("The type of action that this object provides to a previous object. “01” Object Kill, “02” Object Replace, “03” Object Append, “04” Object Reference.") };
static ImageTag TAG_IPTC_ARMID               = { "IPTC.ARMID",                 N_("ARM Identifier"),                   N_("Identifies the Abstract Relationship Method (ARM).") };
static ImageTag TAG_IPTC_ARMVERSION          = { "IPTC.ARMVersion",            N_("ARM Version"),                      N_("Identifies the version of the Abstract Relationship Method (ARM).") };
static ImageTag TAG_IPTC_AUDIODURATION       = { "IPTC.AudioDuration",         N_("Audio Duration"),                   N_("The running time of the audio data in the form HHMMSS.") };
static ImageTag TAG_IPTC_AUDIOOUTCUE         = { "IPTC.AudioOutcue",           N_("Audio Outcue"),                     N_("The content at the end of the audio data.") };
static ImageTag TAG_IPTC_AUDIOSAMPLINGRATE   = { "IPTC.AudioSamplingRate",     N_("Audio Sampling Rate"),              N_("The sampling rate in Hz of the audio data.") };
static ImageTag TAG_IPTC_AUDIOSAMPLINGRES    = { "IPTC.AudioSamplingRes",      N_("Audio Sampling Resolution"),        N_("The number of bits in each audio sample.") };
static ImageTag TAG_IPTC_AUDIOTYPE           = { "IPTC.AudioType",             N_("Audio Type"),                       N_("The number of channels and type of audio (music, text, etc.) in the object.") };
static ImageTag TAG_IPTC_CATEGORY            = { "IPTC.Category",              N_("Category"),                         N_("Identifies the subject of the object in the opinion of the provider (Deprecated).") };
static ImageTag TAG_IPTC_CHARACTERSET        = { "IPTC.CharacterSet",          N_("Coded Character Set"),              N_("Control functions used for the announcement, invocation or designation of coded character sets.") };
static ImageTag TAG_IPTC_CONFIRMEDDATASIZE   = { "IPTC.ConfirmedDataSize",     N_("Confirmed Data Size"),              N_("Total size of the object data.") };
static ImageTag TAG_IPTC_CONTENTLOCCODE      = { "IPTC.ContentLocCode",        N_("Content Location Code"),            N_("The code of a country/geographical location referenced by the content of the object (multiple values allowed).") };
static ImageTag TAG_IPTC_CONTENTLOCNAME      = { "IPTC.ContentLocName",        N_("Content Location Name"),            N_("A full, publishable name of a country/geographical location referenced by the content of the object (multiple values allowed).") };
static ImageTag TAG_IPTC_DATECREATED         = { "IPTC.DateCreated",           N_("Date Created"),                     N_("The date the intellectual content of the object was created rather than the date of the creation of the physical representation.") };
static ImageTag TAG_IPTC_DATESENT            = { "IPTC.DateSent",              N_("Date Sent"),                        N_("The day the service sent the material.") };
static ImageTag TAG_IPTC_DESTINATION         = { "IPTC.Destination",           N_("Destination"),                      N_("Routing information.") };
static ImageTag TAG_IPTC_EDITORIALUPDATE     = { "IPTC.EditorialUpdate",       N_("Editorial Update"),                 N_("The type of update this object provides to a previous object. The link to the previous object is made using the ARM. “01” indicates an additional language.") };
static ImageTag TAG_IPTC_EDITSTATUS          = { "IPTC.EditStatus",            N_("Edit Status"),                      N_("Status of the object, according to the practice of the provider.") };
static ImageTag TAG_IPTC_ENVELOPENUM         = { "IPTC.EnvelopeNum",           N_("Envelope Number"),                  N_("A number unique for the date and the service ID.") };
static ImageTag TAG_IPTC_ENVELOPEPRIORITY    = { "IPTC.EnvelopePriority",      N_("Envelope Priority"),                N_("Specifies the envelope handling priority and not the editorial urgency. “1” for most urgent, “5” for normal, and “8” for least urgent. “9” is user-defined.") };
static ImageTag TAG_IPTC_EXPIRATIONDATE      = { "IPTC.ExpirationDate",        N_("Expiration Date"),                  N_("Designates the latest date the provider intends the object to be used.") };
static ImageTag TAG_IPTC_EXPIRATIONTIME      = { "IPTC.ExpirationTime",        N_("Expiration Time"),                  N_("Designates the latest time the provider intends the object to be used.") };
static ImageTag TAG_IPTC_FILEFORMAT          = { "IPTC.FileFormat",            N_("File Format"),                      N_("File format of the data described by this metadata.") };
static ImageTag TAG_IPTC_FILEVERSION         = { "IPTC.FileVersion",           N_("File Version"),                     N_("Version of the file format.") };
static ImageTag TAG_IPTC_FIXTUREID           = { "IPTC.FixtureID",             N_("Fixture Identifier"),               N_("Identifies objects that recur often and predictably, enabling users to immediately find or recall such an object.") };
static ImageTag TAG_IPTC_IMAGETYPE           = { "IPTC.ImageType",             N_("Image Type"),                       N_("The data format of the image object.") };
static ImageTag TAG_IPTC_LANGUAGEID          = { "IPTC.LanguageID",            N_("Language Identifier"),              N_("The major national language of the object, according to the 2-letter codes of ISO 639:1988.") };
static ImageTag TAG_IPTC_MAXOBJECTSIZE       = { "IPTC.MaxObjectSize",         N_("Maximum Object Size"),              N_("The largest possible size of the object if the size is not known.") };
static ImageTag TAG_IPTC_MAXSUBFILESIZE      = { "IPTC.MaxSubfileSize",        N_("Max Subfile Size"),                 N_("The maximum size for a subfile dataset containing a portion of the object data.") };
static ImageTag TAG_IPTC_MODELVERSION        = { "IPTC.ModelVersion",          N_("Model Version"),                    N_("Version of IIM part 1.") };
static ImageTag TAG_IPTC_OBJECTATTRIBUTE     = { "IPTC.ObjectAttribute",       N_("Object Attribute Reference"),       N_("Defines the nature of the object independent of the subject (multiple values allowed).") };
static ImageTag TAG_IPTC_OBJECTCYCLE         = { "IPTC.ObjectCycle",           N_("Object Cycle"),                     N_("Where “a” is morning, “p” is evening, “b” is both.") };
static ImageTag TAG_IPTC_OBJECTNAME          = { "IPTC.ObjectName",            N_("Object Name"),                      N_("A shorthand reference for the object.") };
static ImageTag TAG_IPTC_OBJECTSIZEANNOUNCED = { "IPTC.ObjectSizeAnnounced",   N_("Object Size Announced"),            N_("The total size of the object data if it is known.") };
static ImageTag TAG_IPTC_OBJECTTYPE          = { "IPTC.ObjectType",            N_("Object Type Reference"),            N_("Distinguishes between different types of objects within the IIM.") };
static ImageTag TAG_IPTC_ORIGINATINGPROGRAM  = { "IPTC.OriginatingProgram",    N_("Originating Program"),              N_("The type of program used to originate the object.") };
static ImageTag TAG_IPTC_ORIGTRANSREF        = { "IPTC.OrigTransRef",          N_("Original Transmission Reference"),  N_("A code representing the location of original transmission.") };
static ImageTag TAG_IPTC_PREVIEWDATA         = { "IPTC.PreviewData",           N_("Preview Data"),                     N_("The object preview data.") };
static ImageTag TAG_IPTC_PREVIEWFORMAT       = { "IPTC.PreviewFileFormat",     N_("Preview File Format"),              N_("Binary value indicating the file format of the object preview data.") };
static ImageTag TAG_IPTC_PREVIEWFORMATVER    = { "IPTC.PreviewFileFormatVer",  N_("Preview File Format Version"),      N_("The version of the preview file format.") };
static ImageTag TAG_IPTC_PRODUCTID           = { "IPTC.ProductID",             N_("Product ID"),                       N_("Allows a provider to identify subsets of its overall service.") };
static ImageTag TAG_IPTC_PROGRAMVERSION      = { "IPTC.ProgramVersion",        N_("Program Version"),                  N_("The version of the originating program.") };
static ImageTag TAG_IPTC_RASTERIZEDCAPTION   = { "IPTC.RasterizedCaption",     N_("Rasterized Caption"),               N_("Contains rasterized object description and is used where characters that have not been coded are required for the caption.") };
static ImageTag TAG_IPTC_RECORDVERSION       = { "IPTC.RecordVersion",         N_("Record Version"),                   N_("Identifies the version of the IIM, Part 2.") };
static ImageTag TAG_IPTC_REFERENCEDATE       = { "IPTC.RefDate",               N_("Reference Date"),                   N_("The date of a prior envelope to which the current object refers.") };
static ImageTag TAG_IPTC_REFERENCENUMBER     = { "IPTC.RefNumber",             N_("Reference Number"),                 N_("The Envelope Number of a prior envelope to which the current object refers.") };
static ImageTag TAG_IPTC_REFERENCESERVICE    = { "IPTC.RefService",            N_("Reference Service"),                N_("The Service Identifier of a prior envelope to which the current object refers.") };
static ImageTag TAG_IPTC_RELEASEDATE         = { "IPTC.ReleaseDate",           N_("Release Date"),                     N_("Designates the earliest date the provider intends the object to be used.") };
static ImageTag TAG_IPTC_RELEASETIME         = { "IPTC.ReleaseTime",           N_("Release Time"),                     N_("Designates the earliest time the provider intends the object to be used.") };
static ImageTag TAG_IPTC_SERVICEID           = { "IPTC.ServiceID",             N_("Service Identifier"),               N_("Identifies the provider and product.") };
static ImageTag TAG_IPTC_SIZEMODE            = { "IPTC.SizeMode",              N_("Size Mode"),                        N_("Set to 0 if the size of the object is known and 1 if not known.") };
static ImageTag TAG_IPTC_SOURCE              = { "IPTC.Source",                N_("Source"),                           N_("The original owner of the intellectual content of the object.") };
static ImageTag TAG_IPTC_SUBFILE             = { "IPTC.Subfile",               N_("Subfile"),                          N_("The object data itself. Subfiles must be sequential so that the subfiles may be reassembled.") };
static ImageTag TAG_IPTC_SUBJECTREFERENCE    = { "IPTC.SubjectRef",            N_("Subject Reference"),                N_("A structured definition of the subject matter. It must contain an IPR, an 8 digit Subject Reference Number and an optional Subject Name, Subject Matter Name, and Subject Detail Name each separated by a colon (:).") };
static ImageTag TAG_IPTC_SUPPLCATEGORY       = { "IPTC.SupplCategory",         N_("Supplemental Category"),            N_("Further refines the subject of the object (Deprecated).") };
static ImageTag TAG_IPTC_TIMECREATED         = { "IPTC.TimeCreated",           N_("Time Created"),                     N_("The time the intellectual content of the object was created rather than the date of the creation of the physical representation (multiple values allowed).") };
static ImageTag TAG_IPTC_TIMESENT            = { "IPTC.TimeSent",              N_("Time Sent"),                        N_("The time the service sent the material.") };
static ImageTag TAG_IPTC_UNO                 = { "IPTC.UNO",                   N_("Unique Name of Object"),            N_("An eternal, globally unique identification for the object, independent of provider and for any media form.") };
static ImageTag TAG_IPTC_WRITEREDITOR        = { "IPTC.WriterEditor",          N_("Writer/Editor"),                    N_("The name of the person involved in the writing, editing or correcting the object or caption/abstract (multiple values allowed).") };

static ImageTag *IMAGE_TAGS[] = {
    &TAG_IMAGE_ALBUM,           &TAG_IMAGE_MAKE,            &TAG_IMAGE_MODEL,       &TAG_IMAGE_COMMENTS,
    &TAG_IMAGE_COPYRIGHT,       &TAG_IMAGE_CREATOR,         &TAG_IMAGE_DATE,        &TAG_IMAGE_DESCRIPTION,
    &TAG_IMAGE_EXPOSUREPROGRAM, &TAG_IMAGE_EXPOSURETIME,    &TAG_IMAGE_FLASH,       &TAG_IMAGE_FNUMBER,
    &TAG_IMAGE_FOCALLENGTH,     &TAG_IMAGE_ISOSPEED,        &TAG_IMAGE_KEYWORDS,    &TAG_IMAGE_METERINGMODE,
    &TAG_IMAGE_ORIENTATION,     &TAG_IMAGE_SOFTWARE,        &TAG_IMAGE_TITLE,       &TAG_IMAGE_WHITEBALANCE,
    &TAG_IMAGE_WIDTH,           &TAG_IMAGE_HEIGHT,
    &TAG_EXIF_COPYRIGHT,                   &TAG_EXIF_DATETIME,                    &TAG_EXIF_EXPOSUREBIASVALUE,           &TAG_EXIF_EXPOSUREMODE,
    &TAG_EXIF_EXPOSUREPROGRAM,             &TAG_EXIF_FLASH,                       &TAG_EXIF_FLASHENERGY,                 &TAG_EXIF_FNUMBER,
    &TAG_EXIF_FOCALLENGTH,                 &TAG_EXIF_ISOSPEEDRATINGS,             &TAG_EXIF_MAXAPERTUREVALUE,            &TAG_EXIF_METERINGMODE,
    &TAG_EXIF_SHUTTERSPEEDVALUE,           &TAG_EXIF_WHITEBALANCE,                &TAG_EXIF_PIXELXDIMENSION,             &TAG_EXIF_PIXELYDIMENSION,
    &TAG_EXIF_XRESOLUTION,                 &TAG_EXIF_YRESOLUTION,                 &TAG_EXIF_IMAGELENGTH,                 &TAG_EXIF_IMAGEWIDTH,
    &TAG_EXIF_CUSTOMRENDERED,              &TAG_EXIF_COLORSPACE,                  &TAG_EXIF_DOCUMENTNAME,                &TAG_EXIF_USERCOMMENT,
    &TAG_EXIF_APERTUREVALUE,               &TAG_EXIF_ARTIST,                      &TAG_EXIF_BATTERYLEVEL,                &TAG_EXIF_BITSPERSAMPLE,
    &TAG_EXIF_BRIGHTNESSVALUE,             &TAG_EXIF_CFAPATTERN,                  &TAG_EXIF_CFAREPEATPATTERNDIM,         &TAG_EXIF_COMPONENTSCONFIGURATION,
    &TAG_EXIF_COMPRESSEDBITSPERPIXEL,      &TAG_EXIF_COMPRESSION,                 &TAG_EXIF_CONTRAST,                    &TAG_EXIF_DATETIMEDIGITIZED,
    &TAG_EXIF_DATETIMEORIGINAL,            &TAG_EXIF_DEVICESETTINGDESCRIPTION,    &TAG_EXIF_DIGITALZOOMRATIO,            &TAG_EXIF_EXIFVERSION,
    &TAG_EXIF_EXPOSUREINDEX,               &TAG_EXIF_EXPOSURETIME,                &TAG_EXIF_FILESOURCE,                  &TAG_EXIF_FILLORDER,
    &TAG_EXIF_FLASHPIXVERSION,             &TAG_EXIF_FOCALLENGTHIN35MMFILM,       &TAG_EXIF_FOCALPLANERESOLUTIONUNIT,    &TAG_EXIF_FOCALPLANEXRESOLUTION,
    &TAG_EXIF_FOCALPLANEYRESOLUTION,       &TAG_EXIF_GAINCONTROL,                 &TAG_EXIF_GAMMA,                       &TAG_EXIF_GPSALTITUDE,
    &TAG_EXIF_GPSALTITUDEREF,              &TAG_EXIF_GPSINFOIFDPOINTER,           &TAG_EXIF_GPSLATITUDE,                 &TAG_EXIF_GPSLATITUDEREF,
    &TAG_EXIF_GPSLONGITUDE,                &TAG_EXIF_GPSLONGITUDEREF,             &TAG_EXIF_GPSVERSIONID,                &TAG_EXIF_IMAGEDESCRIPTION,
    &TAG_EXIF_IMAGERESOURCES,              &TAG_EXIF_IMAGEUNIQUEID,               &TAG_EXIF_INTERCOLORPROFILE,           &TAG_EXIF_INTEROPERABILITYINDEX,
    &TAG_EXIF_INTEROPERABILITYVERSION,     &TAG_EXIF_IPTCNAA,                     &TAG_EXIF_JPEGINTERCHANGEFORMAT,       &TAG_EXIF_JPEGINTERCHANGEFORMATLENGTH,
    &TAG_EXIF_JPEGPROC,                    &TAG_EXIF_LIGHTSOURCE,                 &TAG_EXIF_MAKE,                        &TAG_EXIF_MAKERNOTE,
    &TAG_EXIF_MODEL,                       &TAG_EXIF_NEWCFAPATTERN,               &TAG_EXIF_NEWSUBFILETYPE,              &TAG_EXIF_OECF,
    &TAG_EXIF_ORIENTATION,                 &TAG_EXIF_PHOTOMETRICINTERPRETATION,   &TAG_EXIF_PLANARCONFIGURATION,         &TAG_EXIF_PRIMARYCHROMATICITIES,
    &TAG_EXIF_REFERENCEBLACKWHITE,         &TAG_EXIF_RELATEDIMAGEFILEFORMAT,      &TAG_EXIF_RELATEDIMAGELENGTH,          &TAG_EXIF_RELATEDIMAGEWIDTH,
    &TAG_EXIF_RELATEDSOUNDFILE,            &TAG_EXIF_RESOLUTIONUNIT,              &TAG_EXIF_ROWSPERSTRIP,                &TAG_EXIF_SAMPLESPERPIXEL,
    &TAG_EXIF_SATURATION,                  &TAG_EXIF_SCENECAPTURETYPE,            &TAG_EXIF_SCENETYPE,                   &TAG_EXIF_SENSINGMETHOD,
    &TAG_EXIF_SHARPNESS,                   &TAG_EXIF_SOFTWARE,                    &TAG_EXIF_SPATIALFREQUENCYRESPONSE,    &TAG_EXIF_SPECTRALSENSITIVITY,
    &TAG_EXIF_STRIPBYTECOUNTS,             &TAG_EXIF_STRIPOFFSETS,                &TAG_EXIF_SUBIFDS,                     &TAG_EXIF_SUBJECTAREA,
    &TAG_EXIF_SUBJECTDISTANCE,             &TAG_EXIF_SUBJECTDISTANCERANGE,        &TAG_EXIF_SUBJECTLOCATION,             &TAG_EXIF_SUBSECTIME,
    &TAG_EXIF_SUBSECTIMEDIGITIZED,         &TAG_EXIF_SUBSECTIMEORIGINAL,          &TAG_EXIF_TIFFEPSTANDARDID,            &TAG_EXIF_TRANSFERFUNCTION,
    &TAG_EXIF_TRANSFERRANGE,               &TAG_EXIF_WHITEPOINT,                  &TAG_EXIF_XMLPACKET,                   &TAG_EXIF_YCBCRCOEFFICIENTS,
    &TAG_EXIF_YCBCRPOSITIONING,            &TAG_EXIF_YCBCRSUBSAMPLING,
    &TAG_IPTC_BYLINE,              &TAG_IPTC_BYLINETITLE,         &TAG_IPTC_CAPTION,             &TAG_IPTC_HEADLINE,
    &TAG_IPTC_SUBLOCATION,         &TAG_IPTC_CITY,                &TAG_IPTC_PROVINCE,            &TAG_IPTC_COUNTRYCODE,
    &TAG_IPTC_COUNTRYNAME,         &TAG_IPTC_CONTACT,             &TAG_IPTC_COPYRIGHTNOTICE,     &TAG_IPTC_CREDIT,
    &TAG_IPTC_KEYWORDS,            &TAG_IPTC_DIGITALCREATIONDATE, &TAG_IPTC_DIGITALCREATIONTIME, &TAG_IPTC_IMAGEORIENTATION,
    &TAG_IPTC_SPECIALINSTRUCTIONS, &TAG_IPTC_URGENCY,             &TAG_IPTC_ACTIONADVISED,       &TAG_IPTC_ARMID,
    &TAG_IPTC_ARMVERSION,          &TAG_IPTC_AUDIODURATION,       &TAG_IPTC_AUDIOOUTCUE,         &TAG_IPTC_AUDIOSAMPLINGRATE,
    &TAG_IPTC_AUDIOSAMPLINGRES,    &TAG_IPTC_AUDIOTYPE,           &TAG_IPTC_CATEGORY,            &TAG_IPTC_CHARACTERSET,
    &TAG_IPTC_CONFIRMEDDATASIZE,   &TAG_IPTC_CONTENTLOCCODE,      &TAG_IPTC_CONTENTLOCNAME,      &TAG_IPTC_DATECREATED,
    &TAG_IPTC_DATESENT,            &TAG_IPTC_DESTINATION,         &TAG_IPTC_EDITORIALUPDATE,     &TAG_IPTC_EDITSTATUS,
    &TAG_IPTC_ENVELOPENUM,         &TAG_IPTC_ENVELOPEPRIORITY,    &TAG_IPTC_EXPIRATIONDATE,      &TAG_IPTC_EXPIRATIONTIME,
    &TAG_IPTC_FILEFORMAT,          &TAG_IPTC_FILEVERSION,         &TAG_IPTC_FIXTUREID,           &TAG_IPTC_IMAGETYPE,
    &TAG_IPTC_LANGUAGEID,          &TAG_IPTC_MAXOBJECTSIZE,       &TAG_IPTC_MAXSUBFILESIZE,      &TAG_IPTC_MODELVERSION,
    &TAG_IPTC_OBJECTATTRIBUTE,     &TAG_IPTC_OBJECTCYCLE,         &TAG_IPTC_OBJECTNAME,          &TAG_IPTC_OBJECTSIZEANNOUNCED,
    &TAG_IPTC_OBJECTTYPE,          &TAG_IPTC_ORIGINATINGPROGRAM,  &TAG_IPTC_ORIGTRANSREF,        &TAG_IPTC_PREVIEWDATA,
    &TAG_IPTC_PREVIEWFORMAT,       &TAG_IPTC_PREVIEWFORMATVER,    &TAG_IPTC_PRODUCTID,           &TAG_IPTC_PROGRAMVERSION,
    &TAG_IPTC_RASTERIZEDCAPTION,   &TAG_IPTC_RECORDVERSION,       &TAG_IPTC_REFERENCEDATE,       &TAG_IPTC_REFERENCENUMBER,
    &TAG_IPTC_REFERENCESERVICE,    &TAG_IPTC_RELEASEDATE,         &TAG_IPTC_RELEASETIME,         &TAG_IPTC_SERVICEID,
    &TAG_IPTC_SIZEMODE,            &TAG_IPTC_SOURCE,              &TAG_IPTC_SUBFILE,             &TAG_IPTC_SUBJECTREFERENCE,
    &TAG_IPTC_SUPPLCATEGORY,       &TAG_IPTC_TIMECREATED,         &TAG_IPTC_TIMESENT,            &TAG_IPTC_UNO,
    &TAG_IPTC_WRITEREDITOR
};


template <typename T>
void readTags(GnomeCmdExiv2Plugin *plugin, const T &data, GnomeCmdFileMetadataExtractorAddTag add, gpointer user_data)
{
    auto priv = (GnomeCmdExiv2PluginPrivate *) gnome_cmd_exiv2_plugin_get_instance_private (plugin);

    if (data.empty())
        return;

    typename T::const_iterator end = data.end();

    for (typename T::const_iterator i = data.begin(); i != end; ++i)
    {
        ImageTag *tag = (ImageTag *) g_hash_table_lookup (priv->exiv2_tags, i->key().c_str());
        if (tag == nullptr || tag == &TAG_EXIF_MAKERNOTE)
        {
            continue;
        }
        else if (tag == &TAG_EXIF_EXIFVERSION || tag == &TAG_EXIF_FLASHPIXVERSION || tag == &TAG_EXIF_INTEROPERABILITYVERSION)
        {
            vector<Exiv2::byte> buff(i->value().size() + 1);
            i->value().copy(&buff[0], invalidByteOrder);
            add(tag->id, (char *) &buff[0], user_data);
        }
        else if (tag == &TAG_IMAGE_DESCRIPTION || tag == &TAG_IPTC_CAPTION)
        {
            std::string value = i->toString();
            add(TAG_FILE_DESCRIPTION, value.c_str(), user_data);
            add(tag->id, value.c_str(), user_data);
        }
        else if (tag == &TAG_IMAGE_KEYWORDS || tag == &TAG_IPTC_KEYWORDS)
        {
            std::string value = i->toString();
            add(TAG_FILE_KEYWORDS, value.c_str(), user_data);
            add(tag->id, value.c_str(), user_data);
        }
        else if (tag == &TAG_IMAGE_CREATOR || tag == &TAG_IPTC_BYLINE)
        {
            std::string value = i->toString();
            add(TAG_FILE_PUBLISHER, value.c_str(), user_data);
            add(tag->id, value.c_str(), user_data);
        }
        else
        {
            std::string value = i->toString();
            add(tag->id, value.c_str(), user_data);
        }
    }
}


GStrv supported_tags (GnomeCmdFileMetadataExtractor *plugin)
{
    GStrvBuilder *builder = g_strv_builder_new ();
    for (auto tag : IMAGE_TAGS)
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
    if (!g_strcmp0("Image", cls))
        return g_strdup(_("Image"));
    return nullptr;
}


gchar *tag_name (GnomeCmdFileMetadataExtractor *plugin, const gchar *tag_id)
{
    for (auto tag : IMAGE_TAGS)
        if (!g_strcmp0(tag->id, tag_id))
            return g_strdup(_(tag->name));
    return nullptr;
}


gchar *tag_description (GnomeCmdFileMetadataExtractor *plugin, const gchar *tag_id)
{
    for (auto tag : IMAGE_TAGS)
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

    try
    {
#if EXIV2_TEST_VERSION(0,28,0)
        Exiv2::Image::UniquePtr image = ImageFactory::open(fname);
#else
        Image::AutoPtr image = ImageFactory::open(fname);
#endif

        image->readMetadata();

        readTags (GNOME_CMD_EXIV2_PLUGIN (plugin), image->exifData(), add, user_data);
        readTags (GNOME_CMD_EXIV2_PLUGIN (plugin), image->iptcData(), add, user_data);
    }

#if EXIV2_TEST_VERSION(0,28,0)
    catch (Error &e)
#else
    catch (AnyError &e)
#endif
    {
    }
}


static void finalize (GObject *object)
{
    auto plugin = GNOME_CMD_EXIV2_PLUGIN (object);
    auto priv = (GnomeCmdExiv2PluginPrivate *) gnome_cmd_exiv2_plugin_get_instance_private (plugin);

    g_clear_pointer (&priv->exiv2_tags, g_hash_table_unref);

    G_OBJECT_CLASS (gnome_cmd_exiv2_plugin_parent_class)-> finalize (object);
}


static void gnome_cmd_exiv2_plugin_class_init (GnomeCmdExiv2PluginClass *klass)
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


static void gnome_cmd_exiv2_plugin_init (GnomeCmdExiv2Plugin *plugin)
{
    auto priv = (GnomeCmdExiv2PluginPrivate *) gnome_cmd_exiv2_plugin_get_instance_private (plugin);

    priv->exiv2_tags = g_hash_table_new (g_str_hash, g_str_equal);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonSi.ApertureValue",               &TAG_EXIF_APERTUREVALUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Olympus.ApertureValue",               &TAG_EXIF_APERTUREVALUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ApertureValue",                 &TAG_EXIF_APERTUREVALUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.Artist",                        &TAG_EXIF_ARTIST);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.BatteryLevel",                  &TAG_EXIF_BATTERYLEVEL);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.BitsPerSample",                 &TAG_EXIF_BITSPERSAMPLE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.Brightness",             &TAG_EXIF_BRIGHTNESSVALUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Olympus.Brightness",                  &TAG_EXIF_BRIGHTNESSVALUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.BrightnessValue",               &TAG_EXIF_BRIGHTNESSVALUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.CFAPattern",                    &TAG_EXIF_CFAPATTERN);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.CFAPattern",                    &TAG_EXIF_CFAPATTERN);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.CFARepeatPatternDim",           &TAG_EXIF_CFAREPEATPATTERNDIM);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs7D.ColorSpace",              &TAG_EXIF_COLORSPACE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Nikon3.ColorSpace",                   &TAG_EXIF_COLORSPACE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ColorSpace",                    &TAG_EXIF_COLORSPACE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Sigma.ColorSpace",                    &TAG_EXIF_COLORSPACE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ComponentsConfiguration",       &TAG_EXIF_COMPONENTSCONFIGURATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.CompressedBitsPerPixel",        &TAG_EXIF_COMPRESSEDBITSPERPIXEL);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.Compression",                   &TAG_EXIF_COMPRESSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonCs.Contrast",                    &TAG_EXIF_CONTRAST);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs5D.Contrast",                &TAG_EXIF_CONTRAST);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs7D.Contrast",                &TAG_EXIF_CONTRAST);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.Contrast",               &TAG_EXIF_CONTRAST);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Olympus.Contrast",                    &TAG_EXIF_CONTRAST);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Panasonic.Contrast",                  &TAG_EXIF_CONTRAST);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.Contrast",                      &TAG_EXIF_CONTRAST);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Sigma.Contrast",                      &TAG_EXIF_CONTRAST);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.Copyright",                     &TAG_EXIF_COPYRIGHT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.CustomRendered",                &TAG_EXIF_CUSTOMRENDERED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.DateTime",                      &TAG_EXIF_DATETIME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.DateTimeDigitized",             &TAG_EXIF_DATETIMEDIGITIZED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.DateTimeOriginal",              &TAG_EXIF_DATETIMEORIGINAL);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Canon.CameraSettings",                &TAG_EXIF_DEVICESETTINGDESCRIPTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Minolta.CameraSettings5D",            &TAG_EXIF_DEVICESETTINGDESCRIPTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Minolta.CameraSettings7D",            &TAG_EXIF_DEVICESETTINGDESCRIPTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Minolta.CameraSettingsStdNew",        &TAG_EXIF_DEVICESETTINGDESCRIPTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Minolta.CameraSettingsStdOld",        &TAG_EXIF_DEVICESETTINGDESCRIPTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Minolta.CameraSettingsZ1",            &TAG_EXIF_DEVICESETTINGDESCRIPTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.DeviceSettingDescription",      &TAG_EXIF_DEVICESETTINGDESCRIPTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.DigitalZoomRatio",              &TAG_EXIF_DIGITALZOOMRATIO);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.DocumentName",                  &TAG_EXIF_DOCUMENTNAME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ExifVersion",                   &TAG_EXIF_EXIFVERSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ExposureBiasValue",             &TAG_EXIF_EXPOSUREBIASVALUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ExposureIndex",                 &TAG_EXIF_EXPOSUREINDEX);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs5D.ExposureMode",            &TAG_EXIF_EXPOSUREMODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs7D.ExposureMode",            &TAG_EXIF_EXPOSUREMODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.ExposureMode",           &TAG_EXIF_EXPOSUREMODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ExposureMode",                  &TAG_EXIF_EXPOSUREMODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Sigma.ExposureMode",                  &TAG_EXIF_EXPOSUREMODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Fujifilm.FileSource",                 &TAG_EXIF_FILESOURCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.FileSource",                    &TAG_EXIF_FILESOURCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.FillOrder",                     &TAG_EXIF_FILLORDER);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.FlashEnergy",                   &TAG_EXIF_FLASHENERGY);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.FlashpixVersion",               &TAG_EXIF_FLASHPIXVERSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.FocalLengthIn35mmFilm",         &TAG_EXIF_FOCALLENGTHIN35MMFILM);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.FocalPlaneResolutionUnit",      &TAG_EXIF_FOCALPLANERESOLUTIONUNIT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.FocalPlaneXResolution",         &TAG_EXIF_FOCALPLANEXRESOLUTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.FocalPlaneYResolution",         &TAG_EXIF_FOCALPLANEYRESOLUTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.GainControl",                   &TAG_EXIF_GAINCONTROL);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.GPSInfo.GPSAltitude",                 &TAG_EXIF_GPSALTITUDE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.GPSInfo.GPSAltitudeRef",              &TAG_EXIF_GPSALTITUDEREF);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.GPSInfo.GPSLatitude",                 &TAG_EXIF_GPSLATITUDE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.GPSInfo.GPSLatitudeRef",              &TAG_EXIF_GPSLATITUDEREF);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.GPSInfo.GPSLongitude",                &TAG_EXIF_GPSLONGITUDE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.GPSInfo.GPSLongitudeRef",             &TAG_EXIF_GPSLONGITUDEREF);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.GPSInfo.GPSVersionID",                &TAG_EXIF_GPSVERSIONID);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.ImageDescription",              &TAG_EXIF_IMAGEDESCRIPTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.ImageLength",                   &TAG_EXIF_IMAGELENGTH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.ImageResources",                &TAG_EXIF_IMAGERESOURCES);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ImageUniqueID",                 &TAG_EXIF_IMAGEUNIQUEID);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.InterColorProfile",             &TAG_EXIF_INTERCOLORPROFILE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Iop.InteroperabilityIndex",           &TAG_EXIF_INTEROPERABILITYINDEX);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Iop.InteroperabilityVersion",         &TAG_EXIF_INTEROPERABILITYVERSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ISOSpeedRatings",               &TAG_EXIF_ISOSPEEDRATINGS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.JPEGInterchangeFormat",         &TAG_EXIF_JPEGINTERCHANGEFORMAT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.JPEGInterchangeFormatLength",   &TAG_EXIF_JPEGINTERCHANGEFORMATLENGTH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.JPEGProc",                      &TAG_EXIF_JPEGPROC);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Nikon3.LightSource",                  &TAG_EXIF_LIGHTSOURCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.LightSource",                   &TAG_EXIF_LIGHTSOURCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.MakerNote",                     &TAG_EXIF_MAKERNOTE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.MaxApertureValue",              &TAG_EXIF_MAXAPERTUREVALUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.NewSubfileType",                &TAG_EXIF_NEWSUBFILETYPE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.OECF",                          &TAG_EXIF_OECF);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.PhotometricInterpretation",     &TAG_EXIF_PHOTOMETRICINTERPRETATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.PixelXDimension",               &TAG_EXIF_PIXELXDIMENSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.PixelYDimension",               &TAG_EXIF_PIXELYDIMENSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.PlanarConfiguration",           &TAG_EXIF_PLANARCONFIGURATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.PrimaryChromaticities",         &TAG_EXIF_PRIMARYCHROMATICITIES);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.ReferenceBlackWhite",           &TAG_EXIF_REFERENCEBLACKWHITE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Iop.RelatedImageFileFormat",          &TAG_EXIF_RELATEDIMAGEFILEFORMAT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Iop.RelatedImageLength",              &TAG_EXIF_RELATEDIMAGELENGTH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Iop.RelatedImageWidth",               &TAG_EXIF_RELATEDIMAGEWIDTH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.RelatedSoundFile",              &TAG_EXIF_RELATEDSOUNDFILE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.ResolutionUnit",                &TAG_EXIF_RESOLUTIONUNIT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.RowsPerStrip",                  &TAG_EXIF_ROWSPERSTRIP);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.SamplesPerPixel",               &TAG_EXIF_SAMPLESPERPIXEL);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonCs.Saturation",                  &TAG_EXIF_SATURATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs5D.Saturation",              &TAG_EXIF_SATURATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs7D.Saturation",              &TAG_EXIF_SATURATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.Saturation",             &TAG_EXIF_SATURATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Nikon3.Saturation",                   &TAG_EXIF_SATURATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.Saturation",                    &TAG_EXIF_SATURATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Sigma.Saturation",                    &TAG_EXIF_SATURATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SceneCaptureType",              &TAG_EXIF_SCENECAPTURETYPE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SceneType",                     &TAG_EXIF_SCENETYPE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SensingMethod",                 &TAG_EXIF_SENSINGMETHOD);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonCs.Sharpness",                   &TAG_EXIF_SHARPNESS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Fujifilm.Sharpness",                  &TAG_EXIF_SHARPNESS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs5D.Sharpness",               &TAG_EXIF_SHARPNESS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs7D.Sharpness",               &TAG_EXIF_SHARPNESS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.Sharpness",              &TAG_EXIF_SHARPNESS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.Sharpness",                     &TAG_EXIF_SHARPNESS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Sigma.Sharpness",                     &TAG_EXIF_SHARPNESS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonSi.ShutterSpeedValue",           &TAG_EXIF_SHUTTERSPEEDVALUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ShutterSpeedValue",             &TAG_EXIF_SHUTTERSPEEDVALUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SpatialFrequencyResponse",      &TAG_EXIF_SPATIALFREQUENCYRESPONSE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SpectralSensitivity",           &TAG_EXIF_SPECTRALSENSITIVITY);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.StripByteCounts",               &TAG_EXIF_STRIPBYTECOUNTS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.StripOffsets",                  &TAG_EXIF_STRIPOFFSETS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.SubIFDs",                       &TAG_EXIF_SUBIFDS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SubjectArea",                   &TAG_EXIF_SUBJECTAREA);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonSi.SubjectDistance",             &TAG_EXIF_SUBJECTDISTANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SubjectDistance",               &TAG_EXIF_SUBJECTDISTANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SubjectDistanceRange",          &TAG_EXIF_SUBJECTDISTANCERANGE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SubjectLocation",               &TAG_EXIF_SUBJECTLOCATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SubSecTime",                    &TAG_EXIF_SUBSECTIME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SubSecTimeDigitized",           &TAG_EXIF_SUBSECTIMEDIGITIZED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.SubSecTimeOriginal",            &TAG_EXIF_SUBSECTIMEORIGINAL);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.TransferFunction",              &TAG_EXIF_TRANSFERFUNCTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.TransferRange",                 &TAG_EXIF_TRANSFERRANGE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.WhitePoint",                    &TAG_EXIF_WHITEPOINT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.XMLPacket",                     &TAG_EXIF_XMLPACKET);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.XResolution",                   &TAG_EXIF_XRESOLUTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.YCbCrCoefficients",             &TAG_EXIF_YCBCRCOEFFICIENTS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.YCbCrPositioning",              &TAG_EXIF_YCBCRPOSITIONING);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.YCbCrSubSampling",              &TAG_EXIF_YCBCRSUBSAMPLING);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.YResolution",                   &TAG_EXIF_YRESOLUTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.XPComment",                     &TAG_IMAGE_COMMENTS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.UserComment",                   &TAG_IMAGE_COMMENTS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.XPAuthor",                      &TAG_IMAGE_CREATOR);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonCs.ExposureProgram",             &TAG_IMAGE_EXPOSUREPROGRAM);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ExposureProgram",               &TAG_IMAGE_EXPOSUREPROGRAM);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs5D.ExposureTime",            &TAG_IMAGE_EXPOSURETIME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs7D.ExposureTime",            &TAG_IMAGE_EXPOSURETIME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.ExposureTime",           &TAG_IMAGE_EXPOSURETIME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.ExposureTime",                  &TAG_IMAGE_EXPOSURETIME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs5D.Flash",                   &TAG_IMAGE_FLASH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs7D.Flash",                   &TAG_IMAGE_FLASH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.Flash",                  &TAG_IMAGE_FLASH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.Flash",                         &TAG_IMAGE_FLASH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs5D.FNumber",                 &TAG_IMAGE_FNUMBER);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs7D.FNumber",                 &TAG_IMAGE_FNUMBER);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.FNumber",                &TAG_IMAGE_FNUMBER);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.FNumber",                       &TAG_IMAGE_FNUMBER);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.FocalLength",            &TAG_IMAGE_FOCALLENGTH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.FocalLength",                   &TAG_IMAGE_FOCALLENGTH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonPi.ImageHeight",                 &TAG_IMAGE_HEIGHT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Olympus.ImageHeight",                 &TAG_IMAGE_HEIGHT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonCs.ISOSpeed",                    &TAG_IMAGE_ISOSPEED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonSi.ISOSpeed",                    &TAG_IMAGE_ISOSPEED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs5D.ISOSpeed",                &TAG_IMAGE_ISOSPEED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs7D.ISOSpeed",                &TAG_IMAGE_ISOSPEED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.ISOSpeed",               &TAG_IMAGE_ISOSPEED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Nikon1.ISOSpeed",                     &TAG_IMAGE_ISOSPEED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Nikon2.ISOSpeed",                     &TAG_IMAGE_ISOSPEED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Nikon3.ISOSpeed",                     &TAG_IMAGE_ISOSPEED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Olympus.ISOSpeed",                    &TAG_IMAGE_ISOSPEED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.XPKeywords",                    &TAG_IMAGE_KEYWORDS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.Keywords",               &TAG_IMAGE_KEYWORDS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.Make",                          &TAG_IMAGE_MAKE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonCs.MeteringMode",                &TAG_IMAGE_METERINGMODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs5D.MeteringMode",            &TAG_IMAGE_METERINGMODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.MeteringMode",           &TAG_IMAGE_METERINGMODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.MeteringMode",                  &TAG_IMAGE_METERINGMODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Sigma.MeteringMode",                  &TAG_IMAGE_METERINGMODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.Model",                         &TAG_IMAGE_MODEL);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.Orientation",                   &TAG_IMAGE_ORIENTATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ImageOrientation",       &TAG_IMAGE_ORIENTATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.Software",                      &TAG_IMAGE_SOFTWARE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Sigma.Software",                      &TAG_IMAGE_SOFTWARE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.XPTitle",                       &TAG_IMAGE_TITLE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonSi.WhiteBalance",                &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Fujifilm.WhiteBalance",               &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs5D.WhiteBalance",            &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCs7D.WhiteBalance",            &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.MinoltaCsNew.WhiteBalance",           &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Nikon1.WhiteBalance",                 &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Nikon2.WhiteBalance",                 &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Nikon3.WhiteBalance",                 &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Olympus.WhiteBalance",                &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Panasonic.WhiteBalance",              &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Photo.WhiteBalance",                  &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Sigma.WhiteBalance",                  &TAG_IMAGE_WHITEBALANCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.CanonPi.ImageWidth",                  &TAG_IMAGE_WIDTH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Image.ImageWidth",                    &TAG_IMAGE_WIDTH);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Exif.Olympus.ImageWidth",                  &TAG_IMAGE_WIDTH);

    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ActionAdvised",          &TAG_IPTC_ACTIONADVISED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.ARMId",                      &TAG_IPTC_ARMID);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.ARMVersion",                 &TAG_IPTC_ARMVERSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.AudioDuration",          &TAG_IPTC_AUDIODURATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.AudioOutcue",            &TAG_IPTC_AUDIOOUTCUE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.AudioType",              &TAG_IPTC_AUDIOTYPE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.Byline",                 &TAG_IPTC_BYLINE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.BylineTitle",            &TAG_IPTC_BYLINETITLE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.Caption",                &TAG_IPTC_CAPTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.Category",               &TAG_IPTC_CATEGORY);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.CharacterSet",               &TAG_IPTC_CHARACTERSET);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.City",                   &TAG_IPTC_CITY);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.Contact",                &TAG_IPTC_CONTACT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.CountryCode",            &TAG_IPTC_COUNTRYCODE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.CountryName",            &TAG_IPTC_COUNTRYNAME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.Credit",                 &TAG_IPTC_CREDIT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.DateCreated",            &TAG_IPTC_DATECREATED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.DateSent",                   &TAG_IPTC_DATESENT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.Destination",                &TAG_IPTC_DESTINATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.EditorialUpdate",        &TAG_IPTC_EDITORIALUPDATE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.EditStatus",             &TAG_IPTC_EDITSTATUS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.EnvelopePriority",           &TAG_IPTC_ENVELOPEPRIORITY);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ExpirationDate",         &TAG_IPTC_EXPIRATIONDATE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ExpirationTime",         &TAG_IPTC_EXPIRATIONTIME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.FileFormat",                 &TAG_IPTC_FILEFORMAT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.FileVersion",                &TAG_IPTC_FILEVERSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.FixtureId",              &TAG_IPTC_FIXTUREID);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.Headline",               &TAG_IPTC_HEADLINE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ImageType",              &TAG_IPTC_IMAGETYPE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.ModelVersion",               &TAG_IPTC_MODELVERSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ObjectAttribute",        &TAG_IPTC_OBJECTATTRIBUTE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ObjectCycle",            &TAG_IPTC_OBJECTCYCLE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ObjectName",             &TAG_IPTC_OBJECTNAME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ObjectType",             &TAG_IPTC_OBJECTTYPE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.ProductId",                  &TAG_IPTC_PRODUCTID);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ProgramVersion",         &TAG_IPTC_PROGRAMVERSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.RasterizedCaption",      &TAG_IPTC_RASTERIZEDCAPTION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.RecordVersion",          &TAG_IPTC_RECORDVERSION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ReleaseDate",            &TAG_IPTC_RELEASEDATE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.ReleaseTime",            &TAG_IPTC_RELEASETIME);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.ServiceId",                  &TAG_IPTC_SERVICEID);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.Source",                 &TAG_IPTC_SOURCE);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.SpecialInstructions",    &TAG_IPTC_SPECIALINSTRUCTIONS);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.SubLocation",            &TAG_IPTC_SUBLOCATION);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.TimeCreated",            &TAG_IPTC_TIMECREATED);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.TimeSent",                   &TAG_IPTC_TIMESENT);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Envelope.UNO",                        &TAG_IPTC_UNO);
    g_hash_table_insert(priv->exiv2_tags, (gpointer) "Iptc.Application2.Urgency",                &TAG_IPTC_URGENCY);
}


GObject *create_plugin ()
{
    return G_OBJECT (g_object_new (GNOME_CMD_TYPE_EXIV2_PLUGIN, NULL));
}


GnomeCmdPluginInfo *get_plugin_info ()
{
    static const char *authors[] = {
        "Andrey Kutejko <andy128k@gmail.com>",
        nullptr
    };

    static GnomeCmdPluginInfo info = {
        .plugin_system_version = GNOME_CMD_PLUGIN_SYSTEM_CURRENT_VERSION,
        .name = "Exiv2",
        .version = PACKAGE_VERSION,
        .copyright = "Copyright \xc2\xa9 2025 Andrey Kutejko",
        .comments = nullptr,
        .authors = (gchar **) authors,
        .documenters = nullptr,
        .translator = nullptr,
        .webpage = "https://gcmd.github.io"
    };

    if (!info.comments)
        info.comments = g_strdup (_("A plugin for extracting metadata from images using libexiv2 library."));

    return &info;
}

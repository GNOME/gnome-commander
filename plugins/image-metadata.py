#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Wladimir Palant https://palant.info/
#
# SPDX-License-Identifier: GPL-3.0-or-later

from gettext import gettext as _

from plugin_helper import Plugin


def N_(text: str) -> str:
    return text


BLOCKLIST = {
    # No point showing image dimensions, image info is displayed already.
    'ImageWidth', 'ImageLength', 'ExifImageWidth', 'ExifImageLength',
    # Pointer, offset and byte count values are not useful.
    'ExifOffset', 'InteroperabilityOffset', 'GPSInfo',
    'StripOffsets', 'StripByteCounts', 'TileOffsets', 'TileByteCounts',
    'SubIFDs', 'IPTC/NAA',
    'JPEGInterchangeFormat', 'JPEGInterchangeFormatLength',
}

IFD_MAP = {
    'Image': ('Image', _('Image')),
    'Thumbnail': ('Thumbnail', _('Thumbnail')),
    'EXIF': ('Photo', _('Photo')),
}

# See https://github.com/ianare/exif-py/blob/3.5.1/exifread/tags/exif.py.
# The descriptions come from exiv2 library and have been shortened where appropriate.
IMAGE_TAGS = {
    # The image tag names/descriptions come from exiv2 and have been translated there: https://github.com/Exiv2/exiv2/tree/main/po
    'SubfileType': ('NewSubfileType', N_('New Subfile Type'), N_('A general indication of the kind of data contained in this subfile.')),
    'OldSubfileType': ('SubfileType', N_('Subfile Type'), N_('A general indication of the kind of data contained in this subfile.')),
    'BitsPerSample': ('BitsPerSample', N_('Bits per Sample'), N_('The number of bits per image component.')),
    'Compression': ('Compression', N_('Compression'), N_('The compression scheme used for the image data. When thumbnails use JPEG compression, this tag value is set to 6.')),
    'PhotometricInterpretation': ('PhotometricInterpretation', N_('Photometric Interpretation'), N_('The pixel composition.')),
    'Thresholding': ('Thresholding', N_('Thresholding'), N_('For black and white TIFF files that represent shades of gray, the technique used to convert from gray to black and white pixels.')),
    'CellWidth': ('CellWidth', N_('Cell Width'), N_('The width of the dithering or halftoning matrix used to create a dithered or halftoned bilevel file.')),
    'CellLength': ('CellLength', N_('Cell Length'), N_('The length of the dithering or halftoning matrix used to create a dithered or halftoned bilevel file.')),
    'FillOrder': ('FillOrder', N_('Fill Order'), N_('The logical order of bits within a byte')),
    'DocumentName': ('DocumentName', N_('Document Name'), N_('The name of the document from which this image was scanned.')),
    'ImageDescription': ('ImageDescription', N_('Image Description'), N_('A character string giving the title of the image.')),
    'Make': ('Make', N_('Manufacturer'), N_('The manufacturer of the recording equipment. This is the manufacturer of the DSC, scanner, video digitizer or other equipment that generated the image.')),
    'Model': ('Model', N_('Model'), N_('The model name or model number of the equipment. This is the model name or number of the DSC, scanner, video digitizer or other equipment that generated the image.')),
    'Orientation': ('Orientation', N_('Orientation'), N_('The image orientation viewed in terms of rows and columns.')),
    'SamplesPerPixel': ('SamplesPerPixel', N_('Samples per Pixel'), N_('The number of components per pixel.')),
    'RowsPerStrip': ('RowsPerStrip', N_('Rows per Strip'), N_('The number of rows per strip. This is the number of rows in the image of one strip when an image is divided into strips.')),
    'MinSampleValue': ('SMinSampleValue', N_('SMin Sample Value'), N_('This field specifies the minimum sample value.')),
    'MaxSampleValue': ('SMaxSampleValue', N_('SMax Sample Value'), N_('This field specifies the maximum sample value.')),
    'XResolution': ('XResolution', N_('X-Resolution'), N_('The number of pixels per <ResolutionUnit> in the <ImageWidth> direction. When the image resolution is unknown, 72 [dpi] is designated.')),
    'YResolution': ('YResolution', N_('Y-Resolution'), N_('The number of pixels per <ResolutionUnit> in the <ImageLength> direction. The same value as <XResolution> is designated.')),
    'PlanarConfiguration': ('PlanarConfiguration', N_('Planar Configuration'), N_('Indicates whether pixel components are recorded in a chunky or planar format.')),
    'PageName': ('PageName', N_('Page Name'), N_('The name of the page from which this image was scanned.')),
    'XPosition': ('XPosition', N_('X Position'), N_('X position of the image. The X offset in ResolutionUnits of the left side of the image, with respect to the left side of the page.')),
    'YPosition': ('YPosition', N_('Y Position'), N_('Y position of the image. The Y offset in ResolutionUnits of the top of the image, with respect to the top of the page. In the TIFF coordinate scheme, the positive Y direction is down, so that YPosition is always positive.')),
    'GrayResponseUnit': ('GrayResponseUnit', N_('Gray Response Unit'), N_('The precision of the information contained in the GrayResponseCurve.')),
    'GrayResponseCurve': ('GrayResponseCurve', N_('Gray Response Curve'), N_('For grayscale data, the optical density of each possible pixel value.')),
    'T4Options': ('T4Options', N_('T4 Options'), N_('T.4-encoding options.')),
    'T6Options': ('T6Options', N_('T6 Options'), N_('T.6-encoding options.')),
    'ResolutionUnit': ('ResolutionUnit', N_('Resolution Unit'), N_('The unit for measuring <XResolution> and <YResolution>. The same unit is used for both <XResolution> and <YResolution>.')),
    'PageNumber': ('PageNumber', N_('Page Number'), N_('The page number of the page from which this image was scanned.')),
    'TransferFunction': ('TransferFunction', N_('Transfer Function'), N_('A transfer function for the image, described in tabular style.')),
    'Software': ('Software', N_('Software'), N_('This tag records the name and version of the software or firmware of the camera or image input device used to generate the image.')),
    'DateTime': ('DateTime', N_('Date and Time'), N_('The date and time of image creation. In Exif standard, it is the date and time the file was changed.')),
    'Artist': ('Artist', N_('Artist'), N_('This tag records the name of the camera owner, photographer or image creator.')),
    'HostComputer': ('HostComputer', N_('Host Computer'), N_('This tag records information about the host computer used to generate the image.')),
    'Predictor': ('Predictor', N_('Predictor'), N_('A predictor is a mathematical operator that is applied to the image data before an encoding scheme is applied.')),
    'WhitePoint': ('WhitePoint', N_('White Point'), N_('The chromaticity of the white point of the image.')),
    'PrimaryChromaticities': ('PrimaryChromaticities', N_('Primary Chromaticities'), N_('The chromaticity of the three primary colors of the image.')),
    'ColorMap': ('ColorMap', N_('Color Map'), N_('A color map for palette color images. This field defines a Red-Green-Blue color map (often called a lookup table) for palette-color images. In a palette-color image, a pixel value is used to index into an RGB lookup table.')),
    'HalftoneHints': ('HalftoneHints', N_('Halftone Hints'), N_('The purpose of the HalftoneHints field is to convey to the halftone function the range of gray levels within a colorimetrically-specified image that should retain tonal detail.')),
    'TileWidth': ('TileWidth', N_('Tile Width'), N_('The tile width in pixels. This is the number of columns in each tile.')),
    'TileLength': ('TileLength', N_('Tile Length'), N_('The tile length (height) in pixels. This is the number of rows in each tile.')),
    'InkSet': ('InkSet', N_('Ink Set'), N_('The set of inks used in a separated (PhotometricInterpretation=5) image.')),
    'InkNames': ('InkNames', N_('Ink Names'), N_('The name of each ink used in a separated (PhotometricInterpretation=5) image.')),
    'NumberofInks': ('NumberOfInks', N_('Number Of Inks'), N_('The number of inks.')),
    'DotRange': ('DotRange', N_('Dot Range'), N_('The component values that correspond to a 0% dot and 100% dot.')),
    'TargetPrinter': ('TargetPrinter', N_('Target Printer'), N_('A description of the printing environment for which this separation is intended.')),
    'ExtraSamples': ('ExtraSamples', N_('Extra Samples'), N_('Specifies that each pixel has m extra components whose interpretation is defined by one of the values listed below.')),
    'SampleFormat': ('SampleFormat', N_('Sample Format'), N_('This field specifies how to interpret each data sample in a pixel.')),
    'SMinSampleValue': ('SMinSampleValue', N_('SMin Sample Value'), N_('This field specifies the minimum sample value.')),
    'SMaxSampleValue': ('SMaxSampleValue', N_('SMax Sample Value'), N_('This field specifies the maximum sample value.')),
    'TransferRange': ('TransferRange', N_('Transfer Range'), N_('Expands the range of the TransferFunction')),
    'ClipPath': ('ClipPath', N_('Clip Path'), N_('A TIFF ClipPath is intended to mirror the essentials of PostScript\'s path creation functionality.')),
    'JPEGTables': ('JPEGTables', N_('JPEG tables'), N_('This optional tag may be used to encode the JPEG quantization and Huffman tables for subsequent use by the JPEG decompression process.')),
    'JPEGProc': ('JPEGProc', N_('JPEG Process'), N_('This field indicates the process used to produce the compressed data')),
    'YCbCrCoefficients': ('YCbCrCoefficients', N_('YCbCr Coefficients'), N_('The matrix coefficients for transformation from RGB to YCbCr image data.')),
    'YCbCrSubSampling': ('YCbCrSubSampling', N_('YCbCr Sub-Sampling'), N_('The sampling ratio of chrominance components in relation to the luminance component.')),
    'YCbCrPositioning': ('YCbCrPositioning', N_('YCbCr Positioning'), N_('The position of chrominance components in relation to the luminance component.')),
    'ReferenceBlackWhite': ('ReferenceBlackWhite', N_('Reference Black/White'), N_('The reference black point value and reference white point value.')),
    'Rating': ('Rating', N_('Windows Rating'), N_('Rating tag used by Windows')),
    'CFARepeatPatternDim': ('CFARepeatPatternDim', N_('CFA Repeat Pattern Dimension'), N_('Contains two values representing the minimum rows and columns to define the repeating patterns of the color filter array')),
    'CFAPattern': ('CFAPattern', N_('CFA Pattern'), N_('Indicates the color filter array (CFA) geometric pattern of the image sensor when a one-chip color area sensor is used.')),
    'BatteryLevel': ('BatteryLevel', N_('Battery Level'), N_('Contains a value of the battery level as a fraction or string')),
    'Copyright': ('Copyright', N_('Copyright'), N_('Copyright information.')),
    'ExposureTime': ('ExposureTime', N_('Exposure Time'), N_('Exposure time, given in seconds.')),
    'FNumber': ('FNumber', N_('FNumber'), N_('The F number.')),
    'InterColorProfile': ('InterColorProfile', N_('Inter Color Profile'), N_('Contains an InterColor Consortium (ICC) format color space characterization/profile')),
    'ExposureProgram': ('ExposureProgram', N_('Exposure Program'), N_('The class of the program used by the camera to set exposure when the picture is taken.')),
    'SpectralSensitivity': ('SpectralSensitivity', N_('Spectral Sensitivity'), N_('Indicates the spectral sensitivity of each channel of the camera used.')),
    'ISOSpeedRatings': ('ISOSpeedRatings', N_('ISO Speed Ratings'), N_('Indicates the ISO Speed and ISO Latitude of the camera or input device as specified in ISO 12232.')),
    'OECF': ('OECF', N_('OECF'), N_('Indicates the Opto-Electric Conversion Function (OECF) specified in ISO 14524.')),
    'Interlace': ('Interlace', N_('Interlace'), N_('Indicates the field number of multifield images.')),
    'TimeZoneOffset': ('TimeZoneOffset', N_('Time Zone Offset'), N_('This optional tag encodes the time zone of the camera clock (relative to Greenwich Mean Time) used to create the DataTimeOriginal tag-value when the picture was taken. It may also contain the time zone offset of the clock used to create the DateTime tag-value when the image was modified.')),
    'SelfTimerMode': ('SelfTimerMode', N_('Self Timer Mode'), N_('Number of seconds image capture was delayed from button press.')),
    'ExifVersion': ('ExifVersion', N_('Exif Version'), N_('The version of this standard supported.')),
    'DateTimeOriginal': ('DateTimeOriginal', N_('Date Time Original'), N_('The date and time when the original image data was generated.')),
    'DateTimeDigitized': ('DateTimeDigitized', N_('Date and Time (digitized)'), N_('The date and time when the image was stored as digital data.')),
    'OffsetTime': ('OffsetTime', N_('Offset Time'), N_('Time difference from Universal Time Coordinated including daylight saving time of DateTime tag.')),
    'OffsetTimeOriginal': ('OffsetTimeOriginal', N_('Offset Time Original'), N_('Time difference from Universal Time Coordinated including daylight saving time of DateTimeOriginal tag.')),
    'OffsetTimeDigitized': ('OffsetTimeDigitized', N_('Offset Time Digitized'), N_('Time difference from Universal Time Coordinated including daylight saving time of DateTimeDigitized tag.')),
    'ComponentsConfiguration': ('ComponentsConfiguration', N_('Components Configuration'), N_('Information specific to compressed data.')),
    'CompressedBitsPerPixel': ('CompressedBitsPerPixel', N_('Compressed Bits Per Pixel'), N_('Specific to compressed data; states the compressed bits per pixel.')),
    'ShutterSpeedValue': ('ShutterSpeedValue', N_('Shutter Speed Value'), N_('Shutter speed.')),
    'ApertureValue': ('ApertureValue', N_('Aperture Value'), N_('The lens aperture.')),
    'BrightnessValue': ('BrightnessValue', N_('Brightness Value'), N_('The value of brightness.')),
    'ExposureBiasValue': ('ExposureBiasValue', N_('Exposure Bias Value'), N_('The exposure bias.')),
    'MaxApertureValue': ('MaxApertureValue', N_('Max Aperture Value'), N_('The smallest F number of the lens.')),
    'SubjectDistance': ('SubjectDistance', N_('Subject Distance'), N_('The distance to the subject, given in meters.')),
    'MeteringMode': ('MeteringMode', N_('Metering Mode'), N_('The metering mode.')),
    'LightSource': ('LightSource', N_('Light Source'), N_('The kind of light source.')),
    'Flash': ('Flash', N_('Flash'), N_('Indicates the status of flash when the image was shot.')),
    'FocalLength': ('FocalLength', N_('Focal Length'), N_('The actual focal length of the lens, in mm.')),
    'FlashEnergy': ('FlashEnergy', N_('Flash Energy'), N_('Amount of flash energy (BCPS).')),
    'SpatialFrequencyResponse': ('SpatialFrequencyResponse', N_('Spatial Frequency Response'), N_('SFR of the camera.')),
    'Noise': ('Noise', N_('Noise'), N_('Noise measurement values.')),
    'ImageNumber': ('ImageNumber', N_('Image Number'), N_('Number assigned to an image, e.g., in a chained image burst.')),
    'SecurityClassification': ('SecurityClassification', N_('Security Classification'), N_('Security classification assigned to the image.')),
    'ImageHistory': ('ImageHistory', N_('Image History'), N_('Record of what has been done to the image.')),
    'SubjectArea': ('SubjectArea', N_('Subject Area'), N_('This tag indicates the location and area of the main subject in the overall scene.')),
    'ExposureIndex': ('ExposureIndex', N_('Exposure Index'), N_('Encodes the camera exposure index setting when image was captured.')),
    'TIFF/EPStandardID': ('TIFFEPStandardID', N_('TIFF/EP Standard ID'), N_('Contains four ASCII characters representing the TIFF/EP standard version of a TIFF/EP file, eg \'1\', \'0\', \'0\', \'0\'')),
    'MakerNote': ('MakerNote', N_('Maker Note'), N_('A tag for manufacturers of Exif writers to record any desired information.')),
    'UserComment': ('UserComment', N_('User Comment'), N_('A tag for Exif users to write keywords or comments on the image besides those in <ImageDescription>, and without the character code limitations of the <ImageDescription> tag.')),
    'SubSecTime': ('SubSecTime', N_('Sub-seconds Time'), N_('A tag used to record fractions of seconds for the <DateTime> tag.')),
    'SubSecTimeOriginal': ('SubSecTimeOriginal', N_('Sub-seconds Time Original'), N_('A tag used to record fractions of seconds for the <DateTimeOriginal> tag.')),
    'SubSecTimeDigitized': ('SubSecTimeDigitized', N_('Sub-seconds Time Digitized'), N_('A tag used to record fractions of seconds for the <DateTimeDigitized> tag.')),
    'XPTitle': ('XPTitle', N_('Windows Title'), N_('Title tag used by Windows, encoded in UCS2')),
    'XPComment': ('XPComment', N_('Windows Comment'), N_('Comment tag used by Windows, encoded in UCS2')),
    'XPAuthor': ('XPAuthor', N_('Windows Author'), N_('Author tag used by Windows, encoded in UCS2')),
    'XPKeywords': ('XPKeywords', N_('Windows Keywords'), N_('Keywords tag used by Windows, encoded in UCS2')),
    'XPSubject': ('XPSubject', N_('Windows Subject'), N_('Subject tag used by Windows, encoded in UCS2')),
    'FlashPixVersion': ('FlashpixVersion', N_('FlashPix Version'), N_('The FlashPix format version supported by a FPXR file.')),
    'ColorSpace': ('ColorSpace', N_('Color Space'), N_('The color space information tag is always recorded as the color space specifier.')),
    'RelatedSoundFile': ('RelatedSoundFile', N_('Related Sound File'), N_('This tag is used to record the name of an audio file related to the image data.')),
    'FlashEnergy': ('FlashEnergy', N_('Flash Energy'), N_('Amount of flash energy (BCPS).')),
    'SpatialFrequencyResponse': ('SpatialFrequencyResponse', N_('Spatial Frequency Response'), N_('SFR of the camera.')),
    'FocalPlaneXResolution': ('FocalPlaneXResolution', N_('Focal Plane X Resolution'), N_('Number of pixels per FocalPlaneResolutionUnit (37392) in ImageWidth direction for main image.')),
    'FocalPlaneYResolution': ('FocalPlaneYResolution', N_('Focal Plane Y Resolution'), N_('Number of pixels per FocalPlaneResolutionUnit (37392) in ImageLength direction for main image.')),
    'FocalPlaneResolutionUnit': ('FocalPlaneResolutionUnit', N_('Focal Plane Resolution Unit'), N_('Unit of measurement for FocalPlaneXResolution(37390) and FocalPlaneYResolution(37391).')),
    'SubjectLocation': ('SubjectLocation', N_('Subject Location'), N_('Indicates the location and area of the main subject in the overall scene.')),
    'ExposureIndex': ('ExposureIndex', N_('Exposure Index'), N_('Encodes the camera exposure index setting when image was captured.')),
    'SensingMethod': ('SensingMethod', N_('Sensing Method'), N_('Type of image sensor.')),
    'FileSource': ('FileSource', N_('File Source'), N_('Indicates the image source. If a DSC recorded the image, this tag value of this tag always be set to 3, indicating that the image was recorded on a DSC.')),
    'SceneType': ('SceneType', N_('Scene Type'), N_('Indicates the type of scene. If a DSC recorded the image, this tag value must always be set to 1, indicating that the image was directly photographed.')),
    'CustomRendered': ('CustomRendered', N_('Custom Rendered'), N_('This tag indicates the use of special processing on image data, such as rendering geared to output.')),
    'ExposureMode': ('ExposureMode', N_('Exposure Mode'), N_('This tag indicates the exposure mode set when the image was shot. In auto-bracketing mode, the camera shoots a series of frames of the same scene at different exposure settings.')),
    'WhiteBalance': ('WhiteBalance', N_('White Balance'), N_('This tag indicates the white balance mode set when the image was shot.')),
    'DigitalZoomRatio': ('DigitalZoomRatio', N_('Digital Zoom Ratio'), N_('This tag indicates the digital zoom ratio when the image was shot. If the numerator of the recorded value is 0, this indicates that digital zoom was not used.')),
    'FocalLengthIn35mmFilm': ('FocalLengthIn35mmFilm', N_('Focal Length In 35mm Film'), N_('This tag indicates the equivalent focal length assuming a 35mm film camera, in mm. A value of 0 means the focal length is unknown.')),
    'SceneCaptureType': ('SceneCaptureType', N_('Scene Capture Type'), N_('This tag indicates the type of scene that was shot.')),
    'GainControl': ('GainControl', N_('Gain Control'), N_('This tag indicates the degree of overall image gain adjustment.')),
    'Contrast': ('Contrast', N_('Contrast'), N_('This tag indicates the direction of contrast processing applied by the camera when the image was shot.')),
    'Saturation': ('Saturation', N_('Saturation'), N_('This tag indicates the direction of saturation processing applied by the camera when the image was shot.')),
    'Sharpness': ('Sharpness', N_('Sharpness'), N_('This tag indicates the direction of sharpness processing applied by the camera when the image was shot.')),
    'DeviceSettingDescription': ('DeviceSettingDescription', N_('Device Setting Description'), N_('This tag indicates information on the picture-taking conditions of a particular camera model.')),
    'SubjectDistanceRange': ('SubjectDistanceRange', N_('Subject Distance Range'), N_('This tag indicates the distance to the subject.')),
    'ImageUniqueID': ('ImageUniqueID', N_('Image Unique ID'), N_('This tag indicates an identifier assigned uniquely to each image.')),
    'CameraOwnerName': ('CameraOwnerName', N_('Camera Owner Name'), N_('This tag records the owner of a camera used in photography as an ASCII string.')),
    'BodySerialNumber': ('BodySerialNumber', N_('Body Serial Number'), N_('This tag records the serial number of the body of the camera that was used in photography as an ASCII string.')),
    'LensSpecification': ('LensSpecification', N_('Lens Specification'), N_('This tag notes minimum focal length, maximum focal length, minimum F number in the minimum focal length, and minimum F number in the maximum focal length, which are specification information for the lens that was used in photography. When the minimum F number is unknown, the notation is 0/0')),
    'LensMake': ('LensMake', N_('Lens Make'), N_('This tag records the lens manufactor as an ASCII string.')),
    'LensModel': ('LensModel', N_('Lens Model'), N_('This tag records the lens\'s model name and model number as an ASCII string.')),
    'LensSerialNumber': ('LensSerialNumber', N_('Lens Serial Number'), N_('This tag records the serial number of the interchangeable lens that was used in photography as an ASCII string.')),
    'Gamma': ('Gamma', N_('Gamma'), N_('Indicates the value of coefficient gamma.')),
    'BlackLevel': ('BlackLevel', N_('Black Level'), N_('Specifies the zero light (a.k.a. thermal black or black current) encoding level, as a repeating pattern. The origin of this pattern is the top-left corner of the ActiveArea rectangle. The values are stored in row-column-sample scan order.')),
}

MISC_TAGS = {
    'Interoperability InteroperabilityIndex': ('Iop.InteroperabilityIndex', N_('Interoperability Index'), N_('Indicates the identification of the Interoperability rule.')),
    'Interoperability InteroperabilityVersion': ('Iop.InteroperabilityVersion', N_('Interoperability Version'), N_('Interoperability version')),
    'Interoperability RelatedImageFileFormat': ('Iop.RelatedImageFileFormat', N_('Related Image File Format'), N_('File format of image file')),
    'Interoperability RelatedImageWidth': ('Iop.RelatedImageWidth', N_('Related Image Width'), N_('Image width')),
    'Interoperability RelatedImageLength': ('Iop.RelatedImageLength', N_('Related Image Length'), N_('Image height')),
    'GPS GPSVersionID': ('GPSInfo.GPSVersionID', N_('GPS Version ID'), N_('Indicates the version of <GPSInfoIFD>. The version is given as 2.0.0.0.')),
    'GPS GPSLatitudeRef': ('GPSInfo.GPSLatitudeRef', N_('GPS Latitude Reference'), N_('Indicates whether the latitude is north or south latitude. The ASCII value \'N\' indicates north latitude, and \'S\' is south latitude.')),
    'GPS GPSLatitude': ('GPSInfo.GPSLatitude', N_('GPS Latitude'), N_('Indicates the latitude. The latitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is dd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is dd/1,mmmm/100,0/1.')),
    'GPS GPSLongitudeRef': ('GPSInfo.GPSLongitudeRef', N_('GPS Longitude Reference'), N_('Indicates whether the longitude is east or west longitude. ASCII \'E\' indicates east longitude, and \'W\' is west longitude.')),
    'GPS GPSLongitude': ('GPSInfo.GPSLongitude', N_('GPS Longitude'), N_('Indicates the longitude. The longitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. When degrees, minutes and seconds are expressed, the format is ddd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format is ddd/1,mmmm/100,0/1.')),
    'GPS GPSAltitude': ('GPSInfo.GPSAltitude', N_('GPS Altitude'), N_('Indicates the altitude based on the reference in GPSAltitudeRef. Altitude is expressed as one RATIONAL value. The reference unit is meters.')),
    'GPS GPSTimeStamp': ('GPSInfo.GPSTimeStamp', N_('GPS Time Stamp'), N_('Indicates the time as UTC (Coordinated Universal Time). <TimeStamp> is expressed as three RATIONAL values giving the hour, minute, and second (atomic clock).')),
    'GPS GPSSatellites': ('GPSInfo.GPSSatellites', N_('GPS Satellites'), N_('Indicates the GPS satellites used for measurements. This tag can be used to describe the number of satellites, their ID number, angle of elevation, azimuth, SNR and other information in ASCII notation.')),
    'GPS GPSStatus': ('GPSInfo.GPSStatus', N_('GPS Status'), N_('Indicates the status of the GPS receiver when the image is recorded. "A" means measurement is in progress, and "V" means the measurement is Interoperability.')),
    'GPS GPSMeasureMode': ('GPSInfo.GPSMeasureMode', N_('GPS Measure Mode'), N_('Indicates the GPS measurement mode. "2" means two-dimensional measurement and "3" means three-dimensional measurement is in progress.')),
    'GPS GPSDOP': ('GPSInfo.GPSDOP', N_('GPS Data Degree of Precision'), N_('Indicates the GPS DOP (data degree of precision). An HDOP value is written during two-dimensional measurement, and PDOP during three-dimensional measurement.')),
    'GPS GPSSpeedRef': ('GPSInfo.GPSSpeedRef', N_('GPS Speed Reference'), N_('Indicates the unit used to express the GPS receiver speed of movement. "K" "M" and "N" represents kilometers per hour, miles per hour, and knots.')),
    'GPS GPSSpeed': ('GPSInfo.GPSSpeed', N_('GPS Speed'), N_('Indicates the speed of GPS receiver movement.')),
    'GPS GPSTrackRef': ('GPSInfo.GPSTrackRef', N_('GPS Track Ref'), N_('Indicates the reference for giving the direction of GPS receiver movement. "T" denotes true direction and "M" is magnetic direction.')),
    'GPS GPSTrack': ('GPSInfo.GPSTrack', N_('GPS Track'), N_('Indicates the direction of GPS receiver movement. The range of values is from 0.00 to 359.99.')),
    'GPS GPSImgDirectionRef': ('GPSInfo.GPSImgDirectionRef', N_('GPS Image Direction Reference'), N_('Indicates the reference for giving the direction of the image when it is captured. "T" denotes true direction and "M" is magnetic direction.')),
    'GPS GPSImgDirection': ('GPSInfo.GPSImgDirection', N_('GPS Image Direction'), N_('Indicates the direction of the image when it was captured. The range of values is from 0.00 to 359.99.')),
    'GPS GPSMapDatum': ('GPSInfo.GPSMapDatum', N_('GPS Map Datum'), N_('Indicates the geodetic survey data used by the GPS receiver. If the survey data is restricted to Japan, the value of this tag is "TOKYO" or "WGS-84".')),
    'GPS GPSDestLatitudeRef': ('GPSInfo.GPSDestLatitudeRef', N_('GPS Destination Latitude Reference'), N_('Indicates whether the latitude of the destination point is north or south latitude. The ASCII value "N" indicates north latitude, and "S" is south latitude.')),
    'GPS GPSDestLatitude': ('GPSInfo.GPSDestLatitude', N_('GPS Destination Latitude'), N_('Indicates the latitude of the destination point. The latitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. If latitude is expressed as degrees, minutes and seconds, a typical format would be dd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format would be dd/1,mmmm/100,0/1.')),
    'GPS GPSDestLongitudeRef': ('GPSInfo.GPSDestLongitudeRef', N_('GPS Destination Longitude Reference'), N_('Indicates whether the longitude of the destination point is east or west longitude. ASCII "E" indicates east longitude, and "W" is west longitude.')),
    'GPS GPSDestLongitude': ('GPSInfo.GPSDestLongitude', N_('GPS Destination Longitude'), N_('Indicates the longitude of the destination point. The longitude is expressed as three RATIONAL values giving the degrees, minutes, and seconds, respectively. If longitude is expressed as degrees, minutes and seconds, a typical format would be ddd/1,mm/1,ss/1. When degrees and minutes are used and, for example, fractions of minutes are given up to two decimal places, the format would be ddd/1,mmmm/100,0/1.')),
    'GPS GPSDestBearingRef': ('GPSInfo.GPSDestBearingRef', N_('GPS Destination Bearing Reference'), N_('Indicates the reference used for giving the bearing to the destination point. "T" denotes true direction and "M" is magnetic direction.')),
    'GPS GPSDestBearing': ('GPSInfo.GPSDestBearing', N_('GPS Destination Bearing'), N_('Indicates the bearing to the destination point. The range of values is from 0.00 to 359.99.')),
    'GPS GPSDestDistanceRef': ('GPSInfo.GPSDestDistanceRef', N_('GPS Destination Distance Reference'), N_('Indicates the unit used to express the distance to the destination point. "K", "M" and "N" represent kilometers, miles and nautical miles.')),
    'GPS GPSDestDistance': ('GPSInfo.GPSDestDistance', N_('GPS Destination Distance'), N_('Indicates the distance to the destination point.')),
    'GPS GPSProcessingMethod': ('GPSInfo.GPSProcessingMethod', N_('GPS Processing Method'), N_('A character string recording the name of the method used for location finding.')),
    'GPS GPSAreaInformation': ('GPSInfo.GPSAreaInformation', N_('GPS Area Information'), N_('A character string recording the name of the GPS area.')),
    'GPS GPSDate': ('GPSInfo.GPSDateStamp', N_('GPS Date Stamp'), N_('A character string recording date and time information relative to UTC (Coordinated Universal Time). The format is "YYYY:MM:DD.".')),
    'GPS GPSDifferential': ('GPSInfo.GPSDifferential', N_('GPS Differential'), N_('Indicates whether differential correction is applied to the GPS receiver.')),
}


class ImageMetadataPlugin(Plugin):
    async def startup(self) -> None:
        self.send_message('info', {
            'name': 'Image Metadata',
            'version': '1.0',
            'comments': (
                _('A plugin for extracting metadata from images files.') +
                '\n\n' +
                _('Python3 package exifread has to be installed on your system for this plugin to work.')
            ),
            'copyright': 'Copyright © 2026 Wladimir Palant',
            'authors': ['Wladimir Palant'],
            'webpage': 'https://gnome.pages.gitlab.gnome.org/gnome-commander/',
        })

        try:
            import exifread
            import logging

            # ExifRead logs errors instead of raising them, silence them
            logging.getLogger("exifread").setLevel(logging.CRITICAL)
        except ImportError:
            self.fail(
                _('Required exifread module not found. Please install exifread for Python3 on your system.')
            )
        self.send_message('ready')

    async def extract_metadata(self, data: dict) -> list[dict]:
        path: str = data['path']
        try:
            import exifread
            with open(path, 'rb') as input:
                tags = exifread.process_file(input, truncate_tags=False)
        except:
            return []

        response = []
        for key, value in tags.items():
            if key in MISC_TAGS:
                key, name, description = MISC_TAGS[key]
                response.append(
                    (f'Exif.{key}', _(name), _(description), str(value))
                )

            if ' ' not in key:
                continue
            ifd, key = key.split(' ', 1)
            if ifd not in IFD_MAP or key in BLOCKLIST:
                continue

            if key in IMAGE_TAGS:
                key, name, description = IMAGE_TAGS[key]
            else:
                short_type = exifread.tags.FIELD_TYPES[value.field_type][1]
                if short_type in ('B', 'SB', 'U'):
                    # Ignore byte-typed values
                    continue

                name, description = key, ''
            response.append(
                (f'Exif.{IFD_MAP[ifd][0]}.{key}', f'{_(name)} | {IFD_MAP[ifd][1]}',
                 _(description), str(value))
            )
        return [{
            "class": _("EXIF"),
            "tags": response,
        }]

    async def list_supported_tags(self, data: dict) -> list[tuple[str, list[tuple[str, str]]]]:
        return [
            [
                _("EXIF"),
                [
                    (f'Exif.{ifd}.{key}', f'{_(name)} | {ifd_localized}')
                    for (key, name, description) in IMAGE_TAGS.values()
                    for (ifd, ifd_localized) in IFD_MAP.values()
                ] + [(f'Exif.{key}', _(name)) for (key, name, description) in MISC_TAGS.values()]
            ]
        ]


if __name__ == '__main__':
    ImageMetadataPlugin().run_forever()

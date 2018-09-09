picha
======

I couldn't find a library or libraries to encode and decode jpeg and png images and also
allow raw access to the pixel data - so I wrote one. There is also optional support
for tiff and webp images. Color conversion, cropping, and resizing are also supported.

## Install
The image format support requires the supporting libraries (libjpeg, libpng, etc.) to be installed on your system. There is a hard dependency on libjpeg. The other formats will be enabled if the requisite library is available at install time (as determined by pkg-config).

On Ubuntu and other debian variants this should install the required packages:
```
sudo apt-get install libjpeg-dev libpng-dev libwebp-dev libtiff-dev
```
On Red Had and CentOS this is likely to get what you need:
```
yum install libjpeg-devel libpng-devel libtiff-devel
```
On MacOS using [HomeBrew](https://brew.sh/):
```
brew install jpeg libpng webp libtiff
```
Once the dependencies are installed use [npm](http://npmjs.org):
```
npm install picha
```

## Usage
```
var fs = require('fs');
var picha = require('picha');

var image = picha.decodeSync(fs.readFileSync("test.jpeg"));
var resized = picha.resizeSync(image, { width: 100, height: 100 });
var cropped = resized.subView(20, 20, 60, 60);
var compressed = picha.encodePngSync(cropped);
fs.writeFileSync('tested.png', compressed);
```

## API

### Image
### `new picha.Image(opt)`
Construct a new image object from the options opt
```
{
	width: width of the image in pixels,
	height: height of the image in pixels,
	pixel: pixel format (rgb, rgba, grey, greya),
	stride: row stride in bytes - defaults to 4 byte aligned rows,
	data: buffer object of pixel data, if missing a buffer is allocated,
}
```

### `Image.subView(x, y, w, h)`
Return a new image that is a rectangular view into this image defined by the supplied pixel coordinates.

### `Image.copy(targetImage)`
Copy the pixels from this image to the target Image. The pixel format must match.
If the sizes don't match the overlapping sub-view will be copied.

### `Image.pixelSize()`
The size, in bytes, of this images pixel format.

### `Image.row(y)`
Return a slice of buffer for a row of the image.

### `Image.equalPixels(o)`
Return true if this image and another image 'o' are pixel for pixel equal.

### `Image.avgChannelDiff(o)`
Return the average channel by channel difference between this and another image 'o'.

### Image codec

### `picha.catalog`
The set of codec's available is determined at install time based on the availability of the
corresponding native libraries. This is a map of the codec's available. The keys will be a
sub-set of the supported codec's `[image/png, image/jpeg, image/tiff, image/webp]`. The values
will be objects with members `[stat, decode, decodeSync, encode, encodeSync]` referring to
the corresponding codec functions.

### `picha.stat(buf)`
Decode the image header and grab the image vitals. For most formats (not tiff) only the first
handful of bytes of the image data is required. The function returns null or
```
{
    width: the width of the image
    height: the height of the image
    pixel: pixel format
    mimetype: one of the supported mime types
}
```

### `picha.decode(buf, cb)`
Decodes the supplied image data on a libuv thread and calls cb with (err, image).

### `picha.decodeSync(buf)`
Decodes the supplied image data on the v8 thread and returns the image.

### `picha.encodePng(image, cb)`
Encode the supplied image into png format on a libuv thread. The cb receives (err, buffer).

### `picha.encodeJpeg(image, opt, cb)`
Encode the supplied image into jpeg format on a libuv thread. The cb receives (err, buffer).
The optional opt object may specify:
```
{
	quality: (0-100) default is 85,
}
```

### `picha.encodeTiff(image, opt, cb)`
Encode the supplied image into tiff format on a libuv thread. The cb receives (err, buffer).
The optional opt object may specify:
```
{
	compression: compression mode ('lzw' (default), 'deflate', 'none'),
}
```

### `picha.encodeWebP(image, opt, cb)`
Encode the supplied image into webp format on a libuv thread. The cb receives (err, buffer).
The optional opt object may specify:
```
{
	quality: (0-100) default is 85,
	alphaQuality: (0-100) quality for the alpha channel, default is 100,
	preset: WebP compression preset
			('default', 'lossless', 'picture', 'photo', 'drawing', 'icon', 'text'),
	exact: true to preserve the color of transparent pixels,
}
```

### `picha.statPng(buf)`
### `picha.statJpeg(buf)`
### `picha.statTiff(buf)`
### `picha.statWebP(buf)`
Decode the header of the respective image formats and returns null or an object containing the
width, height and pixel format.

### `picha.decodePng(buf, cb)`
### `picha.decodeJpeg(buf, cb)`
### `picha.decodeTiff(buf, cb)`
### `picha.decodeWebP(buf, cb)`
Decode the respective image format data on a libuv thread and call cb with (err, image).

### `picha.decodePngSync(buf)`
### `picha.decodeJpegSync(buf)`
### `picha.decodeTiffSync(buf)`
### `picha.decodeWebPSync(buf)`
Decode the respective image format data on the v8 thread and return the image. Tiff has a dizzying number of
options and internal formats, hence all tiff images are decoded to 'rgba'.

### Image manipulation

### `picha.resize(image, opt, cb)`
Resize the image with the provided options. The computation is performed on a libuv thread and cb receives (err, image).
The optional opt parameter accepts the following options.
```
{
	width: the width to resize,
	height: the height to resize,
	filter: optional resize filter
			(cubic (default), lanczos, catmulrom, mitchel, box, or triangle),
	filterScale: optional scale to apply to the filter (0.70),
}
```

### `picha.resizeSync(image, opt)`
Resize an image on the v8 thread. The resize image is returned.

### `picha.colorConvert(image, opt, cb)`
Convert the color format of the image. The computation is on the a libuv thread and cb receives (err, image).
The optional opt parameter accepts the following options.
```
{
	pixel: the pixel format to convert to (rgb, rgba, grey, greya),
	redWeight: optional weight of the red channel for rgb->grey conversion (0.299),
	greenWeight: optional weight of the green channel for rgb->grey conversion (0.587),
	blueWeight: optional weight of the blue channel for rgb->grey conversion (0.114),
}
```

### `picha.colorConvertSync(image, opt)`
Convert the color format of the image. The computation is on the v8 thread and the resulting image is returned.


## License

  MIT

picha
======

picha (swahili): photo

I couldn't find a library or libraries to encode/decode jpeg and png images and also
allow raw access to the pixel data - so I wrote one.

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
* opt.width: width of the image in pixels
* opt.height: height of the image in pixels
* opt.pixel: pixel format (rgb, rgba, grey, greya)
* opt.stride: row stride in bytes - defaults to 4 byte aligned rows
* opt.data: buffer of data

If the data buffer isn't provided an appropriate buffer will be allocated.

### `Image.subView(x, y, w, h)`
Return a new image that is a rectangulare view into this image defined by the supplied pixel coordinates.

### `Image.copy(targetImage)`
Copy the pixels from this image to the target Image. The pixel format must match.
If the sizes don't match then overlapping or missing pixels are discarded.

### `Image.pixelSize()`
The size, in bytes, of this images pixel format.

### `Image.row(y)`
Return a slice of buffer for a row of the image.

### `Image.equalPixels(o)`
Return true if this image and another image 'o' are pixel for pixel equal.

### `Image.avgChannelDiff(o)`
Return the average channel by channel difference between this and another image 'o'.

### Image codec

### `picha.stat(buf)`
Decode the image header and grab the image vitals. Only yhe first handful of bytes of the
image data is required. The function returns null or
```
{
    width: the width of the image
    height: the height of the image
    pixel: pixel format
    mimetype: one of "image/jpeg" or "image/png"
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
The optional opt object may specify { quality: 85 }.

### `picha.statPng(buf)`
### `picha.statJpeg(buf)`
Decode the header of the respective image formats and returns null or an object containing the
width, height and pixel format.

### `picha.decodePng(buf, cb)`
### `picha.decodeJpeg(buf, cb)`
Decode the respective image format data on a libuv thread and call cb with (err, image).

### `picha.decodePngSync(buf)`
### `picha.decodeJpegSync(buf)`
Decode the respective image format data on the v8 thread and return the image.

### Image manipulation

### `picha.resize(image, opt, cb)`
Resize the image with the provided options. The computation is performed on a libuv thread and cb receives (err, image).
* opt.width: the width to resize
* opt.height: the height to resize
* opt.filter: optional resize filter - cubic (default), lanczos, catmulrom, mitchel, box, or triangle.
* opt.filterScale: optional scale to apply to the filter (0.70)

### `picha.resizeSync(image, opt)`
Resize an image on the v8 thread. The resize image is returned.

### `picha.colorConvert(image, opt, cb)`
Convert the color format of the image. The computation is on the a libuv thread and cb receives (err, image).
* opt.pixel: the pixel format to convert to (rgb, rgba, grey, greya)
* opt.redWeight: optional weight of the red channel for rgb->grey conversion (0.299)
* opt.greenWeight: optional weight of the green channel for rgb->grey conversion (0.587)
* opt.blueWeight: optional weight of the blue channel for rgb->grey conversion (0.114)

### `picha.colorConvertSync(image, opt)`
Convert the color format of the image. The computation is on the v8 thread and the resulting image is returned.


## License

  MIT

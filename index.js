"use strict";

var image = require('./lib/image');
var picha = require('./build/Release/picha.node');

var Image = exports.Image = image.Image;

var catalog = exports.catalog = picha.catalog;
var mimetypes = Object.keys(catalog);

//--

var resize = exports.resize = function(src, opt, cb) {
	picha.resize(src, opt, function(err, dst) {
		return cb(err, dst && new Image(dst));
	});
};

var resizeSync = exports.resizeSync = function(img, opt) {
	return new Image(picha.resizeSync(img, opt));
};

//--

var colorConvert = exports.colorConvert = function(src, opt, cb) {
	picha.colorConvert(src, opt, function(err, dst) {
		return cb(err, dst && new Image(dst));
	});
};

var colorConvertSync = exports.colorConvertSync = function(img, opt) {
	return new Image(picha.colorConvertSync(img, opt));
};

//--

var supportedMap = {
	'rgb': [ 'rgba', 'r16g16b16', 'r16g16b16a16', 'grey', 'greya', 'r16' ],
	'rgba': [ 'r16g16b16a16', 'rgb', 'r16g16b16', 'greya', 'r16g16', 'grey' ],
	'grey': [ 'greya', 'r16', 'rgb', 'rgba', 'r16g16', 'r16g16b16' ],
	'greya': [  'r16g16', 'rgba', 'r16g16b16a16', 'grey', 'r16', 'rgb' ],
	'r16': [ 'r16g16', 'r16g16b16', 'r16g16b16a16', 'grey', 'greya', 'rgb' ],
	'r16g16': [ 'r16g16b16', 'r16g16b16a16', 'greya', 'r16', 'grey', 'rgb' ],
	'r16g16b16': [ 'r16g16b16a16', 'rgb', 'rgba', 'grey', 'greya', 'r16' ],
	'r16g16b16a16': [ 'rgba', 'r16g16b16', 'rgb', 'greya', 'r16g16', 'r16' ],
};

function isSupported(pixel, encodes) {
	return encodes.indexOf(pixel != -1);
}

function chooseSupported(pixel, encodes) {
	var map = supportedMap[pixel];
	if (!map) throw new Error("invalid pixel format: ", pixel);
	for (var i = 0; i < map.length; ++i) {
		if (isSupported(map[i], encodes))
			return map[i];
	}
	return encodes[0];
}

function toSupported(image, encodes, next) {
	if (isSupported(image.pixel, encodes))
		return next(null, image);
	return colorConvert(image, { pixel: chooseSupported(image.pixel, encodes) }, next);
}

function toSupportedSync(image, encodes) {
	if (isSupported(image.pixel, encodes))
		return image;
	return colorConvertSync(image, { pixel: chooseSupported(image.pixel, encodes) });
}

//--

if (catalog['image/png']) {

	var statPng = exports.statPng = picha.statPng;
	var pngEncodes = exports.pngEncodes = catalog['image/png'].encodes;

	var decodePng = exports.decodePng = function(buf, opt, cb) {
		if (typeof opt === 'function') { cb = opt; opt = {}; }
		picha.decodePng(buf, opt, function(err, img) {
			cb(err, img && new Image(img));
		});
	};

	var decodePngSync = exports.decodePngSync = function(buf, opt) {
		return new Image(picha.decodePngSync(buf, opt || {}));
	};

	var encodePng = exports.encodePng = function(img, opt, cb) {
		if (typeof opt === 'function') { cb = opt; opt = {}; }
		toSupported(img, pngEncodes, function(err, img) {
			if (err) return cb(err);
			picha.encodePng(img, opt, cb);
		});
	};

	var encodePngSync = exports.encodePngSync = function(img, opt) {
		return picha.encodePngSync(toSupportedSync(img, pngEncodes), opt || {});
	};
}

//--

if (catalog['image/jpeg']) {

	var statJpeg = exports.statJpeg = picha.statJpeg;
	var jpegEncodes = exports.jpegEncodes = catalog['image/jpeg'].encodes;

	var decodeJpeg = exports.decodeJpeg = function(buf, opt, cb) {
		if (typeof opt === 'function') { cb = opt; opt = {}; }
		picha.decodeJpeg(buf, opt, function(err, img) {
			cb(err, img && new Image(img));
		});
	};

	var decodeJpegSync = exports.decodeJpegSync = function(buf, opt) {
		return new Image(picha.decodeJpegSync(buf, opt || {}));
	};

	var encodeJpeg = exports.encodeJpeg = function(img, opt, cb) {
		if (typeof opt === 'function') { cb = opt; opt = {}; }
		toSupported(img, jpegEncodes, function(err, img) {
			if (err) return cb(err);
			picha.encodeJpeg(img, opt, cb);
		});
	};

	var encodeJpegSync = exports.encodeJpegSync = function(img, opt) {
		return picha.encodeJpegSync(toSupportedSync(img, jpegEncodes), opt || {});
	};
}

//--

if (catalog['image/tiff']) {

	var statTiff = exports.statTiff = picha.statTiff;
	var tiffEncodes = exports.tiffEncodes = catalog['image/tiff'].encodes;

	var decodeTiff = exports.decodeTiff = function(buf, opt, cb) {
		if (typeof opt === 'function') { cb = opt; opt = {}; }
		picha.decodeTiff(buf, opt, function(err, img) {
			cb(err, img && new Image(img));
		});
	};

	var decodeTiffSync = exports.decodeTiffSync = function(buf, opt) {
		return new Image(picha.decodeTiffSync(buf, opt || {}));
	};

	var encodeTiff = exports.encodeTiff = function(img, opt, cb) {
		if (typeof opt === 'function') { cb = opt; opt = {}; }
		toSupported(img, tiffEncodes, function(err, img) {
			if (err) return cb(err);
			picha.encodeTiff(img, opt, cb);
		});
	};

	var encodeTiffSync = exports.encodeTiffSync = function(img, opt) {
		return picha.encodeTiffSync(toSupportedSync(img, tiffEncodes), opt || {});
	};
}

//--

if (catalog['image/webp']) {

	var statWebP = exports.statWebP = picha.statWebP;
	var webpEncodes = exports.webpEncodes = catalog['image/webp'].encodes;

	var decodeWebP = exports.decodeWebP = function(buf, opt, cb) {
		if (typeof opt === 'function') { cb = opt; opt = {}; }
		picha.decodeWebP(buf, opt, function(err, img) {
			cb(err, img && new Image(img));
		});
	};

	var decodeWebPSync = exports.decodeWebPSync = function(buf, opt) {
		return new Image(picha.decodeWebPSync(buf, opt || {}));
	};

	var encodeWebP = exports.encodeWebP = function(img, opt, cb) {
		if (typeof opt === 'function') { cb = opt; opt = {}; }
		toSupported(img, webpEncodes, function(err, img) {
			if (err) return cb(err);
			picha.encodeWebP(img, opt, cb);
		});
	};

	var encodeWebPSync = exports.encodeWebPSync = function(img, opt) {
		return picha.encodeWebPSync(toSupportedSync(img, webpEncodes), opt || {});
	};
}

//--

var stat = exports.stat = function(buf) {
	for (var idx = 0; idx < mimetypes.length; ++idx) {
		var stat = catalog[mimetypes[idx]].stat(buf);
		if (stat) {
			stat.mimetype = mimetypes[idx];
			return stat;
		}
	}
};

var decode = exports.decode = function(buf, opt, cb) {
	if (typeof opt === 'function') { cb = opt; opt = {}; }
	tryNext(0);

	function tryNext(idx) {
		if (idx == mimetypes.length) return cb(new Error("unsupported image file"));
		catalog[mimetypes[idx]].decode(buf, opt, function(err, img) {
			if (!err && img) return cb(err, img && new Image(img));
			tryNext(idx + 1);
		});
	}
};

var decodeSync = exports.decodeSync = function(buf, opt) {
	for (var idx = 0; idx < mimetypes.length; ++idx) {
		try {
			var img = catalog[mimetypes[idx]].decodeSync(buf, opt || {});
			if (img) return new Image(img);
		}
		catch (e) {
		}
	}
	throw new Error("unsupported image file");
};

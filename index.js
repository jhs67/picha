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

if (catalog['image/png']) {

	var statPng = exports.statPng = picha.statPng;

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
		picha.encodePng(img, opt, cb);
	};

	var encodePngSync = exports.encodePngSync = function(img, opt) {
		return picha.encodePngSync(img, opt || {});
	};
}

//--

if (catalog['image/jpeg']) {

	var statJpeg = exports.statJpeg = picha.statJpeg;

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
		picha.encodeJpeg(img, opt, cb);
	};

	var encodeJpegSync = exports.encodeJpegSync = function(img, opt) {
		return picha.encodeJpegSync(img, opt || {});
	};
}

//--

if (catalog['image/tiff']) {

	var statTiff = exports.statTiff = picha.statTiff;

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
		picha.encodeTiff(img, opt, cb);
	};

	var encodeTiffSync = exports.encodeTiffSync = function(img, opt) {
		return picha.encodeTiffSync(img, opt || {});
	};
}

//--

if (catalog['image/webp']) {

	var statWebP = exports.statWebP = picha.statWebP;

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
		picha.encodeWebP(img, opt, cb);
	};

	var encodeWebPSync = exports.encodeWebPSync = function(img, opt) {
		return picha.encodeWebPSync(img, opt || {});
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

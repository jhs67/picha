
var image = require('./lib/image');
var picha = require('./build/Release/picha.node');

var Image = exports.Image = image.Image;

var resize = exports.resize = function(src, opt, cb) {
	picha.resize(src, opt, function(err, dst) {
		return cb(err, dst && new Image(dst));
	});
}

var resizeSync = exports.resizeSync = function(img, opt) {
	return new Image(picha.resizeSync(img, opt));
}

var colorConvert = exports.colorConvert = function(src, opt, cb) {
	picha.colorConvert(src, opt, function(err, dst) {
		return cb(err, dst && new Image(dst));
	});
}

var colorConvertSync = exports.colorConvertSync = function(img, opt) {
	return new Image(picha.colorConvertSync(img, opt));
}

var statPng = exports.statPng = picha.statPng;

var decodePng = exports.decodePng = function(buf, opt, cb) {
	if (typeof opt === 'function') cb = opt, opt = {};
	picha.decodePng(buf, opt, function(err, img) {
		cb(err, img && new Image(img));
	})
}

var decodePngSync = exports.decodePngSync = function(buf, opt) {
	return new Image(picha.decodePngSync(buf, opt || {}));
}

var encodePng = exports.encodePng = function(img, opt, cb) {
	if (typeof opt === 'function') cb = opt, opt = {};
	picha.encodePng(img, opt, cb);
}

var encodePngSync = exports.encodePngSync = function(img, opt) {
	return picha.encodePngSync(img, opt || {});
}

var statJpeg = exports.statJpeg = picha.statJpeg;

var decodeJpeg = exports.decodeJpeg = function(buf, opt, cb) {
	if (typeof opt === 'function') cb = opt, opt = {};
	picha.decodeJpeg(buf, opt, function(err, img) {
		cb(err, img && new Image(img));
	})
}

var decodeJpegSync = exports.decodeJpegSync = function(buf, opt) {
	return new Image(picha.decodeJpegSync(buf, opt || {}));
}

var encodeJpeg = exports.encodeJpeg = function(img, opt, cb) {
	if (typeof opt === 'function') cb = opt, opt = {};
	picha.encodeJpeg(img, opt, cb);
}

var encodeJpegSync = exports.encodeJpegSync = function(img, opt) {
	return picha.encodeJpegSync(img, opt || {});
}


const decoders = [
	{ decode: decodeJpeg, decodeSync: decodeJpegSync, stat: statJpeg, mimetype: "image/jpeg" },
	{ decode: decodePng, decodeSync: decodePngSync, stat: statPng, mimetype: "image/png" },
];

var stat = exports.stat = function(buf) {
	for (var idx = 0; idx < decoders.length; ++idx) {
		var stat = decoders[idx].stat(buf);
		if (stat) {
			stat.mimetype = decoders[idx].mimetype;
			return stat;
		}
	}
}

var decode = exports.decode = function(buf, opt, cb) {
	if (typeof opt === 'function') cb = opt, opt = {};
	tryNext(0);

	function tryNext(idx) {
		if (idx == decoders.length) return cb(new Error("unsupported image file"));
		decoders[idx].decode(buf, opt, function(err, img) {
			if (!err && img) return cb(err, img);
			tryNext(idx + 1);
		});
	}
}

var decodeSync = exports.decodeSync = function(buf, opt) {
	for (var idx = 0; idx < decoders.length; ++idx) {
		try {
			var img = decoders[idx].decodeSync(buf, opt || {});
			if (img) return img;
		}
		catch (e) {
		}
	}
	throw new Error("unsupported image file")
}

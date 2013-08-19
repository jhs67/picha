
var image = require('./lib/image');
var picha = require('./build/Release/picha.node');

var Image = exports.Image = image.Image;

var decodePng = exports.decodePng = function(buf, opt, cb) {
	if (typeof opt === 'function') cb = opt, opt = {};
	picha.decodePng(buf, opt, function(err, img) {
		cb(err, img && new Image(img));
	})
}

var decodePngSync = exports.decodePngSync = function(buf, opt) {
	return new Image(picha.decodePngSync(buf, opt || {}));
}

var encodePng = exports.encodePng = function(img, cb) {
	picha.encodePng(img, cb);
}

var encodePngSync = exports.encodePngSync = function(img) {
	return picha.encodePngSync(img);
}


var buffertools = require('buffertools');

var Image = exports.Image = function(opt) {
	if (opt == null) opt = {};
	this.data = opt.data;
	this.width = opt.width || 0;
	this.height = opt.height || 0;
	this.pixel = opt.pixel || 'rgba';
	var psize = Image.pixelSize(this.pixel);
	this.stride = opt.stride || ((this.width * psize + 3) & ~3);
	if (psize == 0)
		throw new Error("invalid pixel format " + this.pixel);
	if (this.stide < this.width * psize)
		throw new Error("stride too short");
	if (this.width < 0 || this.height < 0)
		throw new Error("invalid dimensions");
	if (this.stride * this.height != 0 && this.data == null)
		this.data = new Buffer(this.stride * this.height);
	if (this.stride * this.height != 0 && this.data.length < this.stride * this.height)
		throw new Error("image data too small");
}

const pixelSizes = {
	'rgb': 3,
	'rgba': 4,
	'grey': 1,
	'greya': 2,
}

Image.pixelSize = function(pixel) {
	return pixelSizes[pixel] || 0;
}

Image.prototype.pixelSize = function() {
	return pixelSizes[this.pixel] || 0;
}

Image.prototype.row = function(y) {
	return this.data.slice(y * this.stride, y * this.stride + this.width * this.pixelSize());
}

Image.prototype.equalPixels = function(o) {
	if (this.width !== o.width || this.height != o.height || this.pixel != o.pixel)
		return false;
	for (var y = 0; y < this.height; ++y)
		if (this.row(y).compare(o.row(y)) != 0)
			return false;
	return true;
}

Image.prototype.avgChannelDiff = function(o) {
	if (this.width !== o.width || this.height != o.height || this.pixel != o.pixel)
		return 255;
	var rw = this.width * this.pixelSize(), s = 0;
	for (var y = 0; y < this.height; ++y)
		for (var x = 0; x < rw; ++x)
			s += Math.abs(this.row(y)[x] - o.row(y)[x]);
	return s / (this.height * rw);
}

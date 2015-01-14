/*global describe, before, after, it */
"use strict";

var fs = require('fs');
var path = require('path');
var assert = require('assert');
var buffertools = require('buffertools');
var picha = require('../index.js');

// Skip tests if no jpeg support
if (!picha.catalog['image/jpeg'])
	return;

describe('jpeg_codec', function() {
	function testFile(name, width, height, pixel) {
		var asyncJpeg, syncJpeg;
		var asyncImage, syncImage;
		describe("decode " + name, function() {
			var file;
			it("should load test jpeg", function(done) {
				fs.readFile(path.join(__dirname, name), function(err, buf) {
					file = buf;
					done(err);
				});
			});
			it("should stat correctly", function() {
				var stat = picha.statJpeg(file);
				assert.notEqual(stat, null);
				assert.equal(stat.width, width);
				assert.equal(stat.height, height);
				assert.equal(stat.pixel, pixel);
			});
			it("should async decode", function(done) {
				picha.decodeJpeg(file, function(err, image) {
					asyncImage = image;
					done(err);
				});
			});
			it("should sync decode", function() {
				syncImage = picha.decodeJpegSync(file);
			});
			it("should be the same sync or async", function() {
				assert(syncImage.equalPixels(asyncImage));
			});
		});
		describe("encode " + name, function() {
			it("should async encode", function(done) {
				picha.encodeJpeg(asyncImage, { quality: 100 }, function(err, blob) {
					asyncJpeg = blob;
					done(err);
				});
			});
			it("should sync encode", function() {
				syncJpeg = picha.encodeJpegSync(syncImage, { quality: 100 });
			});
			it("should be the same sync or async", function() {
				assert(buffertools.compare(asyncJpeg, syncJpeg) === 0);
			});
		});
		describe("round trip " + name, function() {
			it("async should nearly match original", function(done) {
				picha.decodeJpeg(asyncJpeg, function(err, image) {
					assert(image.avgChannelDiff(asyncImage) < 8);
					done(err);
				});
			});
			it("sync should nearly match original", function() {
				var image = picha.decodeJpegSync(syncJpeg);
				assert(image.avgChannelDiff(syncImage) < 8);
			});
		});
	}

	testFile("test.jpeg", 50, 50, 'rgb');
	testFile("test2g.jpg", 76, 50, 'grey');
	testFile("test2cmyk.jpg", 76, 50, 'rgb');

	describe("encodes images with alpha", function() {
		var img;
		it("should load test.png", function() {
			img = picha.decodeSync(fs.readFileSync(path.join(__dirname, "test.png")));
			assert(img.pixel === "rgba");
		});
		it("should jpeg encode rgba", function() {
			var jpeg = picha.encodeJpegSync(img, { quality: 100 });
		});
		it("should jpeg encode greya", function() {
			var jpeg = picha.encodeJpegSync(picha.colorConvertSync(img, { pixel: 'greya' }), { quality: 100 });
		});
	});
});

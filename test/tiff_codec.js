
var fs = require('fs');
var path = require('path');
var assert = require('assert');
var picha = require('../index.js');

// Skip tests if no tiff support
if (!picha.catalog['image/tiff'])
	return;

describe('tiff_codec', function() {
	var asyncTiff, syncTiff;
	var asyncImage, syncImage;
	describe("decode", function() {
		var file;
		it("should load test tiff", function(done) {
			fs.readFile(path.join(__dirname, "smallliz.tif"), function(err, buf) {
				file = buf;
				done(err);
			});
		});
		it("should stat correctly", function() {
			var stat = picha.statTiff(file);
			assert.notEqual(stat, null);
			assert.equal(stat.width, 160);
			assert.equal(stat.height, 160);
			assert.equal(stat.pixel, 'rgba');
		});
		it("should async decode", function(done) {
			picha.decodeTiff(file, function(err, image) {
				asyncImage = image;
				done(err);
			});
		});
		it("should sync decode", function() {
			syncImage = picha.decodeTiffSync(file);
		});
		it("should be the same sync or async", function() {
			assert(syncImage.equalPixels(asyncImage));
		})
	})
	describe("encode", function() {
		it("should async encode", function(done) {
			picha.encodeTiff(asyncImage, function(err, blob) {
				asyncTiff = blob;
				done(err);
			})
		});
		it("should sync encode", function() {
			syncTiff = picha.encodeTiffSync(syncImage);
		})
		it("should be the same sync or async", function() {
			assert(asyncTiff.compare(syncTiff) == 0);
		})
	})
	describe("round trip", function() {
		it("async match original", function(done) {
			picha.decodeTiff(asyncTiff, function(err, image) {
				assert(image.equalPixels(asyncImage));
				done(err);
			});
		})
		it("sync match original", function() {
			image = picha.decodeTiffSync(syncTiff);
			assert(image.equalPixels(syncImage));
		})
	})
})

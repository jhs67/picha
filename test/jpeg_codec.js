
var fs = require('fs');
var path = require('path');
var assert = require('assert');
var picha = require('../index.js');

describe('jpeg_codec', function() {
	var asyncJpeg, syncJpeg;
	var asyncImage, syncImage;
	describe("decode", function() {
		var file;
		it("should load test jpeg", function(done) {
			fs.readFile(path.join(__dirname, "test.jpeg"), function(err, buf) {
				file = buf;
				done(err);
			});
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
		})
	})
	describe("encode", function() {
		it("should async encode", function(done) {
			picha.encodeJpeg(asyncImage, { quality: 100 }, function(err, blob) {
				asyncJpeg = blob;
				done(err);
			})
		});
		it("should sync encode", function() {
			syncJpeg = picha.encodeJpegSync(syncImage, { quality: 100 });
		})
		it("should be the same sync or async", function() {
			assert(asyncJpeg.compare(syncJpeg) == 0);
		})
	})
	describe("round trip", function() {
		it("async should nearly match original", function(done) {
			picha.decodeJpeg(asyncJpeg, function(err, image) {
				assert(image.avgChannelDiff(asyncImage) < 8);
				done(err);
			});
		})
		it("sync should nearly match original", function() {
			image = picha.decodeJpegSync(syncJpeg);
			assert(image.avgChannelDiff(syncImage) < 8);
		})
	})
})

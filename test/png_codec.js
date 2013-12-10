
var fs = require('fs');
var path = require('path');
var assert = require('assert');
var picha = require('../index.js');

// Skip tests if no png support
if (!picha.catalog['image/png'])
	return;

describe('png_codec', function() {
	var asyncPng, syncPng;
	var asyncImage, syncImage;
	describe("decode", function() {
		var file;
		it("should load test png", function(done) {
			fs.readFile(path.join(__dirname, "test.png"), function(err, buf) {
				file = buf;
				done(err);
			});
		});
		it("should stat correctly", function() {
			var stat = picha.statPng(file);
			assert.notEqual(stat, null);
			assert.equal(stat.width, 50);
			assert.equal(stat.height, 50);
			assert.equal(stat.pixel, 'rgba');
		});
		it("should async decode", function(done) {
			picha.decodePng(file, function(err, image) {
				asyncImage = image;
				done(err);
			});
		});
		it("should sync decode", function() {
			syncImage = picha.decodePngSync(file);
		});
		it("should be the same sync or async", function() {
			assert(syncImage.equalPixels(asyncImage));
		})
	})
	describe("encode", function() {
		it("should async encode", function(done) {
			picha.encodePng(asyncImage, function(err, blob) {
				asyncPng = blob;
				done(err);
			})
		});
		it("should sync encode", function() {
			syncPng = picha.encodePngSync(syncImage);
		})
		it("should be the same sync or async", function() {
			assert(asyncPng.compare(syncPng) == 0);
		})
	})
	describe("round trip", function() {
		it("async match original", function(done) {
			picha.decodePng(asyncPng, function(err, image) {
				assert(image.equalPixels(asyncImage));
				done(err);
			});
		})
		it("sync match original", function() {
			image = picha.decodePngSync(syncPng);
			assert(image.equalPixels(syncImage));
		})
	})
})

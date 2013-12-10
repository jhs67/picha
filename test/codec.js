
var fs = require('fs');
var path = require('path');
var assert = require('assert');
var picha = require('../index.js');

describe('codec', function() {
	var image;
	describe("decode", function() {
		if (picha.catalog['image/jpeg']) {
			var jpegFile, jpegImage;
			it("should load test jpeg", function(done) {
				fs.readFile(path.join(__dirname, "test.jpeg"), function(err, buf) {
					jpegFile = buf;
					done(err);
				});
			});
			it("jpeg should stat correctly", function() {
				var jpegStat = picha.stat(jpegFile);
				assert.notEqual(jpegStat, null);
				assert.equal(jpegStat.width, 50);
				assert.equal(jpegStat.height, 50);
				assert.equal(jpegStat.pixel, 'rgb');
				assert.equal(jpegStat.mimetype, 'image/jpeg');
			});
			it("should jpeg async decode", function(done) {
				picha.decode(jpegFile, function(err, image) {
					jpegImage = image;
					done(err);
				});
			});
			it("should jpeg sync decode", function() {
				var image = picha.decodeSync(jpegFile);
				assert(image.equalPixels(jpegImage));
			});
		}

		if (picha.catalog['image/png']) {
			var pngFile, pngImage;
			it("should load test png", function(done) {
				fs.readFile(path.join(__dirname, "test.png"), function(err, buf) {
					pngFile = buf;
					done(err);
				});
			});
			it("png should stat correctly", function() {
				var pngStat = picha.stat(pngFile);
				assert.notEqual(pngStat, null);
				assert.equal(pngStat.width, 50);
				assert.equal(pngStat.height, 50);
				assert.equal(pngStat.pixel, 'rgba');
				assert.equal(pngStat.mimetype, 'image/png');
			});
			it("should png async decode", function(done) {
				picha.decode(pngFile, function(err, image) {
					pngImage = image;
					done(err);
				});
			});
			it("should png sync decode", function() {
				var image = picha.decodeSync(pngFile);
				assert(image.equalPixels(pngImage));
			});
		}
	})
})

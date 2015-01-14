/*global describe, before, after, it */
"use strict";

var fs = require('fs');
var path = require('path');
var assert = require('assert');
var picha = require('../index.js');

// This test relies on working png support
if (!picha.catalog['image/png'])
	return;

describe('color_convert', function() {
	var rgbImage, greyImage;
	it("should load test images", function() {
		var f = fs.readFileSync(path.join(__dirname, "test.png"));
		rgbImage = picha.decodeSync(f);
		greyImage = picha.decodeSync(fs.readFileSync(path.join(__dirname, "greytest.png")));
	});
	it("conversion to grey should match", function() {
		var toGrey = picha.colorConvertSync(rgbImage, { pixel: 'greya' });
		assert.equal(toGrey.width, rgbImage.width);
		assert.equal(toGrey.height, rgbImage.height);
		assert.equal(toGrey.pixel, 'greya');
		assert(toGrey.equalPixels(greyImage));
	});
	it("grey-color-grey should be invariant", function(done) {
		picha.colorConvert(greyImage, { pixel: 'rgba' }, function(err, img) {
			if (err) return done(err);
			picha.colorConvert(img, { pixel: greyImage.pixel }, function(err, rimg) {
				if (err) return done(err);
				assert(greyImage.equalPixels(rimg));
				done();
			});
		});
	});
});

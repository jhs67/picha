/*global describe, before, after, it */
"use strict";

var fs = require('fs');
var path = require('path');
var assert = require('assert');
var picha = require('../index.js');

// This test relies on working jpeg support
if (!picha.catalog['image/jpeg'])
	return;

describe('copy', function() {
	var image;
	it("should load test image", function() {
		image = picha.decodeSync(fs.readFileSync(path.join(__dirname, "test2.jpg")));
	});
	it("should copy pixels", function() {
		var c = new picha.Image({ width: 30, height: 30, pixel: image.pixel });
		image.copy(c);
		assert(image.subView(0, 0, 30, 30).equalPixels(c));
	});
});

/*global describe, before, after, it */
"use strict";

var fs = require('fs');
var path = require('path');
var assert = require('assert');
var buffertools = require('buffertools');
var picha = require('../index.js');

// Skip tests if no webp support
if (!picha.catalog['image/webp'])
	return;

describe('webp_codec', function() {
	var asyncWebP, syncWebP;
	var asyncImage, syncImage;
	describe("decode", function() {
		var file;
		it("should load test webp", function(done) {
			fs.readFile(path.join(__dirname, "test.webp"), function(err, buf) {
				file = buf;
				done(err);
			});
		});
		it("should stat correctly", function() {
			var stat = picha.statWebP(file);
			assert.notEqual(stat, null);
			assert.equal(stat.width, 50);
			assert.equal(stat.height, 50);
			assert.equal(stat.pixel, 'rgb');
		});
		it("should async decode", function(done) {
			picha.decodeWebP(file, function(err, image) {
				asyncImage = image;
				done(err);
			});
		});
		it("should sync decode", function() {
			syncImage = picha.decodeWebPSync(file);
		});
		it("should be the same sync or async", function() {
			assert(syncImage.equalPixels(asyncImage));
		});
	});
	describe("lossless encode", function() {
		it("should async encode", function(done) {
			picha.encodeWebP(asyncImage, { preset: 'lossless' }, function(err, blob) {
				asyncWebP = blob;
				done(err);
			});
		});
		it("should sync encode", function() {
			syncWebP = picha.encodeWebPSync(syncImage, { preset: 'lossless' });
		});
		it("should be the same sync or async", function() {
			assert(buffertools.compare(asyncWebP, syncWebP) === 0);
		});
		it("async should match original", function(done) {
			picha.decodeWebP(asyncWebP, function(err, image) {
				assert(image.equalPixels(asyncImage));
				done(err);
			});
		});
		it("sync should match original", function() {
			var image = picha.decodeWebPSync(syncWebP);
			assert(image.equalPixels(syncImage));
		});
	});
	describe("lossey encode", function() {
		it("should async encode", function(done) {
			picha.encodeWebP(asyncImage, { quality: 70 }, function(err, blob) {
				asyncWebP = blob;
				done(err);
			});
		});
		it("should sync encode", function() {
			syncWebP = picha.encodeWebPSync(syncImage, { quality: 70 });
		});
		it("should be the same sync or async", function() {
			assert(buffertools.compare(asyncWebP, syncWebP) === 0);
		});
		it("async should nearly match original", function(done) {
			picha.decodeWebP(asyncWebP, function(err, image) {
				assert(image.avgChannelDiff(asyncImage) < 8);
				done(err);
			});
		});
		it("sync should nearly match original", function() {
			var image = picha.decodeWebPSync(syncWebP);
			assert(image.avgChannelDiff(syncImage) < 8);
		});
	});
});

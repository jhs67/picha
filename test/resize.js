
var fs = require('fs');
var path = require('path');
var assert = require('assert');
var picha = require('../index.js');

// This test relies on working png/jpeg support
if (!picha.catalog['image/png'] || !picha.catalog['image/jpeg'])
	return;

describe('resize', function() {
	var image, asyncSmall, smallImage;
	var opts = { width: 32, height: 24 };
	it("should load test image", function() {
		image = picha.decodeSync(fs.readFileSync(path.join(__dirname, "test2.jpg")));
		smallImage = picha.decodeSync(fs.readFileSync(path.join(__dirname, "test2.png")));
	});
	it("should async resize", function(done) {
		picha.resize(image, opts, function(err, o) {
			asyncSmall = o;
			assert(asyncSmall.avgChannelDiff(smallImage) < 2);
			done(err);
		});
	})
	it("should sync resize", function() {
		var syncSmall = picha.resizeSync(image, opts);
		assert(syncSmall.avgChannelDiff(smallImage) < 2);
		assert(syncSmall.equalPixels(asyncSmall));
	})
})

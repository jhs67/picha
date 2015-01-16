
#include <stdlib.h>

#include <webp/decode.h>
#include <webp/encode.h>

#include <node.h>
#include <node_buffer.h>

#include "webpcodec.h"
#include "colorconvert.h"

namespace picha {


	//---------------------------------------------------------------------------------------------------------

	struct WebPDecodeCtx {
		Persistent<Object> dstimage;
		Persistent<Object> buffer;
		Persistent<Function> cb;
		const uint8_t * srcdata;
		uint srclen;
		bool error;
		NativeImage dst;
	};

	void UV_decodeWebP(uv_work_t* work_req) {
		WebPDecodeCtx *ctx = reinterpret_cast<WebPDecodeCtx*>(work_req->data);
		uint8_t * out;
		NativeImage& dst = ctx->dst;
		if (dst.pixel == RGBA_PIXEL)
			out = WebPDecodeRGBAInto(ctx->srcdata, ctx->srclen, dst.data, dst.stride * dst.height, dst.stride);
		else
			out = WebPDecodeRGBInto(ctx->srcdata, ctx->srclen, dst.data, dst.stride * dst.height, dst.stride);
		ctx->error = (out == 0);
	}

	void V8_decodeWebP(uv_work_t* work_req, int) {
		NanScope();
		WebPDecodeCtx *ctx = reinterpret_cast<WebPDecodeCtx*>(work_req->data);
		makeCallback(NanNew(ctx->cb), ctx->error ? "decode error" : 0, NanNew(ctx->dstimage));
		NanDisposePersistent(ctx->dstimage);
		NanDisposePersistent(ctx->buffer);
		NanDisposePersistent(ctx->cb);
		delete work_req;
		delete ctx;
	}

	NAN_METHOD(decodeWebP) {
		NanScope();

		if (args.Length() != 3 || !Buffer::HasInstance(args[0]) || !args[1]->IsObject() || !args[2]->IsFunction()) {
			NanThrowError("expected: decodeWebP(srcbuffer, opts, cb)");
			NanReturnUndefined();
		}
		Local<Object> srcbuf = args[0]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		WebPBitstreamFeatures feat;
		if (WebPGetFeatures((const uint8_t*)Buffer::Data(srcbuf), Buffer::Length(srcbuf), &feat)) {
			makeCallback(cb, "invalid image features", NanUndefined());
			NanReturnUndefined();
		}

		WebPDecodeCtx * ctx = new WebPDecodeCtx;
		Local<Object> jsdst = newJsImage(feat.width, feat.height, feat.has_alpha ? RGBA_PIXEL : RGB_PIXEL);
		NanAssignPersistent(ctx->dstimage, jsdst);
		NanAssignPersistent(ctx->buffer, srcbuf);
		NanAssignPersistent(ctx->cb, cb);
		ctx->dst = jsImageToNativeImage(jsdst);
		ctx->srcdata = (const uint8_t*)srcdata;
		ctx->srclen = srclen;

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_decodeWebP, V8_decodeWebP);

		NanReturnUndefined();
	}

	NAN_METHOD(decodeWebPSync) {
		NanScope();

		if (args.Length() != 2 || !Buffer::HasInstance(args[0]) || !args[1]->IsObject()) {
			NanThrowError("expected: decodeWebPSync(srcbuffer, opts)");
			NanReturnUndefined();
		}
		Local<Object> srcbuf = args[0]->ToObject();

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		WebPBitstreamFeatures feat;
		if (WebPGetFeatures((const uint8_t*)Buffer::Data(srcbuf), Buffer::Length(srcbuf), &feat)) {
			NanThrowError("invalid image features");
			NanReturnUndefined();
		}

		Local<Object> jsdst = newJsImage(feat.width, feat.height, feat.has_alpha ? RGBA_PIXEL : RGB_PIXEL);
		NativeImage dst = jsImageToNativeImage(jsdst);

		uint8_t * out;
		if (feat.has_alpha)
			out = WebPDecodeRGBAInto((const uint8_t*)srcdata, srclen, dst.data, dst.stride * dst.height, dst.stride);
		else
			out = WebPDecodeRGBInto((const uint8_t*)srcdata, srclen, dst.data, dst.stride * dst.height, dst.stride);

		if (out == 0) {
			NanThrowError("error decoding image");
			NanReturnUndefined();
		}

		NanReturnValue(jsdst);
	}

	NAN_METHOD(statWebP) {
		NanScope();

		if (args.Length() != 1 || !Buffer::HasInstance(args[0])) {
			NanThrowError("expected: statWebP(srcbuffer)");
			NanReturnUndefined();
		}
		Local<Object> srcbuf = args[0]->ToObject();

		WebPBitstreamFeatures feat;
		if (WebPGetFeatures((const uint8_t*)Buffer::Data(srcbuf), Buffer::Length(srcbuf), &feat))
			NanReturnUndefined();

		Local<Object> stat = NanNew<Object>();
		stat->Set(NanNew(width_symbol), NanNew<Integer>(feat.width));
		stat->Set(NanNew(height_symbol), NanNew<Integer>(feat.height));
		stat->Set(NanNew(pixel_symbol), pixelEnumToSymbol(feat.has_alpha ? RGBA_PIXEL : RGB_PIXEL));
		NanReturnValue(stat);
	}

	//---------------------------------------------------------------------------------------------------------

	namespace {

		double getQuality(Handle<Value> v, double def) {
			double quality = v->NumberValue();
			if (quality != quality)
				quality = def;
			else if (quality < 0)
				quality = 0;
			else if (quality > 100)
				quality = 100;
			return quality;
		}

		bool setupWebPConfig(WebPConfig& config, Handle<Object> opts) {
			float quality = getQuality(opts->Get(NanNew(quality_symbol)), 85);

			bool r = false;
			if (opts->Has(NanNew(preset_symbol))) {
				Local<Value> v = opts->Get(NanNew(preset_symbol));
				if (v->StrictEquals(NanNew(default_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality);
				}
				else if (v->StrictEquals(NanNew(picture_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_PICTURE, quality);
				}
				else if (v->StrictEquals(NanNew(photo_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_PHOTO, quality);
				}
				else if (v->StrictEquals(NanNew(drawing_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_DRAWING, quality);
				}
				else if (v->StrictEquals(NanNew(icon_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_ICON, quality);
				}
				else if (v->StrictEquals(NanNew(text_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_TEXT, quality);
				}
				else if (v->StrictEquals(NanNew(lossless_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality);
					if (r) config.lossless = 1;
				}
			}
			else {
				r = WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality);
			}

			if (r && opts->Has(NanNew(alphaQuality_symbol)))
				config.alpha_quality = getQuality(opts->Get(NanNew(alphaQuality_symbol)), 100);

			return r;
		}

		bool setupWebPPicture(WebPPicture& picture, NativeImage& image) {
			if (!WebPPictureInit(&picture))
				return false;

			picture.use_argb = 1;
			picture.width = image.width;
			picture.height = image.height;

			bool ok = false;
			switch (image.pixel) {
				default :
				case RGBA_PIXEL: {
					ok = WebPPictureImportRGBA(&picture, (const uint8_t*)image.data, image.stride);
					break;
				}
				case RGB_PIXEL: {
					ok = WebPPictureImportRGB(&picture, (const uint8_t*)image.data, image.stride);
					break;
				}
				case GREYA_PIXEL: {
					NativeImage rgba = picha::newNativeImage(image.width, image.height, RGBA_PIXEL);
					doColorConvert(ColorSettings(), image, rgba);
					ok = WebPPictureImportRGBA(&picture, (const uint8_t*)rgba.data, rgba.stride);
					freeNativeImage(rgba);
					break;
				}
				case GREY_PIXEL: {
					NativeImage rgb = newNativeImage(image.width, image.height, RGB_PIXEL);
					doColorConvert(ColorSettings(), image, rgb);
					ok = WebPPictureImportRGB(&picture, (const uint8_t*)rgb.data, rgb.stride);
					freeNativeImage(rgb);
					break;
				}
			}

			if (!ok) WebPPictureFree(&picture);
			return ok;
		}

	}

	struct WebPEncodeCtx {
		Persistent<Value> buffer;
		Persistent<Function> cb;
		NativeImage image;
		WebPConfig config;

		char *dstdata;
		size_t dstlen;
		bool error;
	};

	void UV_encodeWebP(uv_work_t* work_req) {
		WebPEncodeCtx *ctx = reinterpret_cast<WebPEncodeCtx*>(work_req->data);

		WebPPicture picture;
		if (!setupWebPPicture(picture, ctx->image)) {
			WebPPictureFree(&picture);
			ctx->error = true;
			return;
		}

		WebPMemoryWriter writer;
		WebPMemoryWriterInit(&writer);
		picture.writer = WebPMemoryWrite;
		picture.custom_ptr = &writer;

		ctx->error = !WebPEncode(&ctx->config, &picture);
		WebPPictureFree(&picture);

		if (ctx->error) {
			if (writer.mem) free(writer.mem);
		}
		else {
			ctx->dstdata = (char*)writer.mem;
			ctx->dstlen = writer.size;
		}
	}

	void V8_encodeWebP(uv_work_t* work_req, int) {
		NanScope();
		WebPEncodeCtx *ctx = reinterpret_cast<WebPEncodeCtx*>(work_req->data);

		bool error = ctx->error;
		size_t dstlen = ctx->dstlen;
		char * dstdata = ctx->dstdata;
		Local<Function> cb = NanNew(ctx->cb);
		NanDisposePersistent(ctx->buffer);
		NanDisposePersistent(ctx->cb);
		delete work_req;
		delete ctx;

		Local<Value> e, r;
		if (error) {
			e = Exception::Error(NanNew<String>("webp encode error"));
			r = NanUndefined();
		}
		else {
			e = NanUndefined();
			r = NanNewBufferHandle(dstdata, dstlen);
			free(dstdata);
		}

		TryCatch try_catch;

		Handle<Value> argv[2] = { e, r };
		NanMakeCallback(NanGetCurrentContext()->Global(), cb, 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);

		return;
	}

	NAN_METHOD(encodeWebP) {
		NanScope();

		if (args.Length() != 3 || !args[0]->IsObject() || !args[1]->IsObject() || !args[2]->IsFunction()) {
			NanThrowError("expected: encodeWebP(image, opts, cb)");
			NanReturnUndefined();
		}
		Local<Object> img = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		WebPEncodeCtx *ctx = new WebPEncodeCtx();
		if (!setupWebPConfig(ctx->config, opts)) {
			delete ctx;
			makeCallback(cb, "invalid webp preset", NanUndefined());
			NanReturnUndefined();
		}

		NanAssignPersistent(ctx->buffer, img->Get(NanNew(data_symbol)));
		NanAssignPersistent(ctx->cb, cb);
		ctx->image = jsImageToNativeImage(img);
		if (!ctx->image.data) {
			delete ctx;
			makeCallback(cb, "invalid image", NanUndefined());
			NanReturnUndefined();
		}

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_encodeWebP, V8_encodeWebP);

		NanReturnUndefined();
	}

	NAN_METHOD(encodeWebPSync) {
		NanScope();

		if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsObject()) {
			NanThrowError("expected: encodeWebPSync(image, opts)");
			NanReturnUndefined();
		}
		Local<Object> img = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();

		WebPConfig config;
		if (!setupWebPConfig(config, opts)) {
			NanThrowError("invalid webp preset");
			NanReturnUndefined();
		}

		NativeImage image = jsImageToNativeImage(img);
		if (!image.data) {
			NanThrowError("invalid image");
			NanReturnUndefined();
		}

		WebPPicture picture;
		if (!setupWebPPicture(picture, image)) {
			NanThrowError("error setting up webp picture");
			NanReturnUndefined();
		}

		WebPMemoryWriter writer;
		WebPMemoryWriterInit(&writer);
		picture.writer = WebPMemoryWrite;
		picture.custom_ptr = &writer;

		bool ok = WebPEncode(&config, &picture);
		WebPPictureFree(&picture);

		if (!ok) {
			if (writer.mem) free(writer.mem);
			NanThrowError("webp encode error");
			NanReturnUndefined();
		}

		Local<Value> r = NanNewBufferHandle((const char*)writer.mem, writer.size);
		free(writer.mem);
		NanReturnValue(r);
	}

}

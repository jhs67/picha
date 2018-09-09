
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
		Nan::Persistent<Object> dstimage;
		Nan::Persistent<Object> buffer;
		Nan::Persistent<Function> cb;
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
		Nan::HandleScope scope;
		WebPDecodeCtx *ctx = reinterpret_cast<WebPDecodeCtx*>(work_req->data);
		makeCallback(Nan::New(ctx->cb), ctx->error ? "decode error" : 0, Nan::New(ctx->dstimage));
		ctx->dstimage.Reset();
		ctx->buffer.Reset();
		ctx->cb.Reset();
		delete work_req;
		delete ctx;
	}

	NAN_METHOD(decodeWebP) {
		if (info.Length() != 3 || !Buffer::HasInstance(info[0]) || !info[1]->IsObject() || !info[2]->IsFunction()) {
			Nan::ThrowError("expected: decodeWebP(srcbuffer, opts, cb)");
			return;
		}
		Local<Object> srcbuf = info[0]->ToObject();
		Local<Function> cb = Local<Function>::Cast(info[2]);

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		WebPBitstreamFeatures feat;
		if (WebPGetFeatures((const uint8_t*)Buffer::Data(srcbuf), Buffer::Length(srcbuf), &feat)) {
			makeCallback(cb, "invalid image features", Nan::Undefined());
			return;
		}

		WebPDecodeCtx * ctx = new WebPDecodeCtx;
		Local<Object> jsdst = newJsImage(feat.width, feat.height, feat.has_alpha ? RGBA_PIXEL : RGB_PIXEL);
		ctx->dstimage.Reset(jsdst);
		ctx->buffer.Reset(srcbuf);
		ctx->cb.Reset(cb);
		ctx->dst = jsImageToNativeImage(jsdst);
		ctx->srcdata = (const uint8_t*)srcdata;
		ctx->srclen = srclen;

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_decodeWebP, V8_decodeWebP);
	}

	NAN_METHOD(decodeWebPSync) {
		if (info.Length() != 2 || !Buffer::HasInstance(info[0]) || !info[1]->IsObject()) {
			Nan::ThrowError("expected: decodeWebPSync(srcbuffer, opts)");
			return;
		}
		Local<Object> srcbuf = info[0]->ToObject();

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		WebPBitstreamFeatures feat;
		if (WebPGetFeatures((const uint8_t*)Buffer::Data(srcbuf), Buffer::Length(srcbuf), &feat)) {
			Nan::ThrowError("invalid image features");
			return;
		}

		Local<Object> jsdst = newJsImage(feat.width, feat.height, feat.has_alpha ? RGBA_PIXEL : RGB_PIXEL);
		NativeImage dst = jsImageToNativeImage(jsdst);

		uint8_t * out;
		if (feat.has_alpha)
			out = WebPDecodeRGBAInto((const uint8_t*)srcdata, srclen, dst.data, dst.stride * dst.height, dst.stride);
		else
			out = WebPDecodeRGBInto((const uint8_t*)srcdata, srclen, dst.data, dst.stride * dst.height, dst.stride);

		if (out == 0) {
			Nan::ThrowError("error decoding image");
			return;
		}

		info.GetReturnValue().Set(jsdst);
	}

	NAN_METHOD(statWebP) {
		if (info.Length() != 1 || !Buffer::HasInstance(info[0])) {
			Nan::ThrowError("expected: statWebP(srcbuffer)");
			return;
		}
		Local<Object> srcbuf = info[0]->ToObject();

		WebPBitstreamFeatures feat;
		if (WebPGetFeatures((const uint8_t*)Buffer::Data(srcbuf), Buffer::Length(srcbuf), &feat))
			return;

		Local<Object> stat = Nan::New<Object>();
		stat->Set(Nan::New(width_symbol), Nan::New<Integer>(feat.width));
		stat->Set(Nan::New(height_symbol), Nan::New<Integer>(feat.height));
		stat->Set(Nan::New(pixel_symbol), pixelEnumToSymbol(feat.has_alpha ? RGBA_PIXEL : RGB_PIXEL));
		info.GetReturnValue().Set(stat);
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
			float quality = getQuality(opts->Get(Nan::New(quality_symbol)), 85);

			bool r = false;
			if (opts->Has(Nan::New(preset_symbol))) {
				Local<Value> v = opts->Get(Nan::New(preset_symbol));
				if (v->StrictEquals(Nan::New(default_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality);
				}
				else if (v->StrictEquals(Nan::New(picture_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_PICTURE, quality);
				}
				else if (v->StrictEquals(Nan::New(photo_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_PHOTO, quality);
				}
				else if (v->StrictEquals(Nan::New(drawing_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_DRAWING, quality);
				}
				else if (v->StrictEquals(Nan::New(icon_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_ICON, quality);
				}
				else if (v->StrictEquals(Nan::New(text_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_TEXT, quality);
				}
				else if (v->StrictEquals(Nan::New(lossless_symbol))) {
					r = WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality);
					if (r) config.lossless = 1;
				}
			}
			else {
				r = WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality);
			}

			if (r && opts->Has(Nan::New(alphaQuality_symbol)))
				config.alpha_quality = getQuality(opts->Get(Nan::New(alphaQuality_symbol)), 100);

			if (r && opts->Has(Nan::New(exact_symbol)))
				config.exact = opts->Get(Nan::New(exact_symbol))->BooleanValue();

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
		WebPEncodeCtx() : dstdata_(0) {}

		Nan::Persistent<Value> buffer;
		Nan::Persistent<Function> cb;
		NativeImage image;
		WebPConfig config;

		char *dstdata_;
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
			ctx->dstdata_ = (char*)writer.mem;
			ctx->dstlen = writer.size;
		}
	}

	void V8_encodeWebP(uv_work_t* work_req, int) {
		Nan::HandleScope scope;
		WebPEncodeCtx *ctx = reinterpret_cast<WebPEncodeCtx*>(work_req->data);

		bool error = ctx->error;
		size_t dstlen = ctx->dstlen;
		char * dstdata_ = ctx->dstdata_;
		Local<Function> cb = Nan::New(ctx->cb);
		ctx->buffer.Reset();
		ctx->cb.Reset();
		delete work_req;
		delete ctx;

		Local<Value> e, r;
		if (error) {
			e = Nan::Error("webp encode error");
			r = Nan::Undefined();
		}
		else {
			Local<Object> o;
			e = Nan::Undefined();
			if (Nan::NewBuffer(reinterpret_cast<char*>(dstdata_), dstlen).ToLocal(&o)) {
				r = o;
				dstdata_ = 0;
			}
			else {
				r = Nan::Undefined();
			}
		}

		if (dstdata_)
			free(dstdata_);

		Nan::TryCatch try_catch;

		Local<Value> argv[2] = { e, r };
		Nan::AsyncResource ass("picha");
		ass.runInAsyncScope(Nan::GetCurrentContext()->Global(), cb, 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);

		return;
	}

	NAN_METHOD(encodeWebP) {
		if (info.Length() != 3 || !info[0]->IsObject() || !info[1]->IsObject() || !info[2]->IsFunction()) {
			Nan::ThrowError("expected: encodeWebP(image, opts, cb)");
			return;
		}
		Local<Object> img = info[0]->ToObject();
		Local<Object> opts = info[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(info[2]);

		WebPEncodeCtx *ctx = new WebPEncodeCtx();
		if (!setupWebPConfig(ctx->config, opts)) {
			delete ctx;
			makeCallback(cb, "invalid webp preset", Nan::Undefined());
			return;
		}

		ctx->buffer.Reset(img->Get(Nan::New(data_symbol)));
		ctx->cb.Reset(cb);
		ctx->image = jsImageToNativeImage(img);
		if (!ctx->image.data) {
			delete ctx;
			makeCallback(cb, "invalid image", Nan::Undefined());
			return;
		}

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_encodeWebP, V8_encodeWebP);
	}

	NAN_METHOD(encodeWebPSync) {
		if (info.Length() != 2 || !info[0]->IsObject() || !info[1]->IsObject()) {
			Nan::ThrowError("expected: encodeWebPSync(image, opts)");
			return;
		}
		Local<Object> img = info[0]->ToObject();
		Local<Object> opts = info[1]->ToObject();

		WebPConfig config;
		if (!setupWebPConfig(config, opts)) {
			Nan::ThrowError("invalid webp preset");
			return;
		}

		NativeImage image = jsImageToNativeImage(img);
		if (!image.data) {
			Nan::ThrowError("invalid image");
			return;
		}

		WebPPicture picture;
		if (!setupWebPPicture(picture, image)) {
			Nan::ThrowError("error setting up webp picture");
			return;
		}

		WebPMemoryWriter writer;
		WebPMemoryWriterInit(&writer);
		picture.writer = WebPMemoryWrite;
		picture.custom_ptr = &writer;

		bool ok = WebPEncode(&config, &picture);
		WebPPictureFree(&picture);

		if (!ok) {
			if (writer.mem) free(writer.mem);
			Nan::ThrowError("webp encode error");
			return;
		}

		Local<Value> r;
		Local<Object> b;
		if (Nan::NewBuffer(reinterpret_cast<char*>(writer.mem), writer.size).ToLocal(&b)) {
			r = b;
		}
		else {
			free(writer.mem);
			r = Nan::Undefined();
		}
		info.GetReturnValue().Set(r);
	}

}

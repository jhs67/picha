
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
		Persistent<Value> dstimage;
		Persistent<Value> buffer;
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
		HandleScope scope;
		WebPDecodeCtx *ctx = reinterpret_cast<WebPDecodeCtx*>(work_req->data);
		makeCallback(ctx->cb, ctx->error ? "decode error" : 0, ctx->dstimage);
		delete ctx;
	}

	Handle<Value> decodeWebP(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 3 || !Buffer::HasInstance(args[0]) || !args[1]->IsObject() || !args[2]->IsFunction()) {
			ThrowException(Exception::Error(String::New("expected: decodeWebP(srcbuffer, opts, cb)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		WebPBitstreamFeatures feat;
		if (WebPGetFeatures((const uint8_t*)Buffer::Data(srcbuf), Buffer::Length(srcbuf), &feat)) {
			makeCallback(cb, "invalid image features", Undefined());
			return scope.Close(Undefined());
		}

		WebPDecodeCtx * ctx = new WebPDecodeCtx;
		Local<Object> jsdst = newJsImage(feat.width, feat.height, feat.has_alpha ? RGBA_PIXEL : RGB_PIXEL);
		ctx->dstimage = Persistent<Value>::New(jsdst);
		ctx->buffer = Persistent<Value>::New(srcbuf);
		ctx->cb = Persistent<Function>::New(cb);
		ctx->dst = jsImageToNativeImage(jsdst);
		ctx->srcdata = (const uint8_t*)srcdata;
		ctx->srclen = srclen;

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_decodeWebP, V8_decodeWebP);

		return Undefined();
	}

	Handle<Value> decodeWebPSync(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 2 || !Buffer::HasInstance(args[0]) || !args[1]->IsObject()) {
			ThrowException(Exception::Error(String::New("expected: decodeWebPSync(srcbuffer, opts)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		WebPBitstreamFeatures feat;
		if (WebPGetFeatures((const uint8_t*)Buffer::Data(srcbuf), Buffer::Length(srcbuf), &feat)) {
			ThrowException(Exception::Error(String::New("invalid image features")));
			return scope.Close(Undefined());
		}

		Local<Object> jsdst = newJsImage(feat.width, feat.height, feat.has_alpha ? RGBA_PIXEL : RGB_PIXEL);
		NativeImage dst = jsImageToNativeImage(jsdst);

		uint8_t * out;
		if (feat.has_alpha)
			out = WebPDecodeRGBAInto((const uint8_t*)srcdata, srclen, dst.data, dst.stride * dst.height, dst.stride);
		else
			out = WebPDecodeRGBInto((const uint8_t*)srcdata, srclen, dst.data, dst.stride * dst.height, dst.stride);

		if (out == 0) {
			ThrowException(Exception::Error(String::New("error decoding image")));
			return Undefined();
		}

		return scope.Close(jsdst);
	}

	Handle<Value> statWebP(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 1 || !Buffer::HasInstance(args[0])) {
			ThrowException(Exception::Error(String::New("expected: statWebP(srcbuffer)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();

		WebPBitstreamFeatures feat;
		if (WebPGetFeatures((const uint8_t*)Buffer::Data(srcbuf), Buffer::Length(srcbuf), &feat))
			return scope.Close(Undefined());

		Local<Object> stat = Object::New();
		stat->Set(width_symbol, Integer::New(feat.width));
		stat->Set(height_symbol, Integer::New(feat.height));
		stat->Set(pixel_symbol, pixelEnumToSymbol(feat.has_alpha ? RGBA_PIXEL : RGB_PIXEL));
		return scope.Close(stat);
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
			float quality = getQuality(opts->Get(quality_symbol), 85);

			bool r = false;
			if (opts->Has(preset_symbol)) {
				Local<Value> v = opts->Get(preset_symbol);
				if (v->StrictEquals(default_symbol)) {
					r = WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality);
				}
				else if (v->StrictEquals(picture_symbol)) {
					r = WebPConfigPreset(&config, WEBP_PRESET_PICTURE, quality);
				}
				else if (v->StrictEquals(photo_symbol)) {
					r = WebPConfigPreset(&config, WEBP_PRESET_PHOTO, quality);
				}
				else if (v->StrictEquals(drawing_symbol)) {
					r = WebPConfigPreset(&config, WEBP_PRESET_DRAWING, quality);
				}
				else if (v->StrictEquals(icon_symbol)) {
					r = WebPConfigPreset(&config, WEBP_PRESET_ICON, quality);
				}
				else if (v->StrictEquals(text_symbol)) {
					r = WebPConfigPreset(&config, WEBP_PRESET_TEXT, quality);
				}
				else if (v->StrictEquals(lossless_symbol)) {
					r = WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality);
					if (r) config.lossless = 1;
				}
			}
			else {
				r = WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality);
			}

			if (r && opts->Has(alphaQuality_symbol))
				config.alpha_quality = getQuality(opts->Get(alphaQuality_symbol), 100);

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
		HandleScope scope;
		WebPEncodeCtx *ctx = reinterpret_cast<WebPEncodeCtx*>(work_req->data);

		bool error = ctx->error;
		size_t dstlen = ctx->dstlen;
		char * dstdata = ctx->dstdata;
		Local<Function> cb = Local<Function>::New(ctx->cb);
		delete ctx;

		Local<Value> e, r;
		if (error) {
			e = Exception::Error(String::New("webp encode error"));
			r = *Undefined();
		}
		else {
			e = *Undefined();
			Buffer *buf = Buffer::New(dstdata, dstlen);
			r = Local<Value>::New(buf->handle_);
			free(dstdata);
		}

		TryCatch try_catch;

		Handle<Value> argv[2] = { e, r };
		cb->Call(Context::GetCurrent()->Global(), 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);

		return;
	}

	Handle<Value> encodeWebP(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 3 || !args[0]->IsObject() || !args[1]->IsObject() || !args[2]->IsFunction()) {
			ThrowException(Exception::Error(String::New("expected: encodeWebP(image, opts, cb)")));
			return scope.Close(Undefined());
		}
		Local<Object> img = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		WebPEncodeCtx *ctx = new WebPEncodeCtx();
		if (!setupWebPConfig(ctx->config, opts)) {
			delete ctx;
			makeCallback(cb, "invalid webp preset", Undefined());
			return scope.Close(Undefined());
		}

		ctx->buffer = Persistent<Value>::New(img->Get(data_symbol));
		ctx->cb = Persistent<Function>::New(cb);
		ctx->image = jsImageToNativeImage(img);
		if (!ctx->image.data) {
			delete ctx;
			makeCallback(cb, "invalid image", Undefined());
			return scope.Close(Undefined());
		}

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_encodeWebP, V8_encodeWebP);

		return scope.Close(Undefined());
	}

	Handle<Value> encodeWebPSync(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsObject()) {
			ThrowException(Exception::Error(String::New("expected: encodeWebPSync(image, opts)")));
			return scope.Close(Undefined());
		}
		Local<Object> img = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();

		WebPConfig config;
		if (!setupWebPConfig(config, opts)) {
			ThrowException(Exception::Error(String::New("invalid webp preset")));
			return scope.Close(Undefined());
		}

		NativeImage image = jsImageToNativeImage(img);
		if (!image.data) {
			ThrowException(Exception::Error(String::New("invalid image")));
			return scope.Close(Undefined());
		}

		WebPPicture picture;
		if (!setupWebPPicture(picture, image)) {
			ThrowException(Exception::Error(String::New("error setting up webp picture")));
			return scope.Close(Undefined());
		}

		WebPMemoryWriter writer;
		WebPMemoryWriterInit(&writer);
		picture.writer = WebPMemoryWrite;
		picture.custom_ptr = &writer;

		bool ok = WebPEncode(&config, &picture);
		WebPPictureFree(&picture);

		if (!ok) {
			if (writer.mem) free(writer.mem);
			ThrowException(Exception::Error(String::New("webp encode error")));
			return scope.Close(Undefined());
		}

		Buffer *buf = Buffer::New((const char*)writer.mem, writer.size);
		free(writer.mem);
		return scope.Close(buf->handle_);
	}

}


#include "colorconvert.h"

namespace picha {

	void getSettings(ColorSettings& s, Handle<Object> opts) {
		double d;
		Local<Value> v;
		v = opts->Get(Nan::New(redWeight_symbol));
		d = v->NumberValue();
		if (d == d) s.rFactor = d;
		v = opts->Get(Nan::New(greenWeight_symbol));
		d = v->NumberValue();
		if (d == d) s.gFactor = d;
		v = opts->Get(Nan::New(blueWeight_symbol));
		d = v->NumberValue();
		if (d == d) s.bFactor = d;
		float n = 1.0f / (s.rFactor + s.gFactor + s.bFactor);
		s.rFactor *= n;
		s.gFactor *= n;
		s.bFactor *= n;
	}

	template<PixelMode Src, PixelMode Dst> struct ColorConvertOp;

	template <> struct ColorConvertOp<RGBA_PIXEL, RGB_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
		}
	};

	template <> struct ColorConvertOp<RGBA_PIXEL, GREYA_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0] * cs.rFactor + src[1] * cs.gFactor + src[2] * cs.bFactor + 0.5f;
			dst[1] = src[3];
		}
	};

	template <> struct ColorConvertOp<RGBA_PIXEL, GREY_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0] * cs.rFactor + src[1] * cs.gFactor + src[2] * cs.bFactor + 0.5f;
		}
	};

	template <> struct ColorConvertOp<RGB_PIXEL, RGBA_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 255;
		}
	};

	template <> struct ColorConvertOp<RGB_PIXEL, GREYA_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0] * cs.rFactor + src[1] * cs.gFactor + src[2] * cs.bFactor + 0.5f;
			dst[1] = 255;
		}
	};

	template <> struct ColorConvertOp<RGB_PIXEL, GREY_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0] * cs.rFactor + src[1] * cs.gFactor + src[2] * cs.bFactor + 0.5f;
		}
	};

	template <> struct ColorConvertOp<GREYA_PIXEL, RGBA_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0];
			dst[1] = src[0];
			dst[2] = src[0];
			dst[3] = src[1];
		}
	};

	template <> struct ColorConvertOp<GREYA_PIXEL, RGB_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0];
			dst[1] = src[0];
			dst[2] = src[0];
		}
	};

	template <> struct ColorConvertOp<GREYA_PIXEL, GREY_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0];
		}
	};

	template <> struct ColorConvertOp<GREY_PIXEL, RGBA_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0];
			dst[1] = src[0];
			dst[2] = src[0];
			dst[3] = 255;
		}
	};

	template <> struct ColorConvertOp<GREY_PIXEL, RGB_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0];
			dst[1] = src[0];
			dst[2] = src[0];
		}
	};

	template <> struct ColorConvertOp<GREY_PIXEL, GREYA_PIXEL> {
		static void op(const ColorSettings &cs, PixelType * src, PixelType * dst) {
			dst[0] = src[0];
			dst[1] = 255;
		}
	};

	template <PixelMode Src, PixelMode Dst> struct ColorConverter {
		static void op(const ColorSettings &cs, NativeImage& src, NativeImage& dst) {
			assert(dst.width == src.width);
			assert(dst.height == src.height);
			assert(src.pixel == Src);
			assert(dst.pixel == Dst);
			for (int i = 0; i < src.height; ++i) {
				PixelType *s = src.row(i), *d = dst.row(i);
				for (int j = 0; j < src.width; ++j, s += PixelWidth<Src>::value, d += PixelWidth<Dst>::value)
					ColorConvertOp<Src, Dst>::op(cs, s, d);
			}
		}
	};


	void doColorConvert(const ColorSettings &cs, NativeImage& src, NativeImage& dst) {
		switch (src.pixel) {
			case RGBA_PIXEL :
				switch (dst.pixel) {
					case RGB_PIXEL : ColorConverter<RGBA_PIXEL, RGB_PIXEL>::op(cs, src, dst); return;
					case GREYA_PIXEL : ColorConverter<RGBA_PIXEL, GREYA_PIXEL>::op(cs, src, dst); return;
					case GREY_PIXEL : ColorConverter<RGBA_PIXEL, GREY_PIXEL>::op(cs, src, dst); return;
					default: break;
				}
				break;
			case RGB_PIXEL :
				switch (dst.pixel) {
					case RGBA_PIXEL : ColorConverter<RGB_PIXEL, RGBA_PIXEL>::op(cs, src, dst); return;
					case GREYA_PIXEL : ColorConverter<RGB_PIXEL, GREYA_PIXEL>::op(cs, src, dst); return;
					case GREY_PIXEL : ColorConverter<RGB_PIXEL, GREY_PIXEL>::op(cs, src, dst); return;
					default: break;
				}
				break;
			case GREYA_PIXEL :
				switch (dst.pixel) {
					case RGBA_PIXEL : ColorConverter<GREYA_PIXEL, RGBA_PIXEL>::op(cs, src, dst); return;
					case RGB_PIXEL : ColorConverter<GREYA_PIXEL, RGB_PIXEL>::op(cs, src, dst); return;
					case GREY_PIXEL : ColorConverter<GREYA_PIXEL, GREY_PIXEL>::op(cs, src, dst); return;
					default: break;
				}
				break;
			case GREY_PIXEL :
				switch (dst.pixel) {
					case RGBA_PIXEL : ColorConverter<GREY_PIXEL, RGBA_PIXEL>::op(cs, src, dst); return;
					case RGB_PIXEL : ColorConverter<GREY_PIXEL, RGB_PIXEL>::op(cs, src, dst); return;
					case GREYA_PIXEL : ColorConverter<GREY_PIXEL, GREYA_PIXEL>::op(cs, src, dst); return;
					default: break;
				}
				break;
			default: break;
		}

		dst.copy(src);
		return;
	}

	struct ColorConvertContext {
		Nan::Persistent<Object> dstimage;
		Nan::Persistent<Object> buffer;
		Nan::Persistent<Function> cb;
		NativeImage src;
		NativeImage dst;
		ColorSettings cs;
	};

	void UV_colorConvert(uv_work_t* work_req) {
		ColorConvertContext *ctx = reinterpret_cast<ColorConvertContext*>(work_req->data);
		doColorConvert(ctx->cs, ctx->src, ctx->dst);
	}

	void V8_colorConvert(uv_work_t* work_req, int) {
		Nan::HandleScope scope;
		ColorConvertContext *ctx = reinterpret_cast<ColorConvertContext*>(work_req->data);
		makeCallback(Nan::New(ctx->cb), 0, Nan::New(ctx->dstimage));
		ctx->dstimage.Reset();
		ctx->buffer.Reset();
		ctx->cb.Reset();
		delete work_req;
		delete ctx;
	}

	NAN_METHOD(colorConvert) {
		if (info.Length() != 3 || !info[0]->IsObject() || !info[1]->IsObject() || !info[2]->IsFunction()) {
			Nan::ThrowError("expected: colorConvert(image, opts, cb)");
			return;
		}
		Local<Object> img = info[0]->ToObject();
		Local<Object> opts = info[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(info[2]);

		NativeImage src;
		src = jsImageToNativeImage(img);
		if (!src.data) {
			Nan::ThrowError("invalid image");
			return;
		}

		PixelMode toPixel = pixelSymbolToEnum(opts->Get(Nan::New(pixel_symbol)));
		if (toPixel == INVALID_PIXEL) {
			Nan::ThrowError("expected pixel mode");
			return;
		}

		ColorConvertContext *ctx = new ColorConvertContext;
		Local<Object> dstimage = newJsImage(src.width, src.height, toPixel);
		ctx->dstimage.Reset(dstimage);
		ctx->buffer.Reset(img);
		ctx->cb.Reset(cb);
		ctx->dst = jsImageToNativeImage(dstimage);
		assert(ctx->dst.data != 0);
		ctx->src = src;

		getSettings(ctx->cs, opts);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_colorConvert, V8_colorConvert);
	}

	NAN_METHOD(colorConvertSync) {
		if (info.Length() != 2 || !info[0]->IsObject() || !info[1]->IsObject()) {
			Nan::ThrowError("expected: colorConvertSync(image, opts)");
			return;
		}
		Local<Object> img = info[0]->ToObject();
		Local<Object> opts = info[1]->ToObject();

		NativeImage src;
		src = jsImageToNativeImage(img);
		if (!src.data) {
			Nan::ThrowError("invalid image");
			return;
		}

		PixelMode toPixel = pixelSymbolToEnum(opts->Get(Nan::New(pixel_symbol)));
		if (toPixel == INVALID_PIXEL) {
			Nan::ThrowError("expected pixel mode");
			return;
		}

		ColorSettings cs;
		getSettings(cs, opts);

		Local<Object> dstimage = newJsImage(src.width, src.height, toPixel);
		NativeImage dst = jsImageToNativeImage(dstimage);

		doColorConvert(cs, src, dst);

		info.GetReturnValue().Set(dstimage);
	}

}

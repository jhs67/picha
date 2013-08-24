
#include "colorconvert.h"

namespace picha {

	struct ColorSettings {
		ColorSettings() : rFactor(0.299f), gFactor(0.587f), bFactor(0.114) {}
		float rFactor, gFactor, bFactor;
	};

	void getSettings(ColorSettings& s, Handle<Object> opts) {
		double d;
		Local<Value> v;
		v = opts->Get(redWeight_symbol);
		d = v->NumberValue();
		if (d == d) s.rFactor = d;
		v = opts->Get(greenWeight_symbol);
		d = v->NumberValue();
		if (d == d) s.gFactor = d;
		v = opts->Get(blueWeight_symbol);
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
			dst.alloc(src.width, src.height, Dst);
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

		dst.clone(src);
		return;
	}

	struct ColorConvertContext {
		Persistent<Value> buffer;
		Persistent<Function> cb;
		NativeImage src;
		NativeImage dst;
		ColorSettings cs;
	};

	void UV_colorConvert(uv_work_t* work_req) {
		ColorConvertContext *ctx = reinterpret_cast<ColorConvertContext*>(work_req->data);
		doColorConvert(ctx->cs, ctx->src, ctx->dst);
	}

	void V8_colorConvert(uv_work_t* work_req, int) {
		HandleScope scope;
		ColorConvertContext *ctx = reinterpret_cast<ColorConvertContext*>(work_req->data);

		NativeImage dst = ctx->dst;
		Local<Function> cb = Local<Function>::New(ctx->cb);
		delete ctx;

		Local<Object> r = nativeImageToJsImage(dst);
		dst.free();

		TryCatch try_catch;

		Handle<Value> argv[2] = { Undefined(), r };
		cb->Call(Context::GetCurrent()->Global(), 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);

		return;
	}

	Handle<Value> colorConvert(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 3 || !args[0]->IsObject() || !args[1]->IsObject() || !args[2]->IsFunction()) {
			ThrowException(Exception::Error(String::New("expected: colorConvert(image, opts, cb)")));
			return scope.Close(Undefined());
		}
		Local<Object> img = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		NativeImage src;
		src = jsImageToNativeImage(img);
		if (!src.data) {
			ThrowException(Exception::Error(String::New("invalid image")));
			return scope.Close(Undefined());
		}

		PixelMode toPixel = pixelSymbolToEnum(opts->Get(pixel_symbol));
		if (toPixel == INVALID_PIXEL) {
			ThrowException(Exception::Error(String::New("expected pixel mode")));
			return scope.Close(Undefined());
		}

		ColorConvertContext *ctx = new ColorConvertContext;
		ctx->buffer = Persistent<Value>::New(img->Get(data_symbol));
		ctx->cb = Persistent<Function>::New(cb);
		ctx->dst.pixel = toPixel;
		ctx->src = src;

		getSettings(ctx->cs, opts);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_colorConvert, V8_colorConvert);

		return scope.Close(Undefined());
	}

	Handle<Value> colorConvertSync(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsObject()) {
			ThrowException(Exception::Error(String::New("expected: colorConvertSync(image, opts)")));
			return scope.Close(Undefined());
		}
		Local<Object> img = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();

		NativeImage src;
		src = jsImageToNativeImage(img);
		if (!src.data) {
			ThrowException(Exception::Error(String::New("invalid image")));
			return scope.Close(Undefined());
		}

		PixelMode toPixel = pixelSymbolToEnum(opts->Get(pixel_symbol));
		if (toPixel == INVALID_PIXEL) {
			ThrowException(Exception::Error(String::New("expected pixel mode")));
			return scope.Close(Undefined());
		}

		ColorSettings cs;
		getSettings(cs, opts);

		NativeImage dst;
		dst.pixel = toPixel;

		doColorConvert(cs, src, dst);

		Local<Object> r = nativeImageToJsImage(dst);
		dst.free();

		return scope.Close(r);
	}

}

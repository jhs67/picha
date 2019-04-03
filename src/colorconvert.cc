
#include "colorconvert.h"

namespace picha {

	void getSettings(ColorSettings& s, Handle<Object> opts) {
		double d;
		Local<Value> v;
		v = opts->Get(Nan::New(redWeight_symbol));
		d = v->NumberValue(Nan::GetCurrentContext()).FromMaybe(0);
		if (d == d) s.rFactor = d;
		v = opts->Get(Nan::New(greenWeight_symbol));
		d = v->NumberValue(Nan::GetCurrentContext()).FromMaybe(0);
		if (d == d) s.gFactor = d;
		v = opts->Get(Nan::New(blueWeight_symbol));
		d = v->NumberValue(Nan::GetCurrentContext()).FromMaybe(0);
		if (d == d) s.bFactor = d;
		float n = 1.0f / (s.rFactor + s.gFactor + s.bFactor);
		s.rFactor *= n;
		s.gFactor *= n;
		s.bFactor *= n;
	}

	template <int Src, int Dst> struct ChannelConvertOp;

	template <int N>
	struct ChannelConvertOp<N, N> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			for (int i = 0; i < N; ++i)
				d[i] = s[i];
		}
	};

	template <>
	struct ChannelConvertOp<1, 2> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0];
			d[1] = 1;
		}
	};

	template <>
	struct ChannelConvertOp<1, 3> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0];
			d[1] = s[0];
			d[2] = s[0];
		}
	};

	template <>
	struct ChannelConvertOp<1, 4> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0];
			d[1] = s[0];
			d[2] = s[0];
			d[3] = 1;
		}
	};

	template <>
	struct ChannelConvertOp<2, 1> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0];
		}
	};

	template <>
	struct ChannelConvertOp<2, 3> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0];
			d[1] = s[1];
			d[2] = 0;
		}
	};

	template <>
	struct ChannelConvertOp<2, 4> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0];
			d[1] = s[0];
			d[2] = s[0];
			d[3] = s[1];
		}
	};

	template <>
	struct ChannelConvertOp<3, 1> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0] * cs.rFactor + s[1] * cs.gFactor + s[2] * cs.bFactor;
		}
	};

	template <>
	struct ChannelConvertOp<3, 2> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0] * cs.rFactor + s[1] * cs.gFactor + s[2] * cs.bFactor;
			d[1] = 1;
		}
	};

	template <>
	struct ChannelConvertOp<3, 4> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0];
			d[1] = s[1];
			d[2] = s[2];
			d[3] = 1;
		}
	};

	template <>
	struct ChannelConvertOp<4, 1> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0] * cs.rFactor + s[1] * cs.gFactor + s[2] * cs.bFactor;
		}
	};

	template <>
	struct ChannelConvertOp<4, 2> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0] * cs.rFactor + s[1] * cs.gFactor + s[2] * cs.bFactor;
			d[1] = s[3];
		}
	};

	template <>
	struct ChannelConvertOp<4, 3> {
		static void op(const ColorSettings &cs, const float *s, float *d) {
			d[0] = s[0];
			d[1] = s[1];
			d[2] = s[2];
		}
	};

	template <PixelMode Src, PixelMode Dst> struct ColorConverter {
		static void op(const ColorSettings &cs, NativeImage& src, NativeImage& dst) {
			assert(dst.width == src.width);
			assert(dst.height == src.height);
			assert(src.pixel == Src);
			assert(dst.pixel == Dst);
			float srcpack[PixelTraits<Src>::channels], dstpack[PixelTraits<Dst>::channels];
			for (int i = 0; i < src.height; ++i) {
				char *s = src.row(i), *d = dst.row(i);
				for (int j = 0; j < src.width; ++j, s += PixelTraits<Src>::bytes, d += PixelTraits<Dst>::bytes) {
					PixelTraits<Src>::unpack(s, srcpack);
					ChannelConvertOp<PixelTraits<Src>::channels, PixelTraits<Dst>::channels>::op(cs, srcpack, dstpack);
					PixelTraits<Dst>::pack(dstpack, d);
				}
			}
		}
	};


	template <PixelMode Src>
	void doSrcColorConvert(const ColorSettings &cs, NativeImage& src, NativeImage& dst) {
		assert(src.pixel == Src);
		switch (dst.pixel) {
			case RGB_PIXEL : ColorConverter<Src, RGB_PIXEL>::op(cs, src, dst); return;
			case RGBA_PIXEL : ColorConverter<Src, RGBA_PIXEL>::op(cs, src, dst); return;
			case GREYA_PIXEL : ColorConverter<Src, GREYA_PIXEL>::op(cs, src, dst); return;
			case GREY_PIXEL : ColorConverter<Src, GREY_PIXEL>::op(cs, src, dst); return;
			case R16_PIXEL : ColorConverter<Src, R16_PIXEL>::op(cs, src, dst); return;
			case R16G16_PIXEL : ColorConverter<Src, R16G16_PIXEL>::op(cs, src, dst); return;
			case R16G16B16_PIXEL : ColorConverter<Src, R16G16B16_PIXEL>::op(cs, src, dst); return;
			case R16G16B16A16_PIXEL : ColorConverter<Src, R16G16B16A16_PIXEL>::op(cs, src, dst); return;
			default: break;
		}
	}

	void doColorConvert(const ColorSettings &cs, NativeImage& src, NativeImage& dst) {
		if (src.pixel == dst.pixel) {
			dst.copy(src);
			return;
		}

		switch (src.pixel) {
			case RGB_PIXEL : doSrcColorConvert<RGB_PIXEL>(cs, src, dst); break;
			case RGBA_PIXEL : doSrcColorConvert<RGBA_PIXEL>(cs, src, dst); break;
			case GREYA_PIXEL : doSrcColorConvert<GREYA_PIXEL>(cs, src, dst); break;
			case GREY_PIXEL : doSrcColorConvert<GREY_PIXEL>(cs, src, dst); break;
			case R16_PIXEL : doSrcColorConvert<R16_PIXEL>(cs, src, dst); break;
			case R16G16_PIXEL : doSrcColorConvert<R16G16_PIXEL>(cs, src, dst); break;
			case R16G16B16_PIXEL : doSrcColorConvert<R16G16B16_PIXEL>(cs, src, dst); break;
			case R16G16B16A16_PIXEL : doSrcColorConvert<R16G16B16A16_PIXEL>(cs, src, dst); break;
			default: break;
		}
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
		MaybeLocal<Object> mimg = info[0]->ToObject(Nan::GetCurrentContext());
		MaybeLocal<Object> mopts = info[1]->ToObject(Nan::GetCurrentContext());
		if (mimg.IsEmpty() || mopts.IsEmpty())
			return;
		Local<Object> img = mimg.ToLocalChecked();
		Local<Object> opts = mopts.ToLocalChecked();
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
		MaybeLocal<Object> mimg = info[0]->ToObject(Nan::GetCurrentContext());
		MaybeLocal<Object> mopts = info[1]->ToObject(Nan::GetCurrentContext());
		if (mimg.IsEmpty() || mopts.IsEmpty())
			return;
		Local<Object> img = mimg.ToLocalChecked();
		Local<Object> opts = mopts.ToLocalChecked();

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

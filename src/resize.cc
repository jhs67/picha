
#include <string.h>
#include <cmath>
#include <vector>
#include "resize.h"

namespace picha {

	using std::vector;

	typedef vector<float> PixelContribs;

	struct ContribRange {
		int left, right, weights;
	};

	typedef vector<ContribRange> RangeVector;

	template <typename Filter> void makeContribs(RangeVector & ranges, const Filter & filter,
		float scale, PixelContribs& storage, int size) {

		float fscale(std::max(std::max(scale, 1.0f), 1.0f / filter.support()));
		float fsupport(filter.support() * fscale);
		float iscale(1.0f / fscale);

		float center = float(0.5) * scale;
		for (RangeVector::iterator i = ranges.begin(); i != ranges.end(); ++i, center += scale) {
			float totalweight(0);
			int left = int(std::max(float(0), std::ceil(center - fsupport)));
			int right = int(std::min(float(size - 1), std::floor(center + fsupport)));
			while (left < right && filter((center - left) * iscale) == 0)
				left += 1;
			while (right > left && filter((center - right) * iscale) == 0)
				right -= 1;
			i->left = left;
			i->right = right;
			i->weights = storage.size();
			for (int j = left; j <= right; ++j) {
				float o = center - float(j);
				float w = filter(o * iscale);
				storage.push_back(w);
				totalweight += w;
			}

			assert(totalweight > 0);
			float normalize = float(1) / totalweight;
			for (int j = i->weights; j != int(storage.size()); ++j)
				storage[j] *= normalize;
		}
	}

	struct FloatBuffer {
		FloatBuffer(int w, int h, int d) {
			width = w;
			height = h;
			stride = w * d;
			data.resize(stride * height);
		}

		float *row(int y) { return &data[y * stride]; }

		int width, height, stride;
		vector<float> data;
	};

	template <PixelMode Pixel, typename Filter>
	void resizeImagePixel(NativeImage & src, NativeImage & dst, const Filter & filter) {
		assert(src.pixel == Pixel);
		assert(dst.pixel == Pixel);

		// Scale and support values.
		float xscale = src.width / float(dst.width);
		float yscale = src.height / float(dst.height);
		float xfscale = std::max(std::max(xscale, 1.0f), 1.0f / filter.support());
		float yfscale = std::max(std::max(yscale, 1.0f), 1.0f / filter.support());
		float xfsupport = filter.support() * xfscale;
		float yfsupport = filter.support() * yfscale;
		int maxxcontrib = int(std::ceil(2 * xfsupport));
		int maxycontrib = int(std::ceil(2 * yfsupport));

		// A temporary image to hold resized rows as we move through the image.
		const int pixelChannels = PixelTraits<Pixel>::channels;
		FloatBuffer tmp(dst.width, maxycontrib, pixelChannels);

		// Buffer to hold pre-calculated weights
		PixelContribs contribs;
		contribs.reserve(maxxcontrib * dst.width + maxycontrib * dst.height);

		// Pre-computed source contributions for the rows.
		RangeVector rowcontribs;
		rowcontribs.resize(dst.width);
		makeContribs(rowcontribs, filter, xscale, contribs, src.width);

		// Pre-computed source contributions for the columns.
		RangeVector columncontribs;
		columncontribs.resize(dst.height);
		makeContribs(columncontribs, filter, yscale, contribs, src.height);

		float centery = float(0.5) * yscale;
		int srcrow(int(std::max(float(0), std::ceil(centery - yfsupport))));
		for (int y = 0; y < dst.height; ++y, centery += yscale) {

			// Resize any source rows needed for this row of the destination.
			int needrow(std::min(src.height - 1, int(centery + yfsupport)));
			while (srcrow <= needrow) {
				float unpacked[pixelChannels];
				char * srcdata = src.row(srcrow);
				float * tmprow = tmp.row(srcrow % maxycontrib);
				memset(tmprow, 0, sizeof(float) * pixelChannels * tmp.width);
				for (int x = 0; x < tmp.width; ++x) {
					for (int c = rowcontribs[x].left, w = rowcontribs[x].weights; c <= rowcontribs[x].right; ++c, ++w) {
						PixelTraits<Pixel>::unpack(srcdata + c * PixelTraits<Pixel>::bytes, unpacked);
						for (int p = 0; p < pixelChannels; ++p)
							tmprow[p] += contribs[w] * unpacked[p];
					}
					tmprow += pixelChannels;
				}
				++srcrow;
			}

			// Resize this row of the destination using the resized temporary rows.
			char * dstrow = dst.row(y);
			for (int x = 0; x < dst.width; ++x) {
				float pixel[pixelChannels] = {};
				for (int c = columncontribs[y].left, w = columncontribs[y].weights; c <= columncontribs[y].right; ++c, ++w) {
					float * srcpixel = tmp.row(c % maxycontrib) + x * pixelChannels;
					for (int p = 0; p < pixelChannels; ++p)
						pixel[p] += contribs[w] * srcpixel[p];
				}
				PixelTraits<Pixel>::pack(pixel, dstrow);
				dstrow += PixelTraits<Pixel>::bytes;
			}
		}
	}

	template <typename Filter> void resizeImage(NativeImage& src, NativeImage& dst, const Filter& filter) {
		assert(src.pixel == dst.pixel);
		switch (src.pixel) {
			case RGBA_PIXEL : resizeImagePixel<RGBA_PIXEL>(src, dst, filter); break;
			case RGB_PIXEL : resizeImagePixel<RGB_PIXEL>(src, dst, filter); break;
			case GREY_PIXEL : resizeImagePixel<GREY_PIXEL>(src, dst, filter); break;
			case GREYA_PIXEL : resizeImagePixel<GREYA_PIXEL>(src, dst, filter); break;
			case R16_PIXEL : resizeImagePixel<R16_PIXEL>(src, dst, filter); break;
			case R16G16_PIXEL : resizeImagePixel<R16G16_PIXEL>(src, dst, filter); break;
			case R16G16B16_PIXEL : resizeImagePixel<R16G16B16_PIXEL>(src, dst, filter); break;
			case R16G16B16A16_PIXEL : resizeImagePixel<R16G16B16A16_PIXEL>(src, dst, filter); break;
			default : assert(false);
		}
	}

	enum ResizeFilterTag {
		CubicFilterTag,
		LanczosFilterTag,
		CatmulRomFilterTag,
		MitchelFilterTag,
		BoxFilterTag,
		TriangleFilterTag,

		InvalidFilterTag,
	};

	static Persistent<String>* const resizeFilterSymbols[] = {
		&cubic_symbol, &lanczos_symbol, &catmulrom_symbol, &mitchel_symbol, &box_symbol, &triangle_symbol
	};

	ResizeFilterTag symbolToResizeFilter(Local<Value> s) {
		for (int i = 0; i < InvalidFilterTag; ++i)
			if (s->StrictEquals(Nan::New(*resizeFilterSymbols[i])))
				return static_cast<ResizeFilterTag>(i);
		return InvalidFilterTag;
	}

	struct ResizeOptions {
		ResizeOptions() : filter(CubicFilterTag), width(0.70f) {}
		ResizeFilterTag filter;
		float width;
	};

	bool getResizeOptions(ResizeOptions& s, Local<Object> opts) {
		Local<Value> v = opts->Get(Nan::GetCurrentContext(), Nan::New(filter_symbol)).FromMaybe(Local<Value>(Nan::Undefined()));
		if (!v->IsUndefined()) {
			s.width = 1.0f;
			s.filter = symbolToResizeFilter(Nan::Get(opts, Nan::New(filter_symbol)).FromMaybe(Local<Value>(Nan::Undefined())));
			if (s.filter == InvalidFilterTag) {
				Nan::ThrowError("invalid filter mode");
				return false;
			}
		}
		v = opts->Get(Nan::GetCurrentContext(), Nan::New(filterScale_symbol)).FromMaybe(Local<Value>(Nan::Undefined()));
		if (!v->IsUndefined()) {
			s.width = v->NumberValue(Nan::GetCurrentContext()).FromMaybe(0);
			if (s.width != s.width || s.width <= 0) {
				Nan::ThrowError("invalid filter width");
				return false;
			}
		}
		return true;
	}

	struct TriangleFilter {
		float support() const { return 1.0f; }
		float operator () (float o) const { return 1.0f - std::abs(o); }
	};

	struct BoxFilter {
		float support() const { return 0.5f; }
		float operator () (float o) const { return 1.0f; }
	};

	template <typename Base> struct MitchelFamilyFilter {
		float support() const { return 2.0f; }

		float operator () (float o) const {
			const float B = Base::B();
			const float C = Base::C();
			float x = std::abs(o);
			if (x < 1) {
				const float A_3 = (12 - 9 * B - 6 * C) / 6;
				const float A_2 = (-18 + 12 * B + 6 * C) / 6;
				const float A_0 = (6 - 2 * B) / 6;
				return A_0 + (x * x * (A_2 + x * A_3));
			}
			else {
				const float B_3 = (-B - 6 * C) / 6;
				const float B_2 = (6 * B + 30 * C) / 6;
				const float B_1 = (-12 * B - 48 * C) / 6;
				const float B_0 = (8 * B + 24 * C) / 6;
				return B_0 + (x * (B_1 + x * (B_2 + x * B_3)));
			}
		}
	};

	struct CatmulRomParams {
		static float B() { return 0.0f; }
		static float C() { return 0.5f; }
	};

	struct MitchelParams {
		static float B() { return 0.333f; }
		static float C() { return 0.333f; }
	};

	typedef MitchelFamilyFilter<CatmulRomParams> CatmulRomFilter;
	typedef MitchelFamilyFilter<MitchelParams> MitchelFilter;

	template <unsigned A> struct LanczosFamilyFilter {
		float support() const { return float(A); }

		float operator () (float o) const {
			float x = o * float(M_PI), x2 = x * x;
			return x2 == 0 ? 1.0f : A * std::sin(x) * std::sin(x / A) / x2;
		}
	};

	typedef LanczosFamilyFilter<2> LanczosFilter;

	struct CubicFilter {
		float support() const { return 2.0f; }
		float operator () (float o) const { o = std::abs(o); return 1.0f - o * o * (0.75f - 0.25f * o); }
	};

	template <typename F> struct ScaledFilter {
		F filter;
		float scale;
		ScaledFilter(float s) : scale(s) {}
		float support() const { return scale * filter.support(); }
		float operator () (float f) const { return filter(f / scale) / scale; }
	};

	void resizeImage(const ResizeOptions& opts, NativeImage& src, NativeImage& dst) {
		switch (opts.filter) {
			case CubicFilterTag : resizeImage(src, dst, ScaledFilter<CubicFilter>(opts.width)); break;
			case LanczosFilterTag : resizeImage(src, dst, ScaledFilter<LanczosFilter>(opts.width)); break;
			case CatmulRomFilterTag : resizeImage(src, dst, ScaledFilter<CatmulRomFilter>(opts.width)); break;
			case MitchelFilterTag : resizeImage(src, dst, ScaledFilter<MitchelFilter>(opts.width)); break;
			case BoxFilterTag : resizeImage(src, dst, ScaledFilter<BoxFilter>(opts.width)); break;
			case TriangleFilterTag : resizeImage(src, dst, ScaledFilter<TriangleFilter>(opts.width)); break;
			default: assert(false);
		}
	}

	struct ResizeContext {
		Nan::Persistent<Value> srcbuffer;
		Nan::Persistent<Object> dstimage;
		Nan::Persistent<Function> cb;
		ResizeOptions opts;
		NativeImage src;
		NativeImage dst;
	};

	void UV_resize(uv_work_t* work_req) {
		ResizeContext *ctx = reinterpret_cast<ResizeContext*>(work_req->data);
		resizeImage(ctx->opts, ctx->src, ctx->dst);
	}

	void V8_resize(uv_work_t* work_req, int) {
		Nan::HandleScope scope;

		ResizeContext *ctx = reinterpret_cast<ResizeContext*>(work_req->data);

		Local<Value> dst = Nan::New(ctx->dstimage);
		Local<Function> cb = Nan::New<Function>(ctx->cb);
		ctx->srcbuffer.Reset();
		ctx->dstimage.Reset();
		ctx->cb.Reset();
		delete work_req;
		delete ctx;

		Nan::TryCatch try_catch;

		Local<Value> argv[2] = { Nan::Undefined(), dst };
		Nan::AsyncResource ass("picha");
		ass.runInAsyncScope(Nan::GetCurrentContext()->Global(), cb, 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);

		return;
	}

	NAN_METHOD(resize) {
		if (info.Length() != 3 || !info[0]->IsObject() || !info[1]->IsObject() || !info[2]->IsFunction()) {
			Nan::ThrowError("expected: resize(image, opts, cb)");
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

		int width = Nan::Get(opts, Nan::New(width_symbol)).FromMaybe(Local<Value>(Nan::Undefined()))->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);
		int height = Nan::Get(opts, Nan::New(height_symbol)).FromMaybe(Local<Value>(Nan::Undefined()))->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);
		if (width <= 0 || height <= 0) {
			Nan::ThrowError("invalid dimensions");
			return;
		}

		ResizeOptions rsopts;
		if (!getResizeOptions(rsopts, opts)) {
			return;
		}

		ResizeContext * ctx = new ResizeContext;
		Local<Object> jsdst = newJsImage(width, height, src.pixel);
		ctx->srcbuffer.Reset(Nan::Get(img, Nan::New(data_symbol)).FromMaybe(Local<Value>(Nan::Undefined())));
		ctx->dstimage.Reset(jsdst);
		ctx->cb.Reset(cb);
		ctx->dst = jsImageToNativeImage(jsdst);
		ctx->opts = rsopts;
		ctx->src = src;

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_resize, V8_resize);
	}

	NAN_METHOD(resizeSync) {
		if (info.Length() != 2 || !info[0]->IsObject() || !info[1]->IsObject()) {
			Nan::ThrowError("expected: resizeSync(image, opts)");
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
		}

		int width = Nan::Get(opts, Nan::New(width_symbol)).FromMaybe(Local<Value>(Nan::Undefined()))->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);
		int height = Nan::Get(opts, Nan::New(height_symbol)).FromMaybe(Local<Value>(Nan::Undefined()))->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);
		if (width <= 0 || height <= 0) {
			Nan::ThrowError("invalid dimensions");
		}

		ResizeOptions rsopts;
		if (!getResizeOptions(rsopts, opts)) {
			return;
		}

		Local<Object> jsdst = newJsImage(width, height, src.pixel);
		NativeImage dst = jsImageToNativeImage(jsdst);

		resizeImage(rsopts, src, dst);

		info.GetReturnValue().Set(jsdst);
		return;
	}

}

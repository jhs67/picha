#ifndef picha_picha_h_
#define picha_picha_h_

#include <assert.h>
#include <v8.h>
#include <node.h>
#include <nan.h>

namespace picha {

	using namespace v8;
	using namespace node;

	//--------------------------------------------------------------------------------------------------
	//--

#	define STATIC_SYMBOLS \
	SSYMBOL(rgb)\
	SSYMBOL(rgba)\
	SSYMBOL(grey)\
	SSYMBOL(greya)\
	SSYMBOL(r16)\
	SSYMBOL(r16g16)\
	SSYMBOL(r16g16b16)\
	SSYMBOL(r16g16b16a16)\
	SSYMBOL(width)\
	SSYMBOL(height)\
	SSYMBOL(stride)\
	SSYMBOL(pixel)\
	SSYMBOL(data)\
	SSYMBOL(quality)\
	SSYMBOL(redWeight)\
	SSYMBOL(greenWeight)\
	SSYMBOL(blueWeight)\
	SSYMBOL(filter)\
	SSYMBOL(filterScale)\
	SSYMBOL(cubic)\
	SSYMBOL(lanczos)\
	SSYMBOL(catmulrom)\
	SSYMBOL(mitchel)\
	SSYMBOL(triangle)\
	SSYMBOL(box)\
	SSYMBOL(decodeSync)\
	SSYMBOL(decode)\
	SSYMBOL(encodeSync)\
	SSYMBOL(encode)\
	SSYMBOL(stat)\
	SSYMBOL(encodes)\
	SSYMBOL(index)\
	SSYMBOL(compression)\
	SSYMBOL(lzw)\
	SSYMBOL(deflate)\
	SSYMBOL(none)\
	SSYMBOL(preset)\
	SSYMBOL(photo)\
	SSYMBOL(picture)\
	SSYMBOL(drawing)\
	SSYMBOL(text)\
	SSYMBOL(lossless)\
	SSYMBOL(default)\
	SSYMBOL(icon)\
	SSYMBOL(alphaQuality)\
	SSYMBOL(exact)\
	SSYMBOL(deep)\
	/**/

#	define SSYMBOL(a) extern Nan::Persistent<String> a ## _symbol;
	STATIC_SYMBOLS
#	undef SSYMBOL

	inline Local<String> makeSymbol(const char *n) { return Nan::New(n).ToLocalChecked(); }
	void makeCallback(Local<Function> cb, const char * error, Local<Value> v);


	//--------------------------------------------------------------------------------------------------
	//--

	enum PixelMode {
		INVALID_PIXEL = -1,

		RGB_PIXEL = 0,
		RGBA_PIXEL = 1,
		GREY_PIXEL = 2,
		GREYA_PIXEL = 3,
		R16_PIXEL = 4,
		R16G16_PIXEL = 5,
		R16G16B16_PIXEL = 6,
		R16G16B16A16_PIXEL = 7,

		NUM_PIXELS
	};

	template<PixelMode Src> struct PixelTraits;

	namespace i {

		template <int N, typename T>
		void linear_unpack(const void *src, float *dst) {
			const T *s = static_cast<const T*>(src);
			const float i = float(std::numeric_limits<T>::min());
			const float a = float(std::numeric_limits<T>::max());
			for (int n = 0; n < N; ++n)
				*dst++ = (*s++ - i) * (1 / (a - i));
		}

		template <int N, typename T>
		void linear_pack(const float *src, void *dst) {
			T *d = static_cast<T*>(dst);
			const float i = float(std::numeric_limits<T>::min());
			const float a = float(std::numeric_limits<T>::max());
			for (int n = 0; n < N; ++n)
				*d++ = T(std::max(i, std::min(a, i + *src++ * (a - i) + 0.5f)));
		}

	}

	template<> struct PixelTraits<RGB_PIXEL> {
		static const int bytes = 3;
		static const int channels = 3;
		static void unpack(const void *s, float *d) { i::linear_unpack<channels, uint8_t>(s, d); }
		static void pack(const float *s, void *d) { i::linear_pack<channels, uint8_t>(s, d); }
	};

	template<> struct PixelTraits<RGBA_PIXEL> {
		static const int bytes = 4;
		static const int channels = 4;
		static void unpack(const void *s, float *d) { i::linear_unpack<channels, uint8_t>(s, d); }
		static void pack(const float *s, void *d) { i::linear_pack<channels, uint8_t>(s, d); }
	};

	template<> struct PixelTraits<GREY_PIXEL> {
		static const int bytes = 1;
		static const int channels = 1;
		static void unpack(const void *s, float *d) { i::linear_unpack<channels, uint8_t>(s, d); }
		static void pack(const float *s, void *d) { i::linear_pack<channels, uint8_t>(s, d); }
	};

	template<> struct PixelTraits<GREYA_PIXEL> {
		static const int bytes = 2;
		static const int channels = 2;
		static void unpack(const void *s, float *d) { i::linear_unpack<channels, uint8_t>(s, d); }
		static void pack(const float *s, void *d) { i::linear_pack<channels, uint8_t>(s, d); }
	};

	template<> struct PixelTraits<R16_PIXEL> {
		static const int bytes = 2;
		static const int channels = 1;
		static void unpack(const void *s, float *d) { i::linear_unpack<channels, uint16_t>(s, d); }
		static void pack(const float *s, void *d) { i::linear_pack<channels, uint16_t>(s, d); }
	};

	template<> struct PixelTraits<R16G16_PIXEL> {
		static const int bytes = 4;
		static const int channels = 2;
		static void unpack(const void *s, float *d) { i::linear_unpack<channels, uint16_t>(s, d); }
		static void pack(const float *s, void *d) { i::linear_pack<channels, uint16_t>(s, d); }
	};

	template<> struct PixelTraits<R16G16B16_PIXEL> {
		static const int bytes = 6;
		static const int channels = 3;
		static void unpack(const void *s, float *d) { i::linear_unpack<channels, uint16_t>(s, d); }
		static void pack(const float *s, void *d) { i::linear_pack<channels, uint16_t>(s, d); }
	};

	template<> struct PixelTraits<R16G16B16A16_PIXEL> {
		static const int bytes = 8;
		static const int channels = 4;
		static void unpack(const void *s, float *d) { i::linear_unpack<channels, uint16_t>(s, d); }
		static void pack(const float *s, void *d) { i::linear_pack<channels, uint16_t>(s, d); }
	};

	inline int pixelBytes(PixelMode p) {
		switch (p) {
			case RGB_PIXEL: return PixelTraits<RGB_PIXEL>::bytes;
			case RGBA_PIXEL: return PixelTraits<RGBA_PIXEL>::bytes;
			case GREY_PIXEL: return PixelTraits<GREY_PIXEL>::bytes;
			case GREYA_PIXEL: return PixelTraits<GREYA_PIXEL>::bytes;
			case R16_PIXEL: return PixelTraits<R16_PIXEL>::bytes;
			case R16G16_PIXEL: return PixelTraits<R16G16_PIXEL>::bytes;
			case R16G16B16_PIXEL: return PixelTraits<R16G16B16_PIXEL>::bytes;
			case R16G16B16A16_PIXEL: return PixelTraits<R16G16B16A16_PIXEL>::bytes;
			default: return 0;
		}
	}

	inline int pixelChannels(PixelMode p) {
		switch (p) {
			case RGB_PIXEL: return PixelTraits<RGB_PIXEL>::channels;
			case RGBA_PIXEL: return PixelTraits<RGBA_PIXEL>::channels;
			case GREY_PIXEL: return PixelTraits<GREY_PIXEL>::channels;
			case GREYA_PIXEL: return PixelTraits<GREYA_PIXEL>::channels;
			case R16_PIXEL: return PixelTraits<R16_PIXEL>::channels;
			case R16G16_PIXEL: return PixelTraits<R16G16_PIXEL>::channels;
			case R16G16B16_PIXEL: return PixelTraits<R16G16B16_PIXEL>::channels;
			case R16G16B16A16_PIXEL: return PixelTraits<R16G16B16A16_PIXEL>::channels;
			default: return 0;
		}
	}

	struct NativeImage {
		char * data;
		int stride;
		int width, height;
		PixelMode pixel;

		NativeImage() : data(0), stride(0), width(0), height(0), pixel(INVALID_PIXEL) {}

		char * row(int y) const { return data + y * stride; }

		static int row_stride(int w, PixelMode p) {
			int s = pixelBytes(p) * w;
			return (s + 3) & ~3;
		}

		void copy(NativeImage& o);
	};

	NativeImage jsImageToNativeImage(Local<Object>& jimg);
	Local<Object> newJsImage(int w, int h, PixelMode pixel);
	NativeImage newNativeImage(int w, int h, PixelMode pixel);
	void freeNativeImage(NativeImage& image);
	PixelMode pixelSymbolToEnum(Local<Value> obj);
	Local<Value> pixelEnumToSymbol(PixelMode t);

}

#endif // picha_picha_h_

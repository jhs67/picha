#ifndef picha_picha_h_
#define picha_picha_h_

#include <assert.h>
#include <v8.h>
#include <node.h>

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
	SSYMBOL(index)\
	SSYMBOL(compression)\
	SSYMBOL(lzw)\
	SSYMBOL(deflate)\
	SSYMBOL(none)\
	/**/

#	define SSYMBOL(a) extern Persistent<String> a ## _symbol;
	STATIC_SYMBOLS
#	undef SSYMBOL

	void makeCallback(Handle<Function> cb, const char * error, Handle<Value> v);


	//--------------------------------------------------------------------------------------------------
	//--

	enum PixelMode {
		INVALID_PIXEL = -1,

		RGB_PIXEL = 0,
		RGBA_PIXEL = 1,
		GREY_PIXEL = 2,
		GREYA_PIXEL = 3,

		NUM_PIXELS
	};

	template<PixelMode Src> struct PixelWidth;

	template<> struct PixelWidth<RGBA_PIXEL> { static const int value = 4; };
	template<> struct PixelWidth<RGB_PIXEL> { static const int value = 3; };
	template<> struct PixelWidth<GREYA_PIXEL> { static const int value = 2; };
	template<> struct PixelWidth<GREY_PIXEL> { static const int value = 1; };

	inline int pixelWidth(PixelMode p) {
		switch (p) {
			case RGB_PIXEL: return PixelWidth<RGB_PIXEL>::value;
			case RGBA_PIXEL: return PixelWidth<RGBA_PIXEL>::value;
			case GREY_PIXEL: return PixelWidth<GREY_PIXEL>::value;
			case GREYA_PIXEL: return PixelWidth<GREYA_PIXEL>::value;
			default: return 0;
		}
	}

	typedef uint8_t PixelType;

	struct NativeImage {
		PixelType * data;
		int stride;
		int width, height;
		PixelMode pixel;

		NativeImage() : data(0), stride(0), width(0), height(0), pixel(INVALID_PIXEL) {}

		PixelType * row(int y) const { return data + y * stride; }

		static int pixel_size(PixelMode p) {
			static const int sizes[] = { 3, 4, 1, 2 };
			assert(p >= 0 && p < int(sizeof(sizes) / sizeof(sizes[0])));
			return sizes[p];
		}

		static int row_stride(int w, PixelMode p) {
			int s = pixel_size(p) * w;
			return (s + 3) & ~3;
		}

		void copy(NativeImage& o);
	};

	NativeImage jsImageToNativeImage(Local<Object>& jimg);
	Local<Object> newJsImage(int w, int h, PixelMode pixel);
	PixelMode pixelSymbolToEnum(Handle<Value> obj);
	Handle<Value> pixelEnumToSymbol(PixelMode t);

}

#endif // picha_picha_h_

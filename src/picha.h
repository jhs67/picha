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

		void clone(NativeImage& o) { alloc(o.width, o.height, o.pixel); copy(o); }
		void copy(NativeImage& o);
		void alloc(int w, int h, PixelMode p) { width = w; height = h; pixel = p; stride = row_stride(w, p); data = new PixelType[stride * h]; }
		void free() { delete[] data; data = 0; width = 0; height = 0; stride = 0; pixel = INVALID_PIXEL; }
		int dataSize() { return stride * height; }
	};

	NativeImage jsImageToNativeImage(Local<Object>& jimg);
	Local<Object> newJsImage(int w, int h, PixelMode pixel);
	Local<Object> nativeImageToJsImage(NativeImage& cimg);
	PixelMode pixelSymbolToEnum(Handle<Value> obj);
	Handle<Value> pixelEnumToSymbol(PixelMode t);

}

#endif // picha_picha_h_

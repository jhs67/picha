#ifndef picha_picha_h_
#define picha_picha_h_

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

#	define SSYMBOL(a) extern Persistent<String> a ## _symbol;
	STATIC_SYMBOLS
#	undef SSYMBOL


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

	struct NativeImage {
		char * data;
		int stride;
		int width, height;
		PixelMode pixel;

		NativeImage() : data(0), stride(0), width(0), height(0), pixel(INVALID_PIXEL) {}

		char * row(int y) const { return data + y * stride; }

		static int pixel_size(PixelMode p) {
			static const int sizes[] = { 3, 4, 1, 2 };
			assert(p >= 0 && p < sizeof(sizes) / sizeof(sizes[0]));
			return sizes[p];
		}

		static int row_stride(int w, PixelMode p) {
			int s = pixel_size(p) * w;
			return (s + 3) & ~3;
		}

		void alloc(int w, int h, PixelMode p) { width = w; height = h; pixel = p; stride = row_stride(w, p); data = new char[stride * h]; }
		void free() { delete[] data; data = 0; width = 0; height = 0; stride = 0; pixel = INVALID_PIXEL; }
	};

	NativeImage jsImageToNativeImage(Local<Object>& jimg);
	Local<Object> nativeImageToJsImage(NativeImage& cimg);

}

#endif // picha_picha_h_

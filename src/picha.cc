
#include <string.h>
#include <node_buffer.h>

#include "picha.h"
#include "pngcodec.h"
#include "jpegcodec.h"
#include "colorconvert.h"
#include "resize.h"

namespace picha {

	void NativeImage::copy(NativeImage& o) {
		assert(o.width == width);
		assert(o.height == height);
		assert(o.pixel == pixel);
		int rw = width * pixel_size(pixel);
		for (int h = 0; h < height; ++h)
			memcpy(row(h), o.row(h), rw);
	}

	static Persistent<String>* const pixelSymbols[] = {
		&rgb_symbol, &rgba_symbol, &grey_symbol, &greya_symbol
	};

	Handle<Value> pixelEnumToSymbol(PixelMode t) {
		if (t < 0 || t >= NUM_PIXELS) return Undefined();
		return *pixelSymbols[t];
	}

	PixelMode pixelSymbolToEnum(Handle<Value> obj) {
		for (int i = 0; i < NUM_PIXELS; ++i)
			if (obj->StrictEquals(*pixelSymbols[i]))
				return static_cast<PixelMode>(i);
		return INVALID_PIXEL;
	}

	NativeImage jsImageToNativeImage(Local<Object>& img) {
		NativeImage r;

		r.width = img->Get(width_symbol)->Uint32Value();
		r.height = img->Get(height_symbol)->Uint32Value();
		r.stride = img->Get(stride_symbol)->Uint32Value();
		r.pixel = pixelSymbolToEnum(img->Get(pixel_symbol));
		if (r.pixel < NUM_PIXELS && r.pixel > INVALID_PIXEL) {
			Local<Value> data = img->Get(data_symbol);
			if (Buffer::HasInstance(data)) {
				Local<Object> databuf = data->ToObject();
				size_t len = Buffer::Length(databuf);
				size_t rw = pixelWidth(r.pixel) * r.width;
				if (len >= r.height * size_t(r.stride) - r.stride + rw && r.height != 0) {
					r.data = reinterpret_cast<PixelType*>(Buffer::Data(databuf));
				}
			}
		}

		return r;
	}

	Local<Object> nativeImageToJsImage(NativeImage& cimage) {
		Local<Object> image = Object::New();
		image->Set(width_symbol, Integer::New(cimage.width));
		image->Set(height_symbol, Integer::New(cimage.height));
		image->Set(stride_symbol, Integer::New(cimage.stride));
		image->Set(pixel_symbol, pixelEnumToSymbol(cimage.pixel));

		size_t datalen = cimage.height * cimage.stride;
		Buffer *pixelbuf = Buffer::New(datalen);
		memcpy(Buffer::Data(pixelbuf), cimage.data, datalen);
		image->Set(data_symbol, pixelbuf->handle_);

		return image;
	}

	Local<Object> newJsImage(int w, int h, PixelMode pixel) {
		int stride = NativeImage::row_stride(w, pixel);
		Local<Object> image = Object::New();
		image->Set(width_symbol, Integer::New(w));
		image->Set(height_symbol, Integer::New(h));
		image->Set(stride_symbol, Integer::New(stride));
		image->Set(pixel_symbol, pixelEnumToSymbol(pixel));

		size_t datalen = h * stride;
		Buffer *pixelbuf = Buffer::New(datalen);
		image->Set(data_symbol, pixelbuf->handle_);

		return image;
	}


	void makeCallback(Handle<Function> cb, const char * error, Handle<Value> v) {
		Local<Value> e;
		Handle<Value> argv[2] = { Undefined(), v };
		if (error) {
			e = Exception::Error(String::New(error));
			argv[0] = e;
		}

		TryCatch try_catch;

		cb->Call(Context::GetCurrent()->Global(), 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);
	}


#	define SSYMBOL(a) Persistent<String> a ## _symbol;
	STATIC_SYMBOLS
#	undef SSYMBOL

	void init(Handle<Object> target) {
#		define SSYMBOL(a) a ## _symbol = NODE_PSYMBOL(# a);
		STATIC_SYMBOLS
#		undef SSYMBOL

		NODE_SET_METHOD(target, "statPng", statPng);
		NODE_SET_METHOD(target, "decodePng", decodePng);
		NODE_SET_METHOD(target, "decodePngSync", decodePngSync);
		NODE_SET_METHOD(target, "encodePng", encodePng);
		NODE_SET_METHOD(target, "encodePngSync", encodePngSync);

		NODE_SET_METHOD(target, "statJpeg", statJpeg);
		NODE_SET_METHOD(target, "decodeJpeg", decodeJpeg);
		NODE_SET_METHOD(target, "decodeJpegSync", decodeJpegSync);
		NODE_SET_METHOD(target, "encodeJpeg", encodeJpeg);
		NODE_SET_METHOD(target, "encodeJpegSync", encodeJpegSync);

		NODE_SET_METHOD(target, "colorConvert", colorConvert);
		NODE_SET_METHOD(target, "colorConvertSync", colorConvertSync);

		NODE_SET_METHOD(target, "resize", resize);
		NODE_SET_METHOD(target, "resizeSync", resizeSync);
	}

}

NODE_MODULE(picha, picha::init)

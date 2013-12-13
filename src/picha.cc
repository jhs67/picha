
#include <string.h>
#include <node_buffer.h>

#include "picha.h"
#include "resize.h"
#include "colorconvert.h"

#ifdef WITH_PNG
#include "pngcodec.h"
#endif

#ifdef WITH_JPEG
#include "jpegcodec.h"
#endif

#ifdef WITH_TIFF
#include "tiffcodec.h"
#endif

#ifdef WITH_WEBP
#include "webpcodec.h"
#endif

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

	v8::Local<v8::Function> SetPichaMethod(v8::Handle<v8::Object> o, const char * n, v8::InvocationCallback cb) {
		v8::Local<v8::String> name = v8::String::NewSymbol(n);
		v8::Local<v8::Function> fn = v8::FunctionTemplate::New(cb)->GetFunction();
		fn->SetName(name);
		o->Set(name, fn);
		return fn;
	}

#	define SSYMBOL(a) Persistent<String> a ## _symbol;
	STATIC_SYMBOLS
#	undef SSYMBOL


	void init(Handle<Object> target) {
#		define SSYMBOL(a) a ## _symbol = NODE_PSYMBOL(# a);
		STATIC_SYMBOLS
#		undef SSYMBOL

		v8::HandleScope handle_scope;

		v8::Local<v8::Object> catalog = v8::Object::New();
		v8::Local<v8::Function> fn;
		v8::Local<v8::Object> obj;

		target->Set(v8::String::NewSymbol("catalog"), catalog);

		NODE_SET_METHOD(target, "colorConvert", colorConvert);
		NODE_SET_METHOD(target, "colorConvertSync", colorConvertSync);

		NODE_SET_METHOD(target, "resize", resize);
		NODE_SET_METHOD(target, "resizeSync", resizeSync);

#ifdef WITH_JPEG

		obj = v8::Object::New();

		fn = SetPichaMethod(target, "statJpeg", statJpeg);
		obj->Set(stat_symbol, fn);
		fn = SetPichaMethod(target, "decodeJpeg", decodeJpeg);
		obj->Set(decode_symbol, fn);
		fn = SetPichaMethod(target, "decodeJpegSync", decodeJpegSync);
		obj->Set(decodeSync_symbol, fn);
		fn = SetPichaMethod(target, "encodeJpeg", encodeJpeg);
		obj->Set(encode_symbol, fn);
		fn = SetPichaMethod(target, "encodeJpegSync", encodeJpegSync);
		obj->Set(encodeSync_symbol, fn);

		catalog->Set(v8::String::NewSymbol("image/jpeg"), obj);

#endif

#ifdef WITH_PNG

		obj = v8::Object::New();

		fn = SetPichaMethod(target, "statPng", statPng);
		obj->Set(stat_symbol, fn);
		fn = SetPichaMethod(target, "decodePng", decodePng);
		obj->Set(decode_symbol, fn);
		fn = SetPichaMethod(target, "decodePngSync", decodePngSync);
		obj->Set(decodeSync_symbol, fn);
		fn = SetPichaMethod(target, "encodePng", encodePng);
		obj->Set(encode_symbol, fn);
		fn = SetPichaMethod(target, "encodePngSync", encodePngSync);
		obj->Set(encodeSync_symbol, fn);

		catalog->Set(v8::String::NewSymbol("image/png"), obj);

#endif

#ifdef WITH_TIFF

		obj = v8::Object::New();

		fn = SetPichaMethod(target, "statTiff", statTiff);
		obj->Set(stat_symbol, fn);
		fn = SetPichaMethod(target, "decodeTiff", decodeTiff);
		obj->Set(decode_symbol, fn);
		fn = SetPichaMethod(target, "decodeTiffSync", decodeTiffSync);
		obj->Set(decodeSync_symbol, fn);
		fn = SetPichaMethod(target, "encodeTiff", encodeTiff);
		obj->Set(encode_symbol, fn);
		fn = SetPichaMethod(target, "encodeTiffSync", encodeTiffSync);
		obj->Set(encodeSync_symbol, fn);

		catalog->Set(v8::String::NewSymbol("image/tiff"), obj);

#endif

#ifdef WITH_WEBP

		obj = v8::Object::New();

		fn = SetPichaMethod(target, "statWebP", statWebP);
		obj->Set(stat_symbol, fn);
		fn = SetPichaMethod(target, "decodeWebP", decodeWebP);
		obj->Set(decode_symbol, fn);
		fn = SetPichaMethod(target, "decodeWebPSync", decodeWebPSync);
		obj->Set(decodeSync_symbol, fn);
		fn = SetPichaMethod(target, "encodeWebP", encodeWebP);
		obj->Set(encode_symbol, fn);
		fn = SetPichaMethod(target, "encodeWebPSync", encodeWebPSync);
		obj->Set(encodeSync_symbol, fn);

		catalog->Set(v8::String::NewSymbol("image/webp"), obj);

#endif
	}

}

NODE_MODULE(picha, picha::init)

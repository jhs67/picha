
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
		if (t < 0 || t >= NUM_PIXELS) return NanUndefined();
		return NanNew(*pixelSymbols[t]);
	}

	PixelMode pixelSymbolToEnum(Handle<Value> obj) {
		for (int i = 0; i < NUM_PIXELS; ++i)
			if (obj->StrictEquals(NanNew(*pixelSymbols[i])))
				return static_cast<PixelMode>(i);
		return INVALID_PIXEL;
	}

	NativeImage jsImageToNativeImage(Local<Object>& img) {
		NativeImage r;

		r.width = img->Get(NanNew(width_symbol))->Uint32Value();
		r.height = img->Get(NanNew(height_symbol))->Uint32Value();
		r.stride = img->Get(NanNew(stride_symbol))->Uint32Value();
		r.pixel = pixelSymbolToEnum(img->Get(NanNew(pixel_symbol)));
		if (r.pixel < NUM_PIXELS && r.pixel > INVALID_PIXEL) {
			Local<Value> data = img->Get(NanNew(data_symbol));
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
		Local<Object> image = NanNew<Object>();
		image->Set(NanNew(width_symbol), NanNew<Integer>(cimage.width));
		image->Set(NanNew(height_symbol), NanNew<Integer>(cimage.height));
		image->Set(NanNew(stride_symbol), NanNew<Integer>(cimage.stride));
		image->Set(NanNew(pixel_symbol), pixelEnumToSymbol(cimage.pixel));

		size_t datalen = cimage.height * cimage.stride;
		Local<Value> pixelbuf = NanNewBufferHandle(datalen);
		memcpy(Buffer::Data(pixelbuf), cimage.data, datalen);
		image->Set(NanNew(data_symbol), pixelbuf);

		return image;
	}

	Local<Object> newJsImage(int w, int h, PixelMode pixel) {
		int stride = NativeImage::row_stride(w, pixel);
		Local<Object> image = NanNew<Object>();
		image->Set(NanNew(width_symbol), NanNew<Integer>(w));
		image->Set(NanNew(height_symbol), NanNew<Integer>(h));
		image->Set(NanNew(stride_symbol), NanNew<Integer>(stride));
		image->Set(NanNew(pixel_symbol), pixelEnumToSymbol(pixel));

		size_t datalen = h * stride;
		Local<Value> jspixel = NanNewBufferHandle(datalen);
		image->Set(NanNew(data_symbol), jspixel);

		return image;
	}


	void makeCallback(Handle<Function> cb, const char * error, Handle<Value> v) {
		Local<Value> e;
		Handle<Value> argv[2] = { NanUndefined(), v };
		if (error) {
			e = Exception::Error(NanNew<String>(error));
			argv[0] = e;
		}

		TryCatch try_catch;

		NanMakeCallback(NanGetCurrentContext()->Global(), cb, 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);
	}

	v8::Local<v8::Function> SetPichaMethod(v8::Handle<v8::Object> o, const char * n, NAN_METHOD((*cb))) {
		v8::Local<v8::String> name = NanNew<String>(n);
		v8::Local<v8::Function> fn = NanNew<v8::FunctionTemplate>(cb)->GetFunction();
		fn->SetName(name);
		o->Set(name, fn);
		return fn;
	}

#	define SSYMBOL(a) Persistent<String> a ## _symbol;
	STATIC_SYMBOLS
#	undef SSYMBOL


	void init(Handle<Object> target) {
#		define SSYMBOL(a) NanAssignPersistent(a ## _symbol, NanNew<String>(# a));
		STATIC_SYMBOLS
#		undef SSYMBOL

		NanScope();

		v8::Local<v8::Object> catalog = NanNew<v8::Object>();
		v8::Local<v8::Function> fn;
		v8::Local<v8::Object> obj;

		target->Set(NanNew<v8::String>("catalog"), catalog);

		NODE_SET_METHOD(target, "colorConvert", colorConvert);
		NODE_SET_METHOD(target, "colorConvertSync", colorConvertSync);

		NODE_SET_METHOD(target, "resize", resize);
		NODE_SET_METHOD(target, "resizeSync", resizeSync);

#ifdef WITH_JPEG

		obj = NanNew<v8::Object>();

		fn = SetPichaMethod(target, "statJpeg", statJpeg);
		obj->Set(NanNew(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodeJpeg", decodeJpeg);
		obj->Set(NanNew(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodeJpegSync", decodeJpegSync);
		obj->Set(NanNew(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodeJpeg", encodeJpeg);
		obj->Set(NanNew(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodeJpegSync", encodeJpegSync);
		obj->Set(NanNew(encodeSync_symbol), fn);

		catalog->Set(NanNew<v8::String>("image/jpeg"), obj);

#endif

#ifdef WITH_PNG

		obj = NanNew<v8::Object>();

		fn = SetPichaMethod(target, "statPng", statPng);
		obj->Set(NanNew(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodePng", decodePng);
		obj->Set(NanNew(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodePngSync", decodePngSync);
		obj->Set(NanNew(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodePng", encodePng);
		obj->Set(NanNew(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodePngSync", encodePngSync);
		obj->Set(NanNew(encodeSync_symbol), fn);

		catalog->Set(NanNew<v8::String>("image/png"), obj);

#endif

#ifdef WITH_TIFF

		obj = NanNew<v8::Object>();

		fn = SetPichaMethod(target, "statTiff", statTiff);
		obj->Set(NanNew(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodeTiff", decodeTiff);
		obj->Set(NanNew(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodeTiffSync", decodeTiffSync);
		obj->Set(NanNew(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodeTiff", encodeTiff);
		obj->Set(NanNew(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodeTiffSync", encodeTiffSync);
		obj->Set(NanNew(encodeSync_symbol), fn);

		catalog->Set(NanNew<v8::String>("image/tiff"), obj);

#endif

#ifdef WITH_WEBP

		obj = NanNew<v8::Object>();

		fn = SetPichaMethod(target, "statWebP", statWebP);
		obj->Set(NanNew(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodeWebP", decodeWebP);
		obj->Set(NanNew(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodeWebPSync", decodeWebPSync);
		obj->Set(NanNew(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodeWebP", encodeWebP);
		obj->Set(NanNew(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodeWebPSync", encodeWebPSync);
		obj->Set(NanNew(encodeSync_symbol), fn);

		catalog->Set(NanNew<v8::String>("image/webp"), obj);

#endif
	}

}

NODE_MODULE(picha, picha::init)

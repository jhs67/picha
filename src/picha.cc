
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

	static Nan::Persistent<String>* const pixelSymbols[] = {
		&rgb_symbol, &rgba_symbol, &grey_symbol, &greya_symbol
	};

	Handle<Value> pixelEnumToSymbol(PixelMode t) {
		if (t < 0 || t >= NUM_PIXELS) return Nan::Undefined();
		return Nan::New(*pixelSymbols[t]);
	}

	PixelMode pixelSymbolToEnum(Handle<Value> obj) {
		for (int i = 0; i < NUM_PIXELS; ++i)
			if (obj->StrictEquals(Nan::New(*pixelSymbols[i])))
				return static_cast<PixelMode>(i);
		return INVALID_PIXEL;
	}

	NativeImage jsImageToNativeImage(Local<Object>& img) {
		NativeImage r;

		r.width = img->Get(Nan::New(width_symbol))->Uint32Value();
		r.height = img->Get(Nan::New(height_symbol))->Uint32Value();
		r.stride = img->Get(Nan::New(stride_symbol))->Uint32Value();
		r.pixel = pixelSymbolToEnum(img->Get(Nan::New(pixel_symbol)));
		if (r.pixel < NUM_PIXELS && r.pixel > INVALID_PIXEL) {
			Local<Value> data = img->Get(Nan::New(data_symbol));
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

	NativeImage newNativeImage(int w, int h, PixelMode pixel) {
		NativeImage r;
		r.width = w;
		r.height = h;
		r.pixel = pixel;
		r.stride = NativeImage::row_stride(w, pixel);
		r.data = new PixelType[r.height * r.stride];
		return r;
	}

	void freeNativeImage(NativeImage& image) {
		delete[] image.data;
		image.data = 0;
	}

	Local<Object> nativeImageToJsImage(NativeImage& cimage) {
		Local<Object> image = Nan::New<Object>();
		image->Set(Nan::New(width_symbol), Nan::New<Integer>(cimage.width));
		image->Set(Nan::New(height_symbol), Nan::New<Integer>(cimage.height));
		image->Set(Nan::New(stride_symbol), Nan::New<Integer>(cimage.stride));
		image->Set(Nan::New(pixel_symbol), pixelEnumToSymbol(cimage.pixel));

		Local<Value> pixelbuf;
		size_t datalen = cimage.height * cimage.stride;
		if (Nan::NewBuffer(datalen).ToLocal(&pixelbuf)) {
			memcpy(Buffer::Data(pixelbuf), cimage.data, datalen);
			image->Set(Nan::New(data_symbol), pixelbuf);
		}

		return image;
	}

	Local<Object> newJsImage(int w, int h, PixelMode pixel) {
		int stride = NativeImage::row_stride(w, pixel);
		Local<Object> image = Nan::New<Object>();
		image->Set(Nan::New(width_symbol), Nan::New<Integer>(w));
		image->Set(Nan::New(height_symbol), Nan::New<Integer>(h));
		image->Set(Nan::New(stride_symbol), Nan::New<Integer>(stride));
		image->Set(Nan::New(pixel_symbol), pixelEnumToSymbol(pixel));

		size_t datalen = h * stride;
		Local<Value> jspixel;
		if (Nan::NewBuffer(datalen).ToLocal(&jspixel))
			image->Set(Nan::New(data_symbol), jspixel);

		return image;
	}


	void makeCallback(Local<Function> cb, const char * error, Handle<Value> v) {
		Local<Value> e;
		Local<Value> argv[2] = { Nan::Undefined(), v };
		if (error) {
			argv[0] = Nan::Error(error);
		}

		Nan::TryCatch try_catch;

		Nan::AsyncResource ass("picha");
		ass.runInAsyncScope(Nan::GetCurrentContext()->Global(), cb, 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);
	}

	v8::Local<v8::Function> SetPichaMethod(v8::Handle<v8::Object> o, const char * n, NAN_METHOD((*cb))) {
		v8::Local<v8::String> name = Nan::New(n).ToLocalChecked();
		v8::Local<v8::Function> fn = Nan::New<v8::FunctionTemplate>(cb)->GetFunction();
		fn->SetName(name);
		o->Set(name, fn);
		return fn;
	}

#	define SSYMBOL(a) Nan::Persistent<String> a ## _symbol;
	STATIC_SYMBOLS
#	undef SSYMBOL


	void init(Handle<Object> target) {
#		define SSYMBOL(a) a ## _symbol.Reset(Nan::New<String>(# a).ToLocalChecked());
		STATIC_SYMBOLS
#		undef SSYMBOL
		Nan::HandleScope scope;

		v8::Local<v8::Object> catalog = Nan::New<v8::Object>();
		v8::Local<v8::Function> fn;
		v8::Local<v8::Object> obj;

		target->Set(Nan::New("catalog").ToLocalChecked(), catalog);

		Nan::SetMethod(target, "colorConvert", colorConvert);
		Nan::SetMethod(target, "colorConvertSync", colorConvertSync);

		Nan::SetMethod(target, "resize", resize);
		Nan::SetMethod(target, "resizeSync", resizeSync);

#ifdef WITH_JPEG

		obj = Nan::New<v8::Object>();

		fn = SetPichaMethod(target, "statJpeg", statJpeg);
		obj->Set(Nan::New(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodeJpeg", decodeJpeg);
		obj->Set(Nan::New(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodeJpegSync", decodeJpegSync);
		obj->Set(Nan::New(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodeJpeg", encodeJpeg);
		obj->Set(Nan::New(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodeJpegSync", encodeJpegSync);
		obj->Set(Nan::New(encodeSync_symbol), fn);

		Nan::Set(catalog, Nan::New("image/jpeg").ToLocalChecked(), obj);

#endif

#ifdef WITH_PNG

		obj = Nan::New<v8::Object>();

		fn = SetPichaMethod(target, "statPng", statPng);
		obj->Set(Nan::New(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodePng", decodePng);
		obj->Set(Nan::New(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodePngSync", decodePngSync);
		obj->Set(Nan::New(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodePng", encodePng);
		obj->Set(Nan::New(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodePngSync", encodePngSync);
		obj->Set(Nan::New(encodeSync_symbol), fn);

		Nan::Set(catalog, Nan::New("image/png").ToLocalChecked(), obj);

#endif

#ifdef WITH_TIFF

		obj = Nan::New<v8::Object>();

		fn = SetPichaMethod(target, "statTiff", statTiff);
		obj->Set(Nan::New(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodeTiff", decodeTiff);
		obj->Set(Nan::New(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodeTiffSync", decodeTiffSync);
		obj->Set(Nan::New(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodeTiff", encodeTiff);
		obj->Set(Nan::New(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodeTiffSync", encodeTiffSync);
		obj->Set(Nan::New(encodeSync_symbol), fn);

		Nan::Set(catalog, Nan::New("image/tiff").ToLocalChecked(), obj);

#endif

#ifdef WITH_WEBP

		obj = Nan::New<v8::Object>();

		fn = SetPichaMethod(target, "statWebP", statWebP);
		obj->Set(Nan::New(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodeWebP", decodeWebP);
		obj->Set(Nan::New(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodeWebPSync", decodeWebPSync);
		obj->Set(Nan::New(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodeWebP", encodeWebP);
		obj->Set(Nan::New(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodeWebPSync", encodeWebPSync);
		obj->Set(Nan::New(encodeSync_symbol), fn);

		Nan::Set(catalog, Nan::New("image/webp").ToLocalChecked(), obj);

#endif
	}

}

NODE_MODULE(picha, picha::init)

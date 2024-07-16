
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
		int rw = width * pixelBytes(pixel);
		for (int h = 0; h < height; ++h)
			memcpy(row(h), o.row(h), rw);
	}

	static Nan::Persistent<String>* const pixelSymbols[] = {
		&rgb_symbol, &rgba_symbol, &grey_symbol, &greya_symbol,
		&r16_symbol, &r16g16_symbol, &r16g16b16_symbol, &r16g16b16a16_symbol
	};

	Local<Value> pixelEnumToSymbol(PixelMode t) {
		if (t < 0 || t >= NUM_PIXELS) return Nan::Undefined();
		return Nan::New(*pixelSymbols[t]);
	}

	PixelMode pixelSymbolToEnum(Local<Value> obj) {
		for (int i = 0; i < NUM_PIXELS; ++i)
			if (obj->StrictEquals(Nan::New(*pixelSymbols[i])))
				return static_cast<PixelMode>(i);
		return INVALID_PIXEL;
	}

	Local<Value> pixelMap(std::vector<PixelMode> modes) {
		Local<Array> r = Nan::New<Array>();
		for (size_t i = 0; i < modes.size(); ++i) {
			Nan::Set(r, i, pixelEnumToSymbol(modes[i]));
		}
		return r;
	}

	NativeImage jsImageToNativeImage(Local<Object>& img) {
		NativeImage r;

		Local<Value> blah = Nan::Undefined();
		r.width = Nan::Get(img, Nan::New(width_symbol)).FromMaybe(blah)->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);
		r.height = Nan::Get(img, Nan::New(height_symbol)).FromMaybe(blah)->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);
		r.stride = Nan::Get(img, Nan::New(stride_symbol)).FromMaybe(blah)->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);
		r.pixel = pixelSymbolToEnum(Nan::Get(img, Nan::New(pixel_symbol)).FromMaybe(blah));
		if (r.pixel < NUM_PIXELS && r.pixel > INVALID_PIXEL) {
			Local<Value> data = Nan::Get(img, Nan::New(data_symbol)).FromMaybe(blah);
			if (Buffer::HasInstance(data)) {
				MaybeLocal<Object> mdatabuf = data->ToObject(Nan::GetCurrentContext());
				if (mdatabuf.IsEmpty())
					return r;
				Local<Object> databuf = mdatabuf.ToLocalChecked();
				size_t len = Buffer::Length(databuf);
				size_t rw = pixelBytes(r.pixel) * r.width;
				if (len >= r.height * size_t(r.stride) - r.stride + rw && r.height != 0) {
					r.data = Buffer::Data(databuf);
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
		r.data = new char[r.height * r.stride];
		return r;
	}

	void freeNativeImage(NativeImage& image) {
		delete[] image.data;
		image.data = 0;
	}

	Local<Object> nativeImageToJsImage(NativeImage& cimage) {
		Local<Object> image = Nan::New<Object>();
		Nan::Set(image, Nan::New(width_symbol), Nan::New<Integer>(cimage.width));
		Nan::Set(image, Nan::New(height_symbol), Nan::New<Integer>(cimage.height));
		Nan::Set(image, Nan::New(stride_symbol), Nan::New<Integer>(cimage.stride));
		Nan::Set(image, Nan::New(pixel_symbol), pixelEnumToSymbol(cimage.pixel));

		Local<Value> pixelbuf;
		size_t datalen = cimage.height * cimage.stride;
		if (Nan::NewBuffer(datalen).ToLocal(&pixelbuf)) {
			memcpy(Buffer::Data(pixelbuf), cimage.data, datalen);
			Nan::Set(image, Nan::New(data_symbol), pixelbuf);
		}

		return image;
	}

	Local<Object> newJsImage(int w, int h, PixelMode pixel) {
		int stride = NativeImage::row_stride(w, pixel);
		Local<Object> image = Nan::New<Object>();
		Nan::Set(image, Nan::New(width_symbol), Nan::New<Integer>(w));
		Nan::Set(image, Nan::New(height_symbol), Nan::New<Integer>(h));
		Nan::Set(image, Nan::New(stride_symbol), Nan::New<Integer>(stride));
		Nan::Set(image, Nan::New(pixel_symbol), pixelEnumToSymbol(pixel));

		size_t datalen = h * stride;
		Local<Value> jspixel;
		if (Nan::NewBuffer(datalen).ToLocal(&jspixel))
			Nan::Set(image, Nan::New(data_symbol), jspixel);

		return image;
	}


	void makeCallback(Local<Function> cb, const char * error, Local<Value> v) {
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

	v8::Local<v8::Function> SetPichaMethod(v8::Local<v8::Object> o, const char * n, NAN_METHOD((*cb))) {
		v8::Local<v8::String> name = Nan::New(n).ToLocalChecked();
		Nan::MaybeLocal<v8::Function> blah = Nan::GetFunction(Nan::New<v8::FunctionTemplate>(cb));
		v8::Local<v8::Function> fn;
		if (blah.IsEmpty())
			return fn;
		fn = blah.ToLocalChecked();
		fn->SetName(name);
		Nan::Set(o, name, fn);
		return fn;
	}

#	define SSYMBOL(a) Nan::Persistent<String> a ## _symbol;
	STATIC_SYMBOLS
#	undef SSYMBOL


	void init(Local<Object> target) {
#		define SSYMBOL(a) a ## _symbol.Reset(Nan::New<String>(# a).ToLocalChecked());
		STATIC_SYMBOLS
#		undef SSYMBOL
		Nan::HandleScope scope;

		v8::Local<v8::Object> catalog = Nan::New<v8::Object>();
		v8::Local<v8::Value> encodes;
		v8::Local<v8::Function> fn;
		v8::Local<v8::Object> obj;

		Nan::Set(target, Nan::New("catalog").ToLocalChecked(), catalog);

		Nan::SetMethod(target, "colorConvert", colorConvert);
		Nan::SetMethod(target, "colorConvertSync", colorConvertSync);

		Nan::SetMethod(target, "resize", resize);
		Nan::SetMethod(target, "resizeSync", resizeSync);

#ifdef WITH_JPEG

		obj = Nan::New<v8::Object>();

		fn = SetPichaMethod(target, "statJpeg", statJpeg);
		Nan::Set(obj, Nan::New(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodeJpeg", decodeJpeg);
		Nan::Set(obj, Nan::New(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodeJpegSync", decodeJpegSync);
		Nan::Set(obj, Nan::New(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodeJpeg", encodeJpeg);
		Nan::Set(obj, Nan::New(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodeJpegSync", encodeJpegSync);
		Nan::Set(obj, Nan::New(encodeSync_symbol), fn);
		encodes = pixelMap(getJpegEncodes());
		Nan::Set(obj, Nan::New(encodes_symbol), encodes);


		Nan::Set(catalog, Nan::New("image/jpeg").ToLocalChecked(), obj);

#endif

#ifdef WITH_PNG

		obj = Nan::New<v8::Object>();

		fn = SetPichaMethod(target, "statPng", statPng);
		Nan::Set(obj, Nan::New(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodePng", decodePng);
		Nan::Set(obj, Nan::New(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodePngSync", decodePngSync);
		Nan::Set(obj, Nan::New(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodePng", encodePng);
		Nan::Set(obj, Nan::New(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodePngSync", encodePngSync);
		Nan::Set(obj, Nan::New(encodeSync_symbol), fn);
		encodes = pixelMap(getPngEncodes());
		Nan::Set(obj, Nan::New(encodes_symbol), encodes);

		Nan::Set(catalog, Nan::New("image/png").ToLocalChecked(), obj);

#endif

#ifdef WITH_TIFF

		obj = Nan::New<v8::Object>();

		fn = SetPichaMethod(target, "statTiff", statTiff);
		Nan::Set(obj, Nan::New(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodeTiff", decodeTiff);
		Nan::Set(obj, Nan::New(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodeTiffSync", decodeTiffSync);
		Nan::Set(obj, Nan::New(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodeTiff", encodeTiff);
		Nan::Set(obj, Nan::New(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodeTiffSync", encodeTiffSync);
		Nan::Set(obj, Nan::New(encodeSync_symbol), fn);
		encodes = pixelMap(getTiffEncodes());
		Nan::Set(obj, Nan::New(encodes_symbol), encodes);

		Nan::Set(catalog, Nan::New("image/tiff").ToLocalChecked(), obj);

#endif

#ifdef WITH_WEBP

		obj = Nan::New<v8::Object>();

		fn = SetPichaMethod(target, "statWebP", statWebP);
		Nan::Set(obj, Nan::New(stat_symbol), fn);
		fn = SetPichaMethod(target, "decodeWebP", decodeWebP);
		Nan::Set(obj, Nan::New(decode_symbol), fn);
		fn = SetPichaMethod(target, "decodeWebPSync", decodeWebPSync);
		Nan::Set(obj, Nan::New(decodeSync_symbol), fn);
		fn = SetPichaMethod(target, "encodeWebP", encodeWebP);
		Nan::Set(obj, Nan::New(encode_symbol), fn);
		fn = SetPichaMethod(target, "encodeWebPSync", encodeWebPSync);
		Nan::Set(obj, Nan::New(encodeSync_symbol), fn);
		encodes = pixelMap(getWebpEncodes());
		Nan::Set(obj, Nan::New(encodes_symbol), encodes);

		Nan::Set(catalog, Nan::New("image/webp").ToLocalChecked(), obj);

#endif
	}

}

NODE_MODULE(picha, picha::init)

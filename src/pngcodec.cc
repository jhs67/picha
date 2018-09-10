
#include <png.h>
#include <stdlib.h>
#include <node_buffer.h>

#include "pngcodec.h"
#include "writebuffer.h"

namespace picha {

	//----------------------------------------------------------------------------------------------------------------
	//--

	struct ReadBuffer {
		char * srcdata;
		size_t srclen;
	};

	void do_read_record(png_structp png_ptr, png_bytep data, png_size_t length) {
		ReadBuffer * ctx = (ReadBuffer*)png_get_io_ptr(png_ptr);
		if (length > ctx->srclen) {
			png_error(png_ptr, "read past end of buffer");
			return;
		}

		memcpy(data, ctx->srcdata, length);
		ctx->srcdata += length;
		ctx->srclen -= length;
	}

	struct PngReader {
		png_infop info_ptr;
		png_structp png_ptr;
		ReadBuffer readbuf;
		char * error;

		PngReader() : info_ptr(0), png_ptr(0), error(0) {}

		~PngReader() {
			if (png_ptr)
				png_destroy_read_struct(&png_ptr, &info_ptr, 0);
			if (error)
				free(error);
		}

		void close() {
			if (png_ptr)
				png_destroy_read_struct(&png_ptr, &info_ptr, 0);
			info_ptr = 0;
			png_ptr = 0;
		}

		void open(char * data, size_t len);

		void decode(const NativeImage &dst);

		int width() { return png_get_image_width(png_ptr, info_ptr); }

		int height() { return png_get_image_height(png_ptr, info_ptr); }

		PixelMode pixel() {
			png_byte color = png_get_color_type(png_ptr, info_ptr);
			if ((color & (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE)) && (color & PNG_COLOR_MASK_ALPHA))
				return RGBA_PIXEL;
			else if ((color & (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE)))
				return RGB_PIXEL;
			else if ((color & PNG_COLOR_MASK_ALPHA))
				return GREYA_PIXEL;
			else
				return GREY_PIXEL;
		}

		static void onError(png_structp png_ptr, png_const_charp error) {
			PngReader * self = (PngReader*)png_get_error_ptr(png_ptr);
			self->error = strdup(error);
			longjmp(png_jmpbuf(png_ptr), 1);
		}

		static void onWarn(png_structp png_ptr, png_const_charp error) {}
	};

	void PngReader::open(char * buf, size_t len) {
		readbuf.srclen = len;
		readbuf.srcdata = buf;

		if (png_sig_cmp((png_bytep)readbuf.srcdata, 0, readbuf.srclen < 8 ? readbuf.srclen : 8) != 0) {
			error = strdup("png signature mismatch");
			return;
		}

		png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
		if (png_ptr == 0) {
			error = strdup("failed to initialize png reader");
			return;
		}

		png_set_error_fn(png_ptr, this, PngReader::onError, PngReader::onWarn);
		if (setjmp(png_jmpbuf(png_ptr)))
			return;

		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == 0) {
			png_error(png_ptr, "failed to initialize png reader");
			return;
		}

		png_set_read_fn(png_ptr, &readbuf, do_read_record);

		png_read_info(png_ptr, info_ptr);
	}

	void PngReader::decode(const NativeImage &dst) {
		assert(dst.width == width());
		assert(dst.height == height());

		png_bytep * rows = 0;
		png_set_error_fn(png_ptr, this, PngReader::onError, PngReader::onWarn);
		if (setjmp(png_jmpbuf(png_ptr))) {
			delete[] rows;
			return;
		}

		if (dst.pixel == RGB_PIXEL) {
			png_set_gray_to_rgb(png_ptr);
			png_set_palette_to_rgb(png_ptr);
			png_set_expand(png_ptr);
			png_set_strip_alpha(png_ptr);
		}
		else if (dst.pixel == RGBA_PIXEL) {
			png_set_gray_to_rgb(png_ptr);
			png_set_palette_to_rgb(png_ptr);
			png_set_expand(png_ptr);
			png_set_tRNS_to_alpha(png_ptr);
			png_set_add_alpha(png_ptr, ~0, PNG_FILLER_AFTER);
		}
		else if (dst.pixel == GREY_PIXEL) {
			png_set_strip_alpha(png_ptr);
			png_set_rgb_to_gray(png_ptr, 1, -1, -1);
			png_set_expand_gray_1_2_4_to_8(png_ptr);
		}
		else if (dst.pixel == GREYA_PIXEL) {
			png_set_rgb_to_gray(png_ptr, 1, -1.0, -1.0);
			png_set_expand_gray_1_2_4_to_8(png_ptr);
			png_set_tRNS_to_alpha(png_ptr);
			png_set_add_alpha(png_ptr, ~0, PNG_FILLER_AFTER);
		}

		png_set_strip_16(png_ptr);
		png_read_update_info(png_ptr, info_ptr);

		rows = new png_bytep[dst.height];
		for (int y = 0; y < dst.height; ++y)
			rows[y] = (png_bytep)dst.row(y);
		png_read_image(png_ptr, rows);
		delete[] rows;
	}

	struct PngDecodeCtx {
		Nan::Persistent<Object> dstimage;
		Nan::Persistent<Object> buffer;
		Nan::Persistent<Function> cb;
		PngReader reader;

		NativeImage dst;
	};

	void UV_decodePNG(uv_work_t* work_req) {
		PngDecodeCtx *ctx = reinterpret_cast<PngDecodeCtx*>(work_req->data);
		ctx->reader.decode(ctx->dst);
		ctx->reader.close();
	}

	void V8_decodePNG(uv_work_t* work_req, int) {
		Nan::HandleScope scope;
		PngDecodeCtx *ctx = reinterpret_cast<PngDecodeCtx*>(work_req->data);
		makeCallback(Nan::New(ctx->cb), ctx->reader.error, Nan::New(ctx->dstimage));
		ctx->dstimage.Reset();
		ctx->buffer.Reset();
		ctx->cb.Reset();
		delete work_req;
		delete ctx;
	}

	NAN_METHOD(decodePng) {
		if (info.Length() != 3 || !Buffer::HasInstance(info[0]) || !info[1]->IsObject() || !info[2]->IsFunction()) {
			Nan::ThrowError("expected: decodePng(srcbuffer, opts, cb)");
			return;
		}
		Local<Object> srcbuf = info[0]->ToObject();
		Local<Object> opts = info[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(info[2]);

		Local<Value> jpixel = opts->Get(Nan::New(pixel_symbol));
		PixelMode pixel = pixelSymbolToEnum(jpixel);
		if (!jpixel->IsUndefined() && pixel == INVALID_PIXEL) {
			Nan::ThrowError("invalid pixel mode");
			return;
		}

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		PngDecodeCtx * ctx = new PngDecodeCtx;
		ctx->reader.open(srcdata, srclen);
		if (ctx->reader.error) {
			delete ctx;
			makeCallback(cb, ctx->reader.error, Nan::Undefined());
			return;
		}

		if (pixel == INVALID_PIXEL)
			pixel = ctx->reader.pixel();

		Local<Object> jsdst = newJsImage(ctx->reader.width(), ctx->reader.height(), pixel);
		ctx->dstimage.Reset(jsdst);
		ctx->buffer.Reset(srcbuf);
		ctx->cb.Reset(cb);
		ctx->dst = jsImageToNativeImage(jsdst);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_decodePNG, V8_decodePNG);
	}

	NAN_METHOD(decodePngSync) {
		if (info.Length() != 2 || !Buffer::HasInstance(info[0]) || !info[1]->IsObject()) {
			Nan::ThrowError("expected: decodePngSync(srcbuffer, opts)");
			return;
		}
		Local<Object> srcbuf = info[0]->ToObject();
		Local<Object> opts = info[1]->ToObject();

		Local<Value> jpixel = opts->Get(Nan::New(pixel_symbol));
		PixelMode pixel = pixelSymbolToEnum(jpixel);
		if (!jpixel->IsUndefined() && pixel == INVALID_PIXEL) {
			Nan::ThrowError("invalid pixel mode");
			return;
		}

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		PngReader reader;
		reader.open(srcdata, srclen);
		if (reader.error) {
			Nan::ThrowError(reader.error);
			return;
		}

		if (pixel == INVALID_PIXEL)
			pixel = reader.pixel();

		Local<Object> jsdst = newJsImage(reader.width(), reader.height(), pixel);

		reader.decode(jsImageToNativeImage(jsdst));

		if (reader.error) {
			Nan::ThrowError(reader.error);
			return;
		}

		info.GetReturnValue().Set(jsdst);
	}

	NAN_METHOD(statPng) {
		if (info.Length() != 1 || !Buffer::HasInstance(info[0])) {
			Nan::ThrowError("expected: statPng(buffer)");
			return;
		}
		Local<Object> srcbuf = info[0]->ToObject();

		PngReader reader;
		reader.open(Buffer::Data(srcbuf), Buffer::Length(srcbuf));
		if (reader.error)
			return;

		Local<Object> stat = Nan::New<Object>();
		stat->Set(Nan::New(width_symbol), Nan::New<Integer>(reader.width()));
		stat->Set(Nan::New(height_symbol), Nan::New<Integer>(reader.height()));
		stat->Set(Nan::New(pixel_symbol), pixelEnumToSymbol(reader.pixel()));
		info.GetReturnValue().Set(stat);
	}


	//----------------------------------------------------------------------------------------------------------------
	//--

	//----------------------------------------------------------------------------------------------------------------
	//--

	struct PngEncodeCtx {
		PngEncodeCtx() : dstdata_(0), error(0) {}

		Nan::Persistent<Value> buffer;
		Nan::Persistent<Function> cb;

		NativeImage image;

		char *dstdata_;
		size_t dstlen;

		char *error;

		void doWork();

		static void onError(png_structp png_ptr, png_const_charp error) {
			PngEncodeCtx * self = (PngEncodeCtx*)png_get_error_ptr(png_ptr);
			self->error = strdup(error);
			longjmp(png_jmpbuf(png_ptr), 1);
		}

		static void onWarn(png_structp png_ptr, png_const_charp error) {}
	};

	int pixelToPngMode(PixelMode p) {
		static const int pngmode[] = { PNG_COLOR_TYPE_RGB, PNG_COLOR_TYPE_RGB_ALPHA,
		PNG_COLOR_TYPE_GRAY, PNG_COLOR_TYPE_GRAY_ALPHA };
		assert(p >= 0 && p < int(sizeof(pngmode) / sizeof(pngmode[0])));
		return pngmode[p];
	}

	void pngWrite(png_structp png_ptr, png_bytep data, png_size_t length) {
		WriteBuffer * buf = (WriteBuffer*)png_get_io_ptr(png_ptr);
		buf->write(reinterpret_cast<char*>(data), length);
	}

	void pngFlush(png_structp png_ptr) {
	}

	void PngEncodeCtx::doWork() {
		png_infop info_ptr = 0;
		png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
		if (png_ptr == 0) {
			error = strdup("failed to initialize png writer");
			return;
		}

		WriteBuffer *writebuf = new WriteBuffer;
		png_set_error_fn(png_ptr, this, PngEncodeCtx::onError, PngEncodeCtx::onWarn);
		if (setjmp(png_jmpbuf(png_ptr))) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
			delete writebuf;
			return;
		}

		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == 0) {
			png_error(png_ptr, "failed to initialize png writer");
			return;
		}

		png_set_write_fn(png_ptr, writebuf, pngWrite, pngFlush);

		png_set_IHDR(png_ptr, info_ptr, image.width, image.height, 8, pixelToPngMode(image.pixel),
			PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_write_info(png_ptr, info_ptr);

		for (int y = 0; y < image.height; ++y)
			png_write_row(png_ptr, reinterpret_cast<png_bytep>(image.data + image.stride * y));

		png_write_end(png_ptr, info_ptr);

		dstlen = writebuf->totallen;
		dstdata_ = writebuf->consolidate_();
		delete writebuf;
	}

	void UV_encodePNG(uv_work_t* work_req) {
		PngEncodeCtx *ctx = reinterpret_cast<PngEncodeCtx*>(work_req->data);
		ctx->doWork();
	}

	void V8_encodePNG(uv_work_t* work_req, int) {
		Nan::HandleScope scope;
		PngEncodeCtx *ctx = reinterpret_cast<PngEncodeCtx*>(work_req->data);

		char * error = ctx->error;
		size_t dstlen = ctx->dstlen;
		char * dstdata_ = ctx->dstdata_;
		Local<Function> cb = Nan::New(ctx->cb);
		ctx->buffer.Reset();
		ctx->cb.Reset();
		delete work_req;
		delete ctx;

		Local<Value> e, r;
		if (error) {
			e = Nan::Error(error);
			r = Nan::Undefined();
		}
		else {
			Local<Object> b;
			e = Nan::Undefined();
			if (Nan::NewBuffer(dstdata_, dstlen).ToLocal(&b)) {
				r = b;
				dstdata_ = 0;
			}
			else {
				r = Nan::Undefined();
			}
		}

		free(error);
		if (dstdata_)
			free(dstdata_);

		Nan::TryCatch try_catch;

		Local<Value> argv[2] = { e, r };
		Nan::AsyncResource ass("picha");
		ass.runInAsyncScope(Nan::GetCurrentContext()->Global(), cb, 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);
	}

	NAN_METHOD(encodePng) {
		if (info.Length() != 3 || !info[0]->IsObject() || !info[2]->IsFunction()) {
			Nan::ThrowError("expected: encodePng(image, opts, cb)");
			return;
		}
		Local<Object> img = info[0]->ToObject();
		Local<Function> cb = Local<Function>::Cast(info[2]);

		PngEncodeCtx * ctx = new PngEncodeCtx;
		ctx->image = jsImageToNativeImage(img);
		if (!ctx->image.data) {
			delete ctx;
			Nan::ThrowError("invalid image");
			return;
		}

		ctx->buffer.Reset(img->Get(Nan::New(data_symbol)));
		ctx->cb.Reset(cb);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_encodePNG, V8_encodePNG);
	}

	NAN_METHOD(encodePngSync) {
		if (info.Length() != 2 || !info[0]->IsObject()) {
			Nan::ThrowError("expected: encodePngSync(image, opts)");
			return;
		}
		Local<Object> img = info[0]->ToObject();

		PngEncodeCtx ctx;
		ctx.image = jsImageToNativeImage(img);
		if (!ctx.image.data) {
			Nan::ThrowError("invalid image");
			return;
		}

		ctx.doWork();

		Local<Value> r = Nan::Undefined();
		if (ctx.error) {
			Nan::ThrowError(ctx.error);
			free(ctx.error);
		}
		else {
			Local<Object> o;
			if (Nan::NewBuffer(ctx.dstdata_, ctx.dstlen).ToLocal(&o)) {
				ctx.dstdata_ = 0;
				r = o;
			}
			else {
				r = Nan::Undefined();
			}
		}

		if (ctx.dstdata_)
			free(ctx.dstdata_);
		info.GetReturnValue().Set(r);
	}

	std::vector<PixelMode> getPngEncodes() {
		return std::vector<PixelMode>({ RGB_PIXEL, RGBA_PIXEL, GREY_PIXEL, GREYA_PIXEL });
	}

}

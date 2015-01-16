
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
		Persistent<Object> dstimage;
		Persistent<Object> buffer;
		Persistent<Function> cb;
		PngReader reader;

		NativeImage dst;
	};

	void UV_decodePNG(uv_work_t* work_req) {
		PngDecodeCtx *ctx = reinterpret_cast<PngDecodeCtx*>(work_req->data);
		ctx->reader.decode(ctx->dst);
		ctx->reader.close();
	}

	void V8_decodePNG(uv_work_t* work_req, int) {
		NanScope();
		PngDecodeCtx *ctx = reinterpret_cast<PngDecodeCtx*>(work_req->data);
		makeCallback(NanNew(ctx->cb), ctx->reader.error, NanNew(ctx->dstimage));
		NanDisposePersistent(ctx->dstimage);
		NanDisposePersistent(ctx->buffer);
		NanDisposePersistent(ctx->cb);
		delete work_req;
		delete ctx;
	}

	NAN_METHOD(decodePng) {
		NanScope();

		if (args.Length() != 3 || !Buffer::HasInstance(args[0]) || !args[1]->IsObject() || !args[2]->IsFunction()) {
			NanThrowError("expected: decodePng(srcbuffer, opts, cb)");
			NanReturnUndefined();
		}
		Local<Object> srcbuf = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		Local<Value> jpixel = opts->Get(NanNew(pixel_symbol));
		PixelMode pixel = pixelSymbolToEnum(jpixel);
		if (!jpixel->IsUndefined() && pixel == INVALID_PIXEL) {
			NanThrowError("invalid pixel mode");
			NanReturnUndefined();
		}

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		PngDecodeCtx * ctx = new PngDecodeCtx;
		ctx->reader.open(srcdata, srclen);
		if (ctx->reader.error) {
			delete ctx;
			makeCallback(cb, ctx->reader.error, NanUndefined());
			NanReturnUndefined();
		}

		if (pixel == INVALID_PIXEL)
			pixel = ctx->reader.pixel();

		Local<Object> jsdst = newJsImage(ctx->reader.width(), ctx->reader.height(), pixel);
		NanAssignPersistent(ctx->dstimage, jsdst);
		NanAssignPersistent(ctx->buffer, srcbuf);
		NanAssignPersistent(ctx->cb, cb);
		ctx->dst = jsImageToNativeImage(jsdst);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_decodePNG, V8_decodePNG);

		NanReturnUndefined();
	}

	NAN_METHOD(decodePngSync) {
		NanScope();

		if (args.Length() != 2 || !Buffer::HasInstance(args[0]) || !args[1]->IsObject()) {
			NanThrowError("expected: decodePngSync(srcbuffer, opts)");
			NanReturnUndefined();
		}
		Local<Object> srcbuf = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();

		Local<Value> jpixel = opts->Get(NanNew(pixel_symbol));
		PixelMode pixel = pixelSymbolToEnum(jpixel);
		if (!jpixel->IsUndefined() && pixel == INVALID_PIXEL) {
			NanThrowError("invalid pixel mode");
			NanReturnUndefined();
		}

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		PngReader reader;
		reader.open(srcdata, srclen);
		if (reader.error) {
			NanThrowError(reader.error);
			NanReturnUndefined();
		}

		if (pixel == INVALID_PIXEL)
			pixel = reader.pixel();

		Local<Object> jsdst = newJsImage(reader.width(), reader.height(), pixel);

		reader.decode(jsImageToNativeImage(jsdst));

		if (reader.error) {
			NanThrowError(reader.error);
			NanReturnUndefined();
		}

		NanReturnValue(jsdst);
	}

	NAN_METHOD(statPng) {
		NanScope();

		if (args.Length() != 1 || !Buffer::HasInstance(args[0])) {
			NanThrowError("expected: statPng(buffer)");
			NanReturnUndefined();
		}
		Local<Object> srcbuf = args[0]->ToObject();

		PngReader reader;
		reader.open(Buffer::Data(srcbuf), Buffer::Length(srcbuf));
		if (reader.error)
			NanReturnUndefined();

		Local<Object> stat = NanNew<Object>();
		stat->Set(NanNew(width_symbol), NanNew<Integer>(reader.width()));
		stat->Set(NanNew(height_symbol), NanNew<Integer>(reader.height()));
		stat->Set(NanNew(pixel_symbol), pixelEnumToSymbol(reader.pixel()));
		NanReturnValue(stat);
	}


	//----------------------------------------------------------------------------------------------------------------
	//--

	//----------------------------------------------------------------------------------------------------------------
	//--

	struct PngEncodeCtx {
		PngEncodeCtx() : dstdata(0), error(0) {}

		Persistent<Value> buffer;
		Persistent<Function> cb;

		NativeImage image;

		char *dstdata;
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
		dstdata = writebuf->consolidate();
		delete writebuf;
	}

	void UV_encodePNG(uv_work_t* work_req) {
		PngEncodeCtx *ctx = reinterpret_cast<PngEncodeCtx*>(work_req->data);
		ctx->doWork();
	}

	void V8_encodePNG(uv_work_t* work_req, int) {
		NanScope();
		PngEncodeCtx *ctx = reinterpret_cast<PngEncodeCtx*>(work_req->data);

		char * error = ctx->error;
		size_t dstlen = ctx->dstlen;
		char * dstdata = ctx->dstdata;
		Local<Function> cb = NanNew(ctx->cb);
		NanDisposePersistent(ctx->buffer);
		NanDisposePersistent(ctx->cb);
		delete work_req;
		delete ctx;

		Local<Value> e, r;
		if (error) {
			e = Exception::Error(NanNew<String>(error));
			r = NanUndefined();
		}
		else {
			e = NanUndefined();
			r = NanNewBufferHandle(dstdata, dstlen);
		}

		free(error);
		delete[] dstdata;

		TryCatch try_catch;

		Handle<Value> argv[2] = { e, r };
		NanMakeCallback(NanGetCurrentContext()->Global(), cb, 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);

		return;
	}

	NAN_METHOD(encodePng) {
		NanScope();

		if (args.Length() != 3 || !args[0]->IsObject() || !args[2]->IsFunction()) {
			NanThrowError("expected: encodePng(image, opts, cb)");
			NanReturnUndefined();
		}
		Local<Object> img = args[0]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		PngEncodeCtx * ctx = new PngEncodeCtx;
		ctx->image = jsImageToNativeImage(img);
		if (!ctx->image.data) {
			delete ctx;
			NanThrowError("invalid image");
			NanReturnUndefined();
		}

		NanAssignPersistent(ctx->buffer, img->Get(NanNew(data_symbol)));
		NanAssignPersistent(ctx->cb, cb);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_encodePNG, V8_encodePNG);

		NanReturnUndefined();
	}

	NAN_METHOD(encodePngSync) {
		NanScope();

		if (args.Length() != 2 || !args[0]->IsObject()) {
			NanThrowError("expected: encodePngSync(image, opts)");
			NanReturnUndefined();
		}
		Local<Object> img = args[0]->ToObject();

		PngEncodeCtx ctx;
		ctx.image = jsImageToNativeImage(img);
		if (!ctx.image.data) {
			NanThrowError("invalid image");
			NanReturnUndefined();
		}

		ctx.doWork();

		Local<Value> r = NanUndefined();
		if (ctx.error) {
			NanThrowError(ctx.error);
			free(ctx.error);
		}
		else {
			r = NanNewBufferHandle(ctx.dstdata, ctx.dstlen);
		}

		delete ctx.dstdata;
		NanReturnValue(r);
	}

}

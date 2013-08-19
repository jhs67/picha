
#include <png.h>
#include <stdlib.h>
#include <node_buffer.h>

#include "pngcodec.h"

namespace picha {


	//----------------------------------------------------------------------------------------------------------------
	//--

	struct WriteBuffer {

		WriteBuffer() : blocks(0), totallen(0), space(0) {}

		~WriteBuffer() { delete blocks; }

		struct WriteBlock {
			int length;
			char * data;
			WriteBlock * next;
			WriteBlock() : data(0), next(0) {}
			~WriteBlock() { delete data; delete next; }
		};

		void write(char * data, size_t length) {
			size_t l = length < space ? length : space;
			if (l > 0) {
				memcpy(blocks->data + blocks->length - space, data, l);
				totallen += l;
				length -= l;
				space -= l;
				data += l;
			}

			if (length > 0) {
				const size_t min_block = 64 * 1024;
				WriteBlock * n = new WriteBlock;
				n->length = length > min_block ? length : min_block;
				n->data = new char[n->length];
				n->next = blocks;
				blocks = n;

				memcpy(blocks->data, data, length);
				space = blocks->length - length;
				totallen += length;
			}
		}

		char * consolidate() {
			char * r = 0;
			if (blocks != 0) {
				if (blocks->next == 0) {
					r = blocks->data;
					blocks->data = 0;
				}
				else {
					r = new char[totallen];
					for (WriteBlock * b = blocks; b != 0; b = b->next) {
						size_t l = b->length - space;
						memcpy(r + totallen - l, b->data, l);
						totallen -= l;
						space = 0;
					}
				}
			}
			delete blocks;
			totallen = 0;
			blocks = 0;
			space = 0;
			return r;
		}

		WriteBlock * blocks;
		size_t totallen;
		size_t space;
	};


	//----------------------------------------------------------------------------------------------------------------
	//--

	struct PngCtx {
		char *error;
		PngCtx() : error(0) {}

		static void onError(png_structp png_ptr, png_const_charp error) {
			PngCtx * self = (PngCtx*)png_get_error_ptr(png_ptr);
			self->error = strdup(error);
			longjmp(png_jmpbuf(png_ptr), 1);
		}

		static void onWarn(png_structp png_ptr, png_const_charp error) {}
	};


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

	struct DecodeCtx : PngCtx {
		Persistent<Object> buffer;
		Persistent<Function> cb;

		ReadBuffer readbuf;
		PixelMode pixel;

		NativeImage image;

		void doWork();
	};

	void DecodeCtx::doWork() {
		if (png_sig_cmp((png_bytep)readbuf.srcdata, 0, readbuf.srclen < 8 ? readbuf.srclen : 8) != 0) {
			error = strdup("png signature mismatch");
			return;
		}

		png_infop info_ptr = 0;
		png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
		if (png_ptr == 0) {
			error = strdup("failed to initialize png reader");
			return;
		}

		png_bytep * rows = 0;
		png_set_error_fn(png_ptr, this, PngCtx::onError, PngCtx::onWarn);
		if (setjmp(png_jmpbuf(png_ptr))) {
			png_destroy_read_struct(&png_ptr, &info_ptr, 0);
			delete[] rows;
			return;
		}

		info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == 0) {
			png_error(png_ptr, "failed to initialize png reader");
			return;
		}

		png_set_read_fn(png_ptr, &readbuf, do_read_record);
		png_read_info(png_ptr, info_ptr);

		if (pixel < 0 || pixel >= NUM_PIXELS) {
			png_byte color = png_get_color_type(png_ptr, info_ptr);
			if ((color & (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE)) && (color & PNG_COLOR_MASK_ALPHA))
				pixel = RGBA_PIXEL;
			else if ((color & (PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE)))
				pixel = RGB_PIXEL;
			else if ((color & PNG_COLOR_MASK_ALPHA))
				pixel = GREYA_PIXEL;
			else
				pixel = GREY_PIXEL;
		}

		if (pixel == RGB_PIXEL) {
			png_set_gray_to_rgb(png_ptr);
			png_set_palette_to_rgb(png_ptr);
			png_set_expand(png_ptr);
			png_set_strip_alpha(png_ptr);
		}
		else if (pixel == RGBA_PIXEL) {
			png_set_gray_to_rgb(png_ptr);
			png_set_palette_to_rgb(png_ptr);
			png_set_expand(png_ptr);
			png_set_tRNS_to_alpha(png_ptr);
			png_set_add_alpha(png_ptr, ~0, PNG_FILLER_AFTER);
		}
		else if (pixel == GREY_PIXEL) {
			png_set_strip_alpha(png_ptr);
			png_set_rgb_to_gray(png_ptr, 1, -1, -1);
			png_set_expand_gray_1_2_4_to_8(png_ptr);
		}
		else if (pixel == GREYA_PIXEL) {
			png_set_rgb_to_gray(png_ptr, 1, -1.0, -1.0);
			png_set_expand_gray_1_2_4_to_8(png_ptr);
			png_set_tRNS_to_alpha(png_ptr);
			png_set_add_alpha(png_ptr, ~0, PNG_FILLER_AFTER);
		}

		png_set_strip_16(png_ptr);
		png_read_update_info(png_ptr, info_ptr);

		png_uint_32 width = png_get_image_width(png_ptr, info_ptr);
		png_uint_32 height = png_get_image_height(png_ptr, info_ptr);

		image.alloc(width, height, pixel);
		rows = new png_bytep[height];
		for (png_uint_32 y = 0; y < height; ++y)
			rows[y] = (png_bytep)image.row(y);
		png_read_image(png_ptr, rows);
		delete[] rows;
	}

	void UV_decodePNG(uv_work_t* work_req) {
		DecodeCtx *ctx = reinterpret_cast<DecodeCtx*>(work_req->data);
		ctx->doWork();
	}

	void V8_decodePNG(uv_work_t* work_req, int) {
		HandleScope scope;
		DecodeCtx *ctx = reinterpret_cast<DecodeCtx*>(work_req->data);

		char * error = ctx->error;
		NativeImage image = ctx->image;
		Local<Function> cb = Local<Function>::New(ctx->cb);
		delete ctx;

		Local<Value> e, r;
		if (error) {
			e = String::New(error);
			r = *Undefined();
		}
		else {
			e = *Undefined();
			r = nativeImageToJsImage(image);
		}

		free(error);
		image.free();

		TryCatch try_catch;

		Handle<Value> argv[2] = { e, r };
		cb->Call(Context::GetCurrent()->Global(), 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);

		return;
	}

	Handle<Value> decodePng(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 3 || !Buffer::HasInstance(args[0]) || !args[1]->IsObject() || !args[2]->IsFunction()) {
			ThrowException(Exception::Error(String::New("expected: decodePng(srcbuffer, opts, cb)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		PixelMode pixel = INVALID_PIXEL;

		Local<Value> v = opts->Get(String::NewSymbol("pixel"));
		if (!v->IsUndefined()) {
			uint32_t p = v->Uint32Value();
			if (p >= NUM_PIXELS) {
				ThrowException(Exception::Error(String::New("opts.pixel out of range")));
				return scope.Close(Undefined());
			}
			pixel = static_cast<PixelMode>(p);
		}

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		DecodeCtx * ctx = new DecodeCtx;
		ctx->buffer = Persistent<Object>::New(srcbuf);
		ctx->cb = Persistent<Function>::New(cb);
		ctx->readbuf.srcdata = srcdata;
		ctx->readbuf.srclen = srclen;
		ctx->pixel = pixel;

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_decodePNG, V8_decodePNG);

		return Undefined();
	}

	Handle<Value> decodePngSync(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 2 || !Buffer::HasInstance(args[0]) || !args[1]->IsObject()) {
			ThrowException(Exception::Error(String::New("expected: decodePngSync(srcbuffer, opts)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();

		PixelMode pixel = INVALID_PIXEL;
		Local<Value> v = opts->Get(String::NewSymbol("pixel"));
		if (!v->IsUndefined()) {
			uint32_t p = v->Uint32Value();
			if (p >= NUM_PIXELS) {
				ThrowException(Exception::Error(String::New("opts.pixel out of range")));
				return scope.Close(Undefined());
			}
			pixel = static_cast<PixelMode>(p);
		}

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		DecodeCtx ctx;
		ctx.readbuf.srcdata = srcdata;
		ctx.readbuf.srclen = srclen;
		ctx.pixel = pixel;

		ctx.doWork();

		Local<Value> r = *Undefined();
		if (ctx.error) {
			ThrowException(Exception::Error(String::New(ctx.error)));
			free(ctx.error);
		}
		else {
			r = nativeImageToJsImage(ctx.image);
		}

		ctx.image.free();
		return scope.Close(r);
	}


	//----------------------------------------------------------------------------------------------------------------
	//--

	struct EncodeCtx : PngCtx {
		EncodeCtx() : dstdata(0) {}

		Persistent<Value> buffer;
		Persistent<Function> cb;

		NativeImage image;

		char *dstdata;
		size_t dstlen;

		void doWork();
	};

	int pixelToPngMode(PixelMode p) {
		static const int pngmode[] = { PNG_COLOR_TYPE_RGB, PNG_COLOR_TYPE_RGB_ALPHA,
		PNG_COLOR_TYPE_GRAY, PNG_COLOR_TYPE_GRAY_ALPHA };
		assert(p >= 0 && p < sizeof(pngmode) / sizeof(pngmode[0]));
		return pngmode[p];
	}

	void pngWrite(png_structp png_ptr, png_bytep data, png_size_t length) {
		WriteBuffer * buf = (WriteBuffer*)png_get_io_ptr(png_ptr);
		buf->write(reinterpret_cast<char*>(data), length);
	}

	void pngFlush(png_structp png_ptr) {
	}

	void EncodeCtx::doWork() {
		png_infop info_ptr = 0;
		png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
		if (png_ptr == 0) {
			error = strdup("failed to initialize png writer");
			return;
		}

		WriteBuffer *writebuf = new WriteBuffer;
		png_set_error_fn(png_ptr, this, PngCtx::onError, PngCtx::onWarn);
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

		for (png_uint_32 y = 0; y < image.height; ++y)
			png_write_row(png_ptr, reinterpret_cast<png_bytep>(image.data + image.stride * y));

		png_write_end(png_ptr, info_ptr);

		dstlen = writebuf->totallen;
		dstdata = writebuf->consolidate();
		delete writebuf;
	}

	void UV_encodePNG(uv_work_t* work_req) {
		EncodeCtx *ctx = reinterpret_cast<EncodeCtx*>(work_req->data);
		ctx->doWork();
	}

	void V8_encodePNG(uv_work_t* work_req, int) {
		HandleScope scope;
		EncodeCtx *ctx = reinterpret_cast<EncodeCtx*>(work_req->data);

		char * error = ctx->error;
		size_t dstlen = ctx->dstlen;
		char * dstdata = ctx->dstdata;
		Local<Function> cb = Local<Function>::New(ctx->cb);
		delete ctx;

		Local<Value> e, r;
		if (error) {
			e = String::New(error);
			r = *Undefined();
		}
		else {
			e = *Undefined();
			Buffer *buf = Buffer::New(dstdata, dstlen);
			r = Local<Value>::New(buf->handle_);
		}

		free(error);
		delete[] dstdata;

		TryCatch try_catch;

		Handle<Value> argv[2] = { e, r };
		cb->Call(Context::GetCurrent()->Global(), 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);

		return;
	}

	Handle<Value> encodePng(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsFunction()) {
			ThrowException(Exception::Error(String::New("expected: encodePng(image, cb)")));
			return scope.Close(Undefined());
		}
		Local<Object> img = args[0]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[1]);

		EncodeCtx * ctx = new EncodeCtx;
		ctx->image = jsImageToNativeImage(img);
		if (!ctx->image.data) {
			delete ctx;
			ThrowException(Exception::Error(String::New("invalid image")));
			return scope.Close(Undefined());
		}

		ctx->buffer = Persistent<Value>::New(img->Get(String::NewSymbol("data")));
		ctx->cb = Persistent<Function>::New(cb);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_encodePNG, V8_encodePNG);

		return scope.Close(Undefined());
	}

	Handle<Value> encodePngSync(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 1 || !args[0]->IsObject()) {
			ThrowException(Exception::Error(String::New("expected: encodePngSync(image)")));
			return scope.Close(Undefined());
		}
		Local<Object> img = args[0]->ToObject();

		EncodeCtx ctx;
		ctx.image = jsImageToNativeImage(img);
		if (!ctx.image.data) {
			ThrowException(Exception::Error(String::New("invalid image")));
			return scope.Close(Undefined());
		}

		ctx.doWork();

		Local<Value> r = *Undefined();
		if (ctx.error) {
			ThrowException(Exception::Error(String::New(ctx.error)));
			free(ctx.error);
		}
		else {
			Buffer *buf = Buffer::New(ctx.dstdata, ctx.dstlen);
			r = Local<Value>::New(buf->handle_);
		}

		delete ctx.dstdata;
		return scope.Close(r);
	}

}

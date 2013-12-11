
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <jpeglib.h>
#include <node.h>
#include <node_buffer.h>

#include "jpegcodec.h"

namespace picha {

	struct JpegReader {
		bool isopen;
		char * error;
		jmp_buf jmpbuf;
		jpeg_error_mgr jerr;
		jpeg_decompress_struct cinfo;

		JpegReader() : isopen(false), error(0) {}
		~JpegReader() { close(); if (error) free(error); }

		void close() {
			if (isopen) jpeg_destroy_decompress(&cinfo);
			isopen = false;
		}

		void open(char * buf, size_t len) {
			cinfo.err = jpeg_std_error(&jerr);
			cinfo.err->error_exit = &JpegReader::onError;
			cinfo.client_data = this;

			jpeg_create_decompress(&cinfo);
			jpeg_mem_src(&cinfo, reinterpret_cast<unsigned char*>(buf), len);
			isopen = true;

			if (setjmp(jmpbuf))
				return;

			jpeg_read_header(&cinfo, true);
		}

		void decode(const NativeImage &dst) {
			assert(dst.pixel == RGB_PIXEL);

			jpeg_start_decompress(&cinfo);
			for(int y = 0; y < dst.height; ++y) {
				JSAMPLE* p = (JSAMPLE*)(dst.row(y));
				jpeg_read_scanlines(&cinfo, &p, 1);
			}
			jpeg_finish_decompress(&cinfo);
		}

		int width() { return cinfo.image_width; }

		int height() { return cinfo.image_height; }

		static void onError(j_common_ptr cinfo) {
			char errbuf[JMSG_LENGTH_MAX];
			JpegReader* self = (JpegReader*)cinfo->client_data;
			cinfo->err->format_message(cinfo, errbuf);
			self->error = strdup(errbuf);
			longjmp(self->jmpbuf, 1);
		}
	};

	struct JpegDecodeCtx {
		Persistent<Value> dstimage;
		Persistent<Value> buffer;
		Persistent<Function> cb;

		JpegReader reader;
		NativeImage dst;
	};

	void UV_decodeJpeg(uv_work_t* work_req) {
		JpegDecodeCtx *ctx = reinterpret_cast<JpegDecodeCtx*>(work_req->data);
		ctx->reader.decode(ctx->dst);
	}

	void V8_decodeJpeg(uv_work_t* work_req, int) {
		HandleScope scope;
		JpegDecodeCtx *ctx = reinterpret_cast<JpegDecodeCtx*>(work_req->data);
		makeCallback(ctx->cb, ctx->reader.error, ctx->dstimage);
		delete ctx;
	}

	Handle<Value> decodeJpeg(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 3 || !Buffer::HasInstance(args[0]) || !args[2]->IsFunction()) {
			ThrowException(Exception::Error(String::New("expected: decodeJpeg(srcbuffer, opts, cb)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		JpegDecodeCtx * ctx = new JpegDecodeCtx;
		ctx->reader.open(srcdata, srclen);
		if (ctx->reader.error) {
			delete ctx;
			makeCallback(cb, ctx->reader.error, Undefined());
			return Undefined();
		}

		Local<Object> jsdst = newJsImage(ctx->reader.width(), ctx->reader.height(), RGB_PIXEL);
		ctx->dstimage = Persistent<Value>::New(jsdst);
		ctx->buffer = Persistent<Value>::New(srcbuf);
		ctx->cb = Persistent<Function>::New(cb);
		ctx->dst = jsImageToNativeImage(jsdst);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_decodeJpeg, V8_decodeJpeg);

		return Undefined();
	}

	Handle<Value> decodeJpegSync(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 2 || !Buffer::HasInstance(args[0])) {
			ThrowException(Exception::Error(String::New("expected: decodeJpegSync(srcbuffer, opts)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		JpegReader reader;
		reader.open(srcdata, srclen);
		if (reader.error) {
			ThrowException(Exception::Error(String::New(reader.error)));
			return Undefined();
		}

		Local<Object> jsdst = newJsImage(reader.width(), reader.height(), RGB_PIXEL);

		reader.decode(jsImageToNativeImage(jsdst));

		if (reader.error) {
			ThrowException(Exception::Error(String::New(reader.error)));
			return Undefined();
		}

		return scope.Close(jsdst);
	}

	Handle<Value> statJpeg(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 1 || !Buffer::HasInstance(args[0])) {
			ThrowException(Exception::Error(String::New("expected: statJpeg(srcbuffer)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();

		JpegReader reader;
		reader.open(Buffer::Data(srcbuf), Buffer::Length(srcbuf));
		if (reader.error)
			return scope.Close(Undefined());

		Local<Object> stat = Object::New();
		stat->Set(width_symbol, Integer::New(reader.width()));
		stat->Set(height_symbol, Integer::New(reader.height()));
		stat->Set(pixel_symbol, pixelEnumToSymbol(RGB_PIXEL));
		return scope.Close(stat);
	}


	//------------------------------------------------------------------------------------------------------------
	//--

	struct JpegEncodeCtx {
		JpegEncodeCtx() : error(0), dstdata(0) {}

		char *error;
		jmp_buf jmpbuf;

		Persistent<Value> buffer;
		Persistent<Function> cb;

		NativeImage image;

		PixelType *dstdata;
		size_t dstlen;
		float quality;

		void doWork();

		static void onError(j_common_ptr cinfo) {
			char errbuf[JMSG_LENGTH_MAX];
			JpegEncodeCtx* self = (JpegEncodeCtx*)cinfo->client_data;
			cinfo->err->format_message(cinfo, errbuf);
			self->error = strdup(errbuf);
			longjmp(self->jmpbuf, 1);
		}
	};

	void JpegEncodeCtx::doWork() {
		jpeg_compress_struct cinfo;
		jpeg_error_mgr jerr;

		cinfo.err = jpeg_std_error(&jerr);
		cinfo.err->error_exit = &JpegEncodeCtx::onError;
        jpeg_create_compress(&cinfo);

		unsigned long outsize = 0;
		unsigned char *outbuffer = 0;
        jpeg_mem_dest(&cinfo, &outbuffer, &outsize);

        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        cinfo.image_width = (JDIMENSION)image.width;
        cinfo.image_height = (JDIMENSION)image.height;

        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, quality, true);
        jpeg_start_compress(&cinfo, true);
        fflush(stdout);

		for (int y = 0; y < image.height; ++y) {
			PixelType * p = image.row(y);
			jpeg_write_scanlines(&cinfo, (JSAMPARRAY)(&p), 1);
		}

        jpeg_finish_compress(&cinfo);
		jpeg_destroy_compress(&cinfo);

		dstdata = reinterpret_cast<PixelType*>(outbuffer);
		dstlen = outsize;
	}

	void UV_encodeJpeg(uv_work_t* work_req) {
		JpegEncodeCtx *ctx = reinterpret_cast<JpegEncodeCtx*>(work_req->data);
		ctx->doWork();
	}

	void V8_encodeJpeg(uv_work_t* work_req, int) {
		HandleScope scope;
		JpegEncodeCtx *ctx = reinterpret_cast<JpegEncodeCtx*>(work_req->data);

		char * error = ctx->error;
		size_t dstlen = ctx->dstlen;
		PixelType * dstdata = ctx->dstdata;
		Local<Function> cb = Local<Function>::New(ctx->cb);
		delete ctx;

		Local<Value> e, r;
		if (error) {
			e = Exception::Error(String::New(error));
			r = *Undefined();
		}
		else {
			e = *Undefined();
			Buffer *buf = Buffer::New(reinterpret_cast<const char*>(dstdata), dstlen);
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

	Handle<Value> encodeJpeg(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 3 || !args[0]->IsObject() || !args[1]->IsObject() || !args[2]->IsFunction()) {
			ThrowException(Exception::Error(String::New("expected: encodeJpeg(image, opts, cb)")));
			return scope.Close(Undefined());
		}
		Local<Object> img = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		Local<Value> v = opts->Get(quality_symbol);
		double quality = v->NumberValue();
		if (quality != quality)
			quality = 85;
		else if (quality < 0)
			quality = 0;
		else if (quality > 100)
			quality = 100;

		JpegEncodeCtx * ctx = new JpegEncodeCtx;
		ctx->image = jsImageToNativeImage(img);
		if (!ctx->image.data) {
			delete ctx;
			ThrowException(Exception::Error(String::New("invalid image")));
			return scope.Close(Undefined());
		}

		ctx->quality = quality;
		ctx->buffer = Persistent<Value>::New(img->Get(data_symbol));
		ctx->cb = Persistent<Function>::New(cb);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_encodeJpeg, V8_encodeJpeg);

		return scope.Close(Undefined());
	}

	Handle<Value> encodeJpegSync(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 2 || !args[0]->IsObject() || !args[1]->IsObject()) {
			ThrowException(Exception::Error(String::New("expected: encodeJpegSync(image, opts)")));
			return scope.Close(Undefined());
		}
		Local<Object> img = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();

		Local<Value> v = opts->Get(quality_symbol);
		double quality = v->NumberValue();
		if (quality != quality)
			quality = 85;
		else if (quality < 0)
			quality = 0;
		else if (quality > 100)
			quality = 100;

		JpegEncodeCtx ctx;
		ctx.image = jsImageToNativeImage(img);
		if (!ctx.image.data) {
			ThrowException(Exception::Error(String::New("invalid image")));
			return scope.Close(Undefined());
		}

		ctx.quality = quality;
		ctx.doWork();

		Local<Value> r = *Undefined();
		if (ctx.error) {
			ThrowException(Exception::Error(String::New(ctx.error)));
			free(ctx.error);
		}
		else {
			Buffer *buf = Buffer::New(reinterpret_cast<const char*>(ctx.dstdata), ctx.dstlen);
			r = Local<Value>::New(buf->handle_);
		}

		delete ctx.dstdata;
		return scope.Close(r);
	}

}

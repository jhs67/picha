
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <node.h>
#include <node_buffer.h>

#include "jpegcodec.h"

#include <jpeglib.h>
#include <jerror.h>

namespace picha {

	namespace {
		void initSource (j_decompress_ptr cinfo) {}

		boolean fillInputBuffer(j_decompress_ptr cinfo) {
			ERREXIT(cinfo, JERR_INPUT_EMPTY);
			return TRUE;
		}

		void skipInputData(j_decompress_ptr cinfo, long num_bytes) {
			struct jpeg_source_mgr* src = (struct jpeg_source_mgr*) cinfo->src;
			if (num_bytes > 0) {
				src->next_input_byte += (size_t)num_bytes;
				src->bytes_in_buffer -= (size_t)num_bytes;
			}
		}

		void termSource (j_decompress_ptr cinfo) {
		}
	}

	void cmyk_to_rgb(unsigned char *cmyk, unsigned char *rgb, int width) {
		for (int i = 0; i < width; ++i, cmyk += 4, rgb += 3) {
			rgb[0] = int(cmyk[0]) * cmyk[3] / 255;
			rgb[1] = int(cmyk[1]) * cmyk[3] / 255;
			rgb[2] = int(cmyk[2]) * cmyk[3] / 255;
		}
	}

	struct JpegReader {
		bool isopen;
		char * error;
		jmp_buf jmpbuf;
		jpeg_error_mgr jerr;
		jpeg_source_mgr jsrc;
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
			assert(cinfo.src == 0);
			cinfo.src = &jsrc;
			jsrc.init_source = initSource;
			jsrc.fill_input_buffer = fillInputBuffer;
			jsrc.skip_input_data = skipInputData;
			jsrc.resync_to_restart = jpeg_resync_to_restart;
			jsrc.term_source = termSource;
			jsrc.bytes_in_buffer = len;
			jsrc.next_input_byte = (JOCTET*)buf;

			isopen = true;

			if (setjmp(jmpbuf))
				return;

			jpeg_read_header(&cinfo, true);
		}

		void decode(const NativeImage &dst) {
			if (setjmp(jmpbuf))
				return;

			jpeg_start_decompress(&cinfo);

			if (cinfo.out_color_space == JCS_CMYK) {
				unsigned char *buf = new unsigned char[dst.width * 4];
				for(int y = 0; y < dst.height; ++y) {
					JSAMPLE* p = (JSAMPLE*)(&buf[0]);
					int r = jpeg_read_scanlines(&cinfo, &p, 1);
					assert(r == 1);
					cmyk_to_rgb(&buf[0], reinterpret_cast<uint8_t*>(dst.row(y)), dst.width);
				}
				delete[] buf;
			}
			else {
				for(int y = 0; y < dst.height; ++y) {
					JSAMPLE* p = (JSAMPLE*)(dst.row(y));
					int r = jpeg_read_scanlines(&cinfo, &p, 1);
					assert(r == 1);
				}
			}

			jpeg_finish_decompress(&cinfo);
		}

		PixelMode getPixel() {
			if (cinfo.out_color_space == JCS_RGB)
				return RGB_PIXEL;
			if (cinfo.out_color_space == JCS_GRAYSCALE)
				return GREY_PIXEL;
			if (cinfo.out_color_space == JCS_CMYK)
				return RGB_PIXEL;
			return INVALID_PIXEL;
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
		Nan::Persistent<Object> dstimage;
		Nan::Persistent<Object> buffer;
		Nan::Persistent<Function> cb;

		JpegReader reader;
		NativeImage dst;
	};

	void UV_decodeJpeg(uv_work_t* work_req) {
		JpegDecodeCtx *ctx = reinterpret_cast<JpegDecodeCtx*>(work_req->data);
		ctx->reader.decode(ctx->dst);
	}

	void V8_decodeJpeg(uv_work_t* work_req, int) {
		Nan::HandleScope scope;
		JpegDecodeCtx *ctx = reinterpret_cast<JpegDecodeCtx*>(work_req->data);
		makeCallback(Nan::New(ctx->cb), ctx->reader.error, Nan::New(ctx->dstimage));
		ctx->dstimage.Reset();
		ctx->buffer.Reset();
		ctx->cb.Reset();
		delete work_req;
		delete ctx;
	}

	NAN_METHOD(decodeJpeg) {
		if (info.Length() != 3 || !Buffer::HasInstance(info[0]) || !info[2]->IsFunction()) {
			Nan::ThrowError("expected: decodeJpeg(srcbuffer, opts, cb)");
			return;
		}
		Local<Object> srcbuf = info[0]->ToObject();
		Local<Function> cb = Local<Function>::Cast(info[2]);

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		JpegDecodeCtx * ctx = new JpegDecodeCtx;
		ctx->reader.open(srcdata, srclen);
		if (ctx->reader.error) {
			makeCallback(cb, ctx->reader.error, Nan::Undefined());
			delete ctx;
			return;
		}

		PixelMode pixel = ctx->reader.getPixel();
		if (pixel == INVALID_PIXEL) {
			makeCallback(cb, "Unsupported jpeg image color space", Nan::Undefined());
			delete ctx;
			return;
		}

		Local<Object> jsdst = newJsImage(ctx->reader.width(), ctx->reader.height(), pixel);
		ctx->dstimage.Reset(jsdst);
		ctx->buffer.Reset(srcbuf);
		ctx->cb.Reset(cb);
		ctx->dst = jsImageToNativeImage(jsdst);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_decodeJpeg, V8_decodeJpeg);
	}

	NAN_METHOD(decodeJpegSync) {
		if (info.Length() != 2 || !Buffer::HasInstance(info[0])) {
			Nan::ThrowError("expected: decodeJpegSync(srcbuffer, opts)");
			return;
		}
		Local<Object> srcbuf = info[0]->ToObject();

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		JpegReader reader;
		reader.open(srcdata, srclen);
		if (reader.error) {
			Nan::ThrowError(reader.error);
			return;
		}

		PixelMode pixel = reader.getPixel();
		if (pixel == INVALID_PIXEL) {
			Nan::ThrowError("Unsupported jpeg image color space");
			return;
		}

		Local<Object> jsdst = newJsImage(reader.width(), reader.height(), pixel);

		reader.decode(jsImageToNativeImage(jsdst));

		if (reader.error) {
			Nan::ThrowError(reader.error);
			return;
		}

		info.GetReturnValue().Set(jsdst);
	}

	NAN_METHOD(statJpeg) {
		if (info.Length() != 1 || !Buffer::HasInstance(info[0])) {
			Nan::ThrowError("expected: statJpeg(srcbuffer)");
			return;
		}
		Local<Object> srcbuf = info[0]->ToObject();

		JpegReader reader;
		reader.open(Buffer::Data(srcbuf), Buffer::Length(srcbuf));
		if (reader.error)
			return;

		PixelMode pixel = reader.getPixel();
		if (pixel == INVALID_PIXEL)
			return;

		Local<Object> stat = Nan::New<Object>();
		stat->Set(Nan::New(width_symbol), Nan::New<Integer>(reader.width()));
		stat->Set(Nan::New(height_symbol), Nan::New<Integer>(reader.height()));
		stat->Set(Nan::New(pixel_symbol), pixelEnumToSymbol(pixel));
		info.GetReturnValue().Set(stat);
	}


	//------------------------------------------------------------------------------------------------------------
	//--

	struct JpegEncodeCtx {
		JpegEncodeCtx() : error(0), dstdata(0) {}

		char *error;
		jmp_buf jmpbuf;

		Nan::Persistent<Value> buffer;
		Nan::Persistent<Function> cb;

		NativeImage image;

		uint8_t *dstdata;
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

	namespace {

		struct JpegDst : public jpeg_destination_mgr {
			JpegDst() : size(0), buf(0) {
				init_destination = initDest_;
				empty_output_buffer = emptyOutput_;
				term_destination = termDest_;
			}

			static void initDest_(j_compress_ptr cinfo) { static_cast<JpegDst*>(cinfo->dest)->initDest(); }
			void initDest() {
				size = 4 * 4096;
				buf = reinterpret_cast<unsigned char*>(malloc(size));
				next_output_byte = buf;
				free_in_buffer = size;
			}

			static boolean emptyOutput_(j_compress_ptr cinfo) { return static_cast<JpegDst*>(cinfo->dest)->emptyOutput(cinfo); }
			boolean emptyOutput(j_compress_ptr cinfo) {
				unsigned long nextsize = size * 2;
				unsigned char * nextbuffer = reinterpret_cast<unsigned char*>(malloc(nextsize));
				if (nextbuffer == NULL) ERREXIT(cinfo, JERR_OUT_OF_MEMORY);
				memcpy(nextbuffer, buf, size);
				free(buf);
				buf = nextbuffer;
				next_output_byte = buf + size;
				free_in_buffer = nextsize - size;
				size = nextsize;
				return TRUE;
			}

			static void termDest_(j_compress_ptr cinfo) { static_cast<JpegDst*>(cinfo->dest)->termDest(); }
			void termDest() {
				size -= free_in_buffer;
			}

			unsigned long size;
			unsigned char *buf;
		};
	}

	void JpegEncodeCtx::doWork() {
		jpeg_compress_struct cinfo;
		jpeg_error_mgr jerr;
		JpegDst jdst;

		cinfo.err = jpeg_std_error(&jerr);
		cinfo.err->error_exit = &JpegEncodeCtx::onError;
        jpeg_create_compress(&cinfo);

		cinfo.dest = &jdst;

		if (image.pixel == GREY_PIXEL) {
	        cinfo.input_components = 1;
	        cinfo.in_color_space = JCS_GRAYSCALE;
		}
		else {
	        cinfo.input_components = 3;
	        cinfo.in_color_space = JCS_RGB;
		}
        cinfo.image_width = (JDIMENSION)image.width;
        cinfo.image_height = (JDIMENSION)image.height;

        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, quality, true);
        jpeg_start_compress(&cinfo, true);

		for (int y = 0; y < image.height; ++y) {
			char * p = image.row(y);
			jpeg_write_scanlines(&cinfo, (JSAMPARRAY)(&p), 1);
		}

        jpeg_finish_compress(&cinfo);
		jpeg_destroy_compress(&cinfo);

		dstdata = reinterpret_cast<uint8_t*>(jdst.buf);
		dstlen = jdst.size;
	}

	void UV_encodeJpeg(uv_work_t* work_req) {
		JpegEncodeCtx *ctx = reinterpret_cast<JpegEncodeCtx*>(work_req->data);
		ctx->doWork();
	}

	void V8_encodeJpeg(uv_work_t* work_req, int) {
		Nan::HandleScope scope;
		JpegEncodeCtx *ctx = reinterpret_cast<JpegEncodeCtx*>(work_req->data);

		char * error = ctx->error;
		size_t dstlen = ctx->dstlen;
		uint8_t * dstdata = ctx->dstdata;
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
			Local<Object> o;
			e = Nan::Undefined();
			if (Nan::NewBuffer(reinterpret_cast<char*>(dstdata), dstlen).ToLocal(&o)) {
				dstdata = 0;
				r = o;
			}
			else {
				r = Nan::Undefined();
			}
		}

		free(error);
		if (dstdata)
			free(dstdata);

		Nan::TryCatch try_catch;

		Local<Value> argv[2] = { e, r };
		Nan::AsyncResource ass("picha");
		ass.runInAsyncScope(Nan::GetCurrentContext()->Global(), cb, 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);
	}

	NAN_METHOD(encodeJpeg) {
		if (info.Length() != 3 || !info[0]->IsObject() || !info[1]->IsObject() || !info[2]->IsFunction()) {
			Nan::ThrowError("expected: encodeJpeg(image, opts, cb)");
			return;
		}
		Local<Object> img = info[0]->ToObject();
		Local<Object> opts = info[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(info[2]);

		Local<Value> v = opts->Get(Nan::New(quality_symbol));
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
			Nan::ThrowError("invalid image");
			return;
		}

		ctx->quality = quality;
		ctx->buffer.Reset(img->Get(Nan::New(data_symbol)));
		ctx->cb.Reset(cb);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_encodeJpeg, V8_encodeJpeg);
	}

	NAN_METHOD(encodeJpegSync) {
		if (info.Length() != 2 || !info[0]->IsObject() || !info[1]->IsObject()) {
			Nan::ThrowError("expected: encodeJpegSync(image, opts)");
			return;
		}
		Local<Object> img = info[0]->ToObject();
		Local<Object> opts = info[1]->ToObject();

		Local<Value> v = opts->Get(Nan::New(quality_symbol));
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
			Nan::ThrowError("invalid image");
			return;
		}

		ctx.quality = quality;
		ctx.doWork();

		Local<Value> r;
		if (ctx.error) {
			Nan::ThrowError(ctx.error);
			free(ctx.error);
		}
		else {
			Local<Object> o;
			if (Nan::NewBuffer(reinterpret_cast<char*>(ctx.dstdata), ctx.dstlen).ToLocal(&o)) {
				r = o;
				ctx.dstdata = 0;
			}
			else {
				r = Nan::Undefined();
			}
		}

		if (ctx.dstdata)
			free(ctx.dstdata);
		info.GetReturnValue().Set(r);
	}

	std::vector<PixelMode> getJpegEncodes() {
		return std::vector<PixelMode>({ RGB_PIXEL, GREY_PIXEL });
	}

}


#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>
#include <node.h>
#include <node_buffer.h>
#include <string>

#include "tiffcodec.h"
#include "writebuffer.h"

namespace picha {

	//---------------------------------------------------------------------------------------------------------

	struct TiffErrorBase {
		std::string error;

		struct init {
			init() {
				TIFFSetErrorHandler(0);
				TIFFSetWarningHandler(0);
				TIFFSetErrorHandlerExt(&TiffErrorBase::errorProc);
			}
		};

		static void errorProc(thandle_t h, const char* m, const char* fmt, va_list va);
	};

	namespace { TiffErrorBase::init tiffInit; }

	void TiffErrorBase::errorProc(thandle_t h, const char* m, const char* fmt, va_list va) {
		TiffErrorBase * r = reinterpret_cast<TiffErrorBase*>(h);
		if (r == 0) return;
		char buf[1001];
		vsnprintf(buf, 1000, fmt, va);
		buf[1000] = 0;
		r->error = buf;
	}


	//---------------------------------------------------------------------------------------------------------

	struct TiffReader : public TiffErrorBase {
		char * databuf;
		size_t datalen;
		size_t datapos;

		TIFF * tiff;

		TiffReader() : databuf(0), datalen(0), datapos(0), tiff(0) {}
		~TiffReader() { if (tiff) TIFFClose(tiff); }

		void open(char * b, size_t l, int d);
		int width();
		int height();
		void decode(const NativeImage &dst);
		void close();

		void errorOut(const char * e) { error = e; }
		static tmsize_t readProc(thandle_t h, void* buf, tmsize_t size);
		static tmsize_t writeProc(thandle_t h, void* buf, tmsize_t size) { return 0; }
		static uint64 seekProc(thandle_t h, uint64 off, int whence);
		static toff_t sizeProc(thandle_t h) { return reinterpret_cast<TiffReader*>(h)->datalen; }
		static int closeProc(thandle_t) { return 0; }
		static int mapProc(thandle_t, void**, toff_t*) { return 0; }
		static void unmapProc(thandle_t, void* base, toff_t size) {}
	};

	uint64 TiffReader::seekProc(thandle_t h, uint64 off, int whence) {
		TiffReader * r = reinterpret_cast<TiffReader*>(h);
		switch (whence) {
			case SEEK_SET:
				r->datapos = size_t(off);
				break;
			case SEEK_CUR :
				r->datapos += ptrdiff_t(off);
				break;
			case SEEK_END :
				r->datapos = r->datalen + ptrdiff_t(off);
				break;
		}
		return r->datapos;
	}

	tmsize_t TiffReader::readProc(thandle_t h, void* buf, tmsize_t size) {
		TiffReader * r = reinterpret_cast<TiffReader*>(h);
		if (r->datapos + size > r->datalen) size = r->datalen - r->datapos;
		memcpy(buf, r->databuf + r->datapos, size);
		r->datapos += size;
		return size;
	}

	void TiffReader::open(char * b, size_t l, int d) {
		databuf = b;
		datalen = l;
		datapos = 0;

		tiff = TIFFClientOpen("memory", "rm", reinterpret_cast<thandle_t>(this),
			&TiffReader::readProc, &TiffReader::writeProc, &TiffReader::seekProc,
			&TiffReader::closeProc, &TiffReader::sizeProc, &TiffReader::mapProc, &TiffReader::unmapProc);

		if (tiff == 0) {
			errorOut("failed to open tiff file");
			return;
		}

		if (!TIFFSetDirectory(tiff, d)) {
			errorOut("invalid directory index");
			return;
		}
	}

	int TiffReader::width() {
		uint32 w;
		if (!error.empty() || tiff == 0) return 0;
		TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &w);
		return w;
	}

	int TiffReader::height() {
		uint32 w;
		if (!error.empty() || tiff == 0) return 0;
		TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &w);
		return w;
	}

	void TiffReader::decode(const NativeImage &dst) {
		assert(dst.pixel == RGBA_PIXEL);
		uint32 * p = reinterpret_cast<uint32*>(dst.data);
		if (!TIFFReadRGBAImageOriented(tiff, dst.width, dst.height, p, ORIENTATION_TOPLEFT, 0)) {
			errorOut("failed to read image data");
			return;
		}
	}

	void TiffReader::close() {
		if (tiff == 0) return;
		TIFFClose(tiff);
		tiff = 0;
	}

	//---------------------------------------------------------------------------------------------------------

	struct TiffDecodeCtx {
		Persistent<Value> dstimage;
		Persistent<Value> buffer;
		Persistent<Function> cb;

		TiffReader reader;
		NativeImage dst;
	};

	void UV_decodeTiff(uv_work_t* work_req) {
		TiffDecodeCtx *ctx = reinterpret_cast<TiffDecodeCtx*>(work_req->data);
		ctx->reader.decode(ctx->dst);
		ctx->reader.close();
	}

	void V8_decodeTiff(uv_work_t* work_req, int) {
		HandleScope scope;
		TiffDecodeCtx *ctx = reinterpret_cast<TiffDecodeCtx*>(work_req->data);
		makeCallback(ctx->cb, ctx->reader.error.empty() ? 0 : ctx->reader.error.c_str(), ctx->dstimage);
		delete ctx;
	}

	Handle<Value> decodeTiff(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 3 || !Buffer::HasInstance(args[0]) || !args[1]->IsObject() || !args[2]->IsFunction()) {
			ThrowException(Exception::Error(String::New("expected: decodeTiff(srcbuffer, opts, cb)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		int idx = 0;
		Local<Value> jidx = opts->Get(index_symbol);
		if (jidx->IsNumber())
			idx = jidx->Uint32Value();

		TiffDecodeCtx * ctx = new TiffDecodeCtx;
		ctx->reader.open(srcdata, srclen, idx);
		if (!ctx->reader.error.empty()) {
			delete ctx;
			makeCallback(cb, ctx->reader.error.empty() ? 0 : ctx->reader.error.c_str(), Undefined());
			return Undefined();
		}

		Local<Object> jsdst = newJsImage(ctx->reader.width(), ctx->reader.height(), RGBA_PIXEL);
		ctx->dstimage = Persistent<Value>::New(jsdst);
		ctx->buffer = Persistent<Value>::New(srcbuf);
		ctx->cb = Persistent<Function>::New(cb);
		ctx->dst = jsImageToNativeImage(jsdst);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_decodeTiff, V8_decodeTiff);

		return Undefined();
	}

	Handle<Value> decodeTiffSync(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 2 || !Buffer::HasInstance(args[0])) {
			ThrowException(Exception::Error(String::New("expected: decodeTiffSync(srcbuffer, opts)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();
		Local<Object> opts = args[1]->ToObject();

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		int idx = 0;
		Local<Value> jidx = opts->Get(index_symbol);
		if (jidx->IsNumber())
			idx = jidx->Uint32Value();

		TiffReader reader;
		reader.open(srcdata, srclen, idx);
		if (!reader.error.empty()) {
			ThrowException(Exception::Error(String::New(reader.error.c_str())));
			return Undefined();
		}

		Local<Object> jsdst = newJsImage(reader.width(), reader.height(), RGBA_PIXEL);

		reader.decode(jsImageToNativeImage(jsdst));
		reader.close();

		if (!reader.error.empty()) {
			ThrowException(Exception::Error(String::New(reader.error.c_str())));
			return Undefined();
		}

		return scope.Close(jsdst);
	}

	Handle<Value> statTiff(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 1 || !Buffer::HasInstance(args[0])) {
			ThrowException(Exception::Error(String::New("expected: statTiff(srcbuffer)")));
			return scope.Close(Undefined());
		}
		Local<Object> srcbuf = args[0]->ToObject();

		TiffReader reader;
		reader.open(Buffer::Data(srcbuf), Buffer::Length(srcbuf), 0);
		if (!reader.error.empty())
			return scope.Close(Undefined());

		Local<Object> stat = Object::New();
		stat->Set(width_symbol, Integer::New(reader.width()));
		stat->Set(height_symbol, Integer::New(reader.height()));
		stat->Set(pixel_symbol, pixelEnumToSymbol(RGBA_PIXEL));
		return scope.Close(stat);
	}


	//---------------------------------------------------------------------------------------------------------

	struct TiffWriter : public TiffErrorBase {
		TIFF * tiff;
		WriteBuffer buffer;

		TiffWriter() : tiff(0) {}
		~TiffWriter() { if (tiff) TIFFClose(tiff); }

		void write(NativeImage& image);

		void errorOut(const char * e) { error = e; }
		static tmsize_t readProc(thandle_t h, void* buf, tmsize_t size) { return 0; }
		static tmsize_t writeProc(thandle_t h, void* buf, tmsize_t size);
		static uint64 seekProc(thandle_t h, uint64 off, int whence);
		static toff_t sizeProc(thandle_t h);
		static int closeProc(thandle_t) { return 0; }
		static int mapProc(thandle_t, void**, toff_t*) { return 0; }
		static void unmapProc(thandle_t, void* base, toff_t size) {}
	};

	tmsize_t TiffWriter::writeProc(thandle_t h, void* buf, tmsize_t size) {
		TiffWriter * w = reinterpret_cast<TiffWriter*>(h);
		w->buffer.write(reinterpret_cast<char*>(buf), size);
		return size;
	}

	uint64 TiffWriter::seekProc(thandle_t h, uint64 off, int whence) {
		TiffWriter * w = reinterpret_cast<TiffWriter*>(h);
		w->buffer.seek(off, whence);
		return w->buffer.cursor;
	}

	toff_t TiffWriter::sizeProc(thandle_t h) {
		TiffWriter * w = reinterpret_cast<TiffWriter*>(h);
		return w->buffer.totallen;
	}

	void TiffWriter::write(NativeImage& image) {
		tiff = TIFFClientOpen("memory", "wm", reinterpret_cast<thandle_t>(this),
			&TiffWriter::readProc, &TiffWriter::writeProc, &TiffWriter::seekProc,
			&TiffWriter::closeProc, &TiffWriter::sizeProc, &TiffWriter::mapProc, &TiffWriter::unmapProc);
		if (tiff == 0) {
			errorOut("failed to open tiff file");
			return;
		}

		int sampleperpixel = pixelWidth(image.pixel);
		TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, (uint32)image.width);
		TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, (uint32)image.height);
		TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, (uint16)sampleperpixel);
		TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, (uint16)8);
		TIFFSetField(tiff,TIFFTAG_COMPRESSION,((uint16)COMPRESSION_NONE));
		TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
		TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, (uint16)PLANARCONFIG_CONTIG);
		TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, (uint16)(sampleperpixel < 3 ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB));

		for (int i = 0; i < image.height; ++i) {
			if (!TIFFWriteScanline(tiff, image.row(i), i))
				break;
		}

		TIFFClose(tiff);
		tiff = 0;
	}


	//---------------------------------------------------------------------------------------------------------

	struct TiffEncodeCtx {
		TiffEncodeCtx() : dstdata(0) {}

		Persistent<Value> buffer;
		Persistent<Function> cb;

		TiffWriter writer;
		NativeImage image;

		char *dstdata;
		size_t dstlen;
	};

	void UV_encodeTiff(uv_work_t* work_req) {
		TiffEncodeCtx *ctx = reinterpret_cast<TiffEncodeCtx*>(work_req->data);
		ctx->writer.write(ctx->image);
		ctx->dstlen = ctx->writer.buffer.totallen;
		ctx->dstdata = ctx->writer.buffer.consolidate();
	}

	void V8_encodeTiff(uv_work_t* work_req, int) {
		HandleScope scope;
		TiffEncodeCtx *ctx = reinterpret_cast<TiffEncodeCtx*>(work_req->data);

		std::string error;
		error.swap(ctx->writer.error);
		size_t dstlen = ctx->dstlen;
		char * dstdata = ctx->dstdata;
		Local<Function> cb = Local<Function>::New(ctx->cb);
		delete ctx;

		Local<Value> e, r;
		if (!error.empty()) {
			e = Exception::Error(String::New(error.c_str()));
			r = *Undefined();
		}
		else {
			e = *Undefined();
			Buffer *buf = Buffer::New(dstdata, dstlen);
			r = Local<Value>::New(buf->handle_);
		}

		delete[] dstdata;

		TryCatch try_catch;

		Handle<Value> argv[2] = { e, r };
		cb->Call(Context::GetCurrent()->Global(), 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);

		return;
	}

	Handle<Value> encodeTiff(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 3 || !args[0]->IsObject() || !args[2]->IsFunction()) {
			ThrowException(Exception::Error(String::New("expected: encodeTiff(image, opts, cb)")));
			return scope.Close(Undefined());
		}
		Local<Object> img = args[0]->ToObject();
		Local<Function> cb = Local<Function>::Cast(args[2]);

		TiffEncodeCtx * ctx = new TiffEncodeCtx;
		ctx->image = jsImageToNativeImage(img);
		if (!ctx->image.data) {
			delete ctx;
			ThrowException(Exception::Error(String::New("invalid image")));
			return scope.Close(Undefined());
		}

		ctx->buffer = Persistent<Value>::New(img->Get(data_symbol));
		ctx->cb = Persistent<Function>::New(cb);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_encodeTiff, V8_encodeTiff);

		return scope.Close(Undefined());
	}

	Handle<Value> encodeTiffSync(const Arguments& args) {
		HandleScope scope;

		if (args.Length() != 2 || !args[0]->IsObject()) {
			ThrowException(Exception::Error(String::New("expected: encodeTiffSync(image)")));
			return scope.Close(Undefined());
		}
		Local<Object> img = args[0]->ToObject();

		TiffWriter writer;
		NativeImage image = jsImageToNativeImage(img);
		if (!image.data) {
			ThrowException(Exception::Error(String::New("invalid image")));
			return scope.Close(Undefined());
		}

		writer.write(image);

		Local<Value> r = *Undefined();
		if (!writer.error.empty()) {
			ThrowException(Exception::Error(String::New(writer.error.c_str())));
		}
		else {
			size_t len = writer.buffer.totallen;
			Buffer *buf = Buffer::New(writer.buffer.consolidate(), len);
			r = Local<Value>::New(buf->handle_);
		}

		return scope.Close(r);
	}

}


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
		Nan::Persistent<Object> dstimage;
		Nan::Persistent<Object> buffer;
		Nan::Persistent<Function> cb;

		TiffReader reader;
		NativeImage dst;
	};

	void UV_decodeTiff(uv_work_t* work_req) {
		TiffDecodeCtx *ctx = reinterpret_cast<TiffDecodeCtx*>(work_req->data);
		ctx->reader.decode(ctx->dst);
		ctx->reader.close();
	}

	void V8_decodeTiff(uv_work_t* work_req, int) {
		Nan::HandleScope scope;
		TiffDecodeCtx *ctx = reinterpret_cast<TiffDecodeCtx*>(work_req->data);
		makeCallback(Nan::New(ctx->cb), ctx->reader.error.empty() ? 0 : ctx->reader.error.c_str(), Nan::New(ctx->dstimage));
		ctx->dstimage.Reset();
		ctx->buffer.Reset();
		ctx->cb.Reset();
		delete work_req;
		delete ctx;
	}

	NAN_METHOD(decodeTiff) {
		if (info.Length() != 3 || !Buffer::HasInstance(info[0]) || !info[1]->IsObject() || !info[2]->IsFunction()) {
			Nan::ThrowError("expected: decodeTiff(srcbuffer, opts, cb)");
			return;
		}
		MaybeLocal<Object> msrcbuf = info[0]->ToObject(Nan::GetCurrentContext());
		MaybeLocal<Object> mopts = info[1]->ToObject(Nan::GetCurrentContext());
		if (msrcbuf.IsEmpty() || mopts.IsEmpty())
			return;
		Local<Object> srcbuf = msrcbuf.ToLocalChecked();
		Local<Object> opts = mopts.ToLocalChecked();
		Local<Function> cb = Local<Function>::Cast(info[2]);

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		int idx = 0;
		Local<Value> jidx = opts->Get(Nan::New(index_symbol));
		if (jidx->IsNumber())
			idx = jidx->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);

		TiffDecodeCtx * ctx = new TiffDecodeCtx;
		ctx->reader.open(srcdata, srclen, idx);
		if (!ctx->reader.error.empty()) {
			delete ctx;
			makeCallback(cb, ctx->reader.error.empty() ? 0 : ctx->reader.error.c_str(), Nan::Undefined());
			return;
		}

		Local<Object> jsdst = newJsImage(ctx->reader.width(), ctx->reader.height(), RGBA_PIXEL);
		ctx->dstimage.Reset(jsdst);
		ctx->buffer.Reset(srcbuf);
		ctx->cb.Reset(cb);
		ctx->dst = jsImageToNativeImage(jsdst);

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_decodeTiff, V8_decodeTiff);
	}

	NAN_METHOD(decodeTiffSync) {
		if (info.Length() != 2 || !Buffer::HasInstance(info[0])) {
			Nan::ThrowError("expected: decodeTiffSync(srcbuffer, opts)");
			return;
		}
		MaybeLocal<Object> msrcbuf = info[0]->ToObject(Nan::GetCurrentContext());
		MaybeLocal<Object> mopts = info[1]->ToObject(Nan::GetCurrentContext());
		if (msrcbuf.IsEmpty() || mopts.IsEmpty())
			return;
		Local<Object> srcbuf = msrcbuf.ToLocalChecked();
		Local<Object> opts = mopts.ToLocalChecked();

		char* srcdata = Buffer::Data(srcbuf);
		size_t srclen = Buffer::Length(srcbuf);

		int idx = 0;
		Local<Value> jidx = opts->Get(Nan::New(index_symbol));
		if (jidx->IsNumber())
			idx = jidx->Uint32Value(Nan::GetCurrentContext()).FromMaybe(0);

		TiffReader reader;
		reader.open(srcdata, srclen, idx);
		if (!reader.error.empty()) {
			Nan::ThrowError(reader.error.c_str());
			return;
		}

		Local<Object> jsdst = newJsImage(reader.width(), reader.height(), RGBA_PIXEL);

		reader.decode(jsImageToNativeImage(jsdst));
		reader.close();

		if (!reader.error.empty()) {
			Nan::ThrowError(reader.error.c_str());
			return;
		}

		info.GetReturnValue().Set(jsdst);
	}

	NAN_METHOD(statTiff) {
		if (info.Length() != 1 || !Buffer::HasInstance(info[0])) {
			Nan::ThrowError("expected: statTiff(srcbuffer)");
			return;
		}
		MaybeLocal<Object> msrcbuf = info[0]->ToObject(Nan::GetCurrentContext());
		if (msrcbuf.IsEmpty())
			return;
		Local<Object> srcbuf = msrcbuf.ToLocalChecked();

		TiffReader reader;
		reader.open(Buffer::Data(srcbuf), Buffer::Length(srcbuf), 0);
		if (!reader.error.empty())
			return;

		Local<Object> stat = Nan::New<Object>();
		stat->Set(Nan::New(width_symbol), Nan::New<Integer>(reader.width()));
		stat->Set(Nan::New(height_symbol), Nan::New<Integer>(reader.height()));
		stat->Set(Nan::New(pixel_symbol), pixelEnumToSymbol(RGBA_PIXEL));
		info.GetReturnValue().Set(stat);
	}


	//---------------------------------------------------------------------------------------------------------

	struct TiffWriter : public TiffErrorBase {
		TIFF * tiff;
		WriteBuffer buffer;

		TiffWriter() : tiff(0) {}
		~TiffWriter() { if (tiff) TIFFClose(tiff); }

		void write(NativeImage& image, int comp);

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

	void TiffWriter::write(NativeImage& image, int comp) {
		tiff = TIFFClientOpen("memory", "wm", reinterpret_cast<thandle_t>(this),
			&TiffWriter::readProc, &TiffWriter::writeProc, &TiffWriter::seekProc,
			&TiffWriter::closeProc, &TiffWriter::sizeProc, &TiffWriter::mapProc, &TiffWriter::unmapProc);
		if (tiff == 0) {
			errorOut("failed to open tiff file");
			return;
		}

		int sampleperpixel = pixelChannels(image.pixel);
		TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, (uint32)image.width);
		TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, (uint32)image.height);
		TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, (uint16)sampleperpixel);
		TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, (uint16)(pixelBytes(image.pixel) / sampleperpixel * 8));
		TIFFSetField(tiff,TIFFTAG_COMPRESSION,((uint16)comp));
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
		TiffEncodeCtx() : dstdata_(0) {}

		Nan::Persistent<Value> buffer;
		Nan::Persistent<Function> cb;

		TiffWriter writer;
		NativeImage image;
		int comp;

		char *dstdata_;
		size_t dstlen;
	};

	void UV_encodeTiff(uv_work_t* work_req) {
		TiffEncodeCtx *ctx = reinterpret_cast<TiffEncodeCtx*>(work_req->data);
		ctx->writer.write(ctx->image, ctx->comp);
		ctx->dstlen = ctx->writer.buffer.totallen;
		ctx->dstdata_ = ctx->writer.buffer.consolidate_();
	}

	void V8_encodeTiff(uv_work_t* work_req, int) {
		Nan::HandleScope scope;

		TiffEncodeCtx *ctx = reinterpret_cast<TiffEncodeCtx*>(work_req->data);

		std::string error;
		error.swap(ctx->writer.error);
		size_t dstlen = ctx->dstlen;
		char * dstdata_ = ctx->dstdata_;
		Local<Function> cb = Nan::New(ctx->cb);
		ctx->buffer.Reset();
		ctx->cb.Reset();
		delete work_req;
		delete ctx;

		Local<Value> e, r;
		if (!error.empty()) {
			e = Nan::Error(error.c_str());
			r = Nan::Undefined();
		}
		else {
			Local<Object> o;
			e = Nan::Undefined();
			if (Nan::NewBuffer(reinterpret_cast<char*>(dstdata_), dstlen).ToLocal(&o)) {
				r = o;
				dstdata_ = 0;
			}
			else {
				r = Nan::Undefined();
			}
		}

		if (dstdata_)
			free(dstdata_);

		Nan::TryCatch try_catch;

		Local<Value> argv[2] = { e, r };
		Nan::AsyncResource ass("picha");
		ass.runInAsyncScope(Nan::GetCurrentContext()->Global(), cb, 2, argv);

		if (try_catch.HasCaught())
			FatalException(try_catch);

		return;
	}

	namespace {
		const struct { Persistent<String>* symbol; int tag; } TiffCompressionModes[] = {
			{ &none_symbol, COMPRESSION_NONE },
			{ &lzw_symbol, COMPRESSION_LZW },
			{ &deflate_symbol, COMPRESSION_ADOBE_DEFLATE },
		};

		const int TiffCompressionCount = sizeof(TiffCompressionModes) / sizeof(TiffCompressionModes[0]);

		bool getTiffCompression(Handle<Value> jcomp, int& comp) {
			for (int i = 0; i < TiffCompressionCount; ++i) {
				if (jcomp->StrictEquals(Nan::New(*TiffCompressionModes[i].symbol))) {
					comp = TiffCompressionModes[i].tag;
					return true;
				}
			}
			return false;
		}
	}

	NAN_METHOD(encodeTiff) {
		if (info.Length() != 3 || !info[0]->IsObject() || !info[1]->IsObject() || !info[2]->IsFunction()) {
			Nan::ThrowError("expected: encodeTiff(image, opts, cb)");
			return;
		}
		MaybeLocal<Object> mimg = info[0]->ToObject(Nan::GetCurrentContext());
		MaybeLocal<Object> mopts = info[1]->ToObject(Nan::GetCurrentContext());
		if (mimg.IsEmpty() || mopts.IsEmpty())
			return;
		Local<Object> img = mimg.ToLocalChecked();
		Local<Object> opts = mopts.ToLocalChecked();
		Local<Function> cb = Local<Function>::Cast(info[2]);

		int comp = COMPRESSION_LZW;
		if (opts->Has(Nan::New(compression_symbol)) && !getTiffCompression(opts->Get(Nan::New(compression_symbol)), comp)) {
			Nan::ThrowError("invalid compression option");
			return;
		}

		TiffEncodeCtx * ctx = new TiffEncodeCtx;
		ctx->image = jsImageToNativeImage(img);
		if (!ctx->image.data) {
			delete ctx;
			Nan::ThrowError("invalid image");
			return;
		}

		ctx->buffer.Reset(img->Get(Nan::New(data_symbol)));
		ctx->cb.Reset(cb);
		ctx->comp = comp;

		uv_work_t* work_req = new uv_work_t();
		work_req->data = ctx;
		uv_queue_work(uv_default_loop(), work_req, UV_encodeTiff, V8_encodeTiff);
	}

	NAN_METHOD(encodeTiffSync) {
		if (info.Length() != 2 || !info[0]->IsObject() || !info[1]->IsObject()) {
			Nan::ThrowError("expected: encodeTiffSync(image, opts)");
			return;
		}
		MaybeLocal<Object> mimg = info[0]->ToObject(Nan::GetCurrentContext());
		MaybeLocal<Object> mopts = info[1]->ToObject(Nan::GetCurrentContext());
		if (mimg.IsEmpty() || mopts.IsEmpty())
			return;
		Local<Object> img = mimg.ToLocalChecked();
		Local<Object> opts = mopts.ToLocalChecked();

		int comp = COMPRESSION_LZW;
		if (opts->Has(Nan::New(compression_symbol)) && !getTiffCompression(opts->Get(Nan::New(compression_symbol)), comp)) {
			Nan::ThrowError("invalid compression option");
			return;
		}

		TiffWriter writer;
		NativeImage image = jsImageToNativeImage(img);
		if (!image.data) {
			Nan::ThrowError("invalid image");
			return;
		}

		writer.write(image, comp);

		Local<Value> r;
		if (!writer.error.empty()) {
			Nan::ThrowError(writer.error.c_str());
		}
		else {
			Local<Object> o;
			size_t dstlen = writer.buffer.totallen;
			char * dstdata_ = writer.buffer.consolidate_();
			if (Nan::NewBuffer(dstdata_, dstlen).ToLocal(&o)) {
				r = o;
			}
			else {
				free(dstdata_);
				r = Nan::Undefined();
			}
		}

		info.GetReturnValue().Set(r);
	}

	std::vector<PixelMode> getTiffEncodes() {
		return std::vector<PixelMode>({ RGB_PIXEL, RGBA_PIXEL, GREY_PIXEL, GREYA_PIXEL,
			R16_PIXEL, R16G16_PIXEL, R16G16B16_PIXEL, R16G16B16A16_PIXEL, });
	}

}

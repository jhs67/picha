#ifndef picha_jpegcodec_h_
#define picha_jpegcodec_h_

#include "picha.h"

namespace picha {

	Handle<Value> encodeJpeg(const Arguments& args);
	Handle<Value> decodeJpeg(const Arguments& args);
	Handle<Value> encodeJpegSync(const Arguments& args);
	Handle<Value> decodeJpegSync(const Arguments& args);

}

#endif // picha_jpegcodec_h_

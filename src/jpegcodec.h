#ifndef picha_jpegcodec_h_
#define picha_jpegcodec_h_

#include "picha.h"

namespace picha {

	NAN_METHOD(statJpeg);
	NAN_METHOD(decodeJpeg);
	NAN_METHOD(decodeJpegSync);
	NAN_METHOD(encodeJpeg);
	NAN_METHOD(encodeJpegSync);

}

#endif // picha_jpegcodec_h_

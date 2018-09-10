#ifndef picha_webpcodec_h_
#define picha_webpcodec_h_

#include "picha.h"

namespace picha {

	NAN_METHOD(statWebP);
	NAN_METHOD(decodeWebP);
	NAN_METHOD(decodeWebPSync);
	NAN_METHOD(encodeWebP);
	NAN_METHOD(encodeWebPSync);
	std::vector<PixelMode> getWebpEncodes();

}

#endif // picha_webpcodec_h_

#ifndef picha_pngcodec_h_
#define picha_pngcodec_h_

#include "picha.h"

namespace picha {

	NAN_METHOD(statPng);
	NAN_METHOD(decodePng);
	NAN_METHOD(decodePngSync);
	NAN_METHOD(encodePng);
	NAN_METHOD(encodePngSync);
	std::vector<PixelMode> getPngEncodes();

}

#endif // picha_pngcodec_h_

#ifndef picha_tiffcodec_h_
#define picha_tiffcodec_h_

#include "picha.h"

namespace picha {

	NAN_METHOD(statTiff);
	NAN_METHOD(decodeTiff);
	NAN_METHOD(decodeTiffSync);
	NAN_METHOD(encodeTiff);
	NAN_METHOD(encodeTiffSync);

}

#endif // picha_tiffcodec_h_

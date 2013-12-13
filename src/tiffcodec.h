#ifndef picha_tiffcodec_h_
#define picha_tiffcodec_h_

#include "picha.h"

namespace picha {

	Handle<Value> statTiff(const Arguments& args);
	Handle<Value> decodeTiff(const Arguments& args);
	Handle<Value> decodeTiffSync(const Arguments& args);
	Handle<Value> encodeTiff(const Arguments& args);
	Handle<Value> encodeTiffSync(const Arguments& args);

}

#endif // picha_tiffcodec_h_

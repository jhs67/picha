#ifndef picha_pngcodec_h_
#define picha_pngcodec_h_

#include "picha.h"

namespace picha {

	Handle<Value> encodePng(const Arguments& args);
	Handle<Value> decodePng(const Arguments& args);
	Handle<Value> encodePngSync(const Arguments& args);
	Handle<Value> decodePngSync(const Arguments& args);

}

#endif // picha_pngcodec_h_

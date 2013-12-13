#ifndef picha_webpcodec_h_
#define picha_webpcodec_h_

#include "picha.h"

namespace picha {

	Handle<Value> statWebP(const Arguments& args);
	Handle<Value> decodeWebP(const Arguments& args);
	Handle<Value> decodeWebPSync(const Arguments& args);
	Handle<Value> encodeWebP(const Arguments& args);
	Handle<Value> encodeWebPSync(const Arguments& args);

}

#endif // picha_webpcodec_h_

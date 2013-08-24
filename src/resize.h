#ifndef picha_resize_h_
#define picha_resize_h_

#include "picha.h"

namespace picha {

	Handle<Value> resize(const Arguments& args);
	Handle<Value> resizeSync(const Arguments& args);
}

#endif // picha_resize_h_

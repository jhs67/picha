#ifndef picha_colorconvert_h_
#define picha_colorconvert_h_

#include "picha.h"

namespace picha {

	Handle<Value> colorConvert(const Arguments& args);
	Handle<Value> colorConvertSync(const Arguments& args);

}

#endif // picha_colorconvert_h_

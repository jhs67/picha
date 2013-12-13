#ifndef picha_colorconvert_h_
#define picha_colorconvert_h_

#include "picha.h"

namespace picha {

	Handle<Value> colorConvert(const Arguments& args);
	Handle<Value> colorConvertSync(const Arguments& args);

	struct ColorSettings {
		ColorSettings() : rFactor(0.299f), gFactor(0.587f), bFactor(0.114) {}
		float rFactor, gFactor, bFactor;
	};

	void doColorConvert(const ColorSettings &cs, NativeImage& src, NativeImage& dst);

}

#endif // picha_colorconvert_h_

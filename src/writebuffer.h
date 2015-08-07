#ifndef picha_writebuffer_h_
#define picha_writebuffer_h_

#include "picha.h"

namespace picha {

	//----------------------------------------------------------------------------------------------------------------
	//--

	struct WriteBuffer {

		WriteBuffer() : hblock(0), cblock(0), totallen(0), cursor(0) {}
		~WriteBuffer() { delete hblock; }

		void write(char * data, size_t length);
		void seek(size_t o, int whence);
		char * consolidate_();

		//--

		struct WriteBlock {
			char * data;
			size_t start;
			size_t length;
			WriteBlock * next;
			WriteBlock() : data(0), next(0) {}
			~WriteBlock() { if (data) free(data); delete next; }
		};

		static const size_t min_block = 64 * 1024;

		void seek_(size_t length);

		WriteBlock * hblock;
		WriteBlock * cblock;
		size_t totallen;
		size_t cursor;
	};

}

#endif // picha_writebuffer_h_

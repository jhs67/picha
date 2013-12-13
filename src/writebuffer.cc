
#include <string.h>

#include "writebuffer.h"

namespace picha {

	//---------------------------------------------------------------------------------------------------------

	void WriteBuffer::write(char * data, size_t length) {
		while (length > 0) {
			size_t space = cblock == 0 ? 0 : cblock->length + cblock->start - cursor;
			if (space == 0) {
				if (cblock == 0 || cblock->next == 0) {
					WriteBlock * n = new WriteBlock;
					n->length = length > min_block ? length : min_block;
					n->data = new char[n->length];
					n->start = cursor;
					if (cblock == 0) cblock = hblock = n;
					else cblock = cblock->next = n;
				}
				else {
					cblock = cblock->next;
				}
			}
			else {
				size_t l = length < space ? length : space;
				memcpy(cblock->data + cblock->length - space, data, l);
				cursor += l;
				length -= l;
				data += l;
			}
		}
		if (totallen < cursor)
			totallen = cursor;
	}

	void WriteBuffer::seek_(size_t length) {
		if (cursor == length)
			return;
		cursor = 0;
		cblock = hblock;
		while (length > 0) {
			size_t space = cblock == 0 ? 0 : cblock->length + cblock->start - cursor;
			if (space == 0) {
				if (cblock == 0 || cblock->next == 0) {
					WriteBlock * n = new WriteBlock;
					n->length = length + min_block;
					n->data = new char[n->length];
					memset(n->data, 0, length);
					n->start = cursor;
					if (cblock == 0) cblock = hblock = n;
					else cblock = cblock->next = n;
				}
				else {
					cblock = cblock->next;
				}
			}
			else {
				size_t l = length < space ? length : space;
				cursor += l;
				length -= l;
			}
		}
	}

	void WriteBuffer::seek(size_t o, int whence) {
		switch (whence) {
			case SEEK_SET:
				seek_(o);
				break;
			case SEEK_CUR :
				seek_(cursor + o);
				break;
			case SEEK_END :
				seek_(totallen + o);
				break;
		}
	}

	char * WriteBuffer::consolidate() {
		char * r = 0;
		if (hblock != 0) {
			if (hblock->next == 0) {
				r = hblock->data;
				hblock->data = 0;
			}
			else {
				r = new char[totallen];
				for (WriteBlock * b = hblock; b != 0; b = b->next) {
					size_t l = b->length > totallen - b->start ? totallen - b->start : b->length;
					memcpy(r + b->start, b->data, l);
				}
			}
		}
		delete hblock;
		totallen = 0;
		hblock = 0;
		cblock = 0;
		cursor = 0;
		return r;
	}

}

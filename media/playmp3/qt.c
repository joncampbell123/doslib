#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>

#include "qt.h"

static int mem_read(void *a,void *b,size_t c) {
	int64_t rem;
	quicktime_stack *q = (quicktime_stack*)a;
	if (q->mem_base == NULL) return -1;
	rem = (int64_t)q->mem_size - (int64_t)q->mem_seek;
	if (rem <= 0) return 0;
	if ((int64_t)c > rem) c = (int)rem;
	if (c > 0) memcpy(b,(char*)q->mem_base+q->mem_seek,c);
	q->mem_seek += (size_t)c;
	assert(q->mem_seek <= q->mem_size);
	return (int)c;
}

static int64_t mem_seek(void *a,int64_t b) {
	quicktime_stack *q = (quicktime_stack*)a;
	if (b < 0) b = 0;
	if (b > (int64_t)q->mem_size) b = (int64_t)q->mem_size;
	q->mem_seek = (size_t)b;
	return (int64_t)q->mem_seek;
}

static int64_t mem_filesize(void *a) {
	quicktime_stack *q = (quicktime_stack*)a;
	if (q->mem_base == NULL) return -1LL;
	return (int64_t)q->mem_size;
}

static int file_fd_read(void *a,void *b,size_t c) {
	quicktime_stack *q = (quicktime_stack*)a;
	if (q->fd < 0) return -1;
	return (int)read(q->fd,b,c);
}

static int64_t file_fd_seek(void *a,int64_t b) {
	quicktime_stack *q = (quicktime_stack*)a;
	if (q->fd < 0) return -1LL;
	return (int64_t)lseek(q->fd,b,SEEK_SET);
}

static int64_t file_fd_filesize(void *a) {
	quicktime_stack *q = (quicktime_stack*)a;
	if (q->fd < 0) return -1LL;
	return (int64_t)lseek(q->fd,0,SEEK_END);
}

/* [doc] quicktime_stack_create
 *
 * Create quicktime stack object
 *
 * Parameters:
 *
 *     depth = maximum atom depth
 *
 * Return value:
 *
 *     quicktime stack object, or NULL if error
 *
 */
quicktime_stack *quicktime_stack_create(int depth) {
	quicktime_stack *s = (quicktime_stack*)malloc(sizeof(quicktime_stack));
	if (s == NULL) return NULL;
	memset(s,0,sizeof(*s));

	if (depth == 0)
		depth = 64;
	else if (depth < 16)
		depth = 16;
	else if (depth > 256)
		depth = 256;

	s->fd = -1;
	s->current = -1;
	s->depth = depth;
	s->stack = (quicktime_atom*)malloc(sizeof(quicktime_atom) * s->depth);
	if (s->stack == NULL)
		return quicktime_stack_destroy(s);

	return s;
}

/* [doc] quicktime_stack_empty
 *
 * Empty the quicktime stack
 *
 * Parameters:
 *
 *     s = quicktime stack
 *
 */
void quicktime_stack_empty(quicktime_stack *s) {
	s->eof = 0;
	s->top = NULL;
	s->current = -1;
	s->next_read = 0;
}

/* [doc] quicktime_stack_destroy
 *
 * Destroy and free the quicktime stack object
 *
 * Parameters:
 *
 *     s = quicktime stack
 *
 * Return value:
 *
 *     NULL
 *
 */
quicktime_stack *quicktime_stack_destroy(quicktime_stack *s) {
	if (s) {
		if (s->fd >= 0) {
			if (s->ownfd) close(s->fd);
			s->fd = -1;
		}
		if (s->stack) free(s->stack);
		s->stack = s->top = NULL;
		free(s);
	}
	return NULL;
}

/* [doc] quicktime_stack_pop
 *
 * Pop the atom off the top of the stack
 *
 * Parameters:
 *
 *     s = quicktime stack
 *
 * Return value:
 *
 *     the new top atom of the stack. if the stack is now empty, the function
 *     returns NULL
 *
 */
quicktime_atom *quicktime_stack_pop(quicktime_stack *s) {
	if (s->current < 0)
		return NULL;

	if (s->current-- == 0) { /* NTS: if it *WAS* zero, and is now -1, the nature of post-decrement */
		s->top = NULL;
		return NULL;
	}

	s->top--;
	assert(s->top == (s->stack + s->current));
	return s->top;
}

/* [doc] quicktime_stack_push
 *
 * Push atom onto the top of the stack
 *
 * Parameters:
 *
 *     s = quicktime stack
 *     a = atom to push onto the top
 *
 * Return value:
 *
 *     0 if there is not enough room
 *     1 if success
 *
 */
int quicktime_stack_push(quicktime_stack *s,quicktime_atom *a) {
	if (s->current >= (s->depth-1))
		return 0;

	if (s->current >= 0)
		assert(s->top == (s->stack + s->current));

	s->top = s->stack + (++s->current);
	*(s->top) = *a;
	return 1;
}

/* [doc] quicktime_stack_top
 *
 * Return pointer to the atom on the top of the stack
 *
 * Parameters:
 *
 *     s = quicktime stack
 *
 * Return value:
 *
 *     pointer to the atom on the top of the stack, or NULL if there is nothing
 *     on the stack
 *
 */
quicktime_atom *quicktime_stack_top(quicktime_stack *s) {
	if (s->current >= 0) {
		assert(s->top == (s->stack + s->current));
		return s->top;
	}
	return NULL;
}

/* [doc] quicktime_stack_assign_mem
 *
 * Associate a quicktime stack with a memory buffer. This enables quicktime
 * movies to be parsed from a memory buffer.
 *
 * Parameters:
 *
 *     s = quicktime stack
 *     buffer = pointer to memory containing quicktime movie
 *     len = length of buffer
 *
 * Return value:
 *
 *     1 if successful
 *     0 if error
 *
 */
int quicktime_stack_assign_mem(quicktime_stack *s,void *buffer,size_t len) {
	if (s->current >= 0 || s->fd >= 0 || s->mem_base != NULL)
		return 0;

	s->read = mem_read;
	s->seek = mem_seek;
	s->filesize = mem_filesize;
	s->eof = 0;
	s->fd = -1;
	s->ownfd = 0;
	s->mem_base = buffer;
	s->mem_size = len;
	s->mem_seek = 0;
	return 1;
}

/* [doc] quicktime_stack_assign_fd
 *
 * Associate a quicktime stack with a file descriptor. This enables quicktime
 * movies to be parsed from a file.
 *
 * Parameters:
 *
 *     s = quicktime stack
 *     fd = file descriptor to read from
 *
 * Return value:
 *
 *     1 if successful
 *     0 if error
 *
 */
int quicktime_stack_assign_fd(quicktime_stack *s,int fd) {
	if (s->current >= 0 || s->fd >= 0 || s->mem_base != NULL)
		return 0;

	s->read = file_fd_read;
	s->seek = file_fd_seek;
	s->filesize = file_fd_filesize;
	s->mem_base = NULL;
	s->mem_size = 0LL;
	s->eof = 0;
	s->fd = fd;
	s->ownfd = 0;
	return 1;
}

/* [doc] quicktime_stack_seek
 *
 * seek atom or stack file pointer to the specified offset.
 *
 * Parameters:
 *
 *     s = quicktime stack
 *     a = atom to seek in
 *     offset = atom offset to seek to
 *
 * How to seek to an absolute offset in the quicktime file itself:
 *
 *     s = stack
 *     a = NULL
 *     offset = file offset
 *
 * How to seek to an absolute offset in the atom:
 *
 *     s = stack
 *     a = atom
 *     offset = offset inside atom
 *
 * Return value:
 *
 *     new file position
 *
 */
int64_t quicktime_stack_seek(quicktime_stack *s,quicktime_atom *a,int64_t offset) {
	int64_t ret = -1LL;
	if (offset < 0) offset = 0;

	if (a) { /* within the parent chunk */
		if ((uint64_t)offset > a->data_length) offset = (int64_t)a->data_length;
		a->read_offset = (uint64_t)offset;
		ret = (int64_t)(a->read_offset);
	}
	else { /* at top level */
		ret = s->seek(s,offset);
		if (ret < 0) ret = 0;
		s->next_read = ret;
	}

	return ret;
}

/* [doc] quicktime_stack_read
 *
 * read data from file or atom. the "current position" is updated to reflect
 * the number of bytes read.
 *
 * Parameters:
 *
 *     s = quicktime stack
 *     a = atom to read from
 *     buffer = where to put the data
 *     len = how much data
 *
 * Reading from the file directly:
 *
 *     s = stack, a = NULL
 *
 * Reading from within an atom:
 *
 *     s = stack, a = atom
 *
 * Return value:
 *
 *     number of bytes read
 *
 */
int quicktime_stack_read(quicktime_stack *s,quicktime_atom *a,void *buffer,size_t len) {
	int64_t seekto;
	int ret = 0;

	if (s == NULL) return -1;
	if (buffer == NULL) return 0;
	if (len == (size_t)0u) return 0;

	if (a) {
		int64_t rem;
		if (a->read_offset >= a->data_length) return 0;
		rem = (int64_t)(a->data_length - a->read_offset);
		if ((int64_t)len > rem) len = (int)rem;
		seekto = a->absolute_data_offset + a->read_offset;
		if (s->seek(s,seekto) != (int64_t)seekto) return 0;
		ret = s->read(s,buffer,len);
		if (ret > 0) a->read_offset += ret;
	}
	else {
		if (s->seek(s,s->next_read) != (int64_t)s->next_read) return 0;
		ret = s->read(s,buffer,len);
		if (ret > 0) s->next_read += ret;
	}

	return ret;
}

/* [doc] quicktime_stack_auto_identify_atom_subchunks
 *
 * given the atom, identify whether the atom has sub-atoms
 *
 * Parameters:
 *
 *     a = atom
 *
 * Return value:
 *
 *     whether the atom typically has subchunks
 *
 */
int quicktime_stack_auto_identify_atom_subchunks(quicktime_atom *a) {
	a->reader_knows_there_are_subchunks = 0;
	a->reader_knows_subchunks_are_at = 0;

	if (	a->type == quicktime_type_const('m','o','o','v') ||
		a->type == quicktime_type_const('t','r','a','k') ||
		a->type == quicktime_type_const('u','d','t','a') ||
		a->type == quicktime_type_const('c','l','i','p') ||
		a->type == quicktime_type_const('m','d','i','a') ||
		a->type == quicktime_type_const('m','i','n','f') ||
		a->type == quicktime_type_const('d','i','n','f') ||
		a->type == quicktime_type_const('s','t','b','l') ||
		a->type == quicktime_type_const('e','d','t','s') ||
		a->type == quicktime_type_const('c','m','o','v')) {
		a->reader_knows_there_are_subchunks = 1;
	}

	return a->reader_knows_there_are_subchunks;
}

/* [doc] quicktime_stack_read_atom
 *
 * read and parse an atom header. typically used to read sub-atoms when the
 * caller knows that they are there.
 *
 * Warning:
 *
 *     If the caller is wrong there is no protection whatsoever against reading
 *     and mis-interpreting data as an atom within the atom.
 *
 * Parameters:
 *
 *     s = stack
 *     pa = parent atom (where the data will be read from)
 *     a = child atom structure (to be filled in with what was read if success)
 *
 * Reading directly from the file:
 *
 *     s = stack, pa = NULL
 *
 * Reading from within an atom:
 *
 *     s = stack, pa = parent atom
 *
 * Return value:
 *
 *     nonzero = success
 *     0 = failure
 *
 */
int quicktime_stack_read_atom(quicktime_stack *s,quicktime_atom *pa,quicktime_atom *a) {
	uint64_t offset;
	unsigned char tmp[8];
	memset(a,0,sizeof(*a));

	if (pa) {
		if (!pa->reader_knows_there_are_subchunks)
			return 0;
		if (pa->read_offset < pa->reader_knows_subchunks_are_at)
			pa->read_offset = pa->reader_knows_subchunks_are_at;
		offset = pa->absolute_data_offset + pa->read_offset;
	}
	else {
		offset = s->next_read;
	}

	if (quicktime_stack_read(s,pa,tmp,8) < 8) {
		if (pa == NULL) s->eof = 1;
		return 0;
	}
	if (__be_u32(tmp) == 0) {
		if (pa == NULL) s->eof = 1;
		return 0;
	}

	a->absolute_header_offset = offset;
	a->absolute_data_offset = offset + 8;
	a->data_length = __be_u32(tmp);
	a->type = __be_u32(tmp+4);

	if (a->data_length == 0) {
		if (pa != NULL) {
			/* that's not valid... */
			pa->read_offset = pa->data_length;
			return 0;
		}
		a->data_length = s->filesize(s) - a->absolute_data_offset;
		a->extends_to_eof = 1;
	}
	else if (a->data_length == 1) {
		/* 64-bit length field follows */
		a->absolute_data_offset += 8;
		if (quicktime_stack_read(s,pa,tmp,8) < 8) {
			if (pa == NULL) s->eof = 1;
			return 0;
		}

		a->data_length = __be_u64(tmp);
		if (a->data_length < 16ULL) {
			if (pa == NULL) s->eof = 1;
			else pa->read_offset = pa->data_length;
			return 0;
		}
		a->data_length -= 16ULL;
		a->extended = 1;
	}
	else if (a->data_length >= 8ULL) {
		/* the atom length includes the header */
		a->data_length -= 8ULL;
	}
	else {
		/* invalid */
		if (pa) pa->read_offset = pa->data_length;
		else s->eof = 1;
		return 0;
	}

	a->absolute_atom_end = a->absolute_data_offset + a->data_length;
	if (pa) pa->read_offset += a->data_length;
	else s->next_read += a->data_length;

	quicktime_stack_auto_identify_atom_subchunks(a);
	return 1;
}

/* [doc] quicktime_stack_force_chunk_subchunks
 *
 * force the idea that the parent atom has sub-atoms. this can be used by the
 * caller when it knows that an atom has sub-atoms.
 *
 * Parameters:
 *
 *     s = stack
 *     a = atom
 *     force = state whether there is (1) or is not (0) any sub-atoms
 *     offset = offset within atom of first sub-atom
 *
 * Return value:
 *
 *     nonzero = success
 *     0 = failure
 *
 */
int quicktime_stack_force_chunk_subchunks(quicktime_stack *s,quicktime_atom *a,int force,uint32_t offset) {
	if (s == NULL || a == NULL)
		return 0;

	a->reader_knows_there_are_subchunks = force;
	a->reader_knows_subchunks_are_at = force ? offset : 0U;
	return 1;
}

/* [doc] quicktime_stack_eof
 *
 * return whether or not the end of the quicktime file has been reached.
 * the caller can use this in the outermost loop to run through all the
 * atoms, then terminate properly when the stack is exausted and there
 * are no more atoms to parse.
 *
 * Parameters:
 *
 *     s = stack
 *
 * Return value:
 *
 *     nonzero = eof
 *     0 = keep going
 *
 */
int quicktime_stack_eof(quicktime_stack *s) {
	if (s->current >= 0)
		return 0;

	return s->eof;
}

/* [doc] quicktime_stack_type_to_string
 *
 * Convert a quicktime atom datatype to an ASCII string. Pointer 't' must point
 * to valid memory at least 5 bytes long.
 *
 * Parameters:
 *
 *     a = atom
 *     t = char[] array to output to
 *
 */
void quicktime_stack_type_to_string(quicktime_atom_type_t a,char *t) {
	t[0] = (char)(a >> 24U);
	t[1] = (char)(a >> 16U);
	t[2] = (char)(a >>  8U);
	t[3] = (char)(a >>  0U);
	t[4] = (char)0;
}

/* [doc] quicktime_stack_debug_print_atom
 *
 * [internal function] print a quicktime atom and information tied to it.
 * however if the enable_atoms flag is not set this function does nothing.
 *
 * Parameters:
 *
 *     fp = stdio file object to emit to
 *     current = depth of the atom in the stack
 *     a = the atom
 *
 */
void quicktime_stack_debug_print_atom(FILE *fp,int current,quicktime_atom *a) {
	char tmp[5];
	int i;

	for (i=0;i < current;i++)
		fprintf(fp,"  ");

	quicktime_stack_type_to_string(a->type,tmp);
	fprintf(fp,"[%d] '%s' hdr=%llu data=%llu len=%llu end=%llu ext=%u\n",current,tmp,
		(unsigned long long)a->absolute_header_offset,
		(unsigned long long)a->absolute_data_offset,
		(unsigned long long)a->data_length,
		(unsigned long long)(a->absolute_atom_end),
		(unsigned int)(a->extended ? 1 : 0));
}

/* [doc] quicktime_stack_debug_chunk_dump
 *
 * [internal function] print a quicktime atom's contents on the screen
 * up to 64 bytes for debugging purposes.
 *
 * Parameters:
 *
 *     fp = stdio file object to emit to
 *     riff = quicktime stack
 *     chunk = chunk to print contents of
 *
 */
void quicktime_stack_debug_chunk_dump(FILE *fp,quicktime_stack *riff,quicktime_atom *chunk) {
	uint64_t opos = chunk->read_offset;
	unsigned char tmp[64];
	int rd,i,col=0,o;
	int64_t aaa;

	if ((aaa=quicktime_stack_seek(riff,chunk,0LL)) != 0LL) {
		fprintf(fp,"[error: riff stack seek to 0 result %lld]\n",(long long)aaa);
		return;
	}

	rd = (int)quicktime_stack_read(riff,chunk,tmp,sizeof(tmp));
	for (i=0;i < ((rd+15)&(~15));) {
		if (col == 0) fprintf(fp,"        ");

		col++;
		if (i < rd)	fprintf(fp,"%02X ",tmp[i]);
		else		fprintf(fp,"   ");
		i++;

		if (col >= 16 || i == ((rd+15)&(~15))) {
			for (col=0;col < 16;col++) {
				o = i+col-16;
				if (o >= rd)
					fwrite(" ",1,1,fp);
				else if (tmp[o] >= 32 && tmp[o] < 127)
					fwrite(tmp+o,1,1,fp);
				else
					fwrite(".",1,1,fp);
			}

			col = 0;
			fprintf(fp,"\n");
		}
	}

	quicktime_stack_seek(riff,chunk,opos);
}

/* [doc] quicktime_stack_pstring_to_cstring
 *
 * Convert Apple Macintosh Pascal-style string to C-string. Use for parsing
 * some fields of the QuickTime format.
 *
 * Parameters:
 *
 *     dst = char[] buffer to place c-string into
 *     dstmax = buffer size
 *     src = source buffer containing Pascal-style string
 *     len = max length of the source string
 *
 */
int quicktime_stack_pstring_to_cstring(char *dst,size_t dstmax,const unsigned char *src,size_t len) {
	size_t l,c;
	if ((ssize_t)len <= 1 || (ssize_t)dstmax <= 1) return 0;
	l = (size_t)((unsigned char)src[0]);
	c = l;
	if (c > (len-1)) c = len-1;
	if (c > (size_t)(dstmax-1)) c = (size_t)(dstmax-1);
	if (c > 0) memcpy(dst,src+1,c);
	dst[c] = 0;
	return (int)l;
}

/* [doc] quicktime_stack_decompress_cmvd_zlib
 *
 * Given a Quicktime stack and the cmvd atom, run the contents through the
 * zlib library to decompress the movie header, and return decompressed
 * contents.
 *
 * Warning:
 *
 *     You are expected to verify first that the cmvd atom is zlib compressed
 *     and to obtain the decompressed size from the first 4 bytes.
 *
 * Parameters:
 *
 *     qt = stack
 *     atom = cmvd atom containing compressed header atom
 *     dst_len = length of decompressed data (you should have read this from the first 4 bytes)
 *
 */
void *quicktime_stack_decompress_cmvd_zlib(quicktime_stack *qt,quicktime_atom *atom,size_t dst_len) {
	size_t src_len = (size_t)(atom->data_length - 4LL);
	z_stream z={0};
	void *src,*dst;
	int ret;

	if (quicktime_stack_seek(qt,atom,4LL) != 4LL)
		return NULL;

	if ((src = malloc(src_len)) == NULL)
		return NULL;
	if ((dst = malloc(dst_len)) == NULL) {
		free(src);
		return NULL;
	}
	if (quicktime_stack_read(qt,atom,src,src_len) != (int)src_len) {
		free(src);
		free(dst);
		return NULL;
	}

	z.next_in = src;
	z.avail_in = (uInt)src_len;
	z.next_out = dst;
	z.avail_out = (uInt)dst_len;
	if (inflateInit2(&z,15) != Z_OK) {
		free(src);
		free(dst);
		return NULL;
	}

	ret = inflate(&z,Z_FULL_FLUSH);
	if (ret != Z_STREAM_END) {
		fprintf(stderr,"[ERROR: zlib failed to fully decompress cmvd, err %d]\n",ret);
		inflateEnd(&z);
		free(src);
		free(dst);
		return NULL;
	}

	if (z.avail_in > 0)
		fprintf(stderr,"[WARNING: zlib leftover %u src bytes]\n",z.avail_in);
	if (z.avail_out > 0)
		fprintf(stderr,"[WARNING: zlib decoded %u dst bytes than expected]\n",z.avail_out);

	inflateEnd(&z);
	free(src);
	return dst;
}

/* [doc] quicktime_stack_decompress_cmvd
 *
 * Given a Quicktime stack and the cmvd atom, decompress and return the
 * contents.
 *
 * Parameters:
 *
 *     qt = stack
 *     atom = cmvd atom containing compressed header atom
 *     dcom_type = decompression type (read from another atom)
 *     uncom_size = pointer to size_t where the uncompressed size will be stored
 *
 */
void *quicktime_stack_decompress_cmvd(quicktime_stack *qt,quicktime_atom *atom,quicktime_atom_type_t dcom_type,size_t *uncom_size) {
	size_t dst_len;
	uint32_t dw;

	if (quicktime_stack_seek(qt,atom,0LL) != 0LL)
		return NULL;
	if (quicktime_stack_read(qt,atom,&dw,4) != 4)
		return NULL;
	if (atom->data_length <= 4LL || atom->data_length > (8*1024*1024))
		return NULL;

	dst_len = (size_t)__be_u32(&dw);
	if (dst_len < 8 || dst_len > (8*1024*1024))
		return NULL;

	if (uncom_size != NULL)
		*uncom_size = dst_len;

	if (dcom_type == quicktime_type_const('z','l','i','b'))
		return quicktime_stack_decompress_cmvd_zlib(qt,atom,dst_len);

	return NULL;
}

/* [doc] quicktime_stack_decompress_cmvd_free
 *
 * Free the buffer returned by quicktime_stack_decompress_cmvd
 *
 * Parameters:
 *
 *     buf = buffer returned by decompress_cmvd
 *
 */
void quicktime_stack_decompressed_cmvd_free(void *buf) {
	if (buf != NULL) free(buf);
}

/* [doc] quicktime_cmvd_copy_destroy
 *
 * Destroy and free resources of a cmvd_copy object
 *
 * Parameters:
 *
 *     c = cmvd copy object to free
 *
 */
quicktime_cmvd_copy *quicktime_cmvd_copy_destroy(quicktime_cmvd_copy *c) {
	if (c) {
		if (c->stack) c->stack = quicktime_stack_destroy(c->stack);
		if (c->buffer) quicktime_stack_decompressed_cmvd_free(c->buffer);
		c->buffer_len = 0UL;
		c->buffer = NULL;
		free(c);
	}
	return NULL;
}

/* [doc] quicktime_stack_decompress_and_get_cmvd
 *
 * Given a cmvd atom and data compression type, allocate a cmvd copy object
 * decompress the data, and return the cmvd object.
 *
 * Parameters:
 *
 *     s = quicktime stack
 *     a = cmvd atom
 *     dcom_type = compression type
 *
 */
quicktime_cmvd_copy *quicktime_stack_decompress_and_get_cmvd(quicktime_stack *s,quicktime_atom *a,quicktime_atom_type_t dcom_type) {
	quicktime_cmvd_copy *cc = (quicktime_cmvd_copy*)malloc(sizeof(quicktime_cmvd_copy));
	if (cc == NULL) return NULL;

	cc->buffer = quicktime_stack_decompress_cmvd(s,a,dcom_type,&cc->buffer_len);
	if (cc->buffer == NULL) return quicktime_cmvd_copy_destroy(cc);

	cc->stack = quicktime_stack_create(0);
	if (cc->stack == NULL) return quicktime_cmvd_copy_destroy(cc);

	quicktime_stack_empty(cc->stack);
	quicktime_stack_assign_mem(cc->stack,cc->buffer,cc->buffer_len);
	return cc;
}


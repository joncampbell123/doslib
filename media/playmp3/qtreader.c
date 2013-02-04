/* TODO: Just my luck, right after I implemented this code, Apple updates the QuickTime standard
 *       so that new MOV/MP4 files use the "version 2 rev 0" audio structure, which this code does
 *       not read. So if you encounter really new MP4 files or transcode to MOV/MP4 using the latest
 *       version of ffmpeg (ffmpeg 0.9) this code will not be able to read it */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

#include "qtreader.h"

static quicktime_atom zero_atom = {0};

static void debug_echo_atom(quicktime_reader *q,int level,quicktime_atom *a) {
	if (q->echo_atoms) {
		fprintf(stderr,"   [quicktime_reader %p] ATOM: ",(void*)q);
		quicktime_stack_debug_print_atom(stderr,level,a);
	}
}

static void debug(quicktime_reader *q,const char *message,...) {
	if (q->echo_debug) {
		va_list va;
		va_start(va,message);
		fprintf(stderr,"   [quicktime_reader %p] debug: ",(void*)q);
		vfprintf(stderr,message,va);
		fprintf(stderr,"\n");
		va_end(va);
	}
}

static void warning(quicktime_reader *q,const char *message,...) {
	if (q->echo_warning) {
		va_list va;
		va_start(va,message);
		fprintf(stderr,"   [quicktime_reader %p] WARNING: ",(void*)q);
		vfprintf(stderr,message,va);
		fprintf(stderr,"\n");
		va_end(va);
	}
}

static void error(quicktime_reader *q,const char *message,...) {
	if (q->echo_error) {
		va_list va;
		va_start(va,message);
		fprintf(stderr,"   [quicktime_reader %p] ERROR: ",(void*)q);
		vfprintf(stderr,message,va);
		fprintf(stderr,"\n");
		va_end(va);
	}
}

/* [doc] quicktime_reader_shut_up
 *
 * clear all echo flags to prevent the quicktime reaader from emitting any
 * messages to stdout
 *
 * Parameters:
 *
 *     qr = quicktime reader object
 *
 */
void quicktime_reader_shut_up(quicktime_reader *qr) {
	qr->echo_atoms = 0;
	qr->echo_error = 0;
	qr->echo_debug = 0;
	qr->echo_warning = 0;
}

/* [doc] quicktime_reader_be_default
 *
 * set all echo flags to allow all warnings and errors to be printed to
 * STDOUT. debug messages and atom structure printouts are cleared.
 *
 * Parameters:
 *
 *     qr = quicktime reader object
 *
 */
void quicktime_reader_echo_default(quicktime_reader *qr) {
	qr->echo_atoms = 0;
	qr->echo_error = 1;
	qr->echo_debug = 0;
	qr->echo_warning = 1;
}

/* [doc] quicktime_reader_be_verbose
 *
 * set all echo flags to allow all informative messages to be printed to
 * STDOUT, for debugging purposes
 *
 * Parameters:
 *
 *     qr = quicktime reader object
 *
 */
void quicktime_reader_be_verbose(quicktime_reader *qr) {
	qr->echo_atoms = 1;
	qr->echo_error = 1;
	qr->echo_debug = 1;
	qr->echo_warning = 1;
}

/* [doc] quicktime_reader_create
 *
 * Create a quicktime reader
 *
 * Return value:
 *
 *     quicktime reader, or NULL
 *
 */
quicktime_reader *quicktime_reader_create() {
	quicktime_reader *qr = (quicktime_reader*)malloc(sizeof(quicktime_reader));
	if (qr == NULL) return NULL;
	memset(qr,0,sizeof(*qr));
	quicktime_reader_echo_default(qr);
	return qr;
}

/* [doc] quicktime_reader_track_get_chunk_offset
 *
 * Retrieve file offset of chunk 'chunkofs' for quicktime reader track 't'
 *
 * Parameters:
 *
 *     t = quicktime reader track object
 *     chunkofs = chunk number to look up
 *
 * Return value:
 *
 *     >= 0 file offset
 *     -1 error, or no such chunk
 *
 */
int64_t quicktime_reader_track_get_chunk_offset(quicktime_reader_track *t,unsigned long chunkofs) {
	if (t == NULL) return -1LL;
	if (chunkofs-- == 0UL) return -1LL; /* NTS: chunk references are 1-based, not 0-based */
	if (t->chunk.list == NULL) return -1LL;
	if (chunkofs >= t->chunk.total) return -1LL;
	return (int64_t)(t->chunk.list[chunkofs]);
}

/* [doc] quicktime_reader_track_free
 *
 * Free structures associated with a quicktime reader track
 *
 * Parameters:
 *
 *     t = quicktime reader track object
 *
 */
void quicktime_reader_track_free(quicktime_reader_track *t) {
	if (t->chunk.list) free(t->chunk.list);
	t->chunk.list = NULL;
	t->chunk.total = 0;

	if (t->syncsample.list) free(t->syncsample.list);
	t->syncsample.entries = 0;
	t->syncsample.list = NULL;

	if (t->samplesize.list) free(t->samplesize.list);
	t->samplesize.list = NULL;
	t->samplesize.entries = 0;

	if (t->sampletochunk.list) free(t->sampletochunk.list);
	t->sampletochunk.list = NULL;
	t->sampletochunk.entries = 0;

	if (t->stsd_content) free(t->stsd_content);
	t->stsd_content_length = 0;
	t->stsd_content = NULL;

	if (t->timetosample.list) free(t->timetosample.list);
	t->timetosample.list = NULL;
	t->timetosample.entries = 0;
}

/* [doc] quicktime_reader_destroy_tracks
 *
 * Free all tracks associated with a quicktime reader
 *
 * Parameters:
 *
 *     qt = quicktime reader
 *
 */
void quicktime_reader_destroy_tracks(quicktime_reader *qr) {
	int i;

	if (qr->track) {
		for (i=0;i < qr->max_track;i++) quicktime_reader_track_free(qr->track+i);
		free(qr->track);
		qr->track = NULL;
	}

	qr->alloc_track = 0;
	qr->max_track = 0;
}

/* [doc] quicktime_reader_alloc_tracks
 *
 * Allocate track objects for the quicktime reader. Note that allocated tracks
 * do not necessarily correspond to actual tracks in the file.
 *
 * This function is internal and should not be relied on or used by external
 * code.
 *
 * Parameters:
 *
 *     qt = quicktime reader
 *     howmany = how many tracks to allocate
 *
 * Return value:
 *
 *     0 = unable to fullfill request
 *     1 = success
 *
 * Warnings:
 *
 *     In current versions of the code, the code to realloc() the array is
 *     missing, therefore if the first call was for a low number and the
 *     subsequent calls require more, the function will fail.
 *
 */
int quicktime_reader_alloc_tracks(quicktime_reader *qr,int howmany) {
	if (qr == NULL) return 0;
	if (howmany < 4) howmany = 4;
	if (qr->track != NULL) {
		if (howmany > qr->alloc_track)
			return 0; /* TODO: realloc() */
	}
	else {
		qr->track = (quicktime_reader_track*)malloc(sizeof(quicktime_reader_track) * howmany);
		if (qr->track == NULL) return 0;
		qr->max_track = 0;
		qr->alloc_track = howmany;
	}

	return 1;
}

/* [doc] quicktime_reader_destroy
 *
 * Free resources and structure of a quicktime reader
 *
 * Parameters:
 *
 *     qr = quicktime reader
 *
 * Return value:
 *
 *     NULL
 *
 */
quicktime_reader *quicktime_reader_destroy(quicktime_reader *qr) {
	if (qr) {
		if (qr->track) quicktime_reader_destroy_tracks(qr);
		if (qr->cmvd) qr->cmvd = quicktime_cmvd_copy_destroy(qr->cmvd);
		if (qr->qt && qr->qt_own) quicktime_stack_destroy(qr->qt);
		qr->qt = NULL;
		free(qr);
	}
	return NULL;
}

/* [doc] quicktime_reader_reset
 *
 * Reset quicktime reader state
 *
 * Parameters:
 *
 *     qr = quicktime reader
 *
 */
void quicktime_reader_reset(quicktime_reader *qr) {
	if (qr) {
		if (qr->track) quicktime_reader_destroy_tracks(qr);
		if (qr->cmvd) qr->cmvd = quicktime_cmvd_copy_destroy(qr->cmvd);
		if (qr->qt && qr->qt_own) quicktime_stack_destroy(qr->qt);
		qr->qt = NULL;
	}
}

/* [doc] quicktime_reader_assign_stack
 *
 * Associate a quicktime stack with a reader.
 *
 * This function is internal and should not be used externally.
 *
 * Parameters:
 *
 *     qr = quicktime reader
 *     s = stack to associate
 *
 */
int quicktime_reader_assign_stack(quicktime_reader *qr,quicktime_stack *s) {
	if (qr == NULL) return 0;
	if (qr->qt != NULL) return 0;
	qr->qt = s;
	qr->qt_own = 0;
	return 1;
}

/* [doc] quicktime_reader_assign_fd
 *
 * Associate the reader with a file descriptor. The function will fail if the
 * quicktime reader already has a stack associated with it.
 *
 * Parameters:
 *
 *     qr = quicktime reader
 *     fd = file descriptor
 *
 * Return value:
 *
 *     0 = failure
 *     1 = success
 *
 */
int quicktime_reader_assign_fd(quicktime_reader *qr,int fd) {
	if (qr == NULL) return 0;
	if (qr->qt != NULL) return 0;

	qr->qt = quicktime_stack_create(0);
	if (qr->qt == NULL) return 0;
	qr->qt_own = 1;

	quicktime_stack_empty(qr->qt);
	if (!quicktime_stack_assign_fd(qr->qt,fd)) {
		quicktime_reader_reset(qr);
		return 0;
	}

	return 1;
}

/* [doc] quicktime_reader_parse
 *
 * Parse the general structure of the QuickTime format and populate state
 * information in the reader to enable further parsing.
 *
 * This includes reading the QuickTime 'moov' chunk, including the case
 * where it is compressed inside a 'cmov' chunk. The function also sets up
 * tables on the caller's behalf to facilitate quick chunk to sample to
 * file offset lookups.
 *
 * Parameters:
 *
 *     qr = quicktime reader
 *
 * Return value:
 *
 *     0 = failure
 *     1 = success
 *
 */
int quicktime_reader_parse(quicktime_reader *qr) {
	quicktime_atom atom;
	int count,track;
	uint32_t dw;
	uint64_t qw;

	quicktime_reader_destroy_tracks(qr);
	if (qr->cmvd) qr->cmvd = quicktime_cmvd_copy_destroy(qr->cmvd);
	if (!quicktime_reader_alloc_tracks(qr,64)) return 0;
	qr->r_moov.atom = qr->r_moov.udta = qr->r_moov.mvhd = qr->mdat = qr->moov = qr->cmov = zero_atom;
	memset(&qr->mvhd_header,0,sizeof(qr->mvhd_header));
	qr->r_moov.qt = NULL;	/* NTS: qt = assigned to our stack, or compressed cmvd stack */
	quicktime_stack_empty(qr->qt);

	debug(qr,"Initial scan");
	while (!quicktime_stack_eof(qr->qt)) {
		if (!quicktime_stack_read_atom(qr->qt,quicktime_stack_top(qr->qt),&atom)) {
			quicktime_stack_pop(qr->qt);
		}
		else {
			debug_echo_atom(qr,qr->qt->current+1,&atom);

			if (atom.type == quicktime_type_const('m','d','a','t'))
				qr->mdat = atom;
			else if (atom.type == quicktime_type_const('m','o','o','v')) {
				qr->moov = atom;

				/* step inside and look for a 'cmov' */
				/* NTS: a 'cmov' QuickTime file typically has 'cmov' and only 'cmov' inside 'moov',
				 *      so if we're reading a lot of chunks it's very likely there is no cmov
				 *      compressed QuickTime header */
				count = 0;
				quicktime_stack_push(qr->qt,&qr->moov);
				while (++count < 10 && quicktime_stack_read_atom(qr->qt,quicktime_stack_top(qr->qt),&atom)) {
					if (atom.type == quicktime_type_const('c','m','o','v'))
						qr->cmov = atom;
				}
				quicktime_stack_pop(qr->qt);
			}
		}
	}

	if (qr->mdat.absolute_data_offset == 0LL) {
		error(qr,"missing mdat");
		return 0;
	}
	if (qr->moov.absolute_data_offset == 0LL) {
		error(qr,"missing moov");
		return 0;
	}

	/* now read the 'moov' */
	/* exactly how depends on whether the 'moov' atom contains an actual description,
	 * or a 'cmov' compressed copy of the header */
	if (qr->cmov.absolute_data_offset != 0LL) {
		quicktime_atom_type_t cmvd_dcom=0;
		quicktime_atom cmvd_atom={0};
		uint32_t dw;

		debug(qr,"Compressed header, preparing to decompress");
		quicktime_stack_seek(qr->qt,&qr->cmov,0LL);
		while (quicktime_stack_read_atom(qr->qt,&qr->cmov,&atom)) {
			debug_echo_atom(qr,qr->qt->current+1,&atom);

			if (atom.type == quicktime_type_const('d','c','o','m')) {
				if (quicktime_stack_read(qr->qt,&atom,&dw,4) == 4)
					cmvd_dcom = __be_u32(&dw);
			}
			else if (atom.type == quicktime_type_const('c','m','v','d')) {
				cmvd_atom = atom;
			}
		}

		qr->cmvd = quicktime_stack_decompress_and_get_cmvd(qr->qt,&cmvd_atom,cmvd_dcom);
		if (qr->cmvd == NULL) {
			error(qr,"Unable to read+decompress header");
			return 0;
		}

		qr->r_moov.qt = qr->cmvd->stack;
		/* pick out the 'moov' chunk for the header scan code below */
		while (quicktime_stack_read_atom(qr->r_moov.qt,NULL,&atom)) {
			debug_echo_atom(qr,qr->r_moov.qt->current+1,&atom);

			if (atom.type == quicktime_type_const('m','o','o','v'))
				qr->r_moov.atom = atom;
		}

		if (qr->r_moov.atom.absolute_data_offset == 0LL) {
			error(qr,"Within compressed header, moov chunk not found");
			return 0;
		}
	}
	else {
		qr->r_moov.atom = qr->moov;
		qr->r_moov.qt = qr->qt;
	}

	/* scan the header for information */
	debug(qr,"Reading header");
	quicktime_stack_empty(qr->r_moov.qt);
	quicktime_stack_push(qr->r_moov.qt,&qr->r_moov.atom);
	while (quicktime_stack_read_atom(qr->r_moov.qt,quicktime_stack_top(qr->r_moov.qt),&atom)) {
		debug_echo_atom(qr,qr->r_moov.qt->current+1,&atom);

		if (atom.type == quicktime_type_const('t','r','a','k')) {
			if (qr->max_track < qr->alloc_track) {
				quicktime_reader_track *trk = qr->track+(qr->max_track++);
				memset(trk,0,sizeof(*trk));
				trk->main = atom;
			}
		}
		else if (atom.type == quicktime_type_const('m','v','h','d')) {
			qr->r_moov.mvhd = atom;
		}
		else if (atom.type == quicktime_type_const('u','d','t','a')) {
			qr->r_moov.udta = atom;
		}
	}

	if (qr->r_moov.mvhd.absolute_data_offset == 0LL) {
		error(qr,"mvhd chunk in header not found");
		return 0;
	}

	debug(qr,"Reading mvhd");
	quicktime_stack_push(qr->r_moov.qt,&qr->r_moov.mvhd);
	quicktime_stack_read(qr->r_moov.qt,quicktime_stack_top(qr->r_moov.qt),&qr->mvhd_header,sizeof(qr->mvhd_header));
	quicktime_stack_empty(qr->r_moov.qt);

	for (track=0;track < qr->max_track;track++) {
		quicktime_reader_track *trk = qr->track+track;

		debug(qr,"Reading trak[%u]",track);
		quicktime_stack_empty(qr->r_moov.qt);
		quicktime_stack_push(qr->r_moov.qt,&trk->main);
		while (quicktime_stack_read_atom(qr->r_moov.qt,quicktime_stack_top(qr->r_moov.qt),&atom)) {
			debug_echo_atom(qr,qr->r_moov.qt->current+1,&atom);

			if (atom.type == quicktime_type_const('t','k','h','d')) {
				trk->tkhd = atom;
				quicktime_stack_read(qr->r_moov.qt,&atom,&trk->tkhd_header,sizeof(trk->tkhd_header));
			}
			else if (atom.type == quicktime_type_const('m','d','i','a')) {
				trk->mdia = atom;
				quicktime_stack_push(qr->r_moov.qt,&atom);
				while (quicktime_stack_read_atom(qr->r_moov.qt,quicktime_stack_top(qr->r_moov.qt),&atom)) {
					debug_echo_atom(qr,qr->r_moov.qt->current+1,&atom);

					if (atom.type == quicktime_type_const('m','d','h','d')) {
						trk->mdhd = atom;
						quicktime_stack_read(qr->r_moov.qt,&atom,&trk->mdhd_header,sizeof(trk->mdhd_header));
					}
					else if (atom.type == quicktime_type_const('h','d','l','r')) {
						quicktime_hdlr_header h={0};
						quicktime_stack_read(qr->r_moov.qt,&atom,&h,sizeof(h));
						/* we're looking for the "media handler" */
						if (__be_u32(&h.component_type) == quicktime_type_const('m','h','l','r'))
							trk->mhlr_header = h;
						else if (__be_u32(&h.component_type) == quicktime_type_const(0,0,0,0))
							trk->mhlr_header = h;
					}
					else if (atom.type == quicktime_type_const('m','i','n','f')) {
						trk->minf = atom;
						quicktime_stack_push(qr->r_moov.qt,&atom);
						while (quicktime_stack_read_atom(qr->r_moov.qt,quicktime_stack_top(qr->r_moov.qt),&atom)) {
							debug_echo_atom(qr,qr->r_moov.qt->current+1,&atom);

							if (atom.type == quicktime_type_const('s','t','b','l')) {
								trk->stbl = atom;
								quicktime_stack_push(qr->r_moov.qt,&atom);
								while (quicktime_stack_read_atom(qr->r_moov.qt,quicktime_stack_top(qr->r_moov.qt),&atom)) {
									debug_echo_atom(qr,qr->r_moov.qt->current+1,&atom);

									if (atom.type == quicktime_type_const('s','t','s','d'))
										trk->stsd = atom;
									else if (atom.type == quicktime_type_const('s','t','t','s'))
										trk->stts = atom;
									else if (atom.type == quicktime_type_const('s','t','s','s'))
										trk->stss = atom;
									else if (atom.type == quicktime_type_const('s','t','s','c'))
										trk->stsc = atom;
									else if (atom.type == quicktime_type_const('s','t','s','z'))
										trk->stsz = atom;
									else if (atom.type == quicktime_type_const('s','t','c','o'))
										trk->stco = atom;
									else if (atom.type == quicktime_type_const('c','o','6','4'))
										trk->co64 = atom;
								}
								quicktime_stack_pop(qr->r_moov.qt);
							}
						}
						quicktime_stack_pop(qr->r_moov.qt);
					}
				}
				quicktime_stack_pop(qr->r_moov.qt);
			}
			else if (atom.type == quicktime_type_const('u','d','t','a')) {
				trk->udta = atom;
			}
			else if (atom.type == quicktime_type_const('e','d','t','s')) {
				trk->edts = atom;
			}
		}
	}

	for (track=0;track < qr->max_track;track++) {
		quicktime_reader_track *trk = qr->track+track;

		if (trk->co64.absolute_data_offset != 0LL) { /* 64-bit table > 32-bit table */
			quicktime_stco_header h;
			unsigned long ii;

			atom = trk->co64;
			debug(qr,"Loading track %u chunk table (64-bit)",track);
			quicktime_stack_seek(qr->r_moov.qt,&atom,0LL);
			if (quicktime_stack_read(qr->r_moov.qt,&atom,&h,sizeof(h)) != (int)sizeof(h)) continue;
			trk->chunk.total = __be_u32(&h.number_of_entries);
			debug(qr,"    total chunks = %lu",(unsigned long)trk->chunk.total);
			if (trk->chunk.total > ((128 * 1024 * 1024)/sizeof(uint64_t))) {
				error(qr,"    %lu chunks is WAY too much for me to handle!\n",trk->chunk.total);
				continue;
			}
			else if (trk->chunk.total == 0) {
				warning(qr,"    no chunks in track %u",track);
				continue;
			}

			trk->chunk.list = (uint64_t*)malloc(sizeof(uint64_t) * trk->chunk.total);
			if (trk->chunk.list == NULL) {
				error(qr,"    Not enough memory to allocate chunklist for track %u",track);
				continue;
			}

			/* NTS: remember that for whatever reason chunks are counted from 1, not 0
			 *      as evident by how other structures refer to these chunks */
			for (ii=0;ii < trk->chunk.total;ii++) {
				qw = 0ULL;
				if (quicktime_stack_read(qr->r_moov.qt,&atom,&qw,8) != 8) break;
				trk->chunk.list[ii] = __be_u64(&qw);
			}
		}
		else if (trk->stco.absolute_data_offset != 0LL) {
			quicktime_stco_header h;
			unsigned long ii;

			atom = trk->stco;
			debug(qr,"Loading track %u chunk table (32-bit)",track);
			quicktime_stack_seek(qr->r_moov.qt,&atom,0LL);
			if (quicktime_stack_read(qr->r_moov.qt,&atom,&h,sizeof(h)) != (int)sizeof(h)) continue;
			trk->chunk.total = __be_u32(&h.number_of_entries);
			debug(qr,"    total chunks = %lu",(unsigned long)trk->chunk.total);
			if (trk->chunk.total > ((128 * 1024 * 1024)/sizeof(uint64_t))) {
				error(qr,"    %lu chunks is WAY too much for me to handle!\n",trk->chunk.total);
				continue;
			}
			else if (trk->chunk.total == 0) {
				warning(qr,"    no chunks in track %u",track);
				continue;
			}

			trk->chunk.list = (uint64_t*)malloc(sizeof(uint64_t) * trk->chunk.total);
			if (trk->chunk.list == NULL) {
				error(qr,"    Not enough memory to allocate chunklist for track %u",track);
				continue;
			}

			/* NTS: remember that for whatever reason chunks are counted from 1, not 0
			 *      as evident by how other structures refer to these chunks */
			for (ii=0;ii < trk->chunk.total;ii++) {
				dw = 0;
				if (quicktime_stack_read(qr->r_moov.qt,&atom,&dw,4) != 4) break;
				trk->chunk.list[ii] = (uint64_t)__be_u32(&dw);
			}
		}
		else {
			warning(qr,"Track %u has no chunk offset table",track);
		}
	}

	for (track=0;track < qr->max_track;track++) {
		quicktime_reader_track *trk = qr->track+track;

		if (trk->stsz.absolute_data_offset != 0LL) {
			quicktime_stsz_header h;
			unsigned long ii;

			debug(qr,"Loading track %u sample sizes",track);

			atom = trk->stsz;
			quicktime_stack_seek(qr->r_moov.qt,&atom,0LL);
			if (quicktime_stack_read(qr->r_moov.qt,&atom,&h,sizeof(h)) != (int)sizeof(h)) continue;
			trk->samplesize.entries = __be_u32(&h.number_of_entries);
			trk->samplesize.constval = __be_u32(&h.sample_size);

			if (trk->samplesize.constval != 0UL)
				trk->samplesize.list = NULL;
			else if (trk->samplesize.entries > ((128 * 1024 * 1024)/sizeof(uint32_t)))
				error(qr,"    %lu sample sizes is way too much for me\n",trk->samplesize.entries);
			else if (trk->samplesize.entries != 0UL) {
				trk->samplesize.list = (uint32_t*)malloc(sizeof(uint32_t) * trk->samplesize.entries);
				if (trk->samplesize.list == NULL)
					error(qr,"   Not enough memory to allocate samplesizelist for track %u",track);
				else {
					for (ii=0;ii < (unsigned long)trk->samplesize.entries;ii++) {
						dw = 0;
						if (quicktime_stack_read(qr->r_moov.qt,&atom,&dw,4) != 4) break;
						trk->samplesize.list[ii] = (uint64_t)__be_u32(&dw);
					}
				}
			}
		}
		else {
			warning(qr,"Track %u has no sample size table",track);
		}
	}

	for (track=0;track < qr->max_track;track++) {
		quicktime_reader_track *trk = qr->track+track;

		if (trk->stsc.absolute_data_offset != 0LL) {
			quicktime_stsc_header h;
			quicktime_stsc_entry e;
			unsigned long ii,ents;
			unsigned long ssum;
			unsigned int wen;

			debug(qr,"Loading track %u sample to chunk table",track);

			atom = trk->stsc;
			quicktime_stack_seek(qr->r_moov.qt,&atom,0LL);
			if (quicktime_stack_read(qr->r_moov.qt,&atom,&h,sizeof(h)) != (int)sizeof(h)) continue;
			ents = __be_u32(&h.number_of_entries);

			if (trk->sampletochunk.entries > trk->chunk.total) {
				warning(qr,"    Odd, %u stsc entries > %u chunk offsets",trk->sampletochunk.entries,trk->chunk.total);
				trk->sampletochunk.entries = trk->chunk.total;
			}

			trk->sampletochunk.entries = trk->chunk.total;
			if (trk->sampletochunk.entries > ((128 * 1024 * 1024)/sizeof(uint32_t)))
				error(qr,"    %lu sample sizes is way too much for me\n",trk->sampletochunk.entries);
			else if (trk->sampletochunk.entries != 0UL) {
				trk->sampletochunk.list = (quicktime_reader_stsc_entry*)malloc(sizeof(quicktime_reader_stsc_entry) * trk->sampletochunk.entries);
				if (trk->sampletochunk.list == NULL)
					error(qr,"   Not enough memory to allocate samplesizelist for track %u",track);
				else {
					for (wen=0,ii=0;wen < trk->sampletochunk.entries;ii++) {
						unsigned long first_chunk;

						memset(&e,0,sizeof(e));
						if (quicktime_stack_read(qr->r_moov.qt,&atom,&e,sizeof(e)) != (int)sizeof(e)) break;
						first_chunk = __be_u32(&e.first_chunk);

						if (first_chunk-- == 0) { /* NTS: chunks are 1-based */
							warning(qr,"    sttc entry %u has zero chunk ref",ii);
							continue;
						}
						else if (first_chunk < wen) {
							warning(qr,"    sttc entry %u first_chunk %u is <= current chunk %u",ii,first_chunk,wen);
							continue;
						}
						else if (first_chunk >= trk->sampletochunk.entries) {
							warning(qr,"    sttc entry %u first_chunk %u out of bounds >= %u",ii,first_chunk,trk->sampletochunk.entries);
							continue;
						}
						else if (first_chunk != 0 && wen == 0) {
							warning(qr,"    sttc entry %u first entry has first_chunk %u != 1, not parsing table",ii,first_chunk);
							break;
						}

						if (wen != 0) {
							while (wen < first_chunk) { /* repeat-fill last entry to current */
								quicktime_reader_stsc_entry *pe = trk->sampletochunk.list + (wen-1);
								quicktime_reader_stsc_entry *ne = trk->sampletochunk.list + wen;
								*ne = *pe;
								wen++;
							}
						}

						{ /* fill in current entry */
							quicktime_reader_stsc_entry *ne = trk->sampletochunk.list + wen;
							ne->samples_per_chunk = __be_u32(&e.samples_per_chunk);
							ne->sample_description_id = __be_u32(&e.id);
							wen++;
						}
					}

					if (wen != 0) {
						while (wen < trk->sampletochunk.entries) {
							quicktime_reader_stsc_entry *pe = trk->sampletochunk.list + (wen-1);
							quicktime_reader_stsc_entry *ne = trk->sampletochunk.list + wen;
							*ne = *pe;
							wen++;
						}
					}

					for (wen=0,ssum=0;wen < trk->sampletochunk.entries;wen++) {
						quicktime_reader_stsc_entry *ne = trk->sampletochunk.list + wen;
						ne->first_sample = ssum;
						ssum += ne->samples_per_chunk;
					}

					debug(qr,"Map refers to %lu samples total across %lu chunks",
						ssum,trk->sampletochunk.entries);
				}
			}
		}
		else {
			warning(qr,"Track %u has no sample size table",track);
		}
	}

	for (track=0;track < qr->max_track;track++) {
		quicktime_reader_track *trk = qr->track+track;
		unsigned long ent;
		uint32_t dw;

		if (trk->stss.absolute_data_offset != 0LL) {
			debug(qr,"Loading track %u sync sample table",track);

			atom = trk->stss;
			quicktime_stack_seek(qr->r_moov.qt,&atom,0);
			if (atom.data_length < (1*1024*1024) && atom.data_length >= 12) {
				quicktime_stss_header h;
				quicktime_stack_read(qr->r_moov.qt,&atom,&h,sizeof(h));
				trk->syncsample.entries = __be_u32(&h.number_of_entries);
				if (trk->syncsample.entries != 0 && trk->syncsample.entries < (1*1024*1024)) {
					trk->syncsample.list = (uint32_t*)malloc(sizeof(uint32_t) * trk->syncsample.entries);
					if (trk->syncsample.list == NULL) {
						error(qr,"Track %u cannot alloc mem for sync sample table");
					}
					else {
						for (ent=0;ent < trk->syncsample.entries;ent++) {
							if (quicktime_stack_read(qr->r_moov.qt,&atom,&dw,4) != 4)
								break;

							/* NTS: Apple doesn't mention this at all, but the sample numbers are 1-based */
							trk->syncsample.list[ent] = __be_u32(&dw) - 1;
						}
						while (ent < trk->syncsample.entries)
							trk->syncsample.list[ent++] = 0;
					}
				}
			}
		}
	}

	for (track=0;track < qr->max_track;track++) {
		quicktime_reader_track *trk = qr->track+track;

		trk->stsd_content = NULL;
		trk->stsd_content_length = 0;
		memset(&trk->stsd_header,0,sizeof(trk->stsd_header));
		if (trk->stsc.absolute_data_offset != 0LL) {
			debug(qr,"Loading track %u sample description",track);

			atom = trk->stsd;
			if (atom.data_length < 65536 && atom.data_length >= 12) {
				trk->stsd_content_length = (size_t)atom.data_length;
				trk->stsd_content = malloc(trk->stsd_content_length);
				if (trk->stsd_content != NULL) {
					quicktime_stsd_header *h = (quicktime_stsd_header*)(trk->stsd_content);
					uint8_t *ptr = (uint8_t*)trk->stsd_content + sizeof(*h);
					uint8_t *fence = (uint8_t*)trk->stsd_content + trk->stsd_content_length;
					unsigned long entry;

					memset(trk->stsd_content,0,trk->stsd_content_length);
					quicktime_stack_seek(qr->r_moov.qt,&atom,0LL);
					quicktime_stack_read(qr->r_moov.qt,&atom,trk->stsd_content,trk->stsd_content_length);

					trk->stsd_header.version = h->version;
					memcpy(trk->stsd_header.flags,h->flags,sizeof(h->flags));
					trk->stsd_header.number_of_entries = __be_u32(&h->number_of_entries);
					debug(qr,"stsd track %u, %u entries",track,trk->stsd_header.number_of_entries);

					if (trk->stsd_header.number_of_entries > MAX_QT_STSD_ENTRIES) {
						trk->stsd_header.number_of_entries = MAX_QT_STSD_ENTRIES;
						warning(qr,"stsd track %u, too many entries",track);
					}

					for (entry=0;entry < trk->stsd_header.number_of_entries;entry++) {
						quicktime_stsd_entry_general *en;

						trk->stsd_entry[entry] = NULL;
						if ((ptr+sizeof(quicktime_stsd_entry_general)) > fence) {
							warning(qr,"Track %u, stsd entry %u is past end of chunk",track,entry);
							break;
						}

						en = (quicktime_stsd_entry_general*)ptr;
						ptr += __be_u32(&en->size);
						if (ptr > fence) {
							warning(qr,"Track %u, stsd entry %u extends past end of chunk",track,entry);
							break;
						}

						trk->stsd_entry[entry] = en;
					}

					while (entry < trk->stsd_header.number_of_entries)
						trk->stsd_entry[entry++] = NULL;
				}
				else {
					error(qr,"Cannot allocate memory for track %u stsd",track);
				}
			}
		}
		else {
			warning(qr,"Track %u has no sample size table",track);
		}
	}

	for (track=0;track < qr->max_track;track++) {
		quicktime_reader_track *trk = qr->track+track;

		if (trk->stts.absolute_data_offset != 0LL) {
			quicktime_stts_header h;
			quicktime_stts_entry e;
			atom = trk->stts;

			debug(qr,"Loading track %u time-to-sample tables",track);

			quicktime_stack_seek(qr->r_moov.qt,&atom,0);
			if (atom.data_length >= sizeof(h) && quicktime_stack_read(qr->r_moov.qt,&atom,&h,sizeof(h)) == (int)sizeof(h)) {
				unsigned int max = __be_u32(&h.number_of_entries),entry;
				uint64_t time = 0;

				if (max != 0 && max < 65536) {
					trk->timetosample.entries = max;
					trk->timetosample.list = (quicktime_reader_stts_entry*)
						malloc(sizeof(quicktime_reader_stts_entry) * trk->timetosample.entries);

					if (trk->timetosample.list == NULL) {
						error(qr,"Track %u cannot allocate memory for stts list",track);
					}
					else {
						for (entry=0;entry < max;entry++) {
							quicktime_reader_stts_entry *en = trk->timetosample.list + entry;
							if (quicktime_stack_read(qr->r_moov.qt,&atom,&e,sizeof(e)) != (int)sizeof(e)) {
								warning(qr,"Track %u stts table ends prematurely",track);
								break;
							}

							en->start = time;
							en->count = __be_u32(&e.count);
							en->duration = __be_u32(&e.duration);
							time += ((uint64_t)(en->count)) * ((uint64_t)(en->duration));
							debug(qr,"   start=%llu count=%lu duration=%lu",en->start,en->count,en->duration);
						}
					}
				}
			}
		}
		else {
			warning(qr,"Track %u has no sample size table",track);
		}
	}

	/* and now for one of the uglier features of the QuickTime format:
	 * in the pre-QuickTime 4 era apparently, the developers assumed that
	 * only PCM audio would be used with the format. Then they added
	 * compression, but only for compression formats that support fixed
	 * bitrates. In those cases, the "sample size" fields are NOT in
	 * bytes, they are in samples, which means to make meaningful use
	 * of them we have to multiply them by the number of bytes/sample,
	 * divided by the samples per block. Eugh. */
	for (track=0;track < qr->max_track;track++) {
		quicktime_stsd_entry_audio *stsd;
		unsigned int ent;

		quicktime_reader_track *trk = qr->track+track;
		if (trk->stsd_header.number_of_entries == 0) continue;
		if (__be_u32(&trk->mhlr_header.component_subtype) != quicktime_type_const('s','o','u','n')) continue;

		for (ent=0;ent < trk->stsd_header.number_of_entries;ent++) {
			quicktime_atom_type_t id;
			unsigned int bpp,ch;
			unsigned int dref;

			stsd = (quicktime_stsd_entry_audio*)(trk->stsd_entry[ent]);
			dref = __be_u16(&stsd->general.data_reference_index);
			id = __be_u32(&stsd->general.data_format);

			if (	id == 0 ||
				id == quicktime_type_const('N','O','N','E') ||
				id == quicktime_type_const('r','a','w',' ') ||
				id == quicktime_type_const('t','w','o','s') ||
				id == quicktime_type_const('s','o','w','t')) {
				if (trk->samplesize.constval != 1) continue;
			}
			else {
				continue;
			}

			/* Version 0 PCM */
			bpp = __be_u16(&stsd->sample_size);
			ch = __be_u16(&stsd->number_of_channels);
			if (ch == 0 || ch > 2) {
				warning(qr,"Track %u, stsd[%u], %u-channel PCM is unusual",track,ent,ch);
			}
			else if (!(bpp == 8 || bpp == 16)) {
				warning(qr,"Track %u, stsd[%u], %u-bit/sample PCM is unusual",track,ent,bpp);
			}
			else {
				unsigned int bytes = (bpp/8)*ch;
				assert(bytes >= 1 && bytes <= 4);
				debug(qr,"Track %u, stsd[%u], PCM audio is %u bytes/sample, now adjusting tables",track,ent,bytes);

				if (trk->samplesize.constval != 0)
					trk->samplesize.constval *= bytes;
				else if (trk->samplesize.list != NULL) {
					unsigned long chunk;
					for (chunk=0;chunk < trk->sampletochunk.entries;chunk++) {
						quicktime_reader_stsc_entry *ee = trk->sampletochunk.list+chunk;
						if (ee->sample_description_id == dref) {
							unsigned long sample = ee->first_sample;
							unsigned long last_sample = sample + ee->samples_per_chunk - 1;
							while (sample <= last_sample) {
								if (sample >= trk->samplesize.entries) break;
								trk->samplesize.list[sample] *= bytes;
								sample++;
							}
						}
					}
				}
			}
		}
	}

	return 1;
}


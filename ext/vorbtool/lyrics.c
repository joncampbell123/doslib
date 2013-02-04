/* OggEnc
 **
 ** This program is distributed under the GNU General Public License, version 2.
 ** A copy of this license is included with this source.
 **
 ** This particular file may also be distributed under (at your option) any
 ** later version of the GNU General Public License.
 **
 ** Copyright 2008, ogg.k.ogg.k <ogg.k.ogg.k@googlemail.com>
 **
 ** Portions from ffmpeg2theora, (c) j <j@v2v.cc>
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_KATE
#endif

#include "lyrics.h"
#include "utf8.h"
#include "i18n.h"

typedef enum {
  lf_unknown,
  lf_srt,
  lf_lrc,
} lyrics_format;

#ifdef HAVE_KATE
#endif

oe_lyrics *load_lyrics(const char *filename)
{
#ifdef HAVE_KATE
#else
  return NULL;
#endif
}

void free_lyrics(oe_lyrics *lyrics)
{
#ifdef HAVE_KATE
#endif
}

const oe_lyrics_item *get_lyrics(const oe_lyrics *lyrics, double t, size_t *idx)
{
#ifdef HAVE_KATE
#else
    return NULL;
#endif
}

/******************************************************************************
 
gif_lib.h - service library for decoding and encoding GIF images
                                                                             
*****************************************************************************/

#ifndef _GIF_LIB_H_
#define _GIF_LIB_H_ 1

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define GIFLIB_MAJOR 5
#define GIFLIB_MINOR 1
#define GIFLIB_RELEASE 4

#define GIF_ERROR   0
#define GIF_OK      1

#include <stddef.h>
#include <stdbool.h>

#define GIF_STAMP "GIFVER"          /* First chars in file - GIF stamp.  */
#define GIF_STAMP_LEN sizeof(GIF_STAMP) - 1
#define GIF_VERSION_POS 3           /* Version first character in stamp. */
#define GIF87_STAMP "GIF87a"        /* First chars in file - GIF stamp.  */
#define GIF89_STAMP "GIF89a"        /* First chars in file - GIF stamp.  */

typedef unsigned char GifPixelType;
typedef unsigned char GifByteType;
typedef unsigned short GifPrefixType;
typedef unsigned short GifWord;

#pragma pack(push,1)
typedef struct GifColorType {
    GifByteType Red, Green, Blue;
} GifColorType;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct ColorMapObject {
    unsigned short ColorCount;
    unsigned char BitsPerPixel;
    GifColorType Colors[256];    /* on malloc(3) heap */
} ColorMapObject;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct GifImageDesc {
    GifWord Left, Top, Width, Height;   /* Current image dimensions. */
    bool Interlace;                     /* Sequential/Interlaced lines. */
} GifImageDesc;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct SavedImage {
    GifImageDesc ImageDesc;
    GifByteType *RasterBits;         /* on malloc(3) heap */
} SavedImage;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct GifFileType {
    GifWord SWidth, SHeight;         /* Size of virtual canvas */
    GifWord SBackGroundColor;        /* Background color for virtual canvas */
    ColorMapObject SColorMap;        /* Global colormap, NULL if nonexistent. */
    GifImageDesc Image;              /* Current image (low-level API) */
    SavedImage SavedImages;          /* Image sequence (high-level API) */
    signed short int Error;			 /* Last error condition reported */
    void *Private;                   /* Don't mess with this! */
} GifFileType;
#pragma pack(pop)

typedef enum {
    UNDEFINED_RECORD_TYPE,
    SCREEN_DESC_RECORD_TYPE,
    IMAGE_DESC_RECORD_TYPE, /* Begin with ',' */
    EXTENSION_RECORD_TYPE,  /* Begin with '!' */
    TERMINATE_RECORD_TYPE   /* Begin with ';' */
} GifRecordType;

/******************************************************************************
 GIF89 structures
******************************************************************************/

/******************************************************************************
 GIF encoding routines
******************************************************************************/

/******************************************************************************
 GIF decoding routines
******************************************************************************/

/* Main entry points */
GifFileType *DGifOpenFileName(const char *GifFileName, int *Error);
int DGifCloseFile(GifFileType * GifFile, int *ErrorCode);
int DGifSlurp(GifFileType * GifFile);

#define D_GIF_SUCCEEDED          0
#define D_GIF_ERR_OPEN_FAILED    101    /* And DGif possible errors. */
#define D_GIF_ERR_READ_FAILED    102
#define D_GIF_ERR_NOT_GIF_FILE   103
#define D_GIF_ERR_NO_SCRN_DSCR   104
#define D_GIF_ERR_NO_IMAG_DSCR   105
#define D_GIF_ERR_NO_COLOR_MAP   106
#define D_GIF_ERR_WRONG_RECORD   107
#define D_GIF_ERR_DATA_TOO_BIG   108
#define D_GIF_ERR_NOT_ENOUGH_MEM 109
#define D_GIF_ERR_CLOSE_FAILED   110
#define D_GIF_ERR_NOT_READABLE   111
#define D_GIF_ERR_IMAGE_DEFECT   112
#define D_GIF_ERR_EOF_TOO_SOON   113

/******************************************************************************
 Color table quantization (deprecated)
******************************************************************************/

/******************************************************************************
 Error handling and reporting.
******************************************************************************/
extern const char *GifErrorString(int ErrorCode);     /* new in 2012 - ESR */

/*****************************************************************************
 Everything below this point is new after version 1.2, supporting `slurp
 mode' for doing I/O in two big belts with all the image-bashing in core.
******************************************************************************/

/******************************************************************************
 Color map handling from gif_alloc.c
******************************************************************************/

/******************************************************************************
 Support for the in-core structures allocation (slurp mode).              
******************************************************************************/

/******************************************************************************
 5.x functions for GIF89 graphics control blocks
******************************************************************************/

/******************************************************************************
 The library's internal utility font                          
******************************************************************************/

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _GIF_LIB_H */

/* end */

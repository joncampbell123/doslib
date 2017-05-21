/******************************************************************************

dgif_lib.c - GIF decoding

The functions here and in egif_lib.c are partitioned carefully so that
if you only require one of read and write capability, only one of these
two modules will be linked.  Preserve this property!

*****************************************************************************/

#include <sys/types.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include "dbmalloc.h"

#include "gif_lib.h"
#include "gif_lib_private.h"

#define MAX(x, y)    (((x) > (y)) ? (x) : (y))

/* compose unsigned little endian value */
#define UNSIGNED_LITTLE_ENDIAN(lo, hi)	((lo) | ((hi) << 8))

/* avoid extra function call in case we use fread (TVT) */
#define READ(_gif,_buf,_len)                                     \
  (fread(_buf,1,_len,((GifFilePrivateType*)_gif->Private)->File))

static int DGifGetWord(GifFileType *GifFile, GifWord *Word);
static int DGifSetupDecompress(GifFileType *GifFile);
static int DGifDecompressLine(GifFileType *GifFile, GifPixelType *Line,
                              size_t LineLen);
static int DGifGetPrefixChar(GifPrefixType *Prefix, int Code, int ClearCode);
static int DGifDecompressInput(GifFileType *GifFile, int *Code);
static int DGifBufferedInput(GifFileType *GifFile, GifByteType *Buf,
                             GifByteType *NextByte);

/* These are legacy.  You probably do not want to call them directly */
static int DGifGetScreenDesc(GifFileType *GifFile);
static int DGifGetRecordType(GifFileType *GifFile, GifRecordType *GifType);
static int DGifGetImageDesc(GifFileType *GifFile);
static int DGifGetLine(GifFileType *GifFile, GifPixelType *GifLine, size_t GifLineLen);
static int DGifGetPixel(GifFileType *GifFile, GifPixelType GifPixel);
static int DGifGetComment(GifFileType *GifFile, char *GifComment);
static int DGifGetExtension(GifFileType *GifFile, int *GifExtCode,
                     GifByteType **GifExtension);
static int DGifGetExtensionNext(GifFileType *GifFile, GifByteType **GifExtension);
static int DGifGetCode(GifFileType *GifFile, int *GifCodeSize,
                GifByteType **GifCodeBlock);
static int DGifGetCodeNext(GifFileType *GifFile, GifByteType **GifCodeBlock);

static ColorMapObject *GifMakeMapObject(int ColorCount,
                                     const GifColorType *ColorMap);
static void GifFreeMapObject(ColorMapObject *Object);

static void *
reallocarray(void *optr, size_t nmemb, size_t size);

static SavedImage *GifMakeSavedImage(GifFileType *GifFile,
                                  const SavedImage *CopyFrom);

static GifFileType *DGifOpenFileHandle(int GifFileHandle, int *Error);

static GifFilePrivateType GifPrivate;

/******************************************************************************
  Open a new GIF file for read, given by its name.
  Returns dynamically allocated GifFileType pointer which serves as the GIF
  info record.
 ******************************************************************************/
GifFileType *DGifOpenFileName(const char *FileName, int *Error) {
    int FileHandle;
    GifFileType *GifFile;

    if ((FileHandle = open(FileName, O_RDONLY)) == -1) {
        if (Error != NULL)
            *Error = D_GIF_ERR_OPEN_FAILED;
        return NULL;
    }

    GifFile = DGifOpenFileHandle(FileHandle, Error);
    return GifFile;
}

/******************************************************************************
 Update a new GIF file, given its file handle.
 Returns dynamically allocated GifFileType pointer which serves as the GIF
 info record.
******************************************************************************/
GifFileType *DGifOpenFileHandle(int FileHandle, int *Error) {
    GifFilePrivateType *Private = &GifPrivate;
    char Buf[GIF_STAMP_LEN + 1];
    GifFileType *GifFile;
    FILE *f;

    GifFile = (GifFileType *)malloc(sizeof(GifFileType));
    if (GifFile == NULL) {
        if (Error != NULL)
            *Error = D_GIF_ERR_NOT_ENOUGH_MEM;
        (void)close(FileHandle);
        return NULL;
    }

    /*@i1@*/memset(GifFile, '\0', sizeof(GifFileType));

    /*@i1@*/memset(Private, '\0', sizeof(GifFilePrivateType));

#if defined(_WIN32) || defined(TARGET_MSDOS)
    _setmode(FileHandle, O_BINARY);    /* Make sure it is in binary mode. */
#endif /* _WIN32 */

    f = fdopen(FileHandle, "rb");    /* Make it into a stream: */

    /*@-mustfreeonly@*/
    GifFile->Private = Private;
    Private->FileHandle = FileHandle;
    Private->File = f;
    Private->FileState = FILE_STATE_READ;
    /*@=mustfreeonly@*/

    /* Let's see if this is a GIF file: */
    /* coverity[check_return] */
    if (READ(GifFile, (unsigned char *)Buf, GIF_STAMP_LEN) != GIF_STAMP_LEN) {
        if (Error != NULL)
            *Error = D_GIF_ERR_READ_FAILED;
        (void)fclose(f);
        free((char *)Private);
        free((char *)GifFile);
        return NULL;
    }

    /* Check for GIF prefix at start of file */
    Buf[GIF_STAMP_LEN] = 0;
    if (strncmp(GIF_STAMP, Buf, GIF_VERSION_POS) != 0) {
        if (Error != NULL)
            *Error = D_GIF_ERR_NOT_GIF_FILE;
        (void)fclose(f);
        free((char *)Private);
        free((char *)GifFile);
        return NULL;
    }

    if (DGifGetScreenDesc(GifFile) == GIF_ERROR) {
        (void)fclose(f);
        free((char *)Private);
        free((char *)GifFile);
        return NULL;
    }

    GifFile->Error = 0;
    return GifFile;
}

/******************************************************************************
 This routine should be called before any other DGif calls. Note that
 this routine is called automatically from DGif file open routines.
******************************************************************************/
static int DGifGetScreenDesc(GifFileType *GifFile) {
    int BitsPerPixel;
    bool SortFlag;
    GifByteType Buf[3];
    GifFilePrivateType *Private = (GifFilePrivateType *)GifFile->Private;

    if (!IS_READABLE(Private)) {
        /* This file was NOT open for reading: */
        GifFile->Error = D_GIF_ERR_NOT_READABLE;
        return GIF_ERROR;
    }

    /* Put the screen descriptor into the file: */
    if (DGifGetWord(GifFile, &GifFile->SWidth) == GIF_ERROR ||
            DGifGetWord(GifFile, &GifFile->SHeight) == GIF_ERROR)
        return GIF_ERROR;

    if (READ(GifFile, Buf, 3) != 3) {
        GifFile->Error = D_GIF_ERR_READ_FAILED;
        return GIF_ERROR;
    }
    SortFlag = (Buf[0] & 0x08) != 0;
    BitsPerPixel = (Buf[0] & 0x07) + 1;
    GifFile->SBackGroundColor = Buf[1];
    if (Buf[0] & 0x80) {    /* Do we have global color map? */
        int i;

        GifFile->SColorMap.ColorCount = 1 << BitsPerPixel;
        GifFile->SColorMap.BitsPerPixel = BitsPerPixel;

        /* Get the global color map: */
        for (i = 0; i < GifFile->SColorMap.ColorCount; i++) {
            /* coverity[check_return] */
            if (READ(GifFile, Buf, 3) != 3) {
                GifFile->Error = D_GIF_ERR_READ_FAILED;
                return GIF_ERROR;
            }
            GifFile->SColorMap.Colors[i].Red = Buf[0];
            GifFile->SColorMap.Colors[i].Green = Buf[1];
            GifFile->SColorMap.Colors[i].Blue = Buf[2];
        }
    } else {
        GifFile->SColorMap.ColorCount = 0;
    }

    return GIF_OK;
}

/******************************************************************************
 This routine should be called before any attempt to read an image.
******************************************************************************/
static int DGifGetRecordType(GifFileType *GifFile, GifRecordType* Type) {
    GifByteType Buf;
    GifFilePrivateType *Private = (GifFilePrivateType *)GifFile->Private;

    if (!IS_READABLE(Private)) {
        /* This file was NOT open for reading: */
        GifFile->Error = D_GIF_ERR_NOT_READABLE;
        return GIF_ERROR;
    }

    /* coverity[check_return] */
    if (READ(GifFile, &Buf, 1) != 1) {
        GifFile->Error = D_GIF_ERR_READ_FAILED;
        return GIF_ERROR;
    }

    switch (Buf) {
        case DESCRIPTOR_INTRODUCER:
            *Type = IMAGE_DESC_RECORD_TYPE;
            break;
        case EXTENSION_INTRODUCER:
            *Type = EXTENSION_RECORD_TYPE;
            break;
        case TERMINATOR_INTRODUCER:
            *Type = TERMINATE_RECORD_TYPE;
            break;
        default:
            *Type = UNDEFINED_RECORD_TYPE;
            GifFile->Error = D_GIF_ERR_WRONG_RECORD;
            return GIF_ERROR;
    }

    return GIF_OK;
}

/******************************************************************************
 This routine should be called before any attempt to read an image.
 Note it is assumed the Image desc. header has been read.
******************************************************************************/
static int DGifGetImageDesc(GifFileType *GifFile) {
    unsigned int BitsPerPixel;
    GifByteType Buf[3];
    GifFilePrivateType *Private = (GifFilePrivateType *)GifFile->Private;

    if (!IS_READABLE(Private)) {
        /* This file was NOT open for reading: */
        GifFile->Error = D_GIF_ERR_NOT_READABLE;
        return GIF_ERROR;
    }

    if (DGifGetWord(GifFile, &GifFile->Image.Left) == GIF_ERROR ||
            DGifGetWord(GifFile, &GifFile->Image.Top) == GIF_ERROR ||
            DGifGetWord(GifFile, &GifFile->Image.Width) == GIF_ERROR ||
            DGifGetWord(GifFile, &GifFile->Image.Height) == GIF_ERROR)
        return GIF_ERROR;
    if (READ(GifFile, Buf, 1) != 1) {
        GifFile->Error = D_GIF_ERR_READ_FAILED;
        return GIF_ERROR;
    }
    BitsPerPixel = (Buf[0] & 0x07) + 1;
    GifFile->Image.Interlace = (Buf[0] & 0x40) ? true : false;

    /* Does this image have local color map? */
    if (Buf[0] & 0x80) {
        GifFile->Error = D_GIF_ERR_READ_FAILED;
        return GIF_ERROR;
    }

    if (GifFile->SavedImages.RasterBits != NULL) {
        GifFile->Error = D_GIF_ERR_READ_FAILED;
        return GIF_ERROR;
    }

    memcpy(&GifFile->SavedImages.ImageDesc, &GifFile->Image, sizeof(GifImageDesc));

    Private->PixelCount = (long)GifFile->Image.Width *
        (long)GifFile->Image.Height;

    /* Reset decompress algorithm parameters. */
    return DGifSetupDecompress(GifFile);
}

/******************************************************************************
 Get one full scanned line (Line) of length LineLen from GIF file.
******************************************************************************/
static int DGifGetLine(GifFileType *GifFile, GifPixelType *Line, size_t LineLen) {
    GifByteType *Dummy;
    GifFilePrivateType *Private = (GifFilePrivateType *) GifFile->Private;

    if (!IS_READABLE(Private)) {
        /* This file was NOT open for reading: */
        GifFile->Error = D_GIF_ERR_NOT_READABLE;
        return GIF_ERROR;
    }

    if (!LineLen)
        LineLen = GifFile->Image.Width;

    if ((Private->PixelCount -= LineLen) > 0xffff0000UL) {
        GifFile->Error = D_GIF_ERR_DATA_TOO_BIG;
        return GIF_ERROR;
    }

    if (DGifDecompressLine(GifFile, Line, LineLen) == GIF_OK) {
        if (Private->PixelCount == 0) {
            /* We probably won't be called any more, so let's clean up
             * everything before we return: need to flush out all the
             * rest of image until an empty block (size 0)
             * detected. We use GetCodeNext.
             */
            do
                if (DGifGetCodeNext(GifFile, &Dummy) == GIF_ERROR)
                    return GIF_ERROR;
            while (Dummy != NULL) ;
        }
        return GIF_OK;
    } else
        return GIF_ERROR;
}

/******************************************************************************
 Get an extension block (see GIF manual) from GIF file. This routine only
 returns the first data block, and DGifGetExtensionNext should be called
 after this one until NULL extension is returned.
 The Extension should NOT be freed by the user (not dynamically allocated).
 Note it is assumed the Extension description header has been read.
******************************************************************************/
static int DGifGetExtension(GifFileType *GifFile, int *ExtCode, GifByteType **Extension) {
    GifByteType Buf;
    GifFilePrivateType *Private = (GifFilePrivateType *)GifFile->Private;

    if (!IS_READABLE(Private)) {
        /* This file was NOT open for reading: */
        GifFile->Error = D_GIF_ERR_NOT_READABLE;
        return GIF_ERROR;
    }

    /* coverity[check_return] */
    if (READ(GifFile, &Buf, 1) != 1) {
        GifFile->Error = D_GIF_ERR_READ_FAILED;
        return GIF_ERROR;
    }
    *ExtCode = Buf;

    return DGifGetExtensionNext(GifFile, Extension);
}

/******************************************************************************
 Get a following extension block (see GIF manual) from GIF file. This
 routine should be called until NULL Extension is returned.
 The Extension should NOT be freed by the user (not dynamically allocated).
******************************************************************************/
static int DGifGetExtensionNext(GifFileType *GifFile, GifByteType ** Extension) {
    GifByteType Buf;
    GifFilePrivateType *Private = (GifFilePrivateType *)GifFile->Private;

    if (READ(GifFile, &Buf, 1) != 1) {
        GifFile->Error = D_GIF_ERR_READ_FAILED;
        return GIF_ERROR;
    }
    if (Buf > 0) {
        *Extension = Private->Buf;    /* Use private unused buffer. */
        (*Extension)[0] = Buf;  /* Pascal strings notation (pos. 0 is len.). */
        /* coverity[tainted_data,check_return] */
        if (READ(GifFile, &((*Extension)[1]), Buf) != Buf) {
            GifFile->Error = D_GIF_ERR_READ_FAILED;
            return GIF_ERROR;
        }
    } else
        *Extension = NULL;

    return GIF_OK;
}

/******************************************************************************
 This routine should be called last, to close the GIF file.
******************************************************************************/
int DGifCloseFile(GifFileType *GifFile, int *ErrorCode) {
    GifFilePrivateType *Private;

    if (GifFile == NULL || GifFile->Private == NULL)
        return GIF_ERROR;

    if (GifFile->SavedImages.RasterBits) {
        free(GifFile->SavedImages.RasterBits);
        GifFile->SavedImages.RasterBits = NULL;
    }

    Private = (GifFilePrivateType *) GifFile->Private;

    if (!IS_READABLE(Private)) {
        /* This file was NOT open for reading: */
        if (ErrorCode != NULL)
            *ErrorCode = D_GIF_ERR_NOT_READABLE;
        free((char *)GifFile->Private);
        free(GifFile);
        return GIF_ERROR;
    }

    if (Private->File && (fclose(Private->File) != 0)) {
        if (ErrorCode != NULL)
            *ErrorCode = D_GIF_ERR_CLOSE_FAILED;
        free((char *)GifFile->Private);
        free(GifFile);
        return GIF_ERROR;
    }

    free((char *)GifFile->Private);
    free(GifFile);
    if (ErrorCode != NULL)
        *ErrorCode = D_GIF_SUCCEEDED;
    return GIF_OK;
}

/******************************************************************************
 Get 2 bytes (word) from the given file:
******************************************************************************/
static int DGifGetWord(GifFileType *GifFile, GifWord *Word) {
    unsigned char c[2];

    /* coverity[check_return] */
    if (READ(GifFile, c, 2) != 2) {
        GifFile->Error = D_GIF_ERR_READ_FAILED;
        return GIF_ERROR;
    }

    *Word = (GifWord)UNSIGNED_LITTLE_ENDIAN(c[0], c[1]);
    return GIF_OK;
}

/******************************************************************************
 Continue to get the image code in compressed form. This routine should be
 called until NULL block is returned.
 The block should NOT be freed by the user (not dynamically allocated).
******************************************************************************/
static int DGifGetCodeNext(GifFileType *GifFile, GifByteType **CodeBlock) {
    GifByteType Buf;
    GifFilePrivateType *Private = (GifFilePrivateType *)GifFile->Private;

    /* coverity[tainted_data_argument] */
    /* coverity[check_return] */
    if (READ(GifFile, &Buf, 1) != 1) {
        GifFile->Error = D_GIF_ERR_READ_FAILED;
        return GIF_ERROR;
    }

    /* coverity[lower_bounds] */
    if (Buf > 0) {
        *CodeBlock = Private->Buf;    /* Use private unused buffer. */
        (*CodeBlock)[0] = Buf;  /* Pascal strings notation (pos. 0 is len.). */
        /* coverity[tainted_data] */
        if (READ(GifFile, &((*CodeBlock)[1]), Buf) != Buf) {
            GifFile->Error = D_GIF_ERR_READ_FAILED;
            return GIF_ERROR;
        }
    } else {
        *CodeBlock = NULL;
        Private->Buf[0] = 0;    /* Make sure the buffer is empty! */
        Private->PixelCount = 0;    /* And local info. indicate image read. */
    }

    return GIF_OK;
}

/******************************************************************************
 Setup the LZ decompression for this image:
******************************************************************************/
static int DGifSetupDecompress(GifFileType *GifFile) {
    int i;
    GifByteType CodeSize;
    GifPrefixType *Prefix;
    unsigned char BitsPerPixel;
    GifFilePrivateType *Private = (GifFilePrivateType *)GifFile->Private;

    /* coverity[check_return] */
    if (READ(GifFile, &CodeSize, 1) < 1) {    /* Read Code size from file. */
        return GIF_ERROR;    /* Failed to read Code size. */
    }
    BitsPerPixel = CodeSize;

    /* this can only happen on a severely malformed GIF */
    if (BitsPerPixel > 8) {
        GifFile->Error = D_GIF_ERR_READ_FAILED;	/* somewhat bogus error code */
        return GIF_ERROR;    /* Failed to read Code size. */
    }

    Private->Buf[0] = 0;    /* Input Buffer empty. */
    Private->BitsPerPixel = BitsPerPixel;
    Private->ClearCode = (1 << BitsPerPixel);
    Private->EOFCode = Private->ClearCode + 1;
    Private->RunningCode = Private->EOFCode + 1;
    Private->RunningBits = BitsPerPixel + 1;    /* Number of bits per code. */
    Private->MaxCode1 = 1 << Private->RunningBits;    /* Max. code + 1. */
    Private->StackPtr = 0;    /* No pixels on the pixel stack. */
    Private->LastCode = NO_SUCH_CODE;
    Private->CrntShiftState = 0;    /* No information in CrntShiftDWord. */
    Private->CrntShiftDWord = 0;

    Prefix = Private->Prefix;
    for (i = 0; i <= LZ_MAX_CODE; i++)
        Prefix[i] = NO_SUCH_CODE;

    return GIF_OK;
}

/******************************************************************************
 The LZ decompression routine:
 This version decompress the given GIF file into Line of length LineLen.
 This routine can be called few times (one per scan line, for example), in
 order the complete the whole image.
******************************************************************************/
static int DGifDecompressLine(GifFileType *GifFile, GifPixelType *Line, size_t LineLen) {
    size_t i = 0;
    int j, CrntCode, EOFCode, ClearCode, CrntPrefix, LastCode, StackPtr;
    GifByteType *Stack, *Suffix;
    GifPrefixType *Prefix;
    GifFilePrivateType *Private = (GifFilePrivateType *) GifFile->Private;

    StackPtr = Private->StackPtr;
    Prefix = Private->Prefix;
    Suffix = Private->Suffix;
    Stack = Private->Stack;
    EOFCode = Private->EOFCode;
    ClearCode = Private->ClearCode;
    LastCode = Private->LastCode;

    if (StackPtr > LZ_MAX_CODE) {
        return GIF_ERROR;
    }

    if (StackPtr != 0) {
        /* Let pop the stack off before continueing to read the GIF file: */
        while (StackPtr != 0 && i < LineLen)
            Line[i++] = Stack[--StackPtr];
    }

    while (i < LineLen) {    /* Decode LineLen items. */
        if (DGifDecompressInput(GifFile, &CrntCode) == GIF_ERROR)
            return GIF_ERROR;

        if (CrntCode == EOFCode) {
            /* Note however that usually we will not be here as we will stop
             * decoding as soon as we got all the pixel, or EOF code will
             * not be read at all, and DGifGetLine/Pixel clean everything.  */
            GifFile->Error = D_GIF_ERR_EOF_TOO_SOON;
            return GIF_ERROR;
        } else if (CrntCode == ClearCode) {
            /* We need to start over again: */
            for (j = 0; j <= LZ_MAX_CODE; j++)
                Prefix[j] = NO_SUCH_CODE;
            Private->RunningCode = Private->EOFCode + 1;
            Private->RunningBits = Private->BitsPerPixel + 1;
            Private->MaxCode1 = 1 << Private->RunningBits;
            LastCode = Private->LastCode = NO_SUCH_CODE;
        } else {
            /* Its regular code - if in pixel range simply add it to output
             * stream, otherwise trace to codes linked list until the prefix
             * is in pixel range: */
            if (CrntCode < ClearCode) {
                /* This is simple - its pixel scalar, so add it to output: */
                Line[i++] = CrntCode;
            } else {
                /* Its a code to needed to be traced: trace the linked list
                 * until the prefix is a pixel, while pushing the suffix
                 * pixels on our stack. If we done, pop the stack in reverse
                 * (thats what stack is good for!) order to output.  */
                if (Prefix[CrntCode] == NO_SUCH_CODE) {
                    CrntPrefix = LastCode;

                    /* Only allowed if CrntCode is exactly the running code:
                     * In that case CrntCode = XXXCode, CrntCode or the
                     * prefix code is last code and the suffix char is
                     * exactly the prefix of last code! */
                    if (CrntCode == Private->RunningCode - 2) {
                        Suffix[Private->RunningCode - 2] =
                            Stack[StackPtr++] = DGifGetPrefixChar(Prefix,
                                    LastCode,
                                    ClearCode);
                    } else {
                        Suffix[Private->RunningCode - 2] =
                            Stack[StackPtr++] = DGifGetPrefixChar(Prefix,
                                    CrntCode,
                                    ClearCode);
                    }
                } else
                    CrntPrefix = CrntCode;

                /* Now (if image is O.K.) we should not get a NO_SUCH_CODE
                 * during the trace. As we might loop forever, in case of
                 * defective image, we use StackPtr as loop counter and stop
                 * before overflowing Stack[]. */
                while (StackPtr < LZ_MAX_CODE &&
                        CrntPrefix > ClearCode && CrntPrefix <= LZ_MAX_CODE) {
                    Stack[StackPtr++] = Suffix[CrntPrefix];
                    CrntPrefix = Prefix[CrntPrefix];
                }
                if (StackPtr >= LZ_MAX_CODE || CrntPrefix > LZ_MAX_CODE) {
                    GifFile->Error = D_GIF_ERR_IMAGE_DEFECT;
                    return GIF_ERROR;
                }
                /* Push the last character on stack: */
                Stack[StackPtr++] = CrntPrefix;

                /* Now lets pop all the stack into output: */
                while (StackPtr != 0 && i < LineLen)
                    Line[i++] = Stack[--StackPtr];
            }
            if (LastCode != NO_SUCH_CODE && Prefix[Private->RunningCode - 2] == NO_SUCH_CODE) {
                Prefix[Private->RunningCode - 2] = LastCode;

                if (CrntCode == Private->RunningCode - 2) {
                    /* Only allowed if CrntCode is exactly the running code:
                     * In that case CrntCode = XXXCode, CrntCode or the
                     * prefix code is last code and the suffix char is
                     * exactly the prefix of last code! */
                    Suffix[Private->RunningCode - 2] =
                        DGifGetPrefixChar(Prefix, LastCode, ClearCode);
                } else {
                    Suffix[Private->RunningCode - 2] =
                        DGifGetPrefixChar(Prefix, CrntCode, ClearCode);
                }
            }
            LastCode = CrntCode;
        }
    }

    Private->LastCode = LastCode;
    Private->StackPtr = StackPtr;

    return GIF_OK;
}

/******************************************************************************
 Routine to trace the Prefixes linked list until we get a prefix which is
 not code, but a pixel value (less than ClearCode). Returns that pixel value.
 If image is defective, we might loop here forever, so we limit the loops to
 the maximum possible if image O.k. - LZ_MAX_CODE times.
******************************************************************************/
static int DGifGetPrefixChar(GifPrefixType *Prefix, int Code, int ClearCode) {
    int i = 0;

    while (Code > ClearCode && i++ <= LZ_MAX_CODE) {
        if (Code > LZ_MAX_CODE) {
            return NO_SUCH_CODE;
        }
        Code = Prefix[Code];
    }
    return Code;
}

/******************************************************************************
 The LZ decompression input routine:
 This routine is responsable for the decompression of the bit stream from
 8 bits (bytes) packets, into the real codes.
 Returns GIF_OK if read successfully.
******************************************************************************/
static int DGifDecompressInput(GifFileType *GifFile, int *Code) {
    static const unsigned short CodeMasks[] = {
        0x0000, 0x0001, 0x0003, 0x0007,
        0x000f, 0x001f, 0x003f, 0x007f,
        0x00ff, 0x01ff, 0x03ff, 0x07ff,
        0x0fff
    };

    GifFilePrivateType *Private = (GifFilePrivateType *)GifFile->Private;

    GifByteType NextByte;

    /* The image can't contain more than LZ_BITS per code. */
    if (Private->RunningBits > LZ_BITS) {
        GifFile->Error = D_GIF_ERR_IMAGE_DEFECT;
        return GIF_ERROR;
    }

    while (Private->CrntShiftState < Private->RunningBits) {
        /* Needs to get more bytes from input stream for next code: */
        if (DGifBufferedInput(GifFile, Private->Buf, &NextByte) == GIF_ERROR) {
            return GIF_ERROR;
        }
        Private->CrntShiftDWord |=
            ((unsigned long)NextByte) << Private->CrntShiftState;
        Private->CrntShiftState += 8;
    }
    *Code = Private->CrntShiftDWord & CodeMasks[Private->RunningBits];

    Private->CrntShiftDWord >>= Private->RunningBits;
    Private->CrntShiftState -= Private->RunningBits;

    /* If code cannot fit into RunningBits bits, must raise its size. Note
     * however that codes above 4095 are used for special signaling.
     * If we're using LZ_BITS bits already and we're at the max code, just
     * keep using the table as it is, don't increment Private->RunningCode.
     */
    if (Private->RunningCode < LZ_MAX_CODE + 2 &&
            ++Private->RunningCode > Private->MaxCode1 &&
            Private->RunningBits < LZ_BITS) {
        Private->MaxCode1 <<= 1;
        Private->RunningBits++;
    }
    return GIF_OK;
}

/******************************************************************************
 This routines read one GIF data block at a time and buffers it internally
 so that the decompression routine could access it.
 The routine returns the next byte from its internal buffer (or read next
 block in if buffer empty) and returns GIF_OK if succesful.
******************************************************************************/
static int DGifBufferedInput(GifFileType *GifFile, GifByteType *Buf, GifByteType *NextByte) {
    if (Buf[0] == 0) {
        /* Needs to read the next buffer - this one is empty: */
        /* coverity[check_return] */
        if (READ(GifFile, Buf, 1) != 1) {
            GifFile->Error = D_GIF_ERR_READ_FAILED;
            return GIF_ERROR;
        }
        /* There shouldn't be any empty data blocks here as the LZW spec
         * says the LZW termination code should come first.  Therefore we
         * shouldn't be inside this routine at that point.
         */
        if (Buf[0] == 0) {
            GifFile->Error = D_GIF_ERR_IMAGE_DEFECT;
            return GIF_ERROR;
        }
        if (READ(GifFile, &Buf[1], Buf[0]) != Buf[0]) {
            GifFile->Error = D_GIF_ERR_READ_FAILED;
            return GIF_ERROR;
        }
        *NextByte = Buf[1];
        Buf[1] = 2;    /* We use now the second place as last char read! */
        Buf[0]--;
    } else {
        *NextByte = Buf[Buf[1]++];
        Buf[0]--;
    }

    return GIF_OK;
}

/******************************************************************************
 This routine reads an entire GIF into core, hanging all its state info off
 the GifFileType pointer.  Call DGifOpenFileName() or DGifOpenFileHandle()
 first to initialize I/O.  Its inverse is EGifSpew().
*******************************************************************************/
int DGifSlurp(GifFileType *GifFile) {
    uint32_t ImageSize;
    GifRecordType RecordType;
    SavedImage *sp;
    GifByteType *ExtData;
    int ExtFunction;

    do {
        if (DGifGetRecordType(GifFile, &RecordType) == GIF_ERROR)
            return (GIF_ERROR);

        switch (RecordType) {
            case IMAGE_DESC_RECORD_TYPE:
                if (DGifGetImageDesc(GifFile) == GIF_ERROR)
                    return (GIF_ERROR);

                sp = &GifFile->SavedImages;
                ImageSize = (uint32_t)sp->ImageDesc.Width * (uint32_t)sp->ImageDesc.Height;

                if (ImageSize == 0 || ImageSize > (uint32_t)(0x7FFFFFFF / sizeof(GifPixelType)))
                    return GIF_ERROR;

                sp->RasterBits = (unsigned char *)reallocarray(NULL, ImageSize,
                        sizeof(GifPixelType));

                if (sp->RasterBits == NULL) {
                    return GIF_ERROR;
                }

                if (sp->ImageDesc.Interlace) {
                    int i, j;
                    /* 
                     * The way an interlaced image should be read - 
                     * offsets and jumps...
                     */
                    int InterlacedOffset[] = { 0, 4, 2, 1 };
                    int InterlacedJumps[] = { 8, 8, 4, 2 };
                    /* Need to perform 4 passes on the image */
                    for (i = 0; i < 4; i++)
                        for (j = InterlacedOffset[i]; 
                                j < sp->ImageDesc.Height;
                                j += InterlacedJumps[i]) {
                            if (DGifGetLine(GifFile, 
                                        sp->RasterBits+j*sp->ImageDesc.Width, 
                                        sp->ImageDesc.Width) == GIF_ERROR)
                                return GIF_ERROR;
                        }
                }
                else {
                    if (DGifGetLine(GifFile,sp->RasterBits,ImageSize)==GIF_ERROR)
                        return (GIF_ERROR);
                }
                break;

            case EXTENSION_RECORD_TYPE:
                if (DGifGetExtension(GifFile,&ExtFunction,&ExtData) == GIF_ERROR)
                    return (GIF_ERROR);
                while (ExtData != NULL) {
                    if (DGifGetExtensionNext(GifFile, &ExtData) == GIF_ERROR)
                        return (GIF_ERROR);
                }
                break;

            case TERMINATE_RECORD_TYPE:
                break;

            default:    /* Should be trapped by DGifGetRecordType */
                break;
        }
    } while (RecordType != TERMINATE_RECORD_TYPE);

    /* Sanity check for corrupted file */
    if (GifFile->SavedImages.RasterBits == NULL) {
        GifFile->Error = D_GIF_ERR_NO_IMAG_DSCR;
        return(GIF_ERROR);
    }

    return (GIF_OK);
}

/* end */
/*****************************************************************************

 GIF construction tools

****************************************************************************/

/******************************************************************************
 Miscellaneous utility functions                          
******************************************************************************/

/******************************************************************************
  Color map object functions                              
******************************************************************************/

/*******************************************************************************
Free a color map object
*******************************************************************************/

/******************************************************************************
 Image block allocation functions                          
******************************************************************************/

/* end */
/*****************************************************************************

gif_err.c - handle error reporting for the GIF library.

****************************************************************************/

/*****************************************************************************
 Return a string description of  the last GIF error
*****************************************************************************/
const char *GifErrorString(int ErrorCode) {
    const char *Err;

    switch (ErrorCode) {
        case D_GIF_ERR_OPEN_FAILED:
            Err = "Failed to open given file";
            break;
        case D_GIF_ERR_READ_FAILED:
            Err = "Failed to read from given file";
            break;
        case D_GIF_ERR_NOT_GIF_FILE:
            Err = "Data is not in GIF format";
            break;
        case D_GIF_ERR_NO_SCRN_DSCR:
            Err = "No screen descriptor detected";
            break;
        case D_GIF_ERR_NO_IMAG_DSCR:
            Err = "No Image Descriptor detected";
            break;
        case D_GIF_ERR_NO_COLOR_MAP:
            Err = "Neither global nor local color map";
            break;
        case D_GIF_ERR_WRONG_RECORD:
            Err = "Wrong record type detected";
            break;
        case D_GIF_ERR_DATA_TOO_BIG:
            Err = "Number of pixels bigger than width * height";
            break;
        case D_GIF_ERR_NOT_ENOUGH_MEM:
            Err = "Failed to allocate required memory";
            break;
        case D_GIF_ERR_CLOSE_FAILED:
            Err = "Failed to close given file";
            break;
        case D_GIF_ERR_NOT_READABLE:
            Err = "Given file was not opened for read";
            break;
        case D_GIF_ERR_IMAGE_DEFECT:
            Err = "Image is defective, decoding aborted";
            break;
        case D_GIF_ERR_EOF_TOO_SOON:
            Err = "Image EOF detected before image complete";
            break;
        default:
            Err = NULL;
            break;
    }
    return Err;
}

/* end */
/*	$OpenBSD: reallocarray.c,v 1.1 2014/05/08 21:43:49 deraadt Exp $	*/
/*
 * Copyright (c) 2008 Otto Moerbeek <otto@drijf.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This is sqrt(SIZE_MAX+1), as s1*s2 <= SIZE_MAX
 * if both s1 < MUL_NO_OVERFLOW and s2 < MUL_NO_OVERFLOW
 */
#define MUL_NO_OVERFLOW	((size_t)1 << (sizeof(size_t) * 4))

static void *reallocarray(void *optr, size_t nmemb, size_t size) {
    if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
            nmemb > 0 && SIZE_MAX / nmemb < size) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(optr, size * nmemb);
}


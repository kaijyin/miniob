/*
 * Copyright (c) Yann Collet, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */


#include <stdio.h>     // printf
#include <stdlib.h>    // free
#include <string.h>    // memset, strcat, strlen
#include <zstd.h>      // presumes zstd library is installed
#include "common.h"    // Helper functions, CHECK(), and CHECK_ZSTD()

namespace ZSTD{


void compress_file(const char* fname, const char* outName, int cLevel,
                               int nbThreads){
    fprintf (stderr, "Starting compression of %s with level %d, using %d threads\n",
             fname, cLevel, nbThreads);

    /* Open the input and output files. */
    FILE* const fin  = fopen_orDie(fname, "rb");
    FILE* const fout = fopen_orDie(outName, "wb+");
    /* Create the input and output buffers.
     * They may be any size, but we recommend using these functions to size them.
     * Performance will only suffer significantly for very tiny buffers.
     */
    size_t const buffInSize = ZSTD_CStreamInSize();
    void*  const buffIn  = malloc_orDie(buffInSize);
    size_t const buffOutSize = ZSTD_CStreamOutSize();
    void*  const buffOut = malloc_orDie(buffOutSize);

    /* Create the context. */
    ZSTD_CCtx* const cctx = ZSTD_createCCtx();
    CHECK(cctx != NULL, "ZSTD_createCCtx() failed!");

    /* Set any parameters you want.
     * Here we set the compression level, and enable the checksum.
     */
    CHECK_ZSTD( ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, cLevel) );
    CHECK_ZSTD( ZSTD_CCtx_setParameter(cctx, ZSTD_c_checksumFlag, 1) );
    ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, nbThreads);

    /* This loop read from the input file, compresses that entire chunk,
     * and writes all output produced to the output file.
     */
    size_t const toRead = buffInSize;
    for (;;) {
        size_t read = fread_orDie(buffIn, toRead, fin);
        /* Select the flush mode.
         * If the read may not be finished (read == toRead) we use
         * ZSTD_e_continue. If this is the last chunk, we use ZSTD_e_end.
         * Zstd optimizes the case where the first flush mode is ZSTD_e_end,
         * since it knows it is compressing the entire source in one pass.
         */
        int const lastChunk = (read < toRead);
        ZSTD_EndDirective const mode = lastChunk ? ZSTD_e_end : ZSTD_e_continue;
        /* Set the input buffer to what we just read.
         * We compress until the input buffer is empty, each time flushing the
         * output.
         */
        ZSTD_inBuffer input = { buffIn, read, 0 };
        int finished;
        do {
            /* Compress into the output buffer and write all of the output to
             * the file so we can reuse the buffer next iteration.
             */
            ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
            size_t const remaining = ZSTD_compressStream2(cctx, &output , &input, mode);
            CHECK_ZSTD(remaining);
            fwrite_orDie(buffOut, output.pos, fout);
            /* If we're on the last chunk we're finished when zstd returns 0,
             * which means its consumed all the input AND finished the frame.
             * Otherwise, we're finished when we've consumed all the input.
             */
            finished = lastChunk ? (remaining == 0) : (input.pos == input.size);
        } while (!finished);
        CHECK(input.pos == input.size,
              "Impossible: zstd only returns 0 when the input is completely consumed!");

        if (lastChunk) {
            break;
        }
    }

    ZSTD_freeCCtx(cctx);
    fclose_orDie(fout);
    fclose_orDie(fin);
    free(buffIn);
    free(buffOut);
}

void decompress_file(const char* fin_name,const char* fout_name)
{
    FILE* const fin  = fopen_orDie(fin_name, "rb");
    size_t const buffInSize = ZSTD_DStreamInSize();
    void*  const buffIn  = malloc_orDie(buffInSize);
    FILE* const fout = fopen_orDie(fout_name, "wb+");
    size_t const buffOutSize = ZSTD_DStreamOutSize();  /* Guarantee to successfully flush at least one complete compressed block in all circumstances. */
    void*  const buffOut = malloc_orDie(buffOutSize);

    ZSTD_DCtx* const dctx = ZSTD_createDCtx();
    CHECK(dctx != NULL, "ZSTD_createDCtx() failed!");

    /* This loop assumes that the input file is one or more concatenated zstd
     * streams. This example won't work if there is trailing non-zstd data at
     * the end, but streaming decompression in general handles this case.
     * ZSTD_decompressStream() returns 0 exactly when the frame is completed,
     * and doesn't consume input after the frame.
     */
    size_t const toRead = buffInSize;
    size_t read;
    size_t lastRet = 0;
    int isEmpty = 1;
    while ( (read = fread_orDie(buffIn, toRead, fin)) ) {
        isEmpty = 0;
        ZSTD_inBuffer input = { buffIn, read, 0 };
        /* Given a valid frame, zstd won't consume the last byte of the frame
         * until it has flushed all of the decompressed data of the frame.
         * Therefore, instead of checking if the return code is 0, we can
         * decompress just check if input.pos < input.size.
         */
        while (input.pos < input.size) {
            ZSTD_outBuffer output = { buffOut, buffOutSize, 0 };
            /* The return code is zero if the frame is complete, but there may
             * be multiple frames concatenated together. Zstd will automatically
             * reset the context when a frame is complete. Still, calling
             * ZSTD_DCtx_reset() can be useful to reset the context to a clean
             * state, for instance if the last decompression call returned an
             * error.
             */
            size_t const ret = ZSTD_decompressStream(dctx, &output , &input);
            CHECK_ZSTD(ret);
            fwrite_orDie(buffOut, output.pos, fout);
            lastRet = ret;
        }
    }

    if (isEmpty) {
        fprintf(stderr, "input is empty\n");
        exit(1);
    }

    if (lastRet != 0) {
        /* The last return value from ZSTD_decompressStream did not end on a
         * frame, but we reached the end of the file! We assume this is an
         * error, and the input was truncated.
         */
        fprintf(stderr, "EOF before end of stream: %zu\n", lastRet);
        exit(1);
    }

    ZSTD_freeDCtx(dctx);
    fclose_orDie(fin);
    fclose_orDie(fout);
    free(buffIn);
    free(buffOut);
}

};  // namespace ZSTD

// static char* createOutFilename_orDie(const char* filename)
// {
//     size_t const inL = strlen(filename);
//     size_t const outL = inL + 5;
//     void* const outSpace = malloc_orDie(outL);
//     memset(outSpace, 0, outL);
//     strcat(outSpace, filename);
//     strcat(outSpace, ".zst");
//     return (char*)outSpace;
// }

// int main(int argc, const char** argv)
// {
//     const char* const exeName = argv[0];

//     if (argc < 2) {
//         printf("wrong arguments\n");
//         printf("usage:\n");
//         printf("%s FILE [LEVEL] [THREADS]\n", exeName);
//         return 1;
//     }

//     int cLevel = 1;
//     int nbThreads = 4;

//     if (argc >= 3) {
//       cLevel = atoi (argv[2]);
//       CHECK(cLevel != 0, "can't parse LEVEL!");
//     }

//     if (argc >= 4) {
//       nbThreads = atoi (argv[3]);
//       CHECK(nbThreads != 0, "can't parse THREADS!");
//     }

//     const char* const inFilename = argv[1];

//     char* const outFilename = createOutFilename_orDie(inFilename);
//     compressFile_orDie(inFilename, outFilename, cLevel, nbThreads);

//     free(outFilename);   /* not strictly required, since program execution stops there,
//                           * but some static analyzer may complain otherwise */
//     return 0;
// }

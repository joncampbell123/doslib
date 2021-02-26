
#include <cstdio>
#include <iostream>
#include <fstream>

#include <stdint.h>
#include <assert.h>
#include <endian.h>

using namespace std;

uint32_t ifdw(ifstream &f) {
    assert(sizeof(uint32_t) == 4);
    uint32_t w;

    if (f.eof()) return 0;

    f.read((char*)(&w),4);

    if (f.eof()) return 0;

    return be32toh(w);
}

int main(int argc,char **argv) {
    uint32_t w,addr=0;

    if (argc < 2) {
        fprintf(stderr,"Specify a file\n");
        return 1;
    }

    ifstream ifs(argv[1], ifstream::in | ifstream::binary);
    if (!ifs.is_open()) {
        fprintf(stderr,"Failed to open\n");
        return 1;
    }

    while (!ifs.eof()) {
        w = ifdw(ifs);

        printf("%08x: %08x  ",addr,w);
        printf("\n");

        addr += 4;
    }

    return 0;
}


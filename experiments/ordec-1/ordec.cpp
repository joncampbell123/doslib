
#include <cstdio>
#include <string>
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

template <class V,class M> static inline V sgnextmsk(const V val,const M bits) {
    const V sgn = (V)1u << (V)(bits - (M)1u);
    const V msk = sgn - (V)1u;

    if (val & sgn)
        return (val & msk) - sgn;
    else
        return (val & msk);
}

template <class V> string or1k_hex(const V val);

template <> string or1k_hex<uint32_t>(const uint32_t val) {
    char tmp[2+(2*4)+1];

    sprintf(tmp,"0x%08lx",(unsigned long)val);
    return tmp;
}

void or1k_dec(string &s,uint32_t w,const uint32_t addr) {
    int32_t ti32;

    s.clear();
    switch (w >> ((uint32_t)26)) {
        case 0x0: /* l.j <signed 26-bit relative address in words> */
            ti32 = sgnextmsk(w,(uint32_t)26) << (int32_t)2;
            s  = "l.j         "; // 12-char
            s += or1k_hex((uint32_t)(ti32 + addr));
            return;
        case 0x5:
            if ((w&0xFF000000ul) == 0x15000000ul) {
                s = "l.nop       "; // 12-char
                return;
            }
            break;
    }

    s = "(unknown)";
}

int main(int argc,char **argv) {
    uint32_t w,addr=0;
    string ins;

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

        or1k_dec(ins,w,addr);
        printf("%08x: %08x  %s",addr,w,ins.c_str());
        printf("\n");

        addr += 4;
    }

    return 0;
}



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

template <class V,class M> static inline V zextmsk(const V val,const M bits) {
    const V msk = ((V)1u << (V)bits) - (V)1u;
    return (val & msk);
}

template <class V> string or1k_hex(const V val);
template <class V> string or1k_dec(const V val);

template <> string or1k_hex<uint32_t>(const uint32_t val) {
    char tmp[2+(2*4)+1];

    sprintf(tmp,"0x%08lx",(unsigned long)val);
    return tmp;
}

template <> string or1k_dec<int32_t>(const int32_t val) {
    char tmp[64];

    sprintf(tmp,"%ld",(signed long)val);
    return tmp;
}

string gregn(const unsigned char n) {
    char tmp[12];

    sprintf(tmp,"%u",n);
    return string("r") + tmp;
}

void or1k_dec(string &s,uint32_t w,const uint32_t addr) {
    (void)w;
    (void)addr;

    s.clear();

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


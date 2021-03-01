
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
    unsigned char rD,rA,rB;
    uint32_t tu32;
    int32_t ti32;

    s.clear();
    switch (w >> ((uint32_t)26)) {
        case 0x00: /* l.j <signed 26-bit relative address in words> */
            ti32 = sgnextmsk(w,(uint32_t)26) << (int32_t)2;
            s  = "l.j         "; // 12-char
            s += or1k_hex((uint32_t)(ti32 + addr));
            return;
        case 0x01: /* l.jal <signed 26-bit relative address in words> */
            ti32 = sgnextmsk(w,(uint32_t)26) << (int32_t)2;
            s  = "l.jal       "; // 12-char
            s += or1k_hex((uint32_t)(ti32 + addr));
            return;
        case 0x03: /* l.j <signed 26-bit relative address in words> */
            ti32 = sgnextmsk(w,(uint32_t)26) << (int32_t)2;
            s  = "l.bnf       "; // 12-char
            s += or1k_hex((uint32_t)(ti32 + addr));
            return;
        case 0x04: /* l.jal <signed 26-bit relative address in words> */
            ti32 = sgnextmsk(w,(uint32_t)26) << (int32_t)2;
            s  = "l.bf        "; // 12-char
            s += or1k_hex((uint32_t)(ti32 + addr));
            return;
        case 0x05:
            if ((w&0xFF000000ul) == 0x15000000ul) {
                s = "l.nop       "; // 12-char
                return;
            }
            break;
        case 0x06:
            if ((w&0x00010000ul) == 0) {
                s = "l.movhi     "; // 12-char
                tu32 = zextmsk(w,(uint32_t)16); /* immediate (K) */
                rD = (unsigned char)((w >> (uint32_t)21ul) & (uint32_t)0x1Ful);
                s += gregn(rD);
                s += ",";
                s += "hi16("; // GNU gas uses hi(val)
                s += or1k_hex((uint32_t)tu32 << (uint32_t)16);
                s += ")        ; high16 <= K << 16, K=";
                s += or1k_hex((uint32_t)tu32);
                return;
            }
            break;
        case 0x11: /* l.jr rB */
            rB = (unsigned char)((w >> (uint32_t)11ul) & (uint32_t)0x1Ful);
            s  = "l.jr        "; // 12-char
            s += gregn(rB);
            return;
        case 0x21: /* l.lwz rD,I(rA)      EA = I + rA */
            ti32 = sgnextmsk(w,(uint32_t)16); /* immediate */
            rD = (unsigned char)((w >> (uint32_t)21ul) & (uint32_t)0x1Ful);
            rA = (unsigned char)((w >> (uint32_t)16ul) & (uint32_t)0x1Ful);
            s  = "l.lwz       "; // 12-char
            s += gregn(rD);
            s += ",";
            s += or1k_dec(ti32);
            s += "(";
            s += gregn(rA);
            s += ")";
            s += "               ; (EA is ";
            s += gregn(rA);
            s += " + ";
            s += or1k_hex((uint32_t)ti32);
            s += ")";
            return;
        case 0x27: /* l.addi <rD,rA,Imm> */
            ti32 = sgnextmsk(w,(uint32_t)16); /* immediate */
            rA = (unsigned char)((w >> (uint32_t)16ul) & (uint32_t)0x1Ful);
            rD = (unsigned char)((w >> (uint32_t)21ul) & (uint32_t)0x1Ful);
            s  = "l.addi      "; // 12-char
            s += gregn(rD);
            s += ",";
            s += gregn(rA);
            s += ",";
            s += or1k_hex((uint32_t)ti32);
            s += "       ; (decimal ";
            s += or1k_dec(ti32);
            s += ")";
            return;
        case 0x2A: /* l.ori <rD,rA,K> */
            tu32 = zextmsk(w,(uint32_t)16); /* immediate K */
            rA = (unsigned char)((w >> (uint32_t)16ul) & (uint32_t)0x1Ful);
            rD = (unsigned char)((w >> (uint32_t)21ul) & (uint32_t)0x1Ful);
            s  = "l.ori       "; // 12-char
            s += gregn(rD);
            s += ",";
            s += gregn(rA);
            s += ",";
            s += or1k_hex((uint32_t)tu32);
            return;
        case 0x35: /* l.sw I(rA), rB     EA = I + rA */
            ti32 = (int32_t)sgnextmsk((((w >> (uint32_t)21ul) & (uint32_t)0x1Ful) << (uint32_t)11ul) + ((uint32_t)w & 0x7FFul),(uint32_t)16);
            rA = (unsigned char)((w >> (uint32_t)16ul) & (uint32_t)0x1Ful);
            rB = (unsigned char)((w >> (uint32_t)11ul) & (uint32_t)0x1Ful);
            s  = "l.sw        "; // 12-char
            s += or1k_dec(ti32);
            s += "(";
            s += gregn(rA);
            s += "),";
            s += gregn(rB);
            s += "               ; (EA is ";
            s += gregn(rA);
            s += " + ";
            s += or1k_hex((uint32_t)ti32);
            s += ")";
            return;
        case 0x38: /* l.add rD,rA,rB  [9:8] = 0  [3:0] = 0 */
            if ((w&0x300ul) == 0) {
                if ((w&0x0Ful) == 0) {
                    rD = (unsigned char)((w >> (uint32_t)21ul) & (uint32_t)0x1Ful);
                    rA = (unsigned char)((w >> (uint32_t)16ul) & (uint32_t)0x1Ful);
                    rB = (unsigned char)((w >> (uint32_t)11ul) & (uint32_t)0x1Ful);
                    s  = "l.add       "; // 12-char
                    s += gregn(rD);
                    s += ",";
                    s += gregn(rA);
                    s += ",";
                    s += gregn(rB);
                    return;
                }
                else if ((w&0x0Ful) == 2) {
                    rD = (unsigned char)((w >> (uint32_t)21ul) & (uint32_t)0x1Ful);
                    rA = (unsigned char)((w >> (uint32_t)16ul) & (uint32_t)0x1Ful);
                    rB = (unsigned char)((w >> (uint32_t)11ul) & (uint32_t)0x1Ful);
                    s  = "l.sub       "; // 12-char
                    s += gregn(rD);
                    s += ",";
                    s += gregn(rA);
                    s += ",";
                    s += gregn(rB);
                    return;
                }
            }
            break;
        case 0x39:
            rD = (unsigned char)((w >> (uint32_t)21ul) & (uint32_t)0x1Ful);
            rA = (unsigned char)((w >> (uint32_t)16ul) & (uint32_t)0x1Ful);
            rB = (unsigned char)((w >> (uint32_t)11ul) & (uint32_t)0x1Ful);
            switch (rD) {
                case 0x0:
                    s  = "l.sfeq      "; // 12-char
                    s += gregn(rA);
                    s += ",";
                    s += gregn(rB);
                    return;
                case 0x1:
                    s  = "l.sfne      "; // 12-char
                    s += gregn(rA);
                    s += ",";
                    s += gregn(rB);
                    return;
                case 0xB:
                    s  = "l.sfges     "; // 12-char
                    s += gregn(rA);
                    s += ",";
                    s += gregn(rB);
                    return;
            }
            break;
        default:
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


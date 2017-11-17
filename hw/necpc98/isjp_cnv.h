/* UTF-8 to SHIFT-JIS string converted copy. Do not edit. Modifications will be lost when converted again. */


/* This is UTF-8. It will be converted to SHIFT-JIS by utility Perl script for display on PC-98 */
/* NTS: Google Translate has a bug, I think. It will say the end is "de" but it's supposed to end in "desu". Kore Wa Nihongo Desu, right? --J.C. */
#define THIS_IS_JAPANESE "\x82\xb1\x82\xea\x82\xcd\x93\xfa\x96\x7b\x8c\xea\x82\xc5\x82\xb7"/* UTF-8 to Shift-JIS of "これは日本語です" */

#define HIRAGANA_CHART \
    "    a   i   u   e   o" "\n" \
    "  +------------------" "\n" \
    "  | \x82\xa0  \x82\xa2  \x82\xa4  \x82\xa6  \x82\xa8"/* UTF-8 to Shift-JIS of "    a   i   u   e   o"\n"  +------------------"\n"  | あ  い  う  え  お" */ "\n" \
    "k | \x82\xa9  \x82\xab  \x82\xad  \x82\xaf  \x82\xb1"/* UTF-8 to Shift-JIS of "\n"k | か  き  く  け  こ" */ "\n" \
    "s | \x82\xb3  \x82\xb5  \x82\xb7  \x82\xb9  \x82\xbb"/* UTF-8 to Shift-JIS of "\n"s | さ  し  す  せ  そ" */ "\n" \
    "t | \x82\xbd  \x82\xbf  \x82\xc2  \x82\xc4  \x82\xc6"/* UTF-8 to Shift-JIS of "\n"t | た  ち  つ  て  と" */ "\n" \
    "n | \x82\xc8  \x82\xc9  \x82\xca  \x82\xcb  \x82\xcc"/* UTF-8 to Shift-JIS of "\n"n | な  に  ぬ  ね  の" */ "\n" \
    "h | \x82\xcd  \x82\xd0  \x82\xd3  \x82\xd6  \x82\xd9"/* UTF-8 to Shift-JIS of "\n"h | は  ひ  ふ  へ  ほ" */ "\n" \
    "m | \x82\xdc  \x82\xdd  \x82\xde  \x82\xdf  \x82\xe0"/* UTF-8 to Shift-JIS of "\n"m | ま  み  む  め  も" */ "\n" \
    "y | \x82\xe2      \x82\xe4      \x82\xe6"/* UTF-8 to Shift-JIS of "\n"y | や      ゆ      よ" */ "\n" \
    "r | \x82\xe7  \x82\xe8  \x82\xe9  \x82\xea  \x82\xeb"/* UTF-8 to Shift-JIS of "\n"r | ら  り  る  れ  ろ" */ "\n" \
    "w | \x82\xed  \x82\xee      \x82\xef  \x82\xf0"/* UTF-8 to Shift-JIS of "\n"w | わ  ゐ      ゑ  を" */ "\n" \
                             "\n" \
    "        \x82\xf1 (n)"/* UTF-8 to Shift-JIS of "\n"\n"        ん (n)" */         "\n"

#define KATAKANA_CHART \
    "    a   i   u   e   o" "\n" \
    "  +------------------" "\n" \
    "  | \x83\x41  \x83\x43  \x83\x45  \x83\x47  \x83\x49"/* UTF-8 to Shift-JIS of "\n"    a   i   u   e   o"\n"  +------------------"\n"  | ア  イ  ウ  エ  オ" */ "\n" \
    "k | \x83\x4a  \x83\x4c  \x83\x4e  \x83\x50  \x83\x52"/* UTF-8 to Shift-JIS of "\n"k | カ  キ  ク  ケ  コ" */ "\n" \
    "g | \x83\x4b  \x83\x4d  \x83\x4f  \x83\x51  \x83\x53"/* UTF-8 to Shift-JIS of "\n"g | ガ  ギ  グ  ゲ  ゴ" */ "\n" \
    "s | \x83\x54  \x83\x56  \x83\x58  \x83\x5a  \x83\x5c"/* UTF-8 to Shift-JIS of "\n"s | サ  シ  ス  セ  ソ" */ "\n" \
    "z | \x83\x55  \x83\x57  \x83\x59  \x83\x5b  \x83\x5d"/* UTF-8 to Shift-JIS of "\n"z | ザ  ジ  ズ  ゼ  ゾ" */ "\n" \
    "t | \x83\x5e  \x83\x60  \x83\x63  \x83\x65  \x83\x67"/* UTF-8 to Shift-JIS of "\n"t | タ  チ  ツ  テ  ト" */ "\n" \
    "d | \x83\x5f  \x83\x61  \x83\x64  \x83\x66  \x83\x68"/* UTF-8 to Shift-JIS of "\n"d | ダ  ヂ  ヅ  デ  ド" */ "\n" \
    "n | \x83\x69  \x83\x6a  \x83\x6b  \x83\x6c  \x83\x6d"/* UTF-8 to Shift-JIS of "\n"n | ナ  ニ  ヌ  ネ  ノ" */ "\n" \
    "h | \x83\x6e  \x83\x71  \x83\x74  \x83\x77  \x83\x7a"/* UTF-8 to Shift-JIS of "\n"h | ハ  ヒ  フ  ヘ  ホ" */ "\n" \
    "b | \x83\x6f  \x83\x72  \x83\x75  \x83\x78  \x83\x7b"/* UTF-8 to Shift-JIS of "\n"b | バ  ビ  ブ  ベ  ボ" */ "\n" \
    "p | \x83\x70  \x83\x73  \x83\x76  \x83\x79  \x83\x7c"/* UTF-8 to Shift-JIS of "\n"p | パ  ピ  プ  ペ  ポ" */ "\n" \
    "m | \x83\x7d  \x83\x7e  \x83\x80  \x83\x81  \x83\x82"/* UTF-8 to Shift-JIS of "\n"m | マ  ミ  ム  メ  モ" */ "\n" \
    "y | \x83\x84      \x83\x86      \x83\x88"/* UTF-8 to Shift-JIS of "\n"y | ヤ      ユ      ヨ" */ "\n" \
    "r | \x83\x89  \x83\x8a  \x83\x8b  \x83\x8c  \x83\x8d"/* UTF-8 to Shift-JIS of "\n"r | ラ  リ  ル  レ  ロ" */ "\n" \
    "w | \x83\x8f  \x83\x90      \x83\x91  \x83\x92"/* UTF-8 to Shift-JIS of "\n"w | ワ  ヰ      ヱ  ヲ" */ "\n" \
                             "\n" \
    "n           \x83\x93"/* UTF-8 to Shift-JIS of "\n"\n"n           ン" */         "\n"


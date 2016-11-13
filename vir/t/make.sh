#!/usr/bin/bash
if [ "$1" == "disk" ]; then
    xz -c -d ../boot.tmpl.xz >test.dsk

    mcopy -i test.dsk Techno.1123 ::vir/techno.com

    mcopy -i test.dsk ../../16-bit/hexmem/hexmem.com ::vir/hexmem.com
    mcopy -i test.dsk ../../16-bit/hellocom/hello1.com ::vir/_1.com
    mcopy -i test.dsk ../../16-bit/hellocom/hello1.com ::vir/_2.com
    mcopy -i test.dsk ../../16-bit/hellocom/hello1.com ::vir/_3.com
    mcopy -i test.dsk ../../16-bit/hellocom/hello1.com ::vir/_4.com
    mcopy -i test.dsk ../../16-bit/hellocom/hello1.com ::vir/_5.com
    mcopy -i test.dsk ../../16-bit/hellocom/hello1.com ::vir/_6.com
    mcopy -i test.dsk ../../16-bit/hellocom/hello1.com ::vir/_7.com
    mcopy -i test.dsk ../../16-bit/hellocom/hello1.com ::vir/_8.com
fi
if [ "$1" == "clean" ]; then
    rm -f test.dsk
    rm -f *~
fi


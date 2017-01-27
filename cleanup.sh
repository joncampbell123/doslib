#!/bin/bash
find -type d -name dos86s -exec rm -Rfv {} +
find -type d -name dos86sd -exec rm -Rfv {} +
find -type d -name dos86l -exec rm -Rfv {} +
find -type d -name dos86ld -exec rm -Rfv {} +
find -type d -name dos386f -exec rm -Rfv {} +
find -type d -name dos386fd -exec rm -Rfv {} +
find -type d -name DOS86S -exec rm -Rfv {} +
find -type d -name DOS86SD -exec rm -Rfv {} +
find -type d -name DOS86L -exec rm -Rfv {} +
find -type d -name DOS86LD -exec rm -Rfv {} +
find -type d -name DOS386F -exec rm -Rfv {} +
find -type d -name DOS386FD -exec rm -Rfv {} +
find -name tmp.cmd -exec rm -fv {} +
find -name tmp1.cmd -exec rm -fv {} +
find -name tmp2.cmd -exec rm -fv {} +
find -name cargs.cmd -exec rm -fv {} +
./buildall.sh clean
find -type l -delete
find -name \*~ -delete
find -name tmp.cmd -delete
find -name tmp1.cmd -delete
find -name tmp2.cmd -delete
find -name cargs.cmd -delete
for ext in err obj sym map exe lib com; do find -name \*.$ext -delete; done
find -name foo.gz -delete
find -name FOO.GZ -delete
find -name \*.\$\$\$ -delete
rm -v std{out,err}.txt
# FIXME: Until these folders stop coming back, remove them at every opportunity!
rmdir -p 16-bit/* 2>/dev/null
rmdir -p 16-bit 2>/dev/null
rmdir -p 32-bit/* 2>/dev/null
rmdir -p 32-bit 2>/dev/null


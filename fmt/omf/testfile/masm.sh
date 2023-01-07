#!/bin/bash
asm="$1"
obj="$2"
uc=`echo "$obj" | tr '[:lower:]' '[:upper:]'`
rm -f "$obj" "$uc"

cat >dosbox.conf <<_EOF
[autoexec]
mount c: .
c:
masm $asm,$obj;
_EOF

/usr/src/dosbox-x/src/dosbox-x -conf dosbox.conf -fastlaunch -exit -log-con
rm -f dosbox.conf
mv -v "$uc" "$obj"
[ -f "$obj" ] || exit 1
exit 0


#!/bin/bash
rm -Rfv {*/*,*/*/*,*/*/*/*}/*.{BAT,bat}
rm -Rfv {*/*,*/*/*,*/*/*/*}/*.{CMD,cmd}
find -name \*.cmd -exec rm -Rfv {} +
find -name \*.err -exec rm -Rfv {} +
find -name \*.bat -exec rm -Rfv {} +
find -name \*.BAT -exec rm -Rfv {} +
exit 0


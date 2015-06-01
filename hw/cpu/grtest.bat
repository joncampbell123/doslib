@echo off
echo Make sure you are running this from the dos86l or dos386f dir.
echo Grind test in progress. Results will be written to root directory of this drive.
rem MS-DOS doesn't support >> append mode. Argh.
test >\GRIND1.TXT
grind >\GRIND2.TXT
copy \GRIND1.TXT+\GRIND2.TXT \GRIND.TXT
del \GRIND1.TXT
del \GRIND2.TXT

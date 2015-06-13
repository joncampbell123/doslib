@echo off
echo Make sure you are running this from the dos86l or dos386f dir.
echo Grind test in progress. Results will be written to the current directory.
echo Look for GR_ADD.CPU and GR_ADD.BIN, GR_SUB.BIN, GR_MUL.BIN, and GR_DIV.BIN.
rem MS-DOS doesn't support >> append mode. Argh.
test >GR_ADD.CPU
gr_add


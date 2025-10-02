
all: ccminic

ccminic: ccminic.o cctokentype.o ccsfo.o ccsfo_fd.o ccsft.o ccddipmergeable.o ccrbufcslit.o cccslit.o ccsourcefiles.o ccrbufsfrefill.o ccrbuf.o ccpathslashxlate.o ccpathcomb.o ccutf8e.o ccutf16e.o ccutf8d.o ccutf16d.o ccgetc.o ccstorcls.o cctypespec.o cctypequal.o cctargetcpu.o ccdatatypepresets.o ccfloatval.o ccintegerval.o ccidentifier.o ccidentifierlist.o ccerr.o cctoken.o ccctype.o ccrwhitespace.o ccrcomment.o ccrchkarrowstr.o cclgtokrstrlit.o cclgtokcslitget.o cclgtok.o ccpptokmacro.o ccpptokstate.o ccpptokdefallowed.o ccpptoklgtok.o ccpptok.o
	g++ -std=c++11 -o $@ $+

%.o: %.cpp
	g++ -g3 -O0 -std=c++11 -Wall -pedantic -Wextra -c -o $@ $+

clean:
	rm -f ccminic *.o


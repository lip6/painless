MAKEFLAGS=-j $(if $(CORES),$(CORES),1)

CC=gcc
CFLAGS=-W -Wall -O3 -I../yalsat -DNLGLOG -DNDEBUG -DNCHKSOL -DNLGLDRUPLIG -DNLGLFILES -DNLGLDEMA

LIBS=-lm -L../yalsat -lyals
LDEPS=../yalsat/libyals.a
HDEPS=../yalsat/yals.h
EXTRAOBJS=
AIGER=

all: targets

-include makefile.bcd
-include makefile.tune
-include makefile.mus
-include makefile.other

targets: liblgl.a
targets: lingeling plingeling ilingeling treengeling
targets: lglmbt lgluntrace lglddtrace
targets: 

analyze:
	clang --analyze $(CFLAGS) $(shell ls *.c *.h)

liblgl.a: lglib.o lglbnr.o lgldimacs.o lglopts.o makefile $(EXTRAOBJS)
	ar rc $@ lglib.o lglbnr.o lgldimacs.o lglopts.o $(EXTRAOBJS)
	ranlib $@

lingeling: lglmain.o liblgl.a makefile $(LDEPS)
	$(CC) $(CFLAGS) -o $@ lglmain.o -L. -llgl $(LIBS)
plingeling: plingeling.o liblgl.a makefile $(LDEPS)
	$(CC) $(CFLAGS) -pthread -o $@ plingeling.o -L. -llgl $(LIBS)
treengeling: treengeling.o liblgl.a makefile $(LDEPS)
	$(CC) $(CFLAGS) -pthread -o $@ treengeling.o -L. -llgl $(LIBS)
blimc: blimc.o liblgl.a makefile $(AIGER)/aiger.o $(LDEPS)
	$(CC) $(CFLAGS) -o $@ blimc.o -L. $(AIGER)/aiger.o -llgl $(LIBS)
ilingeling: ilingeling.o liblgl.a makefile $(LDEPS)
	$(CC) $(CFLAGS) -pthread -o $@ ilingeling.o -L. -llgl $(LIBS)
lglmbt: lglmbt.o liblgl.a $(LDEPS)
	$(CC) $(CFLAGS) -o $@ lglmbt.o -L. -llgl $(LIBS)
lgluntrace: lgluntrace.o liblgl.a makefile $(LDEPS)
	$(CC) $(CFLAGS) -o $@ lgluntrace.o -L. -llgl $(LIBS)
lglddtrace: lglddtrace.o liblgl.a makefile $(LDEPS)
	$(CC) $(CFLAGS) -o $@ lglddtrace.o -L. -llgl $(LIBS)

lglmain.o: lglmain.c lglib.h makefile
	$(CC) $(CFLAGS) -c lglmain.c
plingeling.o: plingeling.c lglib.h makefile
	$(CC) $(CFLAGS) -c plingeling.c
treengeling.o: treengeling.c lglib.h makefile
	$(CC) $(CFLAGS) -c treengeling.c
ilingeling.o: ilingeling.c lglib.h makefile
	$(CC) $(CFLAGS) -c ilingeling.c
blimc.o: blimc.c lglib.h $(AIGER)/aiger.h makefile
	$(CC) $(CFLAGS) -I$(AIGER) -c -o $@ blimc.c
lglmbt.o: lglmbt.c lglib.h makefile
	$(CC) $(CFLAGS) -c lglmbt.c
lgluntrace.o: lgluntrace.c lglib.h makefile
	$(CC) $(CFLAGS) -c lgluntrace.c
lglddtrace.o: lglddtrace.c lglib.h makefile
	$(CC) $(CFLAGS) -c lglddtrace.c

lglib.o: lglib.c lglib.h lglconst.h lgloptl.h lglopts.h makefile $(HDEPS)
	$(CC) $(CFLAGS) -c lglib.c
lgldimacs.o: lgldimacs.c lgldimacs.h makefile
	$(CC) $(CFLAGS) -c lgldimacs.c
lglbnr.o: lglbnr.c lglcfg.h lglcflags.h makefile
	$(CC) $(CFLAGS) -c lglbnr.c
lglopts.o: lglopts.c lglopts.h lglconst.h lgloptl.h makefile
	$(CC) $(CFLAGS) -c lglopts.c

lglcfg.h: VERSION mkconfig.sh lglbnr.c lglib.c lglmain.c lglcflags.h makefile
	rm -f $@
	./mkconfig.sh > $@
lglcflags.h: makefile
	rm -f $@
	echo '#define LGL_CC "$(shell $(CC) --version|head -1)"' >> $@
	echo '#define LGL_CFLAGS "$(CFLAGS)"' >> $@

test: lingeling plingeling lglmbt
	./lglmbt -m 100

clean: clean-all clean-config
clean-config:
	rm -f makefile lglcfg.h lglcflags.h
clean-all:
	rm -f lingeling plingeling ilingeling treengeling blimc
	rm -f lglmbt lgluntrace lglddtrace
	rm -f *.gcno *.gcda cscope.out gmon.out *.gcov *.gch *.plist
	rm -f *.E *.o *.s *.a log/*.log 

.PHONY: all targets test clean clean-config clean-all

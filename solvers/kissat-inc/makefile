all:
	$(MAKE) -C "/home/mazigh/Develop/official/painless/solvers/kissat-inc/build"
kissat:
	$(MAKE) -C "/home/mazigh/Develop/official/painless/solvers/kissat-inc/build" kissat
tissat:
	$(MAKE) -C "/home/mazigh/Develop/official/painless/solvers/kissat-inc/build" tissat
clean:
	rm -f "/home/mazigh/Develop/official/painless/solvers/kissat-inc"/makefile
	-$(MAKE) -C "/home/mazigh/Develop/official/painless/solvers/kissat-inc/build" clean
	rm -rf "/home/mazigh/Develop/official/painless/solvers/kissat-inc/build"
coverage:
	$(MAKE) -C "/home/mazigh/Develop/official/painless/solvers/kissat-inc/build" coverage
indent:
	$(MAKE) -C "/home/mazigh/Develop/official/painless/solvers/kissat-inc/build" indent
test:
	$(MAKE) -C "/home/mazigh/Develop/official/painless/solvers/kissat-inc/build" test
.PHONY: all clean coverage indent kissat test tissat

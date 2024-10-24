all:
	$(MAKE) -C "/home/tbdoneal/SAT/official-painless/painless/solvers/kissat-inc/build"
kissat:
	$(MAKE) -C "/home/tbdoneal/SAT/official-painless/painless/solvers/kissat-inc/build" kissat
tissat:
	$(MAKE) -C "/home/tbdoneal/SAT/official-painless/painless/solvers/kissat-inc/build" tissat
clean:
	rm -f "/home/tbdoneal/SAT/official-painless/painless/solvers/kissat-inc"/makefile
	-$(MAKE) -C "/home/tbdoneal/SAT/official-painless/painless/solvers/kissat-inc/build" clean
	rm -rf "/home/tbdoneal/SAT/official-painless/painless/solvers/kissat-inc/build"
coverage:
	$(MAKE) -C "/home/tbdoneal/SAT/official-painless/painless/solvers/kissat-inc/build" coverage
indent:
	$(MAKE) -C "/home/tbdoneal/SAT/official-painless/painless/solvers/kissat-inc/build" indent
test:
	$(MAKE) -C "/home/tbdoneal/SAT/official-painless/painless/solvers/kissat-inc/build" test
.PHONY: all clean coverage indent kissat test tissat

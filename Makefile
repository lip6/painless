all:
	##################################################
	###                    m4ri                    ###
	##################################################
	cd libs/m4ri-20200125 && autoreconf --install && ./configure --enable-thread-safe
	+ $(MAKE) -C libs/m4ri-20200125
	
	##################################################
	###                 kissat_mab                 ###
	##################################################
	cd solvers/kissat_mab && bash ./configure --compact
	+ $(MAKE) -C solvers/kissat_mab

	##################################################
	###               GASPIKISSAT                  ###
	##################################################
	cd solvers/GASPIKISSAT && bash ./configure --compact --quiet
	+ $(MAKE) -C solvers/GASPIKISSAT

	##################################################
	###                   yalsat                   ###
	################################################## 
	cd solvers/yalsat && bash ./configure.sh
	+ $(MAKE) -C solvers/yalsat

	##################################################
	###                 MapleCOMSPS                ###
	##################################################
	+ $(MAKE) -C solvers/mapleCOMSPS r


	##################################################
	###                   PaInleSS                 ###
	##################################################
	+ $(MAKE) -C painless-src
	mv painless-src/painless painless

clean:
	##################################################
	###                   Clean                    ###
	##################################################
	+ $(MAKE) clean -C solvers/kissat_mab -f makefile.in
	rm -rf solvers/kissat_mab/build
	+ $(MAKE) clean -C solvers/GASPIKISSAT -f makefile.in
	rm -rf solvers/GASPIKISSAT/build
	+ $(MAKE) -C solvers/mapleCOMSPS clean
	+ $(MAKE) clean -C painless-src
	rm -f painless

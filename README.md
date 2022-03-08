Painless: a Framework for Parallel SAT Solving 
==============================================

* Ludovic LE FRIOUX (ludovic@lefrioux.fr)
* Vincent VALLADE (vincent.vallade@lip6.fr) 
* Saeed Nejati
* Souheib Baarir

Content
-------
* painless-src/:
   Contains the code of the framework.
   * clauses/:
      Contains the code to manage shared clauses.
   * working/:
      Code links to the worker organization.
   * sharing/:
      Code links to the learnt clause sharing management.
   * solvers/:
      Contains wrapper for the sequential solvers.
   * utils/:
      Contains code for clauses management. But also useful data structures.

* mapleCOMSPS/:
   Contains the code of MapleCOMSPS from the SAT Competition 17 with some little changes.

* slime/:
   Contains the code of Slime from the SAT Competition 2021 with our Bayesian Moment Matching algorithm.
   * utils/SearchInitializer.cc:
      Contains the BMM implementation.
   * core/Solver.cc:
      Contains the CDCL algorithm (BMM is added to the solve_() and search() methods).

To compile the project
----------------------

* In the painless-mcomsps home directory use 'make' to compile the code.

* In the painless-mcomsps home directory use 'make clean' to clean.


To run the solvers
------------------
* painless-mcomsps

Exemple: 

- Run a solver with 26 sequential solver, 2 Sharer, all-to-all sharing, and BMM inprocessing:

  ./painless-mcomsps dimacs\_filename -c=26 -shr-strat=1 -pol-init=1 -act-init=1 -restart-gap=50

- Run a Portfolio of Slime solvers (without BMM) in the same configuration as above:

  ./painless-mcomsps dimacs\_filename -c=26 -shr-strat=1 -pol-init=0 -act-init=0 -sls

- Run a solver with half SLIME/half SLIME-BMM Sequential Worker:

  ./painless-mcomsps dimacs\_filename -c=26 -shr-strat=1 -slime=2

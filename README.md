##### PaInleSS: A Framework for Parallel SAT Solving #####

Authors:  Ludovic LE FRIOUX, Vincent VALLADE, Souheib Baarir

Contacts:  vincent.vallade@lip6.fr, Souheib.baarir@lip6.fr

===== Content =====

- painless-src:
   Contains the code of the framework.
   - clauses:
      Contains the code to manage shared clauses.
   - working:
      Code links to the worker organization.
   - sharing:
      Code links to the learnt clause sharing management.
   - solvers:
      Contains wrapper for the sequential solvers.
   - utils:
      Contains code for clauses management. But also useful data structures.

- mapleCOMSPS:
   Contains the code of MapleCOMSPS from the SAT Competition 17 with some little
   changes.


===== To compile the project =====

- In the painless-mcomsps home directory use 'make' to compile.

- In the painless-mcomsps home directory use 'make clean' to clean.


===== To run the solvers =====

- P-MCOMSPS-AI :
   ./painless-mcomsps -c=#cpus -shr-strat=2 -shr-sleep=1500000 -lbd-limit=2 dimacs_filename

- P-MCOMSPS-L2:
   ./painless-mcomsps -c=#cpus -shr-strat=1 -shr-sleep=500000 -lbd-limit=2 dimacs_filename

- P-MCOMSPS-L4:
   ./painless-mcomsps -c=#cpus -shr-strat=1 -shr-sleep=500000 -lbd-limit=4 dimacs_filename

To use these solvers with strengthening add -strength flag and -c must be >= 2.

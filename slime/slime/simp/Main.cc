/***************************************************************************************
SLIME -- Copyright (c) 2021, Oscar Riveros, oscar.riveros@peqnp.science,
Santiago, Chile. https://github.com/maxtuno/SLIME

DurianSat -- Copyright (c) 2020, Arijit Shaw, Kuldeep S. Meel

SLIME -- Copyright (c) 2019, Oscar Riveros, oscar.riveros@peqnp.science,
Santiago, Chile. https://maxtuno.github.io/slime-sat-solver

Maple_LCM_Dist_Chrono -- Copyright (c) 2018, Vadim Ryvchin, Alexander Nadel

GlucoseNbSAT -- Copyright (c) 2016,Chu Min LI,Mao Luo and Fan Xiao
                           Huazhong University of science and technology, China
                           MIS, Univ. Picardie Jules Verne, France

MapleSAT -- Copyright (c) 2016, Jia Hui Liang, Vijay Ganesh

MiniSat -- Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
           Copyright (c) 2007-2010  Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <slime/core/Dimacs.h>
#include <slime/simp/SimpSolver.h>
#include <slime/core/SolverTypes.h>
#include <iostream>

#ifdef MASSIVE

#include <mpi.h>

#endif

#define DRAT // Generate unsat proof.

using namespace SLIME;

#if _WIN32 || _WIN64
#include <io.h>
#include <fcntl.h>
void printHeader() {
    _setmode(_fileno(stdout), _O_U16TEXT);
    std::wcout << L"c                                         \n";
    std::wcout << L"c   ██████  ██▓     ██▓ ███▄ ▄███▓▓█████  \n";
    std::wcout << L"c ▒██    ▒ ▓██▒    ▓██▒▓██▒▀█▀ ██▒▓█   ▀  \n";
    std::wcout << L"c ░ ▓██▄   ▒██░    ▒██▒▓██    ▓██░▒███    \n";
    std::wcout << L"c   ▒   ██▒▒██░    ░██░▒██    ▒██ ▒▓█  ▄  \n";
    std::wcout << L"c ▒██████▒▒░██████▒░██░▒██▒   ░██▒░▒████▒ \n";
    std::wcout << L"c ▒ ▒▓▒ ▒ ░░ ▒░▓  ░░▓  ░ ▒░   ░  ░░░ ▒░ ░ \n";
    std::wcout << L"c ░ ░▒  ░ ░░ ░ ▒  ░ ▒ ░░  ░      ░ ░ ░  ░ \n";
    std::wcout << L"c ░  ░  ░    ░ ░    ▒ ░░      ░      ░    \n";
    std::wcout << L"c       ░      ░  ░ ░         ░      ░  ░ \n";
    std::wcout << L"c                                         \n";
    std::wcout << L"c           http://www.peqnp.com          \n";
    std::wcout << L"c                                         \n";
    _setmode(_fileno(stdout), _O_TEXT);
}
#else

void printHeader() {
    return;
    printf("c                                         \n");
    printf("c   ██████  ██▓     ██▓ ███▄ ▄███▓▓█████  \n");
    printf("c ▒██    ▒ ▓██▒    ▓██▒▓██▒▀█▀ ██▒▓█   ▀  \n");
    printf("c ░ ▓██▄   ▒██░    ▒██▒▓██    ▓██░▒███    \n");
    printf("c   ▒   ██▒▒██░    ░██░▒██    ▒██ ▒▓█  ▄  \n");
    printf("c ▒██████▒▒░██████▒░██░▒██▒   ░██▒░▒████▒ \n");
    printf("c ▒ ▒▓▒ ▒ ░░ ▒░▓  ░░▓  ░ ▒░   ░  ░░░ ▒░ ░ \n");
    printf("c ░ ░▒  ░ ░░ ░ ▒  ░ ▒ ░░  ░      ░ ░ ░  ░ \n");
    printf("c ░  ░  ░    ░ ░    ▒ ░░      ░      ░    \n");
    printf("c       ░      ░  ░ ░         ░      ░  ░ \n");
    printf("c                                         \n");
    printf("c           http://www.peqnp.com          \n");
    printf("c                                         \n");
}

#endif

int main(int argc, char *argv[]) {

    if (argc == 1) {
        printf("SLIME -- Copyright (c) 2021, Oscar Riveros, oscar.riveros@peqnp.science,\n");
        printf("Santiago, Chile. https://github.com/maxtuno/SLIME\n");
        printf("\n");
        printf("DurianSat -- Copyright (c) 2020, Arijit Shaw, Kuldeep S. Meel\n");
        printf("\n");
        printf("SLIME -- Copyright (c) 2019, Oscar Riveros, oscar.riveros@peqnp.science,\n");
        printf("Santiago, Chile. https://maxtuno.github.io/slime-sat-solver\n");
        printf("\n");
        printf("Maple_LCM_Dist_Chrono -- Copyright (c) 2018, Vadim Ryvchin, Alexander Nadel\n");
        printf("\n");
        printf("GlucoseNbSAT -- Copyright (c) 2016,Chu Min LI,Mao Luo and Fan Xiao\n");
        printf("                Huazhong University of science and technology, China\n");
        printf("                MIS, Univ. Picardie Jules Verne, France\n");
        printf("\n");
        printf("MapleSAT -- Copyright (c) 2016, Jia Hui Liang, Vijay Ganesh\n");
        printf("\n");
        printf("MiniSat -- Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson\n");
        printf("           Copyright (c) 2007-2010  Niklas Sorensson\n");
        printf("\n");
        printf("Permission is hereby granted, free of charge, to any person obtaining a\n");
        printf("copy of this software and associated documentation files (the\n");
        printf("\"Software\", to deal in the Software without restriction, including\n");
        printf("without limitation the rights to use, copy, modify, merge, publish,\n");
        printf("distribute, sublicense, and/or sell copies of the Software, and to\n");
        printf("permit persons to whom the Software is furnished to do so, subject to\n");
        printf("the following conditions:\n");
        printf("\n");
        printf("The above copyright notice and this permission notice shall be included\n");
        printf("in all copies or substantial portions of the Software.\n");
        printf("\n");
        printf("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS\n");
        printf("OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF\n");
        printf("MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND\n");
        printf("NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE\n");
        printf("LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION\n");
        printf("OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION\n");
        printf("WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n");
        exit(0);
    }

    parseOptions(argc, argv, true);

    SimpSolver S;

#ifdef MASSIVE    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &S.rank);
    MPI_Comm_size(MPI_COMM_WORLD, &S.size);
#else
    S.rank = 0;
    S.size = 1;
#endif

    S.render = true;

    if (S.rank == 0) {
        printHeader();
    }

#ifdef LOG
    S.log = true;
#else
    S.log = false;
#endif

#ifdef DRAT
    if (argc > 2) {
#ifdef MASSIVE
        char *proof_file = (char *) calloc(strlen(argv[2]), sizeof(char) + 10);
        sprintf(proof_file, "%s_%d", argv[2], S.rank);
        S.drup_file = fopen(proof_file, "wb");
#else
        S.drup_file = fopen(argv[2], "wb");
#endif
    }
#endif

    FILE *in = fopen(argv[1], "rb");
    if (in == NULL) {
        std::cout << "c ERROR! Could not open file: " << argv[1] << std::endl;
        return EXIT_FAILURE;
    }
    parse_DIMACS(in, S);
    fclose(in);

    S.cursor = 0;
    S.eliminate();

    vec<Lit> assumptions;
    lbool result = S.solveLimited(assumptions);

#ifdef MASSIVE
    printf("c [%i]\n", S.rank);
#endif

    printf(result == l_True ? "s SATISFIABLE\nv " : result == l_False ? "s UNSATISFIABLE\n" : "s UNKNOWN\n");
    if (result == l_True) {
        for (int i = 0; i < S.nVars(); i++) {        
            if (S.model[i] != l_Undef) {
                printf("%s%s%d", (i == 0) ? "" : " ", (S.model[i] == l_True) ? "" : "-", i + 1);
            }
            if (i && i % 10 == 0) {
                printf("\nv");                
            }
        }
        printf(" 0\n");
    } else {
#ifdef DRAT
        if (argc > 2) {
            fputc('a', S.drup_file);
            fputc(0, S.drup_file);
            fclose(S.drup_file);
        }
#endif
    }

    if (argc > 3) {
#ifdef MASSIVE
        char *model_file = (char *) calloc(strlen(argv[3]), sizeof(char) + 10);
        sprintf(model_file, "%s_%d", argv[3], S.rank);
        FILE *model = fopen(model_file, "wb");
#else
        FILE *model = fopen(argv[3], "w");
#endif
        fprintf(model, result == l_True ? "SAT\n" : result == l_False ? "UNSAT\n" : "UNKNOWN\n");
        if (result == l_True) {
            for (int i = 0; i < S.nVars(); i++)
                if (S.model[i] != l_Undef) {
                    fprintf(model, "%s%s%d", (i == 0) ? "" : " ", (S.model[i] == l_True) ? "" : "-", i + 1);
                }
            fprintf(model, " 0\n");
        }
        fclose(model);
    }

#ifdef MASSIVE
    MPI_Abort(MPI_COMM_WORLD, EXIT_SUCCESS);
    MPI_Finalize();
#endif

    exit(result == l_True ? 10 : result == l_False ? 20 : 0);
}

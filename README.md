Painless: A Framework for Parallel SAT Solving
============================================== 
[TOC]
## Overview
Painless is a flexible framework for parallel and distributed SAT solving that integrates multiple state-of-the-art SAT solvers and preprocessing techniques.

- The doxygen different [Topics](https://lip6.github.io/painless/topics.html) is a good introduction to the different components in the framework.
- For TACAS25 experiment reproducability, please check [TACAS25](./docs/source/TACAS25.md).
- For an overview on the main interfaces, please check [Main Interfaces](./docs/source/DevelopComponents.md).

## Contacts
* Souheib BAARIR [souheib.baarir@lip6.fr](mailto:souheib.baarir@lip6.fr)
* Mazigh SAOUDI [mazigh.saoudi@epita.fr](mailto:mazigh.saoudi@epita.fr)

## Project Structure

### Core Components
- `src/containers/`: Data structures for clause management and formula representation
- `src/sharing/`: Learnt clause sharing management and strategies
- `src/working/`: Worker organization and portfolio implementations
- `src/solvers/`: Solver interface and implementations
- `src/utils/`: Helper utilities and data structures
- `src/preprocessors/`: Preprocessing techniques integration

### Integrated SAT Solvers
The framework integrates several state-of-the-art SAT solvers:
- [Kissat](https://github.com/arminbiere/kissat/tree/71caafb4d182ced9f76cef45b00f37cc598f2a37) (v3.1.1)
- [CaDiCaL](https://github.com/arminbiere/cadical/tree/24d047563f5f4c9e37a74c04fa30059b2bbc4214) (v1.9.1)
- [MapleCOMSPS](https://maplesat.github.io/solvers.html) (SAT Competition 17)
- [Glucose](https://www.labri.fr/perso/lsimon/glucose/)
- [MiniSat](http://minisat.se/)
- [Lingeling](https://github.com/arminbiere/lingeling)
- [YalSAT](https://github.com/arminbiere/yalsat)
- Kissat-MAB and Kissat-INC variants

### Preprocessing Integration
The framework includes preprocessing techniques from:
- [PRS](https://github.com/shaowei-cai-group/PRS-sc23/tree/PRS-sc23)
- [SBVA](https://github.com/hgarrereyn/SBVA)

## Build Requirements

### Prerequisites
- C++20 compatible compiler (GCC recommended)
- [Boost Library](https://www.boost.org/) headers
- [OpenMPI](https://www.open-mpi.org/) implementation
- Standard build tools (make, autoconf)
- POSIX-compatible environment

### Build Instructions
1. Clone the repository:
   ```bash
   git clone https://github.com/lip6/painless
   cd painless
   ```

2. Build the entire project:
   ```bash
   make # -j for parallel and quicker make
   ```
   This will:
   - Compile the M4RI library required by PRS and MapleCOMSPS
   - Build all required SAT solvers
   - Build both debug and release versions of Painless

3. Build specific targets:
   ```bash
   make debug     # Build debug version (uses -fsanitize=address)
   make release   # Build release version
   make solvers   # Build only the SAT solvers
   ```

4. Clean the build:
   ```bash
   make cleanpainless   # Clean Painless build
   make cleansolvers    # Clean solver builds
   make clean           # Clean Painless and Solver builds
   make cleanall        # Clean everything including libraries (M4RI)
   ```

### Output Files
The compiled binaries will be located in:
- Debug build: `build/debug/painless_debug`
- Release build: `build/release/painless_release`

### Project Organization
```
.
├── build/          # Build output directory
├── docs/           # Documentation
├── libs/           # External libraries
├── scripts/        # Utility scripts
├── solvers/        # Sequential SAT solver implementations
└── src/           # Painless source code
```

## Scripts and Tools

### Launch Script (scripts/launch.sh)
A bash script for running and analyzing SAT solver experiments:

```bash
./scripts/launch.sh <parameters_file> <input_files_list> [experiment_name] [debug]
```

#### Features:
- Automated execution of multiple SAT instances
- MPI process management and cleanup (MPI is used if the `-gshr-strat` option is not negatif)
- Enforces timeout per instance via the `timeout` command
- Result validation for SAT solutions
- Performance metrics collection

#### Parameters:
- `parameters_file`: Configuration file containing solver settings
- `input_files_list`: File containing paths to CNF formulas
- `experiment_name`: (Optional) Name for the experiment
- `debug`: (Optional) Use debug build with verbose MPI output (`--verbose --debug-daemons` outputs in `err_*.txt` file)

#### Output Structure:
```
outputs/
  ├── metric_${solver}_L${lstrat}_G${gstrat}_${timestamp}/
  │   ├── logs/                    # Per-instance logs
  │   │   ├── log_instance1.txt    # Solver output
  │   │   └── err_instance1.txt    # Error messages
  │   └── times_*.csv             # Detailed timing results
  └── times.csv                   # Overall experiments times
```

#### Example Usage:
```bash
# Run in release mode
./scripts/launch.sh scripts/parameters.sh formula_list.txt

# Run in debug mode
./scripts/launch.sh scripts/parameters.sh formula_list.txt experiment1 debug
```


### Result Analysis Script (scripts/plot.py)

A comprehensive analysis tool for SAT solver results that can:
- Generate performance statistics and visualizations
- Compare multiple solver configurations
- Create cumulative execution time plots
- Generate scatter plots comparing solver pairs

Usage:
```bash
# Analyze results from metric directories:
python scripts/plot.py --base-dir outputs --timeout 5000

# Use existing CSV file:
python scripts/plot.py --file results.csv --timeout 5000
```

The `--file` and `--base-dir` options are mutually exclusive.

The script supports various options:
- `--scatter-plots`: Generate solver comparison scatter plots
- `--output-dir`: Specify output directory for plots
- `--output-format`: Set plot format (pdf, png, etc.)
- `--dark-mode`: Use dark theme for plots

Output directory structure after a `--base-dir` execution on directory `outputs`:
```
outputs/
  ├── metric_solver1/    # Results for first solver configuration
  │   └── times_*.csv
  ├── metric_solver2/    # Results for second solver configuration
  │   └── times_*.csv
  └── combined_results.csv  # Generated combined statistics
```

The analysis provides:
- Solver performance statistics (solved instances, PAR2 scores, VBS-SMAPE score)
- SAT/UNSAT instance statistics
- Performance visualization plots
- Virtual best solver (VBS) analysis

# Painless: A Framework for Parallel SAT Solving

## Overview
Painless is a flexible framework for parallel and distributed SAT solving that integrates multiple state-of-the-art SAT solvers and preprocessing techniques.
> Distributed solving is disabled for now. Once the new changes are stable, it will be re-introduced.

- The doxygen [Topics](https://lip6.github.io/painless/topics.html) provide a good introduction to the different components in the framework
- For an overview on the main interfaces, please check [Main Interfaces](./docs/source/DevelopComponents.md)

## Quick Start

```bash
# Clone and build
git clone https://github.com/lip6/painless
cd painless
# Full framework
make -j$(nproc)
# Without MPI for distributed mode
make USE_DIST=0 -j$(nproc)

# Install wrapper scripts (adds to ~/.local/bin)
make install

# Run on a CNF file using the legacy CLI flags (still supported)
painless -c=2 -t=300 formula.cnf

# Or describe the full solver / sharing / database graph in a topology JSON
painless -topology=my-topology.json -t=300 formula.cnf

# Or use the binary directly
./build/release/painless_release -topology=my-topology.json -t=300 formula.cnf
```

> The legacy CLI flags (`-solver`, `-shr-strat`, `-prs`, `-gshr-*`, ...) are
> still parsed, but only the surviving working strategies (`PortfolioSimple`
> and `DivideAndConquer`) and surviving solver backends are reachable that way.
> For full control over solvers, sharing strategies, databases and producer/
> client wiring, prefer the JSON topology system; see
> [Topologies](./docs/source/Topologies.md) for the schema, per-entity
> examples and a worked example. The exact CLI surface used by the TACAS25
> paper (PortfolioPRS, KissatMAB / KissatINC, ...) is preserved at tag
> [`v1.24.10`](https://github.com/lip6/painless/tree/v1.24.10).

## Build Requirements

### Prerequisites

| Dependency | Minimum Version        | Purpose                                              |
| ---------- | ---------------------- | ---------------------------------------------------- |
| GCC        | 8.0+ (10+ recommended) | C++20 compiler support                               |
| Boost      | 1.75+                  | `boost::json` (topology parsing), lock-free queues   |
| OpenMPI    | 3.0+                   | Distributed solving (optional)                       |
| Make       | 4.0+                   | Build system                                         |
| autoconf   | 2.69+                  | Library configuration                                |

**Notes:**
- **GCC 8-9**: Uses `-std=c++2a` flag (partial C++20 support)
- **GCC 10+**: Uses `-std=c++20` flag (full C++20 support)
- **Boost 1.75+** is a hard requirement: it is the first release that ships
  `boost::json`, which is what parses the topology file
  (`#include <boost/json.hpp>` / `#include <boost/json/src.hpp>` in
  `src/config/`). On Debian/Ubuntu, `libboost-all-dev` from the system
  repositories on bullseye/jammy or newer is sufficient; older distros need a
  manual Boost install.
- **OpenMPI**: Optional - the framework can compile without MPI using the `USE_DIST=0` flag
- Mainly tested on Linux.

### Build Instructions

1. **Clone the repository:**
   ```bash
   git clone https://github.com/lip6/painless
   cd painless
   ```

2. **Build the entire project:**
   ```bash
   make              # Build solvers, Painless, and libraries
   make -j$(nproc)   # Parallel build (faster)
   ```
   
   This will:
   - Compile the M4RI library (required by MapleCOMSPS)
   - Build all integrated SAT solvers
   - Build both debug and release versions of Painless
   - Create shared libraries in the `libs/` directory

3. **Build specific targets:**
   ```bash
   make debug        # Build debug version only
   make release      # Build release version only
   make solvers      # Build only the SAT solvers
   make lib          # Build shared libraries (debug + release)
   make m4ri         # Build M4RI library only
   ```

4. **Build without MPI (local-only mode):**
   ```bash
   USE_DIST=0 make   # Compiles without distributed solving support
   ```

5. **View build configuration:**
   ```bash
   make info         # Show current build configuration
   make infov        # Show verbose configuration (all source files)
   ```

6. **Clean the build:**
   ```bash
   make cleanpainless   # Clean Painless build only
   make cleansolvers    # Clean solver builds
   make cleanm4ri       # Clean M4RI library
   make clean           # Clean Painless, solvers, and M4RI
   ```

### Installation

After building, you can install wrapper scripts for convenient command-line access:

```bash
make install
```

This will:
- Create wrapper scripts configured with absolute paths to the binaries
- Install symlinks to `~/.local/bin/` (ensure this is in your PATH)
- Install two commands:
  - **`painless`**: Runs the release version
  - **`painlessgdb`**: Runs the debug version under GDB for debugging

**Binary RPATH Configuration:**
- Binaries use relative RPATH via `$ORIGIN` for library discovery
- Libraries are automatically found relative to the binary location
- No need to set `LD_LIBRARY_PATH` after installation

**To uninstall:**
```bash
make uninstall
```

### Output Files

After building, the compiled binaries and libraries will be located in:

```
build/
├── debug/
│   └── painless_debug          # Debug executable
└── release/
    └── painless_release        # Release executable (optimized)
    
libs/
├── libpainless.so              # Painless shared library (release)
├── libpainless.so.debug        # Painless shared library (debug)
├── libkissat.so                # Kissat solver
├── libmapleCOMSPS.so           # MapleCOMSPS solver
├── libminisat.so               # MiniSat solver
└── libm4ri.so                  # M4RI library
```

The archive files `.a` for the other solvers are kept in their respective folders, under `/solvers`.

## Usage

### Command-Line Interface

After installation, use the wrapper script:

```bash
painless [OPTIONS] input.cnf

General options:
  input.cnf            CNF input file (DIMACS format) - positional argument
  -topology=<path>     Path to a topology JSON describing the solver / sharing
                       graph. When set, the legacy portfolio / sharing flags
                       below are ignored.
  -c=<n>               Number of solver threads (0 = auto-detect)
  -t=<seconds>         Timeout in seconds (default: 0 = no timeout)
  -v=<level>           Verbosity level (0-5, default: 0)
  -no-model            Disable model output
  -color=<auto|always> Force-color the log output
  -dist                Enable distributed solving (initializes MPI)
  -help                Display full help message
  -details=<category>  Show detailed help for a category

Solving options (legacy CLI path, ignored when -topology= is set):
  -strat=<name>       Working strategy name (default: "PortfolioSimple",
                      also supports "DivideAndConquer")
  -solver=<string>    Portfolio of solvers (default: "kcl")
                      Each character represents a solver:
                      g=Glucose, k=Kissat, c=CaDiCaL, l=Lingeling
                      M=MapleCOMSPS, m=Minisat, y=YalSAT, t=TaSSAT
                      (KissatMAB / KissatINC backends were removed; the 'K'
                      and 'I' codes no longer resolve to a solver.)
  -shr-strat=<n>      Local sharing strategy (1-3, default: 1)
                      1-HordeSat sharing ALL-to-ALL
                      2-HordeSat sharing 1HALF-to-ALL + 2HALF-to-ALL
                      3-Simple sharing ALL-to-ALL
  -shr-lit-per-prod=<n>  Literals per producer locally (default: 1500)
  -max-cls-size=<n>   Maximum clause size to share (default: 60)
  -lshrDB=<char>      Local sharing database type (d=PerSize, m=Mallob)
```

**Examples:**

```bash
# Basic usage with timeout
painless formula.cnf -t=300

# Specify number of threads (32 threads)
painless formula.cnf -c=32 -t=300

# Create portfolio: Glucose, Kissat, MapleCOMSPS, CaDiCaL, YalSAT
painless formula.cnf -solver=gkMcy -c=20

# Enable distributed solving with MPI
painless formula.cnf -dist -gshr-strat=1

# Run without distributed mode (local only, default)
painless formula.cnf -gshr-strat=-1

# Run a topology JSON (preferred way to express custom configurations)
painless -topology=my-topology.json -t=300 formula.cnf

# Adjust sharing parameters (legacy CLI path)
painless formula.cnf -gshr-lit=1500 -shr-lit-per-prod=1000

# High verbosity for debugging
painless formula.cnf -v=3 -t=60

# Debug mode with GDB
painlessgdb formula.cnf -v=2

# Direct binary execution
./build/release/painless_release formula.cnf -c=16 -t=300

# Get detailed help on specific topics
painless -details=portfolio
painless -details=sharing
painless -details=preprocessing
```

### Topology Configuration

`-topology=<path>` points Painless at a single JSON file that describes the
whole solver / sharing graph: which CDCL and local-search solvers to spawn,
which clause databases they import from, which sharing strategies move
clauses between them, and which working strategy orchestrates them. Minimal
example (two Kissat solvers exchanging clauses through one HordeSat strategy
on a single sharer thread):

```json
{
  "databaseTemplates": [
    { "id": "imp_d", "type": "perSize",         "capacity": 10000,  "params": { "max-clause-size": 80 } },
    { "id": "shr_d", "type": "bufferPerEntity", "capacity": 100000, "params": { "max-clause-size": 50 } }
  ],
  "cdclSolvers": [
    { "id": "k0", "type": "kissat", "importDB": "imp_d", "params": { "seed": 0 } },
    { "id": "k1", "type": "kissat", "importDB": "imp_d", "params": { "seed": 1 } }
  ],
  "localSearchers": [],
  "workingStrategy": { "name": "PortfolioSimple", "solvers": ["k0", "k1"], "params": {} },
  "sharingStrategies": [
    {
      "id": "horde", "name": "HordeSat", "db": "shr_d",
      "producers": ["k0", "k1"], "clients": ["k0", "k1"],
      "params": { "literals-per-producer-per-round": 1500 }
    }
  ],
  "sharers": [ { "id": "sharer_main", "strategies": ["horde"] } ]
}
```

See [`docs/source/Topologies.md`](./docs/source/Topologies.md) for the full
schema, per-entity JSON snippets (`perSize` / `bufferPerEntity` / `mallob` /
`singleBuffer` databases, every CDCL backend, `yalsat` / `tassat`,
`HordeSat` / `Simple` sharing strategies, `PortfolioSimple` /
`DivideAndConquer` working strategies), validation rules (id uniqueness,
duplicate-id rejection in `producers` / `clients` / `solvers` /
`strategies` lists, ...) and a worked example walking through the build
order. A helper script that emits ready-made topologies lives at
`scripts/config/generate_topology.py`.

### Using as a Library

Painless can be used as a shared library in other applications. See the [API documentation](docs/) for details.

**Basic example:**
```cpp
#include <painless/solver.hpp>

Painless* solver = create_painless("PortfolioSimple", "config.json", 1);
solver->addLiteral(1);
solver->addLiteral(-2);
solver->addLiteral(0);  // End clause

result_t result = solver->solve();
if (result.answer == SatAnswer::SAT) {
    // Process model
}
destroy_painless(solver);
```

The JSON can only be passed by file to `create_painless`.

**Important:** JSON must be **flat** (no nested objects). Keys must match command-line parameter names.

**Example configuration file (config.json):**
```json
{
  "t": 300,
  "c": 16,
  "v": 2,
  "solver": "gkMcy",
  "gshr-strat": 1,
  "gshr-lit": 2000,
  "shr-lit-per-prod": 1500,
  "max-cls-size": 60,
  "prs": false,
  "mallob": false,
  "dist": true
}
```

## Project Structure

<!-- ### Directory Organization

```
.
├── build/                # Build output directory
│   ├── debug/           # Debug builds
│   └── release/         # Release builds
├── docs/                # Documentation
│   └── source/          # Documentation sources
├── include/             # Public header files
│   └── painless/        # Main API headers
├── libs/                # Compiled libraries and dependencies
├── scripts/             # Utility scripts
│   ├── bin/            # Wrapper scripts
│   ├── launch.sh       # Experiment launcher
│   └── plot.py         # Result analysis
├── solvers/             # Integrated SAT solver implementations
│   ├── kissat/
│   ├── cadical/
│   ├── glucose/
│   ├── minisat/
│   ├── mapleCOMSPS/
│   ├── lingeling/
│   ├── yalsat/
│   └── tassat/
└── src/                 # Painless source code
    ├── containers/      # Clause and formula data structures
    ├── sharing/         # Clause sharing strategies
    ├── working/         # Worker management
    ├── solvers/         # Solver interface implementations
    ├── utils/           # Utilities and helpers
    └── preprocessors/   # Preprocessing integration
``` -->

### Core Components

- **`src/containers/`**: Data structures for clause management and formula representation
- **`src/sharing/`**: Learnt clause sharing management and strategies
- **`src/working/`**: Worker organization and portfolio implementations
- **`src/solvers/`**: Solver interface and implementations
- **`src/utils/`**: Helper utilities and data structures
- **`src/preprocessors/`**: (Will return soon) Preprocessing techniques integration

### Integrated SAT Solvers

The framework integrates several state-of-the-art SAT solvers:

| Solver                                                 | Version     | Type         | Notes                          |
| ------------------------------------------------------ | ----------- | ------------ | ------------------------------ |
| [Kissat](https://github.com/arminbiere/kissat)         | v4.0.4      | CDCL         | MAB and INC variants removed; see tag [`v1.24.10`](https://github.com/lip6/painless/tree/v1.24.10) |
| [CaDiCaL](https://github.com/arminbiere/cadical)       | v1.9.1      | CDCL         |                                |
| [MapleCOMSPS](https://maplesat.github.io/solvers.html) | SAT Comp 17 | CDCL         | Requires M4RI                  |
| [Glucose](https://www.labri.fr/perso/lsimon/glucose/)  | 4.x         | CDCL         |                                |
| [MiniSat](http://minisat.se/)                          | 2.2         | CDCL         |                                |
| [Lingeling](https://github.com/arminbiere/lingeling)   | Latest      | CDCL         |                                |
| [YalSAT](https://github.com/arminbiere/yalsat)         | Latest      | Local Search |                                |
| [TaSSAT](https://zenodo.org/records/10042124)          | Latest      | Local Search |                                |

### Preprocessing Integration (Will be back soon)

The framework includes preprocessing techniques from:
- [PRS](https://github.com/shaowei-cai-group/PRS-sc23/tree/PRS-sc23) - Preprocessing and simplification
- [SBVA](https://github.com/hgarrereyn/SBVA) - Bounded variable addition

## Scripts and Tools

### Launch Script (scripts/launch.sh)

A bash script for running and analyzing SAT solver experiments across multiple instances:

```bash
./scripts/launch.sh <parameters_file> <input_files_list> [experiment_name] [debug]
```

#### Features:
- Automated execution of multiple SAT instances
- MPI process management and cleanup (enabled when `-gshr-strat` ≥ 0)
- Enforces timeout per instance via the `timeout` command
- Result validation for SAT solutions
- Performance metrics collection and CSV export

#### Parameters:
- **`parameters_file`**: Configuration file containing solver settings
- **`input_files_list`**: File containing paths to CNF formulas (one per line)
- **`experiment_name`**: (Optional) Name for the experiment
- **`debug`**: (Optional) Use debug build with verbose MPI output

#### Output Structure:
```
outputs/
├── metric_${solver}_L${lstrat}_G${gstrat}_${timestamp}/
│   ├── logs/                    # Per-instance logs
│   │   ├── log_instance1.txt   # Solver stdout
│   │   └── err_instance1.txt   # Solver stderr (debug mode)
│   └── times_*.csv             # Detailed timing results
└── times.csv                    # Aggregated results across experiments
```

#### Example Usage:
```bash
# Create a file list
ls benchmarks/*.cnf > formulas.txt

# Run experiments in release mode
./scripts/launch.sh scripts/parameters.sh formulas.txt my_experiment

# Run in debug mode with verbose MPI logging
./scripts/launch.sh scripts/parameters.sh formulas.txt my_experiment debug
```

### Result Analysis Script (scripts/plot.py)

A comprehensive Python tool for analyzing SAT solver experimental results:

**Features:**
- Performance statistics and comparisons
- Cumulative execution time plots
- Scatter plots for solver-to-solver comparisons
- Virtual Best Solver (VBS) analysis
- PAR2 score computation

**Usage:**
```bash
# Analyze from metric directories
python scripts/plot.py --base-dir outputs --timeout 5000

# Analyze from existing CSV
python scripts/plot.py --file combined_results.csv --timeout 5000

# Generate with scatter plots and custom output
python scripts/plot.py --base-dir outputs --timeout 5000 \
                       --scatter-plots \
                       --output-dir plots/ \
                       --output-format pdf
```

**Options:**
- `--base-dir <dir>`: Directory containing metric subdirectories
- `--file <csv>`: Pre-existing CSV file to analyze
- `--timeout <sec>`: Timeout value for PAR2 computation
- `--scatter-plots`: Generate pairwise scatter plots
- `--output-dir <dir>`: Directory for plot output (default: base-dir)
- `--output-format <fmt>`: Plot format (pdf, png, svg, etc.)
- `--dark-mode`: Use dark theme for plots

**Output Structure:**
```
outputs/
├── metric_solver1/              # Results from first configuration
│   └── times_*.csv
├── metric_solver2/              # Results from second configuration
│   └── times_*.csv
├── combined_results.csv         # Merged statistics
├── cactus_plot.pdf              # Cumulative time plot
└── scatter_solver1_vs_solver2.pdf  # Pairwise comparison
```

**Generated Statistics:**
- Instances solved (SAT/UNSAT/TIMEOUT)
- Mean/median solving times
- PAR2 scores (Penalized Average Runtime)
- VBS-SMAPE (Virtual Best Solver performance)

## Troubleshooting

### Build Issues

**GCC version error:**
```
GCC version X.X does not support C++20
```
→ Install GCC 8+ (GCC 10+ recommended): `sudo apt install g++-10`

**Boost not found:**
```
fatal error: boost/json.hpp: No such file or directory
```
→ Install Boost 1.75+: `sudo apt install libboost-all-dev` (check version with `dpkg -s libboost-dev`)

**MPI errors during compilation:**
```
mpic++ not found, compiling in NDIST mode
```
→ Either install OpenMPI (`sudo apt install libopenmpi-dev`) or build without MPI: `USE_DIST=0 make`

### Runtime Issues

**Library not found:**
```
error while loading shared libraries: libkissat.so
```
→ Libraries should be auto-discovered via RPATH. If not, check that `libs/` directory exists relative to binary.

**MPI launch failure:**
```
mpirun was unable to launch the specified application
```
→ Ensure OpenMPI is installed and configured: `mpirun --version`
→ Try local mode: Use `-gshr-strat=-1` option

**Memory exhausted:**
```
std::bad_alloc or killed by OOM
```
→ Reduce parallel solvers: `-c=<lower>`
→ Increase system swap or use machine with more RAM

### Common Questions

**Q: Which build should I use?**
A: Use `release` for performance, `debug` only for development/debugging

**Q: Do I need MPI for parallel solving?**
A: No, MPI is only needed for distributed (multi-node) solving. Local parallelism works without it.

**Q: How do I generate API documentation?**
A: Run `doxygen doxygen-configFile` in the project `docs/` directory (requires Doxygen installation)

**Q: Can I add my own SAT solver?**
A: Yes! Implement the solver interface in `src/solvers/` - see existing solver implementations for examples.

## Development

### Building for Development

```bash
# Build with debug symbols and sanitizers
make debug

# Run under GDB
painlessgdb formula.cnf

# Or manually
gdb --args ./build/debug/painless_debug formula.cnf
```

### Running Tests (Soon)

```bash
# TODO: Add test suite information when available
```

### Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes with clear commit messages
4. Ensure code compiles with both debug and release builds
5. Submit a pull request

For coding standards and detailed contribution guidelines, contact the maintainers.

## Contacts

* **Souheib BAARIR** - [souheib.baarir@lip6.fr](mailto:souheib.baarir@lip6.fr)
* **Mazigh SAOUDI** - [mazigh.saoudi@epita.fr](mailto:mazigh.saoudi@epita.fr)

See also: [Contributors](./CONTRIBUTORS.md)


## References

```bibtex
@InProceedings{10.1007/978-3-319-66263-3_15,
author="Le Frioux, Ludovic
and Baarir, Souheib
and Sopena, Julien
and Kordon, Fabrice",
editor="Gaspers, Serge
and Walsh, Toby",
title="PaInleSS: A Framework for Parallel SAT Solving",
booktitle="Theory and Applications of Satisfiability Testing -- SAT 2017",
year="2017",
publisher="Springer International Publishing",
address="Cham",
pages="233--250",
isbn="978-3-319-66263-3"
}
```

```bibtex
@InProceedings{10.1007/978-3-031-90653-4_3,
author="Saoudi, Mazigh
and Baarir, Souheib
and Sopena, Julien
and Lejemble, Thibault",
editor="Gurfinkel, Arie
and Heule, Marijn",
title="D-Painless: A Framework for Distributed Portfolio SAT Solving",
booktitle="Tools and Algorithms for the Construction and Analysis of Systems",
year="2025",
publisher="Springer Nature Switzerland",
address="Cham",
pages="45--64",
isbn="978-3-031-90653-4"
}

```
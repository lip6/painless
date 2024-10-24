# Reproducing D-Painless SAT Solver Experiments from TACAS25

This guide details how to reproduce the experimental results presented in our TACAS25 paper. We evaluated four different configurations of Painless, each representing different sharing strategies and solver combinations.

[TOC]

## Experimental Environment

### Hardware Configuration
- **Platform**: Grid5000 Paravance Cluster
- **Nodes**: 25 machines (50 CPUs total)
- **Architecture**: 2 Intel Xeon E5-2630 v3 per node, with 8 cores/CPU (16 cores/node)
- **Main Memory**: 128 GiB/node
- **Network**:  	2 x 10 Gbps (SR‑IOV)

### Software Environment
- **Operating System**: Debian 11 (bullseye)
- **MPI Implementation**: OpenMPI 4.1.0
- **Compiler**: GCC 10.2.1

### Setup

#### Environment Setup:
```bash
# Set up hostfile from node allocation in bashrc
export HOSTFILE=~/software/hostfile
uniq $OAR_NODE_FILE > $HOSTFILE
```

#### Compile Painless:
```bash
make cleanall && make
```

#### Create Input File List:
- Create a text file containing paths to CNF formulas, one per line.
> [!important]
> The instances from the International SAT Competition 2024([GBD-ISC24](https://benchmark-database.de/?track=main_2024&context=cnf)) were used. However, due to some out of memory errors, not all instances were solved. The intersection of the non crashed instances can be found [here](https://gist.github.com/S-Mazigh/f6ad9157a7f47cffb34887ab0bfd203b).

#### Launch Script:
- For each configuration:
```bash
./scripts/launch.sh scripts/<config>-params.sh formula_list.txt [experiment_name]
```
- Example runs for each configuration:
```bash
# in launch-solvers.sh

# PL-Mallob-kc
cd path/to/painless && bash ./scripts/launch.sh scripts/mallob-params.sh formulas.txt PL-Mallob-kc

# PL-Horde+Generic-PRS
cd path/to/painless && bash ./scripts/launch.sh scripts/prs-params.sh formulas.txt PL-Horde+Generic-PRS

# PL-Horde+Mallob-kc
cd path/to/painless && bash ./scripts/launch.sh scripts/horde-mallob-params.sh formulas.txt PL-Horde+Mallob-kc

# PL-Horde+Allgather-kc
cd path/to/painless && bash ./scripts/launch.sh scripts/allgather-params.sh formulas.txt PL-Horde+Allgather-kc
```

#### Run in Grid5000:
```bash
# Reserve 25 nodes on Paravance cluster for a night
oarsub -l nodes=25,walltime=13:30:00 -p "cluster='paravance'" -t night "bash /path/to/launch-solvers.sh"
```

## Configuration Files Explained

The experiments use parameter files located in the `scripts/` directory. Each file configures Painless for a specific sharing strategy and solver combination.

### Common Parameters Across Configurations

All configurations share some base parameters:
```bash
nb_physical_cores=$(lscpu | grep ^Core\(s\)\\sper\\ssocket:\\s | awk '{print $4}')
nb_solvers=$nb_physical_cores     # One solver per physical core
verbose=1                         # Enable basic logging
timeout=500                       # Timeout in seconds

hostfile=$HOSTFILE # Hostfile is set to the environment variable
# Number of mpi processes == number of sockets per node * number of nodes
nb_procs_per_node=$(lscpu | grep ^Socket\(s\):\\s | awk '{print $2}')
nb_nodes=$(ls $hostfile | wc)
# mpi mapping is: --bind-to hwthread --map-by ppr:$nb_procs_per_node:node:pe=$nb_physical_cores
```

### Configuration Details

- Most of the options are used with their default values, thus they are not set in the configuration files.

1. **PL-Mallob-kc** (mallob-params.sh)
```sh
solver="kc"                       # Kissat and cadical as base solvers
lstrat=-1                        # Disable local sharing strategies (no hordesat or simple share)
gstrat=2                         # Mallob global sharing
sharing_per_sec=2                # Share clauses twice per second
gshr_lit=$(($nb_physical_cores * 400 / $sharing_per_sec))  # Base buffer size
flags="-simple -mallob -gshr-sleep=10000 -gshr-lit=${gshr_lit} -importDB-cap=$(($gshr_lit * 10))" # Use PortfolioSimple in Mallob mode
```

2. **PL-Horde+Generic-PRS** (prs-params.sh)
```bash
solver="IMl"  # Kissat-Inc, MapleCompls, and lingeling
# shr-strat and -gshr-strat options are only used for naming
flags="-gshr-lit=0 -one-sharer -gshr-sleep=0"  # Single sharer configuration using PortfolioPRS with unlimited buffer for GenericGlobalSharing
```

3. **PL-Horde+Mallob-kc** (horde-mallob-params.sh)
```bash
solver="kc"                      # Kissat and Cadical as base solvers
lstrat=1                        # Local sharing via HordeSat
gstrat=2                        # Global Mallob sharing
sharing_per_sec=2               # Share clauses twice per second
gshr_lit=$(($nb_physical_cores * 400 / $sharing_per_sec))
flags="-simple -gshr-sleep=300000 -importDB=m -importDB-cap=$(($gshr_lit * 10)) -gshr-lit=${gshr_lit}" # Use PortfolioSimple
```

4. **PL-Horde+Allgather-kc** (allgather-params.sh)
```bash
solver="kc"      # Kissat and Cadical as base solvers
lstrat=1         # Local sharing via HordeSat
gstrat=1         # All-gather global sharing
shr_sleep=200000 # Sleep time for HordSat
gshr_lit=$(($nb_physical_cores * 1500))
flags="-simple -gshr-sleep=300000 -gshr-lit=${gshr_lit}" # Use PortfolioSimple
```

> [!note] 
> Grid 5000 Specific Notes:
> - The MPI configuration automatically adapts to the Grid5000 environment using the provided hostfile
> - The parameter files detect the number of physical cores per node using `lscpu`
> - The launch script includes Grid5000-specific MPI flags: `--mca pml ^ucx`
> - The MPI job is aborted if a solution was found and returned by a process (return value = 10 or 20) due to the option `--mca orte_abort_on_non_zero_status true`


## Analysis

- Results are organized in the `outputs/` directory:
```
outputs/
  ├── metric_${solver}_L${lstrat}_G${gstrat}_${timestamp}/
  │   ├── logs/                    # Per-instance solver logs
  │   ├── times_*.csv             # Detailed timing results
  └── times.csv                   # Summary of all experiments
```


- Use the provided plotting script to analyze results:

```bash
python scripts/plot.py --base-dir outputs --timeout 500
```

This generates:
- Performance statistics (PAR2 scores, solved instances)
- Cumulative time plots

The generated `combined_results.csv` file can be used to generate scatter plots by using the `--file` and `--scatter-plots` options.

For complete parameter documentation, see `src/utils/Parameters.hpp`.

For Grid5000-specific support:
- Reference: https://www.grid5000.fr/
- Support: https://www.grid5000.fr/w/Support
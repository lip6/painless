# Core Interfaces

[TOC]

## Introduction

Painless is built around five main interfaces that you can extend to create new components:

- [SolverInterface](@ref SolverInterface) - Base interface for SAT solvers
- [SharingEntity](@ref SharingEntity) - Base interface for entities that can exchange clauses
- [SharingStrategy](@ref SharingStrategy) - Interface for clause sharing strategies
- [ClauseDatabase](@ref ClauseDatabase) - Interface for clause storage
- [WorkingStrategy](@ref WorkingStrategy) - Interface for solver execution strategies

## 1. Implementing SolverInterface

[SolverInterface](@ref SolverInterface) is the base class for implementing SAT solvers. With specialized interfaces [SolverCdclInterface](@ref SolverCdclInterface) and [LocalSearchInterface](@ref LocalSearchInterface).

### Required Functions

You must override these pure virtual functions:

```cpp
// Get the current number of variables
virtual unsigned int getVariablesCount() = 0;

// Get variable for search splitting
virtual int getDivisionVariable() = 0;

// Set/unset solver interruption
virtual void setSolverInterrupt() = 0;
virtual void unsetSolverInterrupt() = 0;

// Core solving function
virtual SatResult solve(const std::vector<int>& cube) = 0;

// Clause management
virtual void addClause(ClauseExchangePtr clause) = 0;
virtual void addClauses(const std::vector<ClauseExchangePtr>& clauses) = 0;
virtual void addInitialClauses(const std::vector<simpleClause>& clauses, 
                             unsigned int nbVars) = 0;

// Load formula from DIMACS file
virtual void loadFormula(const char* filename) = 0;

// Get model for SAT results
virtual std::vector<int> getModel() = 0;

// Diversification function
virtual void diversify(const SeedGenerator& getSeed) = 0;
```

The interface provides default implementations for:

- `printWinningLog()` - Prints solver type info
- `printStatistics()` - Prints warning that stats aren't implemented 
- `printParameters()` - Prints warning that parameters aren't implemented

> [!note]
> - Use `initializeTypeId<Derived>()` function in most derived class' constructors to set up type IDs
> - The solver type is set via the constructor using `SolverAlgorithmType`


### CDCL Solver Interface

The SolverCdclInterface extends SolverInterface for CDCL (Conflict-Driven Clause Learning) solvers with these additional virtual functions:

```cpp
// Set the initial phase for a variable
virtual void setPhase(const unsigned int var, const bool phase) = 0;

// Increase the activity of a variable
virtual void bumpVariableActivity(const int var, const int times) = 0;

// Get final analysis in case of UNSAT result
virtual std::vector<int> getFinalAnalysis() = 0;

// Get current SAT solver assumptions
virtual std::vector<int> getSatAssumptions() = 0;
```

- It inherits from both SolverInterface and SharingEntity for clause sharing. Thus the **virtual methods from SharingEntity must also be implemented**.
- It includes a clause database (`m_clausesToImport`) shared pointer for importing received clauses
- There is an enum SolverCdclType to identify specific CDCL solver implementations:

  ```cpp
  enum class SolverCdclType {
      GLUCOSE = 0,
      LINGELING = 1,
      CADICAL = 2,
      MINISAT = 3,
      KISSAT = 4,
      MAPLECOMSPS = 5,
      KISSATMAB = 6,
      KISSATINC = 7
  };
  ```

## 2. Implementing SharingEntity

[SharingEntity](@ref SharingEntity) is the base class for components that can exchange clauses. It defines a subscription behavior, where clients subscribes to a producer that exports clauses for them to import.

### Required Functions

```cpp
// Import a single clause
virtual bool importClause(const ClauseExchangePtr& clause) = 0;

// Import multiple clauses
virtual void importClauses(const std::vector<ClauseExchangePtr>& clauses) = 0;
```

### Default Behaviors

The class provides default implementations for:

- `addClient()` - Adds a sharing entity as a client
- `removeClient()` - Removes a client
- `clearClients()` - Removes all clients
- `exportClauseToClient()` - Basic clause export to a single client
- `exportClause()` - Exports clause to all clients
- `exportClauses()` - Exports multiple clauses to all clients

- `std::enable_shared_from_this` is used, thus objects must be managed by `std::shared_ptr`
- Client management uses mutex protection
- Automatic sharing ID assignment
- Default clause export can be modified by overriding `exportClauseToClient()`

## 3. Implementing SharingStrategy

[SharingStrategy](@ref SharingStrategy) inherits from [SharingEntity](@ref SharingEntity) and manages clause distribution.

### Required Functions

```cpp
// Execute the sharing process
virtual bool doSharing() = 0;
```

### Default Behaviors

Provides default implementations for:

- `getSleepingTime()` - Returns configured sharing sleep time via the option `shr-sleep`
- `printStats()` - Prints sharing statistics
- Producer/consumer management functions

### Implementation Notes

- Overrides `exportClauseToClient()` to not export a clause to its producer
- Uses a clause database (`m_clauseDB`) for storing shared clauses
- Producer list management uses mutex protection
- Returns true from `doSharing()` when sharing should end (see Sharer)

### GlobalSharingStrategy Interface

GlobalSharingStrategy specializes SharingStrategy to enable distributed clause sharing through MPI (Message Passing Interface) across multiple processes.

GlobalSharingStrategy adds two functions to the SharingStrategy interface:

```cpp
virtual void initMpiVariable() = 0;
virtual void joinProcess() = 0;
```

- `initMpiVariable()` sets up the MPI related variables for distributed communication. 

- `joinProcess()` provides synchronization mechanisms for process termination and resource cleanup.

It also overrides `doSharing()` to execute a periodic end detection algorithm. It returns true when the sharing process should terminate based on global consensus. Thus `GlobalSharingStrategy::doSharing()` can be used in derived classes for termination detection.


## 4. Implementing ClauseDatabase

[ClauseDatabase](@ref ClauseDatabase) is the base class for clause storage implementations. The [ClauseBuffer](@ref ClauseBuffer) class presented in [Clause Management](./ClauseManagement.md) can be used as a basis for an hypothetical implementation.

### Required Functions

```cpp
// Add a single clause to the database
virtual bool addClause(ClauseExchangePtr clause) = 0;

// Get clauses up to the literal count limit
virtual size_t giveSelection(std::vector<ClauseExchangePtr>& selectedCls,
                           unsigned int literalCountLimit) = 0;

// Get all clauses from the database
virtual void getClauses(std::vector<ClauseExchangePtr>& v_cls) = 0;

// Get a single clause from the database
virtual bool getOneClause(ClauseExchangePtr& cls) = 0;

// Get current database size
virtual size_t getSize() const = 0;

// Reduce database size
virtual size_t shrinkDatabase() = 0;

// Remove all clauses
virtual void clearDatabase() = 0;
```

## 5. Implementing WorkingStrategy

[WorkingStrategy](@ref WorkingStrategy) defines how solvers are executed. Currently, it is the component where all the other components are to be managed in order to achieve the needed parallelization or distributed strategy.

### Required Functions

```cpp
// Core solving function
virtual void solve(const std::vector<int>& cube) = 0;

// Handle solver completion
virtual void join(WorkingStrategy* winner, SatResult res, 
                 const std::vector<int>& model) = 0;

// Control solver execution
virtual void setSolverInterrupt() = 0;
virtual void unsetSolverInterrupt() = 0;
virtual void waitInterrupt() = 0;
```

### Default Behaviors

Provides:

- `addSlave()` - Adds a child working strategy

### Implementation Notes

- Uses parent-child relationships
- Implements solver execution patterns
- Launched from main() in painless.cpp
- Controls solver lifecycle

## Launching Working Strategies

In `painless.cpp`, working strategies are initialized and launched:

```cpp
// Create working strategy
if (__globalParameters__.test)
    working = new Test();
else if (__globalParameters__.simple)
    working = new PortfolioSimple();
else
    working = new PortfolioPRS(); // The current default (will be changed)

// Launch working strategy
std::vector<int> cube;
std::thread mainWorker(&WorkingStrategy::solve, working, std::ref(cube));
```

## Implementation Guidelines

1. Reference existing implementations in `src/solvers/PortfolioSimple.cpp`, `src/solvers/AllgatherSharing.cpp`, ...
2. Implement synchronization for thread safety
3. Follow shared pointer memory management patterns
4. Include logging for debugging
5. Test with different SAT instances
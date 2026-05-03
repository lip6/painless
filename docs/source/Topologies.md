@page topologies Topologies

[TOC]

# Overview

A *topology* is a JSON file that fully describes the parallel solving graph Painless instantiates at startup: which CDCL solvers to spawn, which clause databases they import from, which sharing strategies move clauses between them, and which working strategy orchestrates them. It replaces the older flag-driven (`-c`, `-shr-strat`, `-portfolio`, ...) configuration with an explicit, reproducible declaration.

Pass it on the command line:

```sh
painless --topology=my-topology.json formula.cnf
```

The file is parsed by `topology::parseJsonTopology` (see @ref TopologyConfigurator.hpp) into a `topology::TopologyDesc`, then turned into live objects by `topology::buildPermanentWorkers`. Iteration is insertion-ordered, so for a given JSON file solver ids and the wiring graph are deterministic across runs.

# File structure

A topology is a single JSON object with seven top-level keys:

| Key                 | Type   | Required | Purpose                                          |
| ------------------- | ------ | -------- | ------------------------------------------------ |
| `databaseTemplates` | array  | yes      | Reusable ClauseDatabase configurations.          |
| `cdclSolvers`       | array  | yes      | CDCL solver instances.                           |
| `localSearchers`    | array  | yes      | Local-search solver instances.                   |
| `workingStrategy`   | object | yes      | The single orchestrator that drives the solvers. |
| `sharingStrategies` | array  | yes      | Clause-exchange strategies between entities.     |
| `sharers`           | array  | yes      | Threads that drive the sharing strategies.       |

Every entity carries a string `id` that is **unique across the whole file**. Cross-references (a solver's `importDB`, a sharing strategy's `producers` / `clients` / `db`, a sharer's `strategies`, the working strategy's `solvers`) use those ids.

The parser aborts with `PERR_TOPOLOGY` if:

- two entities share the same id;
- an id is declared but never referenced anywhere;
- a sharing strategy is referenced from more than one place;
- the same id appears more than once in the same `producers`, `clients`, `solvers` (working strategy) or `strategies` (sharer) list. Duplicates would otherwise translate into double subscriptions and double clause delivery, since `addClient` and `addSolver` do not deduplicate.

A reference to an unknown id (`importDB`, `db`, or any element of a `producers` / `clients` / `solvers` / `strategies` list) aborts with `PERR_UNHANDLED_EXCEPTION` (raised by the underlying `idRefs.at(...)` lookup).

The concrete `type` / `name` strings (e.g. `"kissat"`, `"persize"`) are **not** validated by the parser. They are looked up by the corresponding factory at build time, and an unknown value aborts there instead.

# Component reference

All `type` / `name` strings are matched **case-insensitively**. The free-form `params` object on each entity is forwarded verbatim to `Configurable::setOption`, so accepted keys depend on the chosen backend. JSON `true`/`false` is mapped to integer `1`/`0`; floats and strings are forwarded as-is.

## Database templates

A template is **cloned per consumer**: each CDCL solver and each sharing strategy referencing the template gets its own fresh ClauseDatabase.

Required JSON fields: `id`, `type`, `capacity`. `params` is optional.

| `type`            | Backend class                 | Recognized `params` keys                                                                                   |
| ----------------- | ----------------------------- | ---------------------------------------------------------------------------------------------------------- |
| `singleBuffer`    | ClauseDatabaseSingleBuffer    | *(none)*                                                                                                   |
| `perSize`         | ClauseDatabasePerSize         | `max-clause-size` (int)                                                                                    |
| `bufferPerEntity` | ClauseDatabaseBufferPerEntity | `max-clause-size` (int)                                                                                    |
| `mallob`          | ClauseDatabaseMallob          | `max-clause-size` (int), `max-partition-lbd` (int), `free-max-size` (int), `literal-capacity` (int/double) |

The `capacity` field on the template object is parsed but currently unused (To be used in upcoming update).

```json
{
  "id": "imp_d",
  "type": "perSize",
  "capacity": 10000,
  "params": { "max-clause-size": 80 }
}
```

```json
{
  "id": "shr_d",
  "type": "bufferPerEntity",
  "capacity": 100000,
  "params": { "max-clause-size": 50 }
}
```

```json
{
  "id": "shr_mallob",
  "type": "mallob",
  "capacity": 100000,
  "params": {
    "max-clause-size": 60,
    "max-partition-lbd": 8,
    "free-max-size": 5,
    "literal-capacity": 100000
  }
}
```

```json
{
  "id": "single_d",
  "type": "singleBuffer",
  "capacity": 50000
}
```

## CDCL solvers

Each entry instantiates one solver, identified inside the running system by the index of its descriptor in the `cdclSolvers` array. Available `type` values depend on which backends were enabled at compile time.

Required JSON fields: `id`, `type`, `importDB`. `params` is optional.

| `type`         | Backend       | `params` semantics                                                                                                                                                                                                                                                                                                |
| -------------- | ------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `kissat`       | Kissat        | Any Kissat option name (forwarded to `kissat_set_option`); aborts if unknown.                                                                                                                                                                                                                                     |
| `cadical`      | CaDiCaL       | Any CaDiCaL option (forwarded to `solver->set`); aborts if unknown.                                                                                                                                                                                                                                               |
| `lingeling`    | Lingeling     | Any Lingeling option (forwarded to `lglsetopt`).                                                                                                                                                                                                                                                                  |
| `minisat`      | MiniSat       | `var-decay`, `clause-decay`, `random-var-freq`, `seed`, `verbosity`, `luby-restart`, `ccmin-mode`, `phase-saving`, `rnd-pol`, `rnd-init-act`, `garbage-frac`, `min-learnts-lim`, `restart-first`, `restart-inc`, `learntsize-factor`, `learntsize-inc`, `learntsize-adjust-start-confl`, `learntsize-adjust-inc`. |
| `glucoseSyrup` | Glucose Syrup | Same family as MiniSat plus `max-var-decay`, `first-reduce-db`, `size-lbd-queue`, `K`, `inc-reduce-db`, `lbd-queue-size`, `reduce-on-size`, `reduce-on-size-size`, `use-flip`, `use-pr`.                                                                                                                          |
| `mapleCOMSPS`  | MapleCOMSPS   | `enable-ge`, `enable-vsids`, `enabled-verso`, `seed`.                                                                                                                                                                                                                                                             |

`importDB` is mandatory and must reference an id declared in `databaseTemplates`.

```json
{
  "id": "kfocused",
  "type": "kissat",
  "importDB": "imp_d",
  "params": { "seed": 0, "stable": 0 }
}
```

```json
{
  "id": "cadi0",
  "type": "cadical",
  "importDB": "imp_d",
  "params": { "seed": 7}
}
```

```json
{
  "id": "ling0",
  "type": "lingeling",
  "importDB": "imp_d",
  "params": { "seed": 3, "plain": 0 }
}
```

```json
{
  "id": "mini0",
  "type": "minisat",
  "importDB": "imp_d",
  "params": {
    "seed": 1,
    "var-decay": 0.95,
    "clause-decay": 0.999,
    "random-var-freq": 0.02,
    "luby-restart": 1,
    "restart-first": 100,
    "restart-inc": 2.0
  }
}
```

```json
{
  "id": "glucose0",
  "type": "glucoseSyrup",
  "importDB": "imp_d",
  "params": {
    "seed": 4,
    "K": 0.8,
    "first-reduce-db": 2000,
    "inc-reduce-db": 300,
    "lbd-queue-size": 50
  }
}
```

```json
{
  "id": "maple0",
  "type": "mapleCOMSPS",
  "importDB": "imp_d",
  "params": { "seed": 2, "enable-vsids": 1, "enable-ge": 1 }
}
```

## Local searchers

Required JSON fields: `id`, `type`. `params` is optional and forwarded to the backend via `Configurable::setOption`.

| `type`   | Backend |
| -------- | ------- |
| `yalsat` | YalSAT  |
| `tassat` | TaSSAT  |

Local searchers do not own a clause database. They take part in solving by being listed in `workingStrategy.solvers` and may also appear as producers in sharing strategies.

```json
{
  "id": "yal0",
  "type": "yalsat",
  "params": { "seed": 11 }
}
```

```json
{
  "id": "tas0",
  "type": "tassat",
  "params": { "seed": 12 }
}
```

## Sharing strategies

A sharing strategy owns its own ClauseDatabase (cloned from `db`). Producer and client ids may point at **CDCL solvers or other sharing strategies**, so hierarchical configurations are possible (e.g. several local strategies feeding a single global strategy).

Required JSON fields: `id`, `name`, `db`, `producers`, `clients`. `params` is optional. `producers` and `clients` may be empty arrays but must be present.

| `name`     | Backend         | `params` keys                                                                                                                                                                                                                                                                           |
| ---------- | --------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `HordeSat` | HordeSatSharing | `literals-per-producer-per-round` (int/double), `initial-lbd-limit` (int), `rounds-before-increase` (int/double), `sleep-time-us` (int/double), `sleep-time-s` (int). The builder additionally injects `producer-ids` automatically from the `producers` list — do not set it manually. |
| `Simple`   | SimpleSharing   | `size-limit-at-import` (int), `literals-per-round` (int/double), `sleep-time-us` (int/double), `sleep-time-s` (int).                                                                                                                                                                    |

A given sharing strategy id may appear at most once across all `producers`/`clients` lists in the file (enforced by the parser). Within a single `producers` or `clients` list each id must also be unique: duplicates abort with `PERR_TOPOLOGY` because they would translate into the same producer subscribing twice (or the same client receiving every clause twice). Listing the same id in *both* `producers` and `clients` is fine — and in fact the standard pattern for round-tripping clauses through a CDCL solver.

```json
{
  "id": "horde0",
  "name": "HordeSat",
  "producers": ["glucose0", "kfocused"],
  "clients":   ["glucose0", "kfocused", "cadi0"],
  "db": "shr_d",
  "params": {
    "sleep-time-us": 500000,
    "literals-per-producer-per-round": 1500,
    "initial-lbd-limit": 2,
    "rounds-before-increase": 1
  }
}
```

```json
{
  "id": "simple0",
  "name": "Simple",
  "producers": ["mini0", "maple0"],
  "clients":   ["kfocused"],
  "db": "shr_d",
  "params": {
    "sleep-time-us": 500000,
    "literals-per-round": 3000,
    "size-limit-at-import": 50
  }
}
```

Hierarchical example — two local strategies feeding a coarser one:

```json
{
  "id": "horde_layer2",
  "name": "HordeSat",
  "producers": ["horde0", "simple0"],
  "clients":   ["maple0", "mini0"],
  "db": "shr_d",
  "params": { "literals-per-producer-per-round": 4000 }
}
```

## Working strategy

Exactly one working strategy is declared per topology.

Required JSON fields: `name`, `solvers`. `params` is optional.

| `name`             | Backend          | `params` keys                                              |
| ------------------ | ---------------- | ---------------------------------------------------------- |
| `PortfolioSimple`  | PortfolioSimple  | *(none)*                                                   |
| `DivideAndConquer` | DivideAndConquer | `dc-divisions` (int or string), `dc-portfolio-size` (int). |

`solvers` lists the ids of the solver descriptors (CDCL or local search) that participate. Order matters: it determines the **strategy-internal slot index** (which solver the strategy treats as its slot 0, slot 1, ...). It does **not** affect the solver's own id, which is fixed at construction time from the descriptor's index in `cdclSolvers` / `localSearchers` and is what shows up in logs, stats and as the `SharingEntity` id. Duplicate ids in this list are rejected.

```json
{
  "name": "PortfolioSimple",
  "solvers": ["kmixed", "kfocused", "yal_0"],
  "params": {}
}
```

```json
{
  "name": "DivideAndConquer",
  "solvers": ["kmixed", "kfocused"],
  "params": {
    "dc-divisions": 4,
    "dc-portfolio-size": 2
  }
}
```

## Sharers

Each sharer is a thread that drives a non-empty list of sharing strategies. Splitting strategies across several sharers increases parallelism at the cost of extra OS threads; bundling them into a single sharer does the opposite. For now, sharer ids do not need to be referenced from anywhere else and are not subject to the dangling-id check.

Required JSON fields: `id`, `strategies`. Duplicate ids inside `strategies` are rejected.

```json
{
  "id": "sharer_main",
  "strategies": ["horde0"]
}
```

```json
{
  "id": "sharer_global",
  "strategies": ["horde0", "horde0", "horde_layer2"]
}
```

# Subscription model

Sharing is subscription-based:

- For each id in `producers`, the builder calls `producer.addClient(strategy)`. At runtime, that producer pushes exported clauses *to* the strategy.
- For each id in `clients`, the builder calls `strategy.addClient(client)`. At runtime, the strategy pushes filtered clauses *to* the client.

For HordeSat in particular, the filter sizes its per-producer buffers at configuration time, which is why the builder also calls `strategy->configure("producer-ids", "...")` automatically. Other strategies do not need this.

# Tips and pitfalls

- **Two distinct ids — don't confuse them.** Each solver carries its own *solver id* (first ctor arg, used in logs, stats and as the `SharingEntity` id), assigned from the index of its descriptor in the `cdclSolvers` / `localSearchers` arrays. Keep `cdclSolvers` stable if you want stable ids across runs.
- **One DB instance per consumer.** A single template referenced by N solvers produces N independent ClauseDatabase instances. They do not share state. If you want one shared database between two entities you cannot express it in the current schema.
- **Sharing strategy uniqueness.** If you need the same logical strategy to appear in two sharers, declare two strategies with separate ids. The one-reference rule is a parser-level check, not a runtime constraint.
- **Unknown solver options abort.** `kissat`/`cadical`/`lingeling` forward every key blindly to the backend; a typo in a `params` key will terminate the run with `PERR_ARGS_ERROR`. The hand-coded backends (`minisat`, `glucoseSyrup`, `mapleCOMSPS`, the local searchers) also checks them. Future implementation must also be done carefully.
- **All TOP Level** arrays and objects must be present even if they are empty. We can have an empty "LocalSearchers", or cdclSolvers, databases or even sharing strategies and sharers.

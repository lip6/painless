import json
import random
import math
import argparse
import sys
from typing import Any
import hashlib

def evaluate_rule(rule: dict, solver_id: int, rng: random.Random) -> Any:
    rule_type = rule["type"]

    if rule_type == "constant":
        return rule["value"]

    elif rule_type == "linear":
        factor = rule.get("factor", 1)
        bias = rule.get("bias", 0)
        return solver_id * factor + bias

    elif rule_type == "uniform_random":
        lo = rule["min"]
        hi = rule["max"]
        is_int = rule.get("integer", True)
        if is_int:
            return rng.randint(lo, hi)
        else:
            return rng.uniform(lo, hi)

    elif rule_type == "normal_random":
        center = rule["center"]
        deviation = rule["deviation"]
        is_int = rule.get("integer", True)
        value = rng.gauss(center, deviation)
        if is_int:
            return int(round(value))
        return value

    elif rule_type == "bernoulli_random":
        p = rule["p"]
        return 1 if rng.random() < p else 0

    else:
        raise ValueError(f"Unknown rule type: '{rule_type}'")


def resolve_params(solver_type: str, spec: dict, solver_id: int, rng: random.Random) -> dict:
    reserved = {"type", "importDB", "idRange"}
    params = {}

    for param_name, rules in spec.items():
        if param_name in reserved:
            continue
        if not isinstance(rules, list):
            continue

        value = None
        for rule in rules:
            lo, hi = rule["range"]
            if lo <= solver_id <= hi:
                value = evaluate_rule(rule, solver_id, rng)
                # last-wins: keep iterating, overwrite

        if value is not None:
            params[param_name] = value

    return params


SOLVER_CATEGORY_MAP = {
    "cdcl": "cdclSolvers",
    "local": "localSearchers",
}


def generate_solvers(abstract_schema: dict, seed: int = 42) -> dict:
    result = {
        "cdclSolvers": [],
        "localSearchers": [],
    }

    for solver_type, spec in abstract_schema.items():
        id_range = spec["idRange"]
        import_db = spec.get("importDB")
        category_key = spec.get("type")

        if category_key not in SOLVER_CATEGORY_MAP:
            raise ValueError(
                f"Solver type '{category_key}' for '{solver_type}' is unknown. "
                f"Expected one of: {list(SOLVER_CATEGORY_MAP.keys())}"
            )

        target_array = SOLVER_CATEGORY_MAP[category_key]
        id_start, id_end = id_range

        for solver_id in range(id_start, id_end + 1):
            # Deterministic RNG per solver: seeded by global seed + type + id
            key = f"{solver_type}_{solver_id}".encode()
            fingerprint = int.from_bytes(hashlib.sha256(key).digest()[:4], "big")
            rng = random.Random(seed ^ fingerprint)

            params = resolve_params(solver_type, spec, solver_id, rng)

            solver = {"id": f"{solver_type}{solver_id}", "type": solver_type}
            if import_db:
                solver["importDB"] = import_db
            solver["params"] = params

            result[target_array].append(solver)

    return result


def main():
    parser = argparse.ArgumentParser(
        description="Convert abstract solver schema to explicit solver array."
    )
    parser.add_argument(
        "input", help="Path to abstract schema JSON file"
    )
    parser.add_argument(
        "-o", "--output", help="Output JSON file (default: stdout)", default=None
    )
    parser.add_argument(
        "-s", "--seed", help="Global RNG seed (default: 42)", type=int, default=42
    )
    parser.add_argument(
        "--indent", help="JSON indentation (default: 2)", type=int, default=2
    )
    args = parser.parse_args()

    with open(args.input, "r") as f:
        abstract_schema = json.load(f)

    result = generate_solvers(abstract_schema, seed=args.seed)

    output = json.dumps(result, indent=args.indent)

    if args.output:
        with open(args.output, "w") as f:
            f.write(output)
        n_cdcl = len(result["cdclSolvers"])
        n_local = len(result["localSearchers"])
        print(f"Written {n_cdcl} cdcl and {n_local} local searchers to {args.output}")
    else:
        print(output)


if __name__ == "__main__":
    main()
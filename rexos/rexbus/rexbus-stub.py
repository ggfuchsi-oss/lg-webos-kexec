#!/usr/bin/env python3
"""
rexbus-stub.py — placeholder for the RexOS-TV native IPC bus.

Loads the harvested luna schema (195 services / 4064 methods) and stands up a
minimal daemon: a health ping + service-count report. This is the seed; the
real on-TV transport (shared-mem ring / VCPU IPC) and typed client land later.

Run on the PC for now:  python3 rexbus-stub.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SCHEMA = os.path.join(HERE, "schema.json")


def load_schema():
    with open(SCHEMA) as f:
        return json.load(f)


def summarize(schema):
    # schema.json: {service_name: {"methods": {method: category, ...}, ...}, ...}
    services = list(schema.keys())
    methods = sum(
        len(v.get("methods", {})) for v in schema.values() if isinstance(v, dict)
    )
    return len(services), methods


def main():
    schema = load_schema()
    n_svc, n_meth = summarize(schema)
    print(f"[rexbus] schema loaded: {n_svc} services, {n_meth} methods")
    print("[rexbus] stub daemon online. Transport + typed client: later phase.")
    # health ping
    while True:
        try:
            input("rexbus> ping (enter to re-report, Ctrl-C to quit): ")
        except EOFError:
            break
        print(f"  pong — {n_svc} services, {n_meth} methods registered")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
rasplab — Lab 6: Python Correlation

The extension streams taint events (one JSON per line). Each request is its own
little subgraph; the magic is stitching them together across storage locations to
reconstruct the *provenance graph* and surface second-order chains.

    SOURCE ─▶ EXPORT@loc  ┅┅(join by loc)┅┅  RECOVER@loc ─▶ SINK
    (req A)                                              (req B)   ⇒ SECOND-ORDER

You fill two TODO(lab6-*):
  (1) join EXPORT/RECOVER events that share a storage location  (the cross edge)
  (2) classify a sink as second-order when its source is in a different request

Run: python3 correlate.py trace.jsonl
"""
import json
import sys


def load(path):
    events = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                events.append(json.loads(line))
    return events


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "trace.jsonl"
    events = load(path)

    source_of = {}     # uuid -> SOURCE event
    request_of = {}    # uuid -> request/service id
    sinks = []         # SINK events
    exports = []       # EXPORT events
    recovers = []      # RECOVER events
    parent = {}        # uuid -> parent uuid  (the cross-boundary edge)
    join_loc = {}      # uuid -> location that stitched it to its parent

    for e in events:
        u = e.get("uuid")
        if u and "req" in e:
            request_of.setdefault(u, e["req"])
        ev = e["event"]
        if ev == "SOURCE":
            source_of[u] = e
        elif ev == "SINK":
            sinks.append(e)
        elif ev == "EXPORT":
            exports.append(e)
        elif ev == "RECOVER":
            recovers.append(e)

    # ============================================================
    # TODO(lab6-1): stitch the graph across storage locations.
    #   For every RECOVER event, find an EXPORT event with the SAME `loc`, and
    #   record the edge from the exporter to the recoverer:
    #       parent[recover["uuid"]]  = export["uuid"]
    #       join_loc[recover["uuid"]] = recover["loc"]
    #   (This is the edge Lab 3 was missing — the one that makes the chain whole.)
    # ============================================================


    # provided: walk parent edges back to the root uuid
    def trace_back(u):
        seen = set()
        while u in parent and u not in seen:
            seen.add(u)
            u = parent[u]
        return u

    findings = []
    for s in sinks:
        root = trace_back(s["uuid"])
        src = source_of.get(root)
        if not src:
            continue  # sink not tied to a known source

        # ============================================================
        # TODO(lab6-2): classify this sink.
        #   It's SECOND-ORDER when the SOURCE and the SINK happened in DIFFERENT
        #   requests/services (taint crossed a boundary). If so, append a finding:
        #       findings.append({
        #           "kind": "second-order",
        #           "source": src["source"],
        #           "src_req": request_of.get(root),
        #           "sink": s["sink"],
        #           "sink_req": request_of.get(s["uuid"]),
        #           "via": join_loc.get(s["uuid"], "?"),
        #           "value": s.get("value"),
        #       })
        #   (same-request source→sink is first-order; you can skip or tag it.)
        # ============================================================
        pass

    # provided: report
    so = [f for f in findings if f["kind"] == "second-order"]
    print("=== provenance correlation ===")
    for f in so:
        print(f'[SECOND-ORDER] {f["source"]} (req {f["src_req"]}) '
              f'--{f["via"]}--> {f["sink"]} (req {f["sink_req"]})   payload={f["value"]!r}')
    print(f'\nsummary: {len(so)} second-order finding(s) across {len(sinks)} sink(s)')
    if not so:
        sys.exit(2)


if __name__ == "__main__":
    main()

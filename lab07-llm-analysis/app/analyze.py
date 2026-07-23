#!/usr/bin/env python3
"""
rasplab — Lab 7: LLM Analysis

Take one correlated finding from Lab 6 and have an LLM (a) explain the bug in
plain English and (b) draft a PoC. Course setup: opencode + model
`ais3/gemma-4-26b` + your own API key (configured in opencode). That's the default
here — opencode is used automatically whenever it's on PATH; the offline MockLLM
is the fallback (e.g. in CI containers with no opencode).

You fill two TODO(lab7-*):
  (1) build_prompt(): turn the finding into a prompt the model can act on
  (2) main(): call the model, parse its JSON, print the report

Run: python3 analyze.py finding.json
"""
import json
import os
import re
import shutil
import subprocess
import sys


# ---- LLM backends (provided) -------------------------------------------- #
class MockLLM:
    """Deterministic stand-in. It answers using the facts it finds in YOUR
    prompt (as `KEY: value` lines) — so if your prompt omits the sink/payload,
    the 'model' can't mention them. Prompt hygiene is graded, in other words."""
    name = "mock"

    def generate(self, prompt: str) -> str:
        f = dict(re.findall(r'^([A-Z_]+):\s*(.+)$', prompt, re.M))
        src = f.get("SOURCE", "?"); sink = f.get("SINK", "?")
        via = f.get("VIA", "?"); payload = f.get("PAYLOAD", "?")
        return json.dumps({
            "severity": "high",
            "explanation": (
                f"Second-order vulnerability. Untrusted input from {src} is "
                f"persisted at {via} during one request and, in a later request, "
                f"flows unsanitized into {sink}. Because the write and the "
                f"execution live in different requests/services, single-request "
                f"taint analysis never sees the whole path."),
            "poc": (
                f"1) send note={payload}  (stored at {via})\n"
                f"2) trigger the request that reads {via} and passes it to {sink}\n"
                f"   => {sink} executes the injected payload {payload!r}"),
        })


class OpencodeLLM:
    """Real backend via the opencode CLI — uses whatever local AI you configured
    in opencode. Model-agnostic: no vendor SDK, no hard-coded model name.
    Optionally pin a model with OPENCODE_MODEL (passed to `opencode run -m`)."""
    name = "opencode"

    def __init__(self, model=None):
        self.model = model

    def generate(self, prompt: str) -> str:
        if not shutil.which("opencode"):
            raise RuntimeError("opencode CLI not found on PATH")
        cmd = ["opencode", "run"]
        if self.model:
            cmd += ["-m", self.model]
        cmd.append(prompt)  # opencode run takes the message as a positional arg
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
        out = res.stdout.strip()
        if res.returncode != 0 or not out:
            raise RuntimeError(
                f"opencode run failed (exit {res.returncode}, model={self.model}): "
                f"{res.stderr.strip()[:300]}")
        return out


# Course model is `ais3/gemma-4-26b`. In opencode it's addressed as
# <your-provider>/ais3/gemma-4-26b — the provider prefix is YOUR personal config,
# so we don't hard-code it. Set that model as your opencode default and we call it
# with no -m; or pass the full id via OPENCODE_MODEL to override.
DEFAULT_MODEL = None


def get_llm():
    # Course default: opencode + your configured default model + your own API key.
    # Used automatically whenever the opencode CLI is on PATH; falls back to the
    # offline MockLLM otherwise (e.g. CI). Force with LLM_BACKEND=opencode|mock.
    backend = os.environ.get("LLM_BACKEND", "").lower()
    if backend == "mock":
        return MockLLM()
    if backend == "opencode" or shutil.which("opencode"):
        return OpencodeLLM(os.environ.get("OPENCODE_MODEL", DEFAULT_MODEL))
    return MockLLM()


# ---- report printer (provided) ------------------------------------------ #
def print_report(finding, llm, data):
    print(f"=== rasplab LLM report (backend: {llm.name}) ===")
    print(f"severity : {data.get('severity', '?')}")
    print(f"chain    : {finding['source']} ({finding['src_req']}) "
          f"--{finding['via']}--> {finding['sink']} ({finding['sink_req']})")
    print("\n[explanation]\n" + data.get("explanation", ""))
    print("\n[poc]\n" + data.get("poc", ""))


def build_prompt(finding) -> str:
    # ============================================================
    # TODO(lab7-1): build a prompt from `finding`.
    #   Requirements:
    #     - state the facts as KEY: value lines the model (and the mock) can read:
    #         SOURCE:  <finding['source']>
    #         SINK:    <finding['sink']>
    #         VIA:     <finding['via']>
    #         PAYLOAD: <finding['value']>
    #     - ask the model to return STRICT JSON with keys:
    #         "severity", "explanation", "poc"
    #   Return the prompt string.
    # ============================================================
    raise NotImplementedError("build_prompt: complete TODO(lab7-1)")


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "finding.json"
    finding = json.load(open(path))
    llm = get_llm()

    # ============================================================
    # TODO(lab7-2): run the analysis.
    #   - prompt = build_prompt(finding)
    #   - raw    = llm.generate(prompt)
    #   - data   = json.loads(raw)         # the model returns JSON
    #   - print_report(finding, llm, data)
    #   (bonus: wrap json.loads in try/except and salvage the largest {...} block,
    #    since real models sometimes wrap JSON in prose.)
    # ============================================================
    raise NotImplementedError("main: complete TODO(lab7-2)")


if __name__ == "__main__":
    main()

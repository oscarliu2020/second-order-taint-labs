#!/usr/bin/env python3
"""
rasplab — Lab 7: LLM Analysis

Take one correlated finding from Lab 6 and have an LLM (a) explain the bug in
plain English and (b) draft a PoC. Ships with an offline MockLLM so it runs in CI
without an API key; set ANTHROPIC_API_KEY to hit a real model.

You fill two TODO(lab7-*):
  (1) build_prompt(): turn the finding into a prompt the model can act on
  (2) main(): call the model, parse its JSON, print the report

Run: python3 analyze.py finding.json
"""
import json
import os
import re
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


class AnthropicLLM:
    """Real backend. Requires `pip install anthropic` and ANTHROPIC_API_KEY."""
    name = "anthropic"

    def __init__(self, key): self.key = key

    def generate(self, prompt: str) -> str:
        from anthropic import Anthropic
        client = Anthropic(api_key=self.key)
        msg = client.messages.create(
            model="claude-sonnet-5", max_tokens=1024,
            messages=[{"role": "user", "content": prompt}])
        return msg.content[0].text


def get_llm():
    key = os.environ.get("ANTHROPIC_API_KEY")
    return AnthropicLLM(key) if key else MockLLM()


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

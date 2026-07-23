# Lab 7 — pure Python. No opencode in the image, so this container always uses
# the offline MockLLM (deterministic, for CI/grading). The course default backend
# (opencode + ais3/gemma-4-26b + your API key) kicks in when you run analyze.py on
# your host, where the opencode CLI is on PATH.
FROM python:3.11-slim
WORKDIR /app
COPY app/ /app/
CMD ["python3", "analyze.py", "finding.json"]

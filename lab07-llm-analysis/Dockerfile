# Lab 7 — pure Python. Offline MockLLM by default; set ANTHROPIC_API_KEY (and
# `pip install anthropic`) to use a real model.
FROM python:3.11-slim
WORKDIR /app
COPY app/ /app/
CMD ["python3", "analyze.py", "finding.json"]

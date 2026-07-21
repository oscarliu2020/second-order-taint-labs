# Lab 6 — pure Python, no PHP. Correlate the taint trace offline.
FROM python:3.11-slim
WORKDIR /app
COPY app/ /app/
CMD ["python3", "correlate.py", "trace.jsonl"]

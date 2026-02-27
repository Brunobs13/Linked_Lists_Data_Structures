#!/usr/bin/env sh
set -eu

DB_HOST="${DB_HOST:-db}"
DB_PORT="${DB_PORT:-5432}"
DB_USER="${DB_USER:-log_engine}"

until pg_isready -h "$DB_HOST" -p "$DB_PORT" -U "$DB_USER" >/dev/null 2>&1; do
  echo "Waiting for PostgreSQL at ${DB_HOST}:${DB_PORT}..."
  sleep 1
done

make build

export ENGINE_LIB_PATH="${ENGINE_LIB_PATH:-/app/build/liblog_engine.so}"
exec uvicorn src.api.app:app --host 0.0.0.0 --port "${API_PORT:-8000}"

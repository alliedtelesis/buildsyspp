#!/bin/sh
# Run cpplint using a local virtual environment.
# Usage: ./run-cpplint.sh [extra cpplint args...]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VENV="$SCRIPT_DIR/.venv-cpplint"

if [ ! -d "$VENV" ]; then
	python3 -m venv "$VENV"
fi

"$VENV/bin/pip" install --quiet -r "$SCRIPT_DIR/requirements.txt"

exec "$VENV/bin/python3" -m cpplint --recursive "$@" "$SCRIPT_DIR/src"

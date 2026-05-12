#!/usr/bin/env bash
set -e

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  echo "Usage: $0 input.epub [output.txt]"
  exit 1
fi

EPUB="$1"
BASENAME=$(basename "$EPUB" .epub | tr ' ' '_' | tr ',' '_' | tr -s '_')

if [ "$#" -eq 2 ]; then
  OUTPUT="$2"
else
  OUTPUT="$(dirname "$EPUB")/${BASENAME}.txt"
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TMP=$(mktemp /tmp/epub2rsvp_XXXXXX.md)

echo "Converting: $EPUB"
pandoc "$EPUB" -o "$TMP" --to markdown --wrap=none
python3 "$SCRIPT_DIR/epub2rsvp.py" "$TMP" "$OUTPUT"
rm "$TMP"

echo "Output: $OUTPUT"

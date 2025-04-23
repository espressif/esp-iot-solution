#!/bin/bash

SRC_DIR="$(realpath "$(dirname "$0")/../frontend_source/dist")"
DEST_DIR="$(realpath "$(dirname "$0")/../spiffs")"

rm -fr "$DEST_DIR"
mkdir -p "$DEST_DIR"

find "$SRC_DIR" -type f | while read -r FILE; do
    REL_PATH="${FILE#$SRC_DIR/}"

    DEST_PATH="$DEST_DIR/$REL_PATH.gz"

    mkdir -p "$(dirname "$DEST_PATH")"

    gzip -c "$FILE" > "$DEST_PATH"
done

#!/bin/bash

FLASH_ARGS_FILE="flash_args"

BOOTLOADER_MERGE_BIN="$1"
PARTITION_TABLE_OFFSET_HEX="$2"

if [ ! -f "$BOOTLOADER_MERGE_BIN" ]; then
    echo "Error: File $BOOTLOADER_MERGE_BIN does not exist."
    exit 1
fi

FILE_SIZE_DEC=$(stat -c %s "$BOOTLOADER_MERGE_BIN")
PARTITION_TABLE_OFFSET_DEC=$((PARTITION_TABLE_OFFSET_HEX))
FILE_SIZE_HEX=$(printf "0x%X" "$FILE_SIZE_DEC")

echo "Bootloader Merge Size: $FILE_SIZE_HEX"

if [ "$FILE_SIZE_DEC" -lt "$PARTITION_TABLE_OFFSET_DEC" ]; then
    echo "Bootloader Merge Size ($FILE_SIZE_HEX) is smaller than CONFIG_PARTITION_TABLE_OFFSET ($PARTITION_TABLE_OFFSET_HEX)."
else
    echo "Bootloader Merge Size ($FILE_SIZE_HEX) is NOT smaller than CONFIG_PARTITION_TABLE_OFFSET ($PARTITION_TABLE_OFFSET_HEX)."
    exit 1
fi

if [ ! -f "$FLASH_ARGS_FILE" ]; then
    echo "Error: $FLASH_ARGS_FILE not found!"
    exit 1
fi

sed -i 's|bootloader/bootloader\.bin|bootloader/bootloader_merge.bin|' "$FLASH_ARGS_FILE"
sed -i '/bootloader_uf2\.bin/d' "$FLASH_ARGS_FILE"

echo "flash_args has been updated successfully."

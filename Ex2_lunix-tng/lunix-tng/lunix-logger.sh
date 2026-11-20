#!/bin/bash
# lunix-logger.sh
# 
# Usage:
#   ./lunix-logger.sh [/dev/lunix0-temp] [measurements.log]

DEV="${1:-/dev/lunix0-temp}"
LOGFILE="${2:-measurements.log}"

if [ ! -e "$DEV" ]; then
    echo "[-] Error: Device node $DEV does not exist."
    exit 1
fi

echo "[*] Monitoring device: $DEV"
echo "[*] Writing logs to:   $LOGFILE"
echo "[*] Press Ctrl+C to stop."
echo "--------------------------------------------------"

# Read line-by-line. Each iteration blocks until the driver provides a new line.
while IFS= read -r line; do
    TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")
    LOG_ENTRY="[$TIMESTAMP] $DEV: $line"
    
    # 1. Display on terminal
    echo "$LOG_ENTRY"
    
    # 2. Append to log file
    echo "$LOG_ENTRY" >> "$LOGFILE"
done < "$DEV"

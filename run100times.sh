#!/bin/bash

LOGFILE="log.txt"
CMD="python3 -m pytest test_server.py -vv -s --timeout 30"

# Clear the old log file
> "$LOGFILE"

echo "to see the log, after the last test open $LOGFILE"

for i in $(seq 1 100); do
    echo "==================== Run #$i ====================" >> "$LOGFILE"
    $CMD >> "$LOGFILE" 2>&1
    echo "" >> "$LOGFILE"
    echo "test $i done"
    tail -n 2 "$LOGFILE" | head -n 1

done

echo "✅ Completed 100 test runs. See $LOGFILE"
echo "experimental pyton test log statistics maker:"
python3 analyze_log.py


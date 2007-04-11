#!/bin/sh

echo "Testing lights"
tranzport_lights &
A=$!
sleep 30
kill $A
echo "Testing interleaved_reads/writes"
tranzport &
A=$!
sleep 30
kill $A

exit 0

# not done yet
echo "Testing_screen"
tranzport_screen &
A=$!
sleep 30
kill $A
echo "Testing_reads"
tranzport_read &
A=$!
sleep 30
kill $A


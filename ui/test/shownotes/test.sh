#!/bin/sh
# TEMP repro loop
[ "${QLC_TEST_REAL_SCREEN:-}" = 1 ] || export QT_QPA_PLATFORM=offscreen
export LD_LIBRARY_PATH=../../src:../../../engine/src
export DYLD_FALLBACK_LIBRARY_PATH=../../src:../../../engine/src
if [ -x ./shownotes_test.app/Contents/MacOS/shownotes_test ]; then B=./shownotes_test.app/Contents/MacOS/shownotes_test; else B=./shownotes_test; fi
bad=0; n=0
while [ $n -lt 150 ]; do
  n=$((n+1))
  $B snapshotAndShowMe > /tmp/sn_$$.log 2>&1; rc=$?
  if [ $rc -ne 0 ]; then bad=$((bad+1)); echo "LOOP run $n rc=$rc"; tail -5 /tmp/sn_$$.log; fi
done
echo "LOOP bad=$bad of $n"
$B > /tmp/sn_full_$$.log 2>&1; rc=$?
echo "FULL rc=$rc"; tail -3 /tmp/sn_full_$$.log
[ $bad -eq 0 ] && [ $rc -eq 0 ]

#!/bin/sh
# Sequential engine stress sweeps (no CPU contention between runs).
# Results go to stdout; run from stresstest/engine.
QS=./build/qlcstress
FX=../../resources/fixtures
line() { echo "------------------------------------------------------------"; }

echo "### A) HEADLINE: 80 universes, full heavy load (target scale)"
$QS engine --mode flatout --universes 80 --fixtures-per-uni 100 \
   --scenes 300 --chasers 80 --matrices 40 --efx 40 --collections 16 \
   --ticks 300 --fixtures-dir $FX 2>/dev/null | grep -E "spec:|flatout"
line

echo "### B) ISOLATION: scale universes under LIGHT function load"
echo "###    (20 scenes, 4 chasers, 2 matrices, 2 efx) -> universe/channel ceiling"
for U in 10 20 40 60 80 120; do
  $QS engine --mode flatout --universes $U --fixtures-per-uni 120 \
     --scenes 20 --chasers 4 --matrices 2 --efx 2 --collections 2 \
     --ticks 400 --fixtures-dir $FX 2>/dev/null \
     | grep "flatout " | sed "s/^flatout/uni=$U  /"
done
line

echo "### C) ISOLATION: scale RGB matrices only (16 universes fixed)"
echo "###    -> how many concurrent RGBMatrix before tick budget blown"
for M in 5 10 20 40 80 160; do
  $QS engine --mode flatout --universes 16 --fixtures-per-uni 120 \
     --scenes 10 --chasers 2 --matrices $M --efx 0 --collections 0 \
     --ticks 400 --fixtures-dir $FX 2>/dev/null \
     | grep "flatout " | sed "s/^flatout/matrices=$M  /"
done
line
echo "### sweeps complete"

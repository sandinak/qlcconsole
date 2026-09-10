# Spikes

Throwaway programs kept because their *measurements* are cited in design docs
and would otherwise be unreproducible.

- `zbuffer_spike.cpp` — software z-buffer vs the rig view's painter's-algorithm
  DrawOp queue. Numbers in `RIG3D_STRATEGY_REVIEW.md` §7. Build:

  ```sh
  clang++ -std=c++17 -O2 -o /tmp/zspike spikes/zbuffer_spike.cpp \
    -F/opt/homebrew/opt/qt6/lib -framework QtCore -framework QtGui \
    -I/opt/homebrew/opt/qt6/lib/QtCore.framework/Headers \
    -I/opt/homebrew/opt/qt6/lib/QtGui.framework/Headers \
    -I/opt/homebrew/opt/qt6/include
  QT_QPA_PLATFORM=offscreen /tmp/zspike /tmp 14 128   # outdir, steps, pixels
  ```

  Not built by CMake and not part of the gate — deliberately.

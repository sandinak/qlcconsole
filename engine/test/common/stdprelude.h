/*
  Q Light Controller Plus - test support
  stdprelude.h

  Include this ABOVE `#define private public` / `#define protected public`.

  The define changes every access specifier in every header that is first
  included after it. libstdc++ rejects that for some of its own headers
  (MSYS2 GCC 16 stopped at <sstream>), and a project header can pull one in
  without saying so. Including the standard headers the project's headers
  use here, before the define, makes them include-guarded no-ops later.
  platforms/linux/check-private-public.sh (part of the gate) fails a test
  whose headers under the define bring in a standard header that isn't
  already above it.

  Licensed under the Apache License, Version 2.0
*/

#ifndef QLC_TEST_STDPRELUDE_H
#define QLC_TEST_STDPRELUDE_H

#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#endif

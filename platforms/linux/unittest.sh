#!/bin/bash

CURRUSER=$(whoami)
TESTPREFIX=""
SLEEPCMD=""
RUN_UI_TESTS="0"
THISCMD=`basename "$0"`

TARGET=${1:-}
# NOTE: this script is not run in place. The root-level ./unittest.sh stages
# it (plus a fresh copy of every test.sh + the resources/ tests need) into
# the build dir and cd's there BEFORE invoking this copy — so by the time we
# get here, CWD already IS the build dir and plain relative paths
# (${TESTDIR}/${test}) are correct. Don't reintroduce a build-dir prefix here.

if [ "$TARGET" != "ui" ] && [ "$TARGET" != "qmlui" ]; then
  echo >&2 "Usage: $THISCMD ui|qmlui"
  exit 1
fi

if [ "$CURRUSER" == "runner" ] \
    || [ "$CURRUSER" == "buildbot" ] \
    || [ "$CURRUSER" == "abuild" ]; then
  echo "Found build environment with CURRUSER='$CURRUSER' and OSTYPE='$OSTYPE'"
  if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if [ $(which xvfb-run) == "" ]; then
      echo "xvfb-run not found in this system. Please install with: sudo apt-get install xvfb"
      exit
    fi

    TESTPREFIX="QT_QPA_PLATFORM=minimal xvfb-run --auto-servernum"
    RUN_UI_TESTS="1"
    # if we're running as build slave, set a sleep time to start/stop xvfb between tests
    SLEEPCMD="sleep 1"

  elif [[ "$OSTYPE" == "darwin"* ]]; then
    TESTPREFIX="QT_QPA_PLATFORM=offscreen"
    RUN_UI_TESTS="1"
  fi

else

  if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    XPID=$(pidof X)
    if [ ! ${#XPID} -gt 0 ]; then
      XPID=$(pidof Xorg)
    fi
    if [ ${#XPID} -gt 0 ]; then
      RUN_UI_TESTS="1"
    fi
  elif [[ "$OSTYPE" == "darwin"* ]]; then
    # No X server concept on macOS to gate on — offscreen doesn't need one.
    TESTPREFIX="QT_QPA_PLATFORM=offscreen"
    RUN_UI_TESTS="1"
  fi
fi

#############################################################################
# Fixture definitions check with xmllint
#############################################################################

pushd resources/fixtures/scripts
./check
RET=$?
popd
if [ $RET -ne 0 ]; then
    echo "Fixture definitions are not valid. Please fix before commit."
    exit $RET
fi

#############################################################################
# Engine tests
#############################################################################

TESTDIR=engine/test
TESTS=$(find ${TESTDIR} -maxdepth 1 -mindepth 1 -type d)
for test in ${TESTS}
do
    # Ignore .git
    if [ $(echo ${test} | grep ".git") ]; then
        continue
    fi

    # Ignore CMakeFiles
    if [ $(echo ${test} | grep "CMakeFiles") ]; then
        continue
    fi

    # Isolate just the test name
    test=$(echo ${test} | sed 's/engine\/test\///')

    $SLEEPCMD
    # Execute the test
    pushd ${TESTDIR}/${test}
    echo "$TESTPREFIX ./test.sh"
    eval $TESTPREFIX ./test.sh
    RESULT=${?}
    popd
    if [ ${RESULT} != 0 ]; then
        echo "${RESULT} Engine unit tests failed. Please fix before commit."
        exit ${RESULT}
    fi
done

#############################################################################
# UI tests
#############################################################################

# Skip ui in qmlui mode
if [ "$RUN_UI_TESTS" -eq "1" ] && [ "$TARGET" != "qmlui" ]; then

TESTDIR=ui/test
TESTS=$(find ${TESTDIR} -maxdepth 1 -mindepth 1 -type d)
for test in ${TESTS}
do
    # Ignore .git
    if [ $(echo ${test} | grep ".git") ]; then
        continue
    fi

    # Ignore CMakeFiles
    if [ $(echo ${test} | grep "CMakeFiles") ]; then
        continue
    fi

    # Isolate just the test name
    test=$(echo ${test} | sed 's/ui\/test\///')

    $SLEEPCMD
    # Execute the test via its checked-in test.sh (sets LD_LIBRARY_PATH/
    # DYLD_FALLBACK_LIBRARY_PATH itself, and already resolves the macOS
    # <name>_test.app/Contents/MacOS/<name>_test bundle path vs. a plain
    # executable — the old bare "./${test}_test" here didn't).
    pushd ${TESTDIR}/${test}
    eval $TESTPREFIX ./test.sh
    RESULT=${?}
    popd
    if [ ${RESULT} != 0 ]; then
        echo "${RESULT} UI unit tests failed. Please fix before commit."
        exit ${RESULT}
    fi
done

fi

#############################################################################
# Enttec wing tests
#############################################################################

$SLEEPCMD
pushd plugins/enttecwing/test
eval $TESTPREFIX ./test.sh
RESULT=$?
if [ $RESULT != 0 ]; then
	echo "${RESULT} Enttec wing unit tests failed. Please fix before commit."
	exit $RESULT
fi
popd

#############################################################################
# Velleman test
#############################################################################

if [[ "$OSTYPE" == "darwin"* ]]; then
  echo "Skip Velleman test (not supported on OSX)"
else
  $SLEEPCMD
  pushd plugins/velleman/test
  eval $TESTPREFIX ./test.sh
  RESULT=$?
  if [ $RESULT != 0 ]; then
    echo "Velleman unit test failed ($RESULT). Please fix before commit."
	exit $RESULT
  fi
  popd
fi

#############################################################################
# MIDI tests
#############################################################################

$SLEEPCMD
pushd plugins/midi/test
eval $TESTPREFIX ./test.sh
RESULT=$?
if [ $RESULT != 0 ]; then
	echo "${RESULT} MIDI unit tests failed. Please fix before commit."
	exit $RESULT
fi
popd

#############################################################################
# ArtNet tests
#############################################################################

$SLEEPCMD
pushd plugins/artnet/test
eval $TESTPREFIX ./test.sh
RESULT=$?
if [ $RESULT != 0 ]; then
	echo "${RESULT} ArtNet unit tests failed. Please fix before commit."
	exit $RESULT
fi
popd

#############################################################################
# Final judgment
#############################################################################

echo "Unit tests passed."

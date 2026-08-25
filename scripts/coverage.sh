#!/bin/bash
# Build safeAPIFramework with gcov instrumentation, run ctest, and produce a
# line/function/branch coverage report with gcovr.
#
# Usage:
#   ./scripts/coverage.sh                 # build (all features ON) + report
#   ./scripts/coverage.sh --no-fail       # report only, don't fail on <100%
#
# See docs/COVERAGE_REPORT.md for the tool choice rationale and the latest
# recorded numbers.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build/coverage"
REPORT_DIR="$BUILD_DIR/report"

echo "=== safeAPIFramework: coverage build (gcov + gcovr) ==="

if ! command -v gcovr &> /dev/null; then
    echo "ERROR: gcovr not found. Install with: brew install gcovr  (or: pip install gcovr)"
    exit 1
fi

echo "gcovr version: $(gcovr --version | head -1)"

echo ""
echo "-- configure --"
cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DSAFEAPI_BUILD_TESTS=ON \
    -DSAFEAPI_ENABLE_COVERAGE=ON

echo ""
echo "-- build --"
cmake --build "$BUILD_DIR" -j4

echo ""
echo "-- ctest --"
(cd "$BUILD_DIR" && ctest --output-on-failure)

echo ""
echo "-- gcovr --"
mkdir -p "$REPORT_DIR"

FAIL_FLAGS="--fail-under-line 100 --fail-under-function 100 --fail-under-branch 100"
if [ "$1" = "--no-fail" ]; then
    FAIL_FLAGS=""
fi

# --gcov-ignore-errors=no_working_dir_found: gcov also emits (harmless, out
# of scope for src/) coverage data for CMake's own compiler-identification
# probe sources, which no longer exist post-configure - see gcovr's own
# no_working_dir_found diagnostic. --filter restricts the report to this
# project's own src/ tree.
set +e
gcovr -r "$PROJECT_ROOT" --object-directory "$BUILD_DIR" \
    --filter 'src/' \
    --exclude-unreachable-branches \
    --gcov-ignore-errors=no_working_dir_found \
    --html --html-details -o "$REPORT_DIR/index.html" \
    --xml "$REPORT_DIR/coverage.xml" \
    --print-summary \
    $FAIL_FLAGS
STATUS=$?
set -e

echo ""
echo "HTML report: $REPORT_DIR/index.html"
echo "Cobertura XML: $REPORT_DIR/coverage.xml"

if [ $STATUS -eq 0 ]; then
    echo "=== coverage PASSED (100% line/function/branch, or --no-fail was passed) ==="
else
    echo "=== coverage FAILED (below 100% line/function/branch) ==="
fi
exit $STATUS

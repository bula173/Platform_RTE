#!/bin/bash
# Run cppcheck with MISRA C:2012 analysis on RteFramework source
#
# Usage:
#   ./scripts/run-cppcheck.sh                    # Run analysis, print to stdout
#   ./scripts/run-cppcheck.sh --html report.html # Generate HTML report
#   ./scripts/run-cppcheck.sh --json report.json # Generate JSON report

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== RteFramework: MISRA C:2012 Static Analysis ==="
echo ""

if ! command -v cppcheck &> /dev/null; then
    echo "ERROR: cppcheck not found. Install with: brew install cppcheck"
    exit 1
fi

echo "cppcheck version: $(cppcheck --version)"
echo ""

# Build compile_commands.json if needed
if [ ! -f "$PROJECT_ROOT/build/compile_commands.json" ]; then
    echo "Building project to generate compile_commands.json..."
    mkdir -p "$PROJECT_ROOT/build"
    cd "$PROJECT_ROOT/build"
    cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON > /dev/null
    cd "$PROJECT_ROOT"
fi

# Determine output format from arguments
OUTPUT_FORMAT="text"
OUTPUT_FILE=""
if [ "$1" = "--html" ]; then
    OUTPUT_FORMAT="html"
    OUTPUT_FILE="$2"
elif [ "$1" = "--json" ]; then
    OUTPUT_FORMAT="json"
    OUTPUT_FILE="$2"
fi

# Build cppcheck command
CPPCHECK_CMD="cppcheck \
    --addon=misra \
    --std=c99 \
    --enable=all \
    --inconclusive \
    --suppressions-list=$PROJECT_ROOT/.cppcheck-suppressions \
    -I $PROJECT_ROOT/include \
    $PROJECT_ROOT/src \
    $PROJECT_ROOT/include"

case "$OUTPUT_FORMAT" in
    html)
        if [ -z "$OUTPUT_FILE" ]; then
            OUTPUT_FILE="$PROJECT_ROOT/cppcheck-report.html"
        fi
        echo "Generating HTML report: $OUTPUT_FILE"
        $CPPCHECK_CMD --html > "$OUTPUT_FILE"
        echo "✓ Report saved to $OUTPUT_FILE"
        ;;
    json)
        if [ -z "$OUTPUT_FILE" ]; then
            OUTPUT_FILE="$PROJECT_ROOT/cppcheck-report.json"
        fi
        echo "Generating JSON report: $OUTPUT_FILE"
        $CPPCHECK_CMD --json > "$OUTPUT_FILE" 2>&1 || true
        echo "✓ Report saved to $OUTPUT_FILE"
        ;;
    *)
        # Text output to stdout
        $CPPCHECK_CMD
        ;;
esac

echo ""
echo "=== Analysis complete ==="
echo "See docs/MISRA_COMPLIANCE_REPORT.md for compliance status and deviations."

#!/bin/bash
# Computes this project's version string: <static-version>+<git-shortsha>[.dirty]
# (real SemVer 2.0 build-metadata syntax). Falls back to +unknown outside a
# git checkout (e.g. inside a Docker build stage, where .git isn't in the
# build context - see .dockerignore).
#
# Usage:
#   scripts/compute_version.sh <static-version>
#   scripts/compute_version.sh 0.1.0   # -> 0.1.0+a3f9c21  or  0.1.0+a3f9c21.dirty
set -euo pipefail

STATIC_VERSION="${1:?usage: compute_version.sh <static-version>}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR/.."

if ! git -C "$PROJECT_ROOT" rev-parse --git-dir >/dev/null 2>&1; then
    echo "${STATIC_VERSION}+unknown"
elif ! git -C "$PROJECT_ROOT" rev-parse HEAD >/dev/null 2>&1; then
    # A real git repo, but no commits yet (e.g. a freshly git-init'd
    # project - see RBC_SA) - no SHA to report.
    echo "${STATIC_VERSION}+nocommit"
else
    SHA="$(git -C "$PROJECT_ROOT" rev-parse --short=8 HEAD)"
    if git -C "$PROJECT_ROOT" diff --quiet HEAD -- 2>/dev/null && git -C "$PROJECT_ROOT" diff --cached --quiet HEAD -- 2>/dev/null; then
        echo "${STATIC_VERSION}+${SHA}"
    else
        echo "${STATIC_VERSION}+${SHA}.dirty"
    fi
fi

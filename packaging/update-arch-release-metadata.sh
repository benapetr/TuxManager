#!/bin/bash
################################################################################
# update-arch-release-metadata.sh - Sync checked-in Arch release metadata
################################################################################

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

exec "$SCRIPT_DIR/render-arch-files.sh" --checksum skip "$@"

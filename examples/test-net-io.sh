#!/bin/bash
#
# Demo script for the C net-logger plugin.
#
# Run this script through `qcontrol run`, similar to test-file-ops.sh:
#
#   qcontrol run --plugins ./net-logger/dist/net-logger-<arch>.<ext> -- ./test-net-io.sh
#
#   qcontrol run --plugins ./net-logger/dist/net-logger-<arch>.<ext> -- ./test-net-io.sh https://example.com/
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
URL="${1:-https://example.com/}"
LOG_FILE="${QCONTROL_LOG_FILE:-/tmp/qcontrol.log}"

echo "Running client request against: $URL"
python3 "$SCRIPT_DIR/test-net-client.py" "$URL"

echo
echo "If the net-logger plugin is active, inspect:"
echo "  $LOG_FILE"
echo
echo "Suggested check:"
echo "  grep net_logger \"$LOG_FILE\""

#!/usr/bin/env bash
# Pretends to be the board on home/system/status, so the web app can be developed
# without flashing the Portenta. Ctrl+C publishes "offline", like the board's Last Will.
#   ./fake_board.sh [host] [user]      (defaults: localhost board; asks for the password)
set -euo pipefail
HOST=${1:-localhost}
USER_NAME=${2:-board}
read -r -s -p "Password for '$USER_NAME': " PASS; echo
TOPIC=home/system/status
pub() { mosquitto_pub -h "$HOST" -p 1883 -u "$USER_NAME" -P "$PASS" -q 1 -r -t "$TOPIC" -m "$1"; }

trap 'pub "{\"online\":false}"; echo " -> offline"; exit 0' INT
START=$(date +%s)
ERRORS=0
echo "Publishing status every 30 s (Ctrl+C to stop)"
while true; do
  UPTIME=$(( $(date +%s) - START ))
  pub "{\"online\":true,\"uptime_s\":$UPTIME,\"errors\":$ERRORS,\"last_error\":\"\",\"fw\":\"0.1.0-fake\"}"
  echo "online, uptime $UPTIME s"
  sleep 30
done

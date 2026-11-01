#!/usr/bin/env bash
# Creates or updates broker accounts in ./passwd (git-ignored).
#   ./setup-users.sh            -> creates "board" and "webadmin" (asks for passwords)
#   ./setup-users.sh <name>     -> adds or updates one account
# A new web user also needs a block in ./acl.
set -euo pipefail
cd "$(dirname "$0")"
[ -f passwd ] || { touch passwd; chmod 600 passwd; }
users=("$@")
[ ${#users[@]} -gt 0 ] || users=(board webadmin)
for u in "${users[@]}"; do
  echo "Password for '$u':"
  mosquitto_passwd passwd "$u"
done
echo "Done. Put the board password in firmware/m7_brain/secrets.h (SECRET_MQTT_PASS)."

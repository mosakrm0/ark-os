#!/bin/bash
set -e
cd /home/mosakram/ark-os
PRE=$(ls -1 rootfs/bin | wc -l)
echo "before=$PRE"
KEEP=(busybox sh ash '[' test echo cat grep sed mkdir rm mv cp ln chmod chown ps kill stat pwd touch uname true false sleep id)
cd rootfs/bin
COUNT=0
for f in *; do
  keep_it=false
  for k in "${KEEP[@]}"; do
    if [ "$f" = "$k" ]; then
      keep_it=true
      break
    fi
  done
  if ! $keep_it; then
    rm -f "$f"
    COUNT=$((COUNT+1))
  fi
done
POST=$(ls -1 rootfs/bin | wc -l)
echo "removed=$COUNT after=$POST"
cd /home/mosakram/ark-os/rootfs/sbin
for f in *; do rm -f "$f"; done
POST_SBIN=$(ls -1 rootfs/sbin | wc -l)
echo "sbin_after=$POST_SBIN"

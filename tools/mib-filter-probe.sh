#!/bin/sh
# Test one candidate payload layout for an undocumented filter MIB by watching
# what it does to the live link, instead of suspending and sending a magic
# packet (minutes per trial, and a null result tells you nothing).
#
#   sh mib-filter-probe.sh <hexbytes> [seconds] [mibid]
#
# Run it over USB.  The whole point is that a correctly shaped FILTER_IN may
# drop every frame, so the WiFi session would go with it.
#
# The reasoning: 0x101C is in the firmware's MIB id table (unknown ids come
# back -22, this one is accepted), but the firmware validates no length for
# any MIB in this family, so nothing it says distinguishes a well-formed
# payload from a malformed one.  Its EFFECT does.  Pair every candidate:
#
#   arm A   the same layout over a pattern that should match all traffic
#   arm B   the same layout over a pattern that should match nothing
#
# with an action of FILTER_IN (2).  A correctly shaped struct makes A and B
# differ -- A starves the link, B does not.  A wrong shape makes them the
# same, whichever way that falls.  One arm on its own proves nothing.
#
# Caveat: bes2600_update_filtering() rewrites the filter MIBs whenever the
# driver reconfigures, so do not roam, scan or reassociate during a trial.
set -u
HEX=${1:?usage: mib-filter-probe.sh <hexbytes> [seconds] [mibid]}
SECS=${2:-10}
MIB=${3:-101c}
D=/sys/kernel/debug/ieee80211/phy0/bes2600
IF=$(ls /sys/class/ieee80211/phy0/device/net 2>/dev/null | head -1)
IF=${IF:-wlan0}

rx() { cat /sys/class/net/$IF/statistics/rx_packets; }
peer() { iw dev $IF link 2>/dev/null | sed -n 's/^Connected to \([0-9a-f:]*\).*/\1/p'; }

AP=$(peer)
[ -z "$AP" ] && { echo "not associated; associate first"; exit 1; }
echo "iface $IF  ap $AP  mib 0x$MIB  payload $HEX"

measure() { # $1 = label
    B=$(rx); ping -q -n -c $SECS -i 1 -W 1 "${AP_IP:-$(ip route | sed -n 's/^default via \([0-9.]*\).*/\1/p' | head -1)}" >/tmp/mfp.ping 2>&1
    A=$(rx)
    LOSS=$(sed -n 's/.*, \([0-9]*\)% packet loss.*/\1/p' /tmp/mfp.ping)
    printf "  %-10s rx+%-6s loss %s%%  assoc %s\n" "$1" "$((A-B))" "${LOSS:-?}" "$([ -n "$(peer)" ] && echo yes || echo NO)"
}

C=$(journalctl -k -n0 --show-cursor 2>/dev/null | sed -n 's/^-- cursor: //p')
measure baseline
echo "wx $MIB $HEX" > $D/mib_probe || { echo "write rejected by the driver"; exit 1; }
measure filtered
echo "wx $MIB 00000000" > $D/mib_probe        # nrFilters = 0, i.e. disable
measure restored
journalctl -k --after-cursor="$C" --no-pager 2>/dev/null | sed -n 's/.*mib_probe: //p' | grep WRITE

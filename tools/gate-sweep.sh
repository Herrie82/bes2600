#!/bin/sh
# Sweep the bes2600 diagnostic module parameters against the JOIN refusal,
# without rebuilding anything.  Run on the device, reached over USB so that
# dropping the WiFi association does not cut the session.
#
#   sh gate-sweep.sh [attempts-per-arm]     (default 8)
#
# Each arm sets one parameter, runs the same 2.4GHz connect loop, and reports
# associations and JOIN refusals.  Everything is restored afterwards.
P=/sys/module/bes2600/parameters
S24=wifi_deadbeef1234_486572726965566c6164615f322e3447487a_managed_psk
N=${1:-8}

set_p() { [ -w "$P/$1" ] && echo "$2" > "$P/$1" 2>/dev/null; }
get_p() { cat "$P/$1" 2>/dev/null; }

arm() { # $1=label, rest=param=value pairs
    LBL=$1; shift
    for kv in "$@"; do set_p "${kv%%=*}" "${kv##*=}"; done
    CUR=$(journalctl -k -n0 --show-cursor 2>/dev/null | sed -n 's/^-- cursor: //p')
    ok=0; i=1
    while [ $i -le $N ]; do
        connmanctl disconnect $S24 >/dev/null 2>&1
        sleep 2
        connmanctl connect $S24 >/dev/null 2>&1
        sleep 8
        [ "$(iw dev wlan0 link 2>/dev/null | sed -n 's/.*freq: \([0-9]*\).*/\1/p')" = "2437" ] && ok=$((ok+1))
        i=$((i+1))
    done
    R=$(journalctl -k --after-cursor="$CUR" --no-pager 2>/dev/null | grep -c "wsm_join_confirm ret")
    printf "%-34s assoc %2d/%-2d  refusals %3d\n" "$LBL" "$ok" "$N" "$R"
}

echo "=== available gates ==="
ls $P 2>/dev/null | tr '\n' ' '; echo
echo "=== baseline values ==="
for k in coex_force_fdd coex_epta_mute coex_inactive_wlan coex_inactive_bt \
         coex_gotip_wlan coex_gotip_bt join_pre_switch join_pre_delay_ms \
         join_retry join_probe rx_retry_max; do
    printf "  %-20s %s\n" "$k" "$(get_p $k)"
done
echo
echo "=== arms (${N} attempts each, 2.4GHz) ==="
arm "control (defaults)"            coex_force_fdd=0 coex_epta_mute=0 join_pre_switch=0 join_pre_delay_ms=0 join_retry=0
arm "coex: force FDD"               coex_force_fdd=1
arm "coex: mute EPTA entirely"      coex_force_fdd=0 coex_epta_mute=1
arm "coex: inactive split 102400/0" coex_epta_mute=0 coex_inactive_wlan=102400 coex_inactive_bt=0
arm "timing: 100ms before JOIN"     coex_inactive_wlan=-1 coex_inactive_bt=-1 join_pre_delay_ms=100
arm "join: retry refusals x3"       join_pre_delay_ms=0 join_retry=3
arm "old behaviour: switch in join" join_retry=0 join_pre_switch=1
arm "control again (drift check)"   join_pre_switch=0

echo
echo "restoring defaults"
set_p coex_force_fdd 0; set_p coex_epta_mute 0
set_p coex_inactive_wlan -1; set_p coex_inactive_bt -1
set_p coex_gotip_wlan -1; set_p coex_gotip_bt -1
set_p join_pre_switch 0; set_p join_pre_delay_ms 0; set_p join_retry 0
echo SWEEP_DONE

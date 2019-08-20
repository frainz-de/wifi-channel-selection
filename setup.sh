#!/usr/bin/env sh
cd /sys/kernel/debug/ieee80211/phy0/ath10k
#ip link set dev wlp1s0 up
iw phy phy0 interface add mon0 type monitor
ip link set mon0 up
iw dev wlp1s0 del
echo background > spectral_scan_ctl
echo trigger > spectral_scan_ctl

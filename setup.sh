#!/usr/bin/env sh
cd /sys/kernel/debug/ieee80211/phy0/ath10k
ip link set dev wlp1s0 up
echo background > spectral_scan_ctl
echo trigger > spectral_scan_ctl

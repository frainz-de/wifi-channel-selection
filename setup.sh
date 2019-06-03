cd /sys/kernel/debug/ieee80211/phy1/ath10k
ip link set dev wlp5s0 up
echo background > spectral_scan_ctl
echo trigger > spectral_scan_ctl

#/#!/usr/bin/env sh

IP="$(ip a | grep "192" | awk '{print substr($2,1,length($2)-3)}' | tr -d '\n')" 
LEN="$(printf $IP | wc -c)"
IE="dd$(printf "%02x" $LEN)$(printf $IP | xxd -p)"
echo $IE

if ! grep -q "vendor_elements=" /etc/hostapd/hostapd.conf; then
	echo "need to write"
	printf "\nvendor_elements=$IE\n" >> /etc/hostapd/hostapd.conf
else
	echo "nothing to write"
fi

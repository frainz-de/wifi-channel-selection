#/#!/usr/bin/env sh

IP="$(ip a | grep "fd00::1" | awk '{print substr($2,1,length($2)-4)}' | tr -d '\n')" 
LEN="$(printf $IP | wc -c)"
IE="dd$(printf "%02x" $(($LEN + 3)))424242$(printf $IP | xxd -p)"
echo $IE

#cp /etc/hostapd/hostapd.conf hostapd.conf-$(date +%F_%T)

sed -i 's/vendor_elements=.*//g' /etc/hostapd/hostapd.conf

#if ! grep -q "vendor_elements=dd" /etc/hostapd/hostapd.conf; then
#    head -n -2 /etc/hostapd/hostapd.conf > /etc/hostapd/hostapd.conf
#fi

if ! grep -q "vendor_elements=" /etc/hostapd/hostapd.conf; then
	echo "need to write"
	printf "vendor_elements=$IE\n" >> /etc/hostapd/hostapd.conf
else
	echo "nothing to write"
fi

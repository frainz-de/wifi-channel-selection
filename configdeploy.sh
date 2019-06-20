for i in  $(seq -f "%02g" 20); do scp add_vendor_elements.sh [fd00::1$i]:~; done

for i in  $(seq -f "%02g" 20); do ssh fd00::1$i ./add_vendor_elements.sh; done

for i in  $(seq -f "%02g" 20); do ssh fd00::1$i systemctl restart hostapd; done

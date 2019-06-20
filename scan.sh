for i in $(iw dev wlp1s0 scan -u | grep "42:42:42" |  awk '{ s = ""; for (i = 6; i <= NF; i++) s = s $i; print s }'); do echo $i | xxd -p -r; printf "\n"; done | sort

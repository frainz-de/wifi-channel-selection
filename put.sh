#!/usr/bin/env sh
for i in "$@"
do
    rsync -zP builddir/collector [fd00::1$i]:~ &
done

for pid in $(jobs -p)
do
    wait $pid
done

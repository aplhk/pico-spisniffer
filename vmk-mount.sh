#!/bin/bash

VOLUME=$1
VMK_INPUT=$2
VMK_LENGTH=${#VMK_INPUT} # should be 64 or 68

if [[ $VMK_LENGTH != 64 ]] && [[ $VMK_LENGTH != 68 ]]; then
    echo "VMK length should be 64 or 68 char hex";
    exit 1
fi

if [[ $VMK_LENGTH == 64 ]]; then
    echo "Not implemented"
    exit 1
fi

mkdir -p /tmp/dislocker-tmp /tmp/dislocker-mount

for i in {2..26..2}; do
    j=$((i+4+1))
    printf "Trying : "
    echo $VMK_INPUT | cut "-b-$i,$j-"

    echo $VMK_INPUT | cut "-b-$i,$j-" | xxd -r -p > /tmp/dislocker-tmp/vmk.txt
    dislocker-fuse -V "$VOLUME" --vmk /tmp/dislocker-tmp/vmk.txt /tmp/dislocker-mount 2>&1 > /dev/null;
    
    if [[ $? == 0 ]]; then
        echo "Unlock successful, setting up loop device..."
        losetup --find --show /tmp/dislocker-mount/dislocker-file
        break
    fi
done

#!/bin/bash
. mingw-env.sh

qemu-kvm -smp 2 -m 1536 -hda $VIRT_IMAGE_PATH -net nic -net user -vga std -soundhw all

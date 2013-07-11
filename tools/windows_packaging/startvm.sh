#!/bin/bash

qemu-kvm -smp 2 -m 1536 -hda $HOME/virt-images/winxp.raw -net nic -net user -vga std -soundhw all

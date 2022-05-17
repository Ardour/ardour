These files are intended to be copied to ` /etc/udev/rules.d/` on a GNU/Linux system,
in order to grant permissions to access various devices to users in the audio group.

After doing so either reboot, or restart udevd as follows:

```
sudo udevadm control --reload-rules
sudo udevadm trigger
```

# Local Debian Install

This branch is intended to be usable as a source checkout that you can clone,
build, and install locally on Debian.

## Clone This Branch

```sh
git clone --branch codex/branch-push-20260707 https://github.com/Jkudjo/ardour.git
cd ardour
```

## Install Build Dependencies

On Debian, the easiest dependency path is to use Debian's Ardour build
dependencies:

```sh
sudo apt update
sudo apt build-dep ardour
sudo apt install build-essential python3 pkg-config git
```

If `apt build-dep ardour` says source repositories are disabled, enable `deb-src`
entries for your Debian release in APT, then run `sudo apt update` again.

If configure stops at `Checking for header jpeglib.h : no`, install the JPEG
development headers:

```sh
sudo apt install libjpeg-dev
```

If configure stops at `Checking for 'jack' >= 1.9.10 : not found`, install JACK
development files:

```sh
sudo apt install libjack-jackd2-dev
```

Or build without JACK support:

```sh
scripts/debian-local-install.sh --backends alsa,pulseaudio
```

## Build And Install For Your User

This installs into `~/.local/ardour-source` by default and does not overwrite a
system Ardour package:

```sh
scripts/debian-local-install.sh
```

After installation, run the launcher printed by the script, usually something
like:

```sh
~/.local/ardour-source/bin/ardour7
```

## Install System-Wide

To install under `/usr/local` instead:

```sh
scripts/debian-local-install.sh --system
```

System-wide installation may ask for your sudo password during the install step.

## Useful Options

Configure only:

```sh
scripts/debian-local-install.sh --configure-only
```

Clean and rebuild:

```sh
scripts/debian-local-install.sh --clean
```

Pass additional Waf configure flags after `--`:

```sh
scripts/debian-local-install.sh -- --no-lxvst --no-vst3
```

The helper invokes Waf as `python3 ./waf`, which avoids Debian systems where the
legacy `python` command is not installed.

#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'USAGE'
Usage: scripts/debian-local-install.sh [options] [-- extra-waf-configure-args...]

Build and install this Ardour checkout on Debian using the repo's Waf build.

Options:
  --prefix PATH       Install prefix. Default: $HOME/.local/ardour-source
  --system            Install to /usr/local. May require sudo for install.
  --backends LIST     Audio backends. Default: jack,alsa,pulseaudio
  --jobs N            Parallel build jobs. Default: nproc
  --configure-only    Run configure, then stop.
  --clean             Run waf distclean before configure.
  -h, --help          Show this help.

Examples:
  scripts/debian-local-install.sh
  scripts/debian-local-install.sh --system
  scripts/debian-local-install.sh --prefix "$HOME/opt/ardour" -- --no-lxvst --no-vst3
USAGE
}

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

prefix="${PREFIX:-$HOME/.local/ardour-source}"
backends="${BACKENDS:-jack,alsa,pulseaudio}"
jobs="${JOBS:-$(nproc 2>/dev/null || printf '2')}"
configure_only=0
clean=0
system_install=0
extra_configure_args=()

while [ "$#" -gt 0 ]; do
    case "$1" in
        --prefix)
            shift
            [ "$#" -gt 0 ] || { echo "missing value for --prefix" >&2; exit 2; }
            prefix="$1"
            ;;
        --prefix=*)
            prefix="${1#--prefix=}"
            ;;
        --system)
            prefix="/usr/local"
            system_install=1
            ;;
        --backends)
            shift
            [ "$#" -gt 0 ] || { echo "missing value for --backends" >&2; exit 2; }
            backends="$1"
            ;;
        --backends=*)
            backends="${1#--backends=}"
            ;;
        --jobs)
            shift
            [ "$#" -gt 0 ] || { echo "missing value for --jobs" >&2; exit 2; }
            jobs="$1"
            ;;
        --jobs=*)
            jobs="${1#--jobs=}"
            ;;
        --configure-only)
            configure_only=1
            ;;
        --clean)
            clean=1
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            extra_configure_args+=("$@")
            break
            ;;
        *)
            extra_configure_args+=("$1")
            ;;
    esac
    shift
done

need() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "missing required command: $1" >&2
        exit 1
    fi
}

need python3
need pkg-config
need gcc
need g++

if [ ! -f /usr/include/jpeglib.h ]; then
    cat >&2 <<'EOF'
missing required header: jpeglib.h

Install the Debian JPEG development package, then rerun this script:

  sudo apt install libjpeg-dev
EOF
    exit 1
fi

case ",$backends," in
    *,jack,*)
        if ! pkg-config --exists jack; then
            cat >&2 <<'EOF'
missing required pkg-config dependency: jack >= 1.9.10

Install JACK development files, then rerun this script:

  sudo apt install libjack-jackd2-dev

Or build without JACK:

  scripts/debian-local-install.sh --backends alsa,pulseaudio
EOF
            exit 1
        fi
        ;;
esac

if [ "$clean" -eq 1 ]; then
    python3 ./waf distclean
fi

python3 ./waf configure \
    --prefix="$prefix" \
    --with-backends="$backends" \
    --optimize \
    --freedesktop \
    --noconfirm \
    "${extra_configure_args[@]}"

if [ "$configure_only" -eq 1 ]; then
    echo "Configure complete. Build/install skipped."
    exit 0
fi

python3 ./waf build -j "$jobs"

if [ "$system_install" -eq 1 ]; then
    sudo python3 ./waf install
else
    mkdir -p "$prefix"
    python3 ./waf install
fi

echo
echo "Installed under: $prefix"
echo "Launchers found:"
find "$prefix/bin" -maxdepth 1 -type f -name 'ardour*' -print 2>/dev/null || true

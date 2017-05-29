#!/bin/sh
# Default to using the latest build scripts available in the official Ardour 
# GitHub repository.
#
# If there's a newer set of build scripts available in Defcronyke's fork,
# pass the -d argument, or set ARG1="-d" to download from Defcronyke's 
# repository instead.
#
# To use a custom repository for the build scripts, you can set the $URL
# environment variable to point wherever you want.
#
# Please note that the build scripts from Defcronyke's fork will always 
# install Ardour from the official Ardour GitHub repository by default. 
# The $URL param is only to change the location we look for the build 
# scripts themselves.

conf()
{
	set -e

	if [ -z ${CONF_INCLUDED+x} ]; then
	
		echo "INFO: Configuration file included"

		# Set your desired default values here.
		if [ "$ARG1" != "-l" ]; then
			echo "INFO: Fetching resources from remote site: $URL"
			$WGET "$URL/README.txt"
			$WGET "$URL/00-00-xx-bin-deps.txt"
			$WGET "$URL/00-00-xx-msys2-deps.txt"
			$WGET "$URL/02-msys2-deps-quirks.sh"
			echo "INFO: Fetching resources from remote site succeeded"
		fi
		
		export SH="${SH:-sh}"
		export WGET="${WGET:-wget}"
		export WGET_ARGS="${WGET_ARGS:-}"
		export REPO_BRANCH="${REPO_BRANCH:-master}"
		export URL_BEGINNING="${URL_BEGINNING:-https://raw.githubusercontent.com/Ardour/ardour}"
		export DEF_URL_BEGINNING="${DEF_URL_BEGINNING:-https://raw.githubusercontent.com/defcronyke/ardour}"
		export URL_END="${URL_END:-tools/windows_packaging_unofficial/win64-mingw64-msys2}"
		export PACMAN="${PACMAN:-pacman}"
		export PACMAN_UPDATE_ARGS="${PACMAN_UPDATE_ARGS:--Syu --noconfirm}"
		export PACMAN_INSTALL_ARGS="${PACMAN_INSTALL_ARGS:--S --force --needed --noconfirm}"
		export MAKEPKG_ARGS="${MAKEPKG_ARGS:---install --needed --noconfirm}"
		export MAKEPKG_EXTRA_ARGS="${MAKEPKG_ARGS:-}"
		
		export BIN_DEPS_FILENAME="${BIN_DEPS_FILENAME:-00-00-xx-bin-deps.txt}"
		readarray BIN_DEPS_FILE < "$BIN_DEPS_FILENAME"
		export BIN_DEPS="${BIN_DEPS:-${BIN_DEPS_FILE[@]}}"
		
		export BUILD_SCRIPT_DIR="${BUILD_SCRIPT_DIR:-$PWD}"
		export BUILD_DIR="${BUILD_DIR:-/ardour-mingw64-msys2}"
		
		export MSYS2_DEPS_FILENAME="${MSYS2_DEPS_FILENAME:-00-00-xx-msys2-deps.txt}"
		readarray MSYS2_DEPS_FILE < "$MSYS2_DEPS_FILENAME"
		export MSYS2_DEPS="${MSYS2_DEPS:-${MSYS2_DEPS_FILE[@]}}"
		export MSYS2_DEPS_REPO_BRANCH="${MSYS2_DEPS_REPO_BRANCH:-guypkgs}"
		export MSYS2_DEPS_REPO_NAME="${MSYS2_DEPS_REPO_NAME:-MINGW-packages}"
		export MSYS2_DEPS_REPO="${PKBUILD_DEP_REPO:-https://github.com/guysherman/$MSYS2_DEPS_REPO_NAME.git}"
		
		export SRC_REPO_NAME="${SRC_REPO_NAME:-ardour}"
		export SRC_REPO="${SRC_REPO:-https://github.com/Ardour/$SRC_REPO_NAME.git}"
		export SRC_VER="${SRC_VER:-5.4}"
		
		if [ "$ARG1" == "-d" ]; then 
			URL_BEGINNING="$DEF_URL"
		elif [ "$ARG1" == "-dt" ]; then 
			URL_BEGINNING="$DEF_URL"
			REPO_BRANCH="-testing"
		fi
		
		URL="${URL:-$URL_BEGINNING/$REPO_BRANCH/$URL_END}"

		PREPARE_SCRIPTS=(
			"01-bin-deps.sh" \
			"02-msys2-deps.sh" \
			"03-get.sh" \
			"04-patch.sh"
		)
		
		PREPARE="${PREPARE:-${PREPARE_SCRIPTS[@]}}"
		PREPARE_CMD_PRE="${PREPARE_CMD_PRE:-$WGET $WGET_ARGS "$URL"}"
		PREPARE_CMD_POST="${PREPARE_CMD_POST:-}"
		PREPARE_CMD=()
		
		INSTALL_SCRIPTS=(
			"05-build.sh" \
			"06-mingw64-deps.sh"
		)
		
		INSTALL="${INSTALL:-${INSTALL_SCRIPTS[@]}}"
		INSTALL_CMD_PRE="${INSTALL_CMD_PRE:-$WGET $WGET_ARGS "$URL"}"
		INSTALL_CMD_POST="${INSTALL_CMD_POST:-}"
		INSTALL_CMD=()
		
		for i in ${PREPARE[@]};	do
			if [ "$ARG1" != "-l" ]; then
				PREPARE_CMD+=("$PREPARE_CMD_PRE/$i $PREPARE_CMD_POST")	
			fi
			PREPARE_CMD+=("./$i")
		done
		export PREPARE_CMD
		
		for i in ${INSTALL[@]};	do
			if [ "$ARG1" != "-l" ]; then
				INSTALL_CMD+=("$INSTALL_CMD_PRE/$i $INSTALL_CMD_POST")
			fi
			INSTALL_CMD+=("./$i")
		done
		export INSTALL_CMD
		
		export CONF_INCLUDED=1
		
		echo "Environment:"
		echo "$(env)"
	fi
}

conf

#!/bin/sh
# Run this in an MSYS2 shell.
#
# Install build dependencies, fetch Ardour, and patch it so it builds on 
# Windows. Make sure to run this script in an MSYS2 shell.

export ARG1="${ARG1:-$1}"

prepare()
{
	echo "INFO: Starting prepare action"
	echo "INFO: Pre-include environment:"
	echo "ARG1=\"$ARG1\""
	
	export WGET="${WGET:-wget}"
	export URL_BEGINNING="${URL_BEGINNING:-https://raw.githubusercontent.com}"
	export REPO_OWNER="${REPO_OWNER:-Ardour}"
	export REPO_NAME="${REPO_NAME:-ardour}"
	export REPO_BRANCH="${REPO_BRANCH:-master}"
	export URL_END="${URL_END:-tools/windows_packaging_unofficial/win64-mingw64-msys2}"
	export INCLUDE_SCRIPT="${INCLUDE_SCRIPT:-00-00-conf.sh}"
	export SOURCE="${SOURCE:-.}"
	
	if [ "$ARG1" == "-d" ]; then
		export REPO_OWNER="defcronyke"
		export REPO_BRANCH="win64-mingw-msys"
	elif [ "$ARG1" == "-dt" ]; then
		export REPO_OWNER="defcronyke"
		export REPO_BRANCH="testing"
	fi
	
	echo "WGET=\"$WGET\""
	echo "URL_BEGINNING=\"$URL_BEGINNING\""
	echo "REPO_OWNER=\"$REPO_OWNER\""
	echo "REPO_NAME=\"$REPO_NAME\""
	echo "REPO_BRANCH=\"$REPO_BRANCH\""
	echo "URL_END=\"$URL_END\""
	echo "INCLUDE_SCRIPT=\"$INCLUDE_SCRIPT\""
	echo "SOURCE=\"$SOURCE\""
	
	if [ "$ARG1" != "-l" ]; then
		export URL="$URL_BEGINNING/$REPO_OWNER/$REPO_NAME/$REPO_BRANCH/$URL_END"
		echo "URL=\"$URL\""
		$WGET $URL/$INCLUDE_SCRIPT
	fi
	
	$SOURCE "./$INCLUDE_SCRIPT"
	for i in "${PREPARE_CMD[@]}"; do
		echo "INFO: Running prepare command: $i"
		$i
		echo "INFO: Prepare command succeeded: $i"
	done
	
	echo "INFO: Prepare commands finished successfully"
}

prepare

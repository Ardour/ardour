#!/bin/sh

if ! test -f wscript || ! test -d gtk2_ardour || ! test -d libs/fluidsynth/;then
	echo "This script needs to run from ardour's top-level src tree"
	exit 1
fi

if test -z "`which rsync`" -o -z "`which git`"; then
	echo "this script needs rsync and git"
	exit 1
fi

ASRC=`pwd`
set -e

TMP=`mktemp -d`
test -d "$TMP"
echo $TMP

trap "rm -rf $TMP" EXIT

cd $TMP
#git clone git://git.code.sf.net/p/fluidsynth/code-git fs-git
git clone git://github.com/FluidSynth/fluidsynth.git fs-git

cd fs-git;
git describe --tags
git log | head
cd $TMP

FSR=fs-git/

rsync -auc --info=progress2 \
	${FSR}src/midi/fluid_midi.c \
	${FSR}src/midi/fluid_midi.h \
	${FSR}src/rvoice/fluid_adsr_env.c \
	${FSR}src/rvoice/fluid_adsr_env.h \
	${FSR}src/rvoice/fluid_chorus.c \
	${FSR}src/rvoice/fluid_chorus.h \
	${FSR}src/rvoice/fluid_iir_filter.c \
	${FSR}src/rvoice/fluid_iir_filter.h \
	${FSR}src/rvoice/fluid_lfo.c \
	${FSR}src/rvoice/fluid_lfo.h \
	${FSR}src/rvoice/fluid_phase.h \
	${FSR}src/rvoice/fluid_rev.c \
	${FSR}src/rvoice/fluid_rev.h \
	${FSR}src/rvoice/fluid_rvoice.c \
	${FSR}src/rvoice/fluid_rvoice_dsp.c \
	${FSR}src/rvoice/fluid_rvoice_event.c \
	${FSR}src/rvoice/fluid_rvoice_event.h \
	${FSR}src/rvoice/fluid_rvoice.h \
	${FSR}src/rvoice/fluid_rvoice_mixer.c \
	${FSR}src/rvoice/fluid_rvoice_mixer.h \
	${FSR}src/sfloader/fluid_defsfont.c \
	${FSR}src/sfloader/fluid_defsfont.h \
	${FSR}src/sfloader/fluid_samplecache.c \
	${FSR}src/sfloader/fluid_samplecache.h \
	${FSR}src/sfloader/fluid_sffile.c \
	${FSR}src/sfloader/fluid_sffile.h \
	${FSR}src/sfloader/fluid_sfont.c \
	${FSR}src/sfloader/fluid_sfont.h \
	${FSR}src/synth/fluid_chan.c \
	${FSR}src/synth/fluid_chan.h \
	${FSR}src/synth/fluid_event.c \
	${FSR}src/synth/fluid_event.h \
	${FSR}src/synth/fluid_gen.c \
	${FSR}src/synth/fluid_gen.h \
	${FSR}src/synth/fluid_mod.c \
	${FSR}src/synth/fluid_mod.h \
	${FSR}src/synth/fluid_synth.c \
	${FSR}src/synth/fluid_synth.h \
	${FSR}src/synth/fluid_synth_monopoly.c \
	${FSR}src/synth/fluid_tuning.c \
	${FSR}src/synth/fluid_tuning.h \
	${FSR}src/synth/fluid_voice.c \
	${FSR}src/synth/fluid_voice.h \
	${FSR}src/utils/fluid_conv.c \
	${FSR}src/utils/fluid_conv.h \
	${FSR}src/utils/fluid_conv_tables.h \
	${FSR}src/utils/fluid_hash.c \
	${FSR}src/utils/fluid_hash.h \
	${FSR}src/utils/fluid_list.c \
	${FSR}src/utils/fluid_list.h \
	${FSR}src/utils/fluid_ringbuffer.c \
	${FSR}src/utils/fluid_ringbuffer.h \
	${FSR}src/utils/fluid_settings.c \
	${FSR}src/utils/fluid_settings.h \
	${FSR}src/utils/fluidsynth_priv.h \
	${FSR}src/utils/fluid_sys.c \
	${FSR}src/utils/fluid_sys.h \
	\
	"$ASRC/libs/fluidsynth/src/"

rsync -auc --info=progress2 \
	--exclude fluidsynth.h \
	${FSR}include/fluidsynth/event.h  \
	${FSR}include/fluidsynth/gen.h  \
	${FSR}include/fluidsynth/log.h \
	${FSR}include/fluidsynth/midi.h \
	${FSR}include/fluidsynth/misc.h  \
	${FSR}include/fluidsynth/mod.h \
	${FSR}include/fluidsynth/settings.h \
	${FSR}include/fluidsynth/sfont.h \
	${FSR}include/fluidsynth/synth.h \
	${FSR}include/fluidsynth/types.h \
	${FSR}include/fluidsynth/voice.h \
	\
	"$ASRC/libs/fluidsynth/fluidsynth/"

cd "$ASRC"
## 1st: apply patch below, fix up any merge-conflicts and git commit the result.
## 2nd run (after commiting the new version): re-create the patch to upstream & amend:
# git diff -R libs/fluidsynth/ > tools/fluid-patches/ardour_fluidsynth.diff
#exit
patch -p1 < tools/fluid-patches/ardour_fluidsynth.diff

# auto-generated files
cp tools/fluid-patches/fluid_conv_tables.inc.h  libs/fluidsynth/src/
cp tools/fluid-patches/fluid_rvoice_dsp_tables.inc.h  libs/fluidsynth/src/

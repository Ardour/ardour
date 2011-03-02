
TEMPLATE = lib
CONFIG += staticlib warn_on release
CONFIG -= qt
OBJECTS_DIR = tmp_obj
MOC_DIR = tmp_moc

linux-g++* {
    QMAKE_CXXFLAGS_RELEASE += -DNDEBUG -O3 -fno-exceptions -fPIC -ffast-math -msse -mfpmath=sse -ftree-vectorize -fomit-frame-pointer
    DEFINES += USE_PTHREADS
    INCLUDEPATH += ../vamp-plugin-sdk ../qm-dsp
    LIBPATH += ../vamp-plugin-sdk/vamp-sdk ../qm-dsp
}

linux-g++-64 {
    QMAKE_CXXFLAGS_RELEASE += -msse2
    INCLUDEPATH += ../qm-vamp-plugins/build/linux/amd64
}

win32-x-g++ {
    QMAKE_CXXFLAGS_RELEASE += -DNDEBUG -O2 -march=pentium3 -msse
    INCLUDEPATH += . include ../include
}

macx-g++* {
    QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.4u.sdk
    CONFIG += x86 ppc
    QMAKE_CXXFLAGS_RELEASE += -O2 -g0 -fvisibility=hidden -I/Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/Accelerate.framework/Versions/A/Frameworks/vecLib.framework/Versions/A/Headers/
    INCLUDEPATH += /Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks/Accelerate.framework/Versions/A/Frameworks/vecLib.framework/Versions/A/Headers/
    DEFINES += USE_PTHREADS
    QMAKE_CXXFLAGS_RELEASE += -fvisibility=hidden
}

solaris* {
    QMAKE_CXXFLAGS_RELEASE += -DNDEBUG -fast
    INCLUDEPATH += /opt/ATLAS3.9.14/include
    DEFINES += USE_PTHREADS
}

INCLUDEPATH += . 

# Input
HEADERS += base/Pitch.h \
           base/Window.h \
           dsp/chromagram/Chromagram.h \
           dsp/chromagram/ConstantQ.h \
           dsp/keydetection/GetKeyMode.h \
           dsp/mfcc/MFCC.h \
           dsp/onsets/DetectionFunction.h \
           dsp/onsets/PeakPicking.h \
           dsp/phasevocoder/PhaseVocoder.h \
           dsp/rateconversion/Decimator.h \
           dsp/rhythm/BeatSpectrum.h \
           dsp/segmentation/cluster_melt.h \
           dsp/segmentation/ClusterMeltSegmenter.h \
           dsp/segmentation/cluster_segmenter.h \
           dsp/segmentation/Segmenter.h \
           dsp/segmentation/segment.h \
           dsp/signalconditioning/DFProcess.h \
           dsp/signalconditioning/Filter.h \
           dsp/signalconditioning/FiltFilt.h \
           dsp/signalconditioning/Framer.h \
           dsp/tempotracking/DownBeat.h \
           dsp/tempotracking/TempoTrack.h \
           dsp/tempotracking/TempoTrackV2.h \
           dsp/tonal/ChangeDetectionFunction.h \
           dsp/tonal/TCSgram.h \
           dsp/tonal/TonalEstimator.h \
           dsp/transforms/FFT.h \
           dsp/wavelet/Wavelet.h \
           hmm/hmm.h \
           maths/Correlation.h \
           maths/CosineDistance.h \
           maths/KLDivergence.h \
           maths/MathAliases.h \
           maths/MathUtilities.h \
           maths/Polyfit.h \
           maths/pca/pca.h \
           thread/AsynchronousTask.h \
           thread/BlockAllocator.h \
           thread/Thread.h
SOURCES += base/Pitch.cpp \
           dsp/chromagram/Chromagram.cpp \
           dsp/chromagram/ConstantQ.cpp \
           dsp/keydetection/GetKeyMode.cpp \
           dsp/mfcc/MFCC.cpp \
           dsp/onsets/DetectionFunction.cpp \
           dsp/onsets/PeakPicking.cpp \
           dsp/phasevocoder/PhaseVocoder.cpp \
           dsp/rateconversion/Decimator.cpp \
           dsp/rhythm/BeatSpectrum.cpp \
           dsp/segmentation/cluster_melt.c \
           dsp/segmentation/ClusterMeltSegmenter.cpp \
           dsp/segmentation/cluster_segmenter.c \
           dsp/segmentation/Segmenter.cpp \
           dsp/signalconditioning/DFProcess.cpp \
           dsp/signalconditioning/Filter.cpp \
           dsp/signalconditioning/FiltFilt.cpp \
           dsp/signalconditioning/Framer.cpp \
           dsp/tempotracking/DownBeat.cpp \
           dsp/tempotracking/TempoTrack.cpp \
           dsp/tempotracking/TempoTrackV2.cpp \
           dsp/tonal/ChangeDetectionFunction.cpp \
           dsp/tonal/TCSgram.cpp \
           dsp/tonal/TonalEstimator.cpp \
           dsp/transforms/FFT.cpp \
           dsp/wavelet/Wavelet.cpp \
           hmm/hmm.c \
           maths/Correlation.cpp \
           maths/CosineDistance.cpp \
           maths/KLDivergence.cpp \
           maths/MathUtilities.cpp \
           maths/pca/pca.c \
           thread/Thread.cpp

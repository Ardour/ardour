QMAKE_MAC_SDK=/Developer/SDKs/MacOSX10.4u.sdk
TEMPLATE = lib
CONFIG += release warn_on staticlib x86 ppc
CONFIG -= qt
OBJECTS_DIR = tmp_obj
MOC_DIR = tmp_moc
QMAKE_CXXFLAGS_RELEASE += -O2 -g0

linux-g++:QMAKE_CXXFLAGS_RELEASE += -DNDEBUG -O3 -march=pentium4 -msse -msse2

#DEPENDPATH += base \
#              dsp/chromagram \
#              dsp/keydetection \
#              dsp/maths \
#              dsp/onsets \
#              dsp/phasevocoder \
#              dsp/rateconversion \
#              dsp/signalconditioning \
#              dsp/tempotracking \
#              dsp/tonal \
#              dsp/transforms
INCLUDEPATH += . include

# Input
HEADERS += base/Pitch.h \
           base/Window.h \
           dsp/chromagram/Chromagram.h \
           dsp/chromagram/ChromaProcess.h \
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
           dsp/segmentation/SavedFeatureSegmenter.h \
           dsp/segmentation/Segmenter.h \
           dsp/segmentation/segment.h \
           dsp/signalconditioning/DFProcess.h \
           dsp/signalconditioning/Filter.h \
           dsp/signalconditioning/FiltFilt.h \
           dsp/signalconditioning/Framer.h \
           dsp/tempotracking/TempoTrack.h \
           dsp/tonal/ChangeDetectionFunction.h \
           dsp/tonal/TCSgram.h \
           dsp/tonal/TonalEstimator.h \
           dsp/transforms/FFT.h \
           hmm/hmm.h \
           maths/Correlation.h \
           maths/CosineDistance.h \
           maths/Histogram.h \
           maths/KLDivergence.h \
           maths/MathAliases.h \
           maths/MathUtilities.h \
           maths/Polyfit.h \
           maths/pca/pca.h
SOURCES += base/Pitch.cpp \
           dsp/chromagram/Chromagram.cpp \
           dsp/chromagram/ChromaProcess.cpp \
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
           dsp/segmentation/SavedFeatureSegmenter.cpp \
           dsp/segmentation/Segmenter.cpp \
           dsp/signalconditioning/DFProcess.cpp \
           dsp/signalconditioning/Filter.cpp \
           dsp/signalconditioning/FiltFilt.cpp \
           dsp/signalconditioning/Framer.cpp \
           dsp/tempotracking/TempoTrack.cpp \
           dsp/tonal/ChangeDetectionFunction.cpp \
           dsp/tonal/TCSgram.cpp \
           dsp/tonal/TonalEstimator.cpp \
           dsp/transforms/FFT.cpp \
           hmm/hmm.c \
           maths/Correlation.cpp \
           maths/CosineDistance.cpp \
           maths/KLDivergence.cpp \
           maths/MathUtilities.cpp \
           maths/pca/pca.c

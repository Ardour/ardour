/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2008 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifdef COMPILER_MSVC
#include "bsd-3rdparty/float_cast/float_cast.h"
#endif
#include "StretchCalculator.h"

#include <algorithm>
#include <math.h>
#include <algorithm>
#include <iostream>
#include <deque>
#include <set>
#include <cassert>
#include <algorithm>

#include "sysutils.h"

namespace RubberBand
{
	
StretchCalculator::StretchCalculator(size_t sampleRate,
                                     size_t inputIncrement,
                                     bool useHardPeaks) :
    m_sampleRate(sampleRate),
    m_increment(inputIncrement),
    m_prevDf(0),
    m_divergence(0),
    m_recovery(0),
    m_prevRatio(1.0),
    m_transientAmnesty(0),
    m_useHardPeaks(useHardPeaks)
{
//    std::cerr << "StretchCalculator::StretchCalculator: useHardPeaks = " << useHardPeaks << std::endl;
}    

StretchCalculator::~StretchCalculator()
{
}

std::vector<int>
StretchCalculator::calculate(double ratio, size_t inputDuration,
                             const std::vector<float> &phaseResetDf,
                             const std::vector<float> &stretchDf)
{
    assert(phaseResetDf.size() == stretchDf.size());
    
    m_lastPeaks = findPeaks(phaseResetDf);
    std::vector<Peak> &peaks = m_lastPeaks;
    size_t totalCount = phaseResetDf.size();

    std::vector<int> increments;

    size_t outputDuration = lrint(inputDuration * ratio);

    if (m_debugLevel > 0) {
        std::cerr << "StretchCalculator::calculate(): inputDuration " << inputDuration << ", ratio " << ratio << ", outputDuration " << outputDuration;
    }

    outputDuration = lrint((phaseResetDf.size() * m_increment) * ratio);

    if (m_debugLevel > 0) {
        std::cerr << " (rounded up to " << outputDuration << ")";
        std::cerr << ", df size " << phaseResetDf.size() << std::endl;
    }

    std::vector<size_t> fixedAudioChunks;
    for (size_t i = 0; i < peaks.size(); ++i) {
        fixedAudioChunks.push_back
            (lrint((double(peaks[i].chunk) * outputDuration) / totalCount));
    }

    if (m_debugLevel > 1) {
        std::cerr << "have " << peaks.size() << " fixed positions" << std::endl;
    }

    size_t totalInput = 0, totalOutput = 0;

    // For each region between two consecutive time sync points, we
    // want to take the number of output chunks to be allocated and
    // the detection function values within the range, and produce a
    // series of increments that sum to the number of output chunks,
    // such that each increment is displaced from the input increment
    // by an amount inversely proportional to the magnitude of the
    // stretch detection function at that input step.

    size_t regionTotalChunks = 0;

    for (size_t i = 0; i <= peaks.size(); ++i) {
        
        size_t regionStart, regionStartChunk, regionEnd, regionEndChunk;
        bool phaseReset = false;

        if (i == 0) {
            regionStartChunk = 0;
            regionStart = 0;
        } else {
            regionStartChunk = peaks[i-1].chunk;
            regionStart = fixedAudioChunks[i-1];
            phaseReset = peaks[i-1].hard;
        }

        if (i == peaks.size()) {
            regionEndChunk = totalCount;
            regionEnd = outputDuration;
        } else {
            regionEndChunk = peaks[i].chunk;
            regionEnd = fixedAudioChunks[i];
        }
        
        size_t regionDuration = regionEnd - regionStart;
        regionTotalChunks += regionDuration;

        std::vector<float> dfRegion;

        for (size_t j = regionStartChunk; j != regionEndChunk; ++j) {
            dfRegion.push_back(stretchDf[j]);
        }

        if (m_debugLevel > 1) {
            std::cerr << "distributeRegion from " << regionStartChunk << " to " << regionEndChunk << " (chunks " << regionStart << " to " << regionEnd << ")" << std::endl;
        }

        dfRegion = smoothDF(dfRegion);
        
        std::vector<int> regionIncrements = distributeRegion
            (dfRegion, regionDuration, ratio, phaseReset);

        size_t totalForRegion = 0;

        for (size_t j = 0; j < regionIncrements.size(); ++j) {

            int incr = regionIncrements[j];

            if (j == 0 && phaseReset) increments.push_back(-incr);
            else increments.push_back(incr);

            if (incr > 0) totalForRegion += incr;
            else totalForRegion += -incr;

            totalInput += m_increment;
        }

        if (totalForRegion != regionDuration) {
            std::cerr << "*** WARNING: distributeRegion returned wrong duration " << totalForRegion << ", expected " << regionDuration << std::endl;
        }

        totalOutput += totalForRegion;
    }

    if (m_debugLevel > 0) {
        std::cerr << "total input increment = " << totalInput << " (= " << totalInput / m_increment << " chunks), output = " << totalOutput << ", ratio = " << double(totalOutput)/double(totalInput) << ", ideal output " << size_t(ceil(totalInput * ratio)) << std::endl;
        std::cerr << "(region total = " << regionTotalChunks << ")" << std::endl;
    }

    return increments;
}

int
StretchCalculator::calculateSingle(double ratio,
                                   float df,
                                   size_t increment)
{
    if (increment == 0) increment = m_increment;

    bool isTransient = false;

    // We want to ensure, as close as possible, that the phase reset
    // points appear at _exactly_ the right audio frame numbers.

    // In principle, the threshold depends on chunk size: larger chunk
    // sizes need higher thresholds.  Since chunk size depends on
    // ratio, I suppose we could in theory calculate the threshold
    // from the ratio directly.  For the moment we're happy if it
    // works well in common situations.

    float transientThreshold = 0.35f;
    if (ratio > 1) transientThreshold = 0.25f;

    if (m_useHardPeaks && df > m_prevDf * 1.1f && df > transientThreshold) {
        isTransient = true;
    }

    if (m_debugLevel > 2) {
        std::cerr << "df = " << df << ", prevDf = " << m_prevDf
                  << ", thresh = " << transientThreshold << std::endl;
    }

    m_prevDf = df;

    bool ratioChanged = (ratio != m_prevRatio);
    m_prevRatio = ratio;

    if (isTransient && m_transientAmnesty == 0) {
        if (m_debugLevel > 1) {
            std::cerr << "StretchCalculator::calculateSingle: transient"
                      << std::endl;
        }
        m_divergence += increment - (increment * ratio);

        // as in offline mode, 0.05 sec approx min between transients
        m_transientAmnesty =
            lrint(ceil(double(m_sampleRate) / (20 * double(increment))));

        m_recovery = m_divergence / ((m_sampleRate / 10.0) / increment);
        return -int(increment);
    }

    if (ratioChanged) {
        m_recovery = m_divergence / ((m_sampleRate / 10.0) / increment);
    }

    if (m_transientAmnesty > 0) --m_transientAmnesty;

    int incr = lrint(increment * ratio - m_recovery);
    if (m_debugLevel > 2 || (m_debugLevel > 1 && m_divergence != 0)) {
        std::cerr << "divergence = " << m_divergence << ", recovery = " << m_recovery << ", incr = " << incr << ", ";
    }
    if (incr < lrint((increment * ratio) / 2)) {
        incr = lrint((increment * ratio) / 2);
    } else if (incr > lrint(increment * ratio * 2)) {
        incr = lrint(increment * ratio * 2);
    }

    double divdiff = (increment * ratio) - incr;

    if (m_debugLevel > 2 || (m_debugLevel > 1 && m_divergence != 0)) {
        std::cerr << "divdiff = " << divdiff << std::endl;
    }

    double prevDivergence = m_divergence;
    m_divergence -= divdiff;
    if ((prevDivergence < 0 && m_divergence > 0) ||
        (prevDivergence > 0 && m_divergence < 0)) {
        m_recovery = m_divergence / ((m_sampleRate / 10.0) / increment);
    }

    return incr;
}

void
StretchCalculator::reset()
{
    m_prevDf = 0;
    m_divergence = 0;
}

std::vector<StretchCalculator::Peak>
StretchCalculator::findPeaks(const std::vector<float> &rawDf)
{
    std::vector<float> df = smoothDF(rawDf);

    // We distinguish between "soft" and "hard" peaks.  A soft peak is
    // simply the result of peak-picking on the smoothed onset
    // detection function, and it represents any (strong-ish) onset.
    // We aim to ensure always that soft peaks are placed at the
    // correct position in time.  A hard peak is where there is a very
    // rapid rise in detection function, and it presumably represents
    // a more broadband, noisy transient.  For these we perform a
    // phase reset (if in the appropriate mode), and we locate the
    // reset at the first point where we notice enough of a rapid
    // rise, rather than necessarily at the peak itself, in order to
    // preserve the shape of the transient.
            
    std::set<size_t> hardPeakCandidates;
    std::set<size_t> softPeakCandidates;

    if (m_useHardPeaks) {

        // 0.05 sec approx min between hard peaks
        size_t hardPeakAmnesty = lrint(ceil(double(m_sampleRate) /
                                            (20 * double(m_increment))));
        size_t prevHardPeak = 0;

        if (m_debugLevel > 1) {
            std::cerr << "hardPeakAmnesty = " << hardPeakAmnesty << std::endl;
        }

        for (size_t i = 1; i + 1 < df.size(); ++i) {

            if (df[i] < 0.1) continue;
            if (df[i] <= df[i-1] * 1.1) continue;
            if (df[i] < 0.22) continue;

            if (!hardPeakCandidates.empty() &&
                i < prevHardPeak + hardPeakAmnesty) {
                continue;
            }

            bool hard = (df[i] > 0.4);
            
            if (hard && (m_debugLevel > 1)) {
                std::cerr << "hard peak at " << i << ": " << df[i] 
                          << " > absolute " << 0.4
                          << std::endl;
            }

            if (!hard) {
                hard = (df[i] > df[i-1] * 1.4);

                if (hard && (m_debugLevel > 1)) {
                    std::cerr << "hard peak at " << i << ": " << df[i] 
                              << " > prev " << df[i-1] << " * 1.4"
                              << std::endl;
                }
            }

            if (!hard && i > 1) {
                hard = (df[i]   > df[i-1] * 1.2 &&
                        df[i-1] > df[i-2] * 1.2);

                if (hard && (m_debugLevel > 1)) {
                    std::cerr << "hard peak at " << i << ": " << df[i] 
                              << " > prev " << df[i-1] << " * 1.2 and "
                              << df[i-1] << " > prev " << df[i-2] << " * 1.2"
                              << std::endl;
                }
            }

            if (!hard && i > 2) {
                // have already established that df[i] > df[i-1] * 1.1
                hard = (df[i] > 0.3 &&
                        df[i-1] > df[i-2] * 1.1 &&
                        df[i-2] > df[i-3] * 1.1);

                if (hard && (m_debugLevel > 1)) {
                    std::cerr << "hard peak at " << i << ": " << df[i] 
                              << " > prev " << df[i-1] << " * 1.1 and "
                              << df[i-1] << " > prev " << df[i-2] << " * 1.1 and "
                              << df[i-2] << " > prev " << df[i-3] << " * 1.1"
                              << std::endl;
                }
            }

            if (!hard) continue;

//            (df[i+1] > df[i] && df[i+1] > df[i-1] * 1.8) ||
//                df[i] > 0.4) {

            size_t peakLocation = i;

            if (i + 1 < rawDf.size() &&
                rawDf[i + 1] > rawDf[i] * 1.4) {

                ++peakLocation;

                if (m_debugLevel > 1) {
                    std::cerr << "pushing hard peak forward to " << peakLocation << ": " << df[peakLocation] << " > " << df[peakLocation-1] << " * " << 1.4 << std::endl;
                }
            }

            hardPeakCandidates.insert(peakLocation);
            prevHardPeak = peakLocation;
        }
    }

    size_t medianmaxsize = lrint(ceil(double(m_sampleRate) /
                                 double(m_increment))); // 1 sec ish

    if (m_debugLevel > 1) {
        std::cerr << "mediansize = " << medianmaxsize << std::endl;
    }
    if (medianmaxsize < 7) {
        medianmaxsize = 7;
        if (m_debugLevel > 1) {
            std::cerr << "adjusted mediansize = " << medianmaxsize << std::endl;
        }
    }

    int minspacing = lrint(ceil(double(m_sampleRate) /
                                (20 * double(m_increment)))); // 0.05 sec ish
    
    std::deque<float> medianwin;
    std::vector<float> sorted;
    int softPeakAmnesty = 0;

    for (size_t i = 0; i < medianmaxsize/2; ++i) {
        medianwin.push_back(0);
    }
    for (size_t i = 0; i < medianmaxsize/2 && i < df.size(); ++i) {
        medianwin.push_back(df[i]);
    }

    size_t lastSoftPeak = 0;

    for (size_t i = 0; i < df.size(); ++i) {
        
        size_t mediansize = medianmaxsize;

        if (medianwin.size() < mediansize) {
            mediansize = medianwin.size();
        }

        size_t middle = medianmaxsize / 2;
        if (middle >= mediansize) middle = mediansize-1;

        size_t nextDf = i + mediansize - middle;

        if (mediansize < 2) {
            if (mediansize > medianmaxsize) { // absurd, but never mind that
                medianwin.pop_front();
            }
            if (nextDf < df.size()) {
                medianwin.push_back(df[nextDf]);
            } else {
                medianwin.push_back(0);
            }
            continue;
        }

        if (m_debugLevel > 2) {
//            std::cerr << "have " << mediansize << " in median buffer" << std::endl;
        }

        sorted.clear();
        for (size_t j = 0; j < mediansize; ++j) {
            sorted.push_back(medianwin[j]);
        }
        std::sort(sorted.begin(), sorted.end());

        size_t n = 90; // percentile above which we pick peaks
        size_t index = (sorted.size() * n) / 100;
        if (index >= sorted.size()) index = sorted.size()-1;
        if (index == sorted.size()-1 && index > 0) --index;
        float thresh = sorted[index];

//        if (m_debugLevel > 2) {
//            std::cerr << "medianwin[" << middle << "] = " << medianwin[middle] << ", thresh = " << thresh << std::endl;
//            if (medianwin[middle] == 0.f) {
//                std::cerr << "contents: ";
//                for (size_t j = 0; j < medianwin.size(); ++j) {
//                    std::cerr << medianwin[j] << " ";
//                }
//                std::cerr << std::endl;
//            }
//        }

        if (medianwin[middle] > thresh &&
            medianwin[middle] > medianwin[middle-1] &&
            medianwin[middle] > medianwin[middle+1] &&
            softPeakAmnesty == 0) {

            size_t maxindex = middle;
            float maxval = medianwin[middle];

            for (size_t j = middle+1; j < mediansize; ++j) {
                if (medianwin[j] > maxval) {
                    maxval = medianwin[j];
                    maxindex = j;
                } else if (medianwin[j] < medianwin[middle]) {
                    break;
                }
            }

            size_t peak = i + maxindex - middle;

//            std::cerr << "i = " << i << ", maxindex = " << maxindex << ", middle = " << middle << ", so peak at " << peak << std::endl;

            if (softPeakCandidates.empty() || lastSoftPeak != peak) {

                if (m_debugLevel > 1) {
                    std::cerr << "soft peak at " << peak << " ("
                              << peak * m_increment << "): "
                              << medianwin[middle] << " > "
                              << thresh << " and "
                              << medianwin[middle]
                              << " > " << medianwin[middle-1] << " and "
                              << medianwin[middle]
                              << " > " << medianwin[middle+1]
                              << std::endl;
                }

                if (peak >= df.size()) {
                    if (m_debugLevel > 2) {
                        std::cerr << "peak is beyond end"  << std::endl;
                    }
                } else {
                    softPeakCandidates.insert(peak);
                    lastSoftPeak = peak;
                }
            }

            softPeakAmnesty = minspacing + maxindex - middle;
            if (m_debugLevel > 2) {
                std::cerr << "amnesty = " << softPeakAmnesty << std::endl;
            }

        } else if (softPeakAmnesty > 0) --softPeakAmnesty;

        if (mediansize >= medianmaxsize) {
            medianwin.pop_front();
        }
        if (nextDf < df.size()) {
            medianwin.push_back(df[nextDf]);
        } else {
            medianwin.push_back(0);
        }
    }

    std::vector<Peak> peaks;

    while (!hardPeakCandidates.empty() || !softPeakCandidates.empty()) {

        bool haveHardPeak = !hardPeakCandidates.empty();
        bool haveSoftPeak = !softPeakCandidates.empty();

        size_t hardPeak = (haveHardPeak ? *hardPeakCandidates.begin() : 0);
        size_t softPeak = (haveSoftPeak ? *softPeakCandidates.begin() : 0);

        Peak peak;
        peak.hard = false;
        peak.chunk = softPeak;

        bool ignore = false;

        if (haveHardPeak &&
            (!haveSoftPeak || hardPeak <= softPeak)) {

            if (m_debugLevel > 2) {
                std::cerr << "Hard peak: " << hardPeak << std::endl;
            }

            peak.hard = true;
            peak.chunk = hardPeak;
            hardPeakCandidates.erase(hardPeakCandidates.begin());

        } else {
            if (m_debugLevel > 2) {
                std::cerr << "Soft peak: " << softPeak << std::endl;
            }
            if (!peaks.empty() &&
                peaks[peaks.size()-1].hard &&
                peaks[peaks.size()-1].chunk + 3 >= softPeak) {
                if (m_debugLevel > 2) {
                    std::cerr << "(ignoring, as we just had a hard peak)"
                              << std::endl;
                }
                ignore = true;
            }
        }            

        if (haveSoftPeak && peak.chunk == softPeak) {
            softPeakCandidates.erase(softPeakCandidates.begin());
        }

        if (!ignore) {
            peaks.push_back(peak);
        }
    }                

    return peaks;
}

std::vector<float>
StretchCalculator::smoothDF(const std::vector<float> &df)
{
    std::vector<float> smoothedDF;
    
    for (size_t i = 0; i < df.size(); ++i) {
        // three-value moving mean window for simple smoothing
        float total = 0.f, count = 0;
        if (i > 0) { total += df[i-1]; ++count; }
        total += df[i]; ++count;
        if (i+1 < df.size()) { total += df[i+1]; ++count; }
        float mean = total / count;
        smoothedDF.push_back(mean);
    }

    return smoothedDF;
}

std::vector<int>
StretchCalculator::distributeRegion(const std::vector<float> &dfIn,
                                    size_t duration, float ratio, bool phaseReset)
{
    std::vector<float> df(dfIn);
    std::vector<int> increments;

    // The peak for the stretch detection function may appear after
    // the peak that we're using to calculate the start of the region.
    // We don't want that.  If we find a peak in the first half of
    // the region, we should set all the values up to that point to
    // the same value as the peak.

    // (This might not be subtle enough, especially if the region is
    // long -- we want a bound that corresponds to acoustic perception
    // of the audible bounce.)

    for (size_t i = 1; i < df.size()/2; ++i) {
        if (df[i] < df[i-1]) {
            if (m_debugLevel > 1) {
                std::cerr << "stretch peak offset: " << i-1 << " (peak " << df[i-1] << ")" << std::endl;
            }
            for (size_t j = 0; j < i-1; ++j) {
                df[j] = df[i-1];
            }
            break;
        }
    }

    float maxDf = 0;

    for (size_t i = 0; i < df.size(); ++i) {
        if (i == 0 || df[i] > maxDf) maxDf = df[i];
    }

    // We want to try to ensure the last 100ms or so (if possible) are
    // tending back towards the maximum df, so that the stretchiness
    // reduces at the end of the stretched region.
    
    int reducedRegion = lrint((0.1 * m_sampleRate) / m_increment);
    if (reducedRegion > int(df.size()/5)) reducedRegion = df.size()/5;

    for (int i = 0; i < reducedRegion; ++i) {
        size_t index = df.size() - reducedRegion + i;
        df[index] = df[index] + ((maxDf - df[index]) * i) / reducedRegion;
    }

    long toAllot = long(duration) - long(m_increment * df.size());
    
    if (m_debugLevel > 1) {
        std::cerr << "region of " << df.size() << " chunks, output duration " << duration << ", toAllot " << toAllot << std::endl;
    }

    size_t totalIncrement = 0;

    // We place limits on the amount of displacement per chunk.  if
    // ratio < 0, no increment should be larger than increment*ratio
    // or smaller than increment*ratio/2; if ratio > 0, none should be
    // smaller than increment*ratio or larger than increment*ratio*2.
    // We need to enforce this in the assignment of displacements to
    // allotments, not by trying to respond if something turns out
    // wrong.

    // Note that the ratio is only provided to this function for the
    // purposes of establishing this bound to the displacement.
    
    // so if
    // maxDisplacement / totalDisplacement > increment * ratio*2 - increment
    // (for ratio > 1)
    // or
    // maxDisplacement / totalDisplacement < increment * ratio/2
    // (for ratio < 1)

    // then we need to adjust and accommodate
    
    bool acceptableSquashRange = false;

    double totalDisplacement = 0;
    double maxDisplacement = 0; // min displacement will be 0 by definition

    maxDf = 0;
    float adj = 0;

    while (!acceptableSquashRange) {

        acceptableSquashRange = true;
        calculateDisplacements(df, maxDf, totalDisplacement, maxDisplacement,
                               adj);

        if (m_debugLevel > 1) {
            std::cerr << "totalDisplacement " << totalDisplacement << ", max " << maxDisplacement << " (maxDf " << maxDf << ", df count " << df.size() << ")" << std::endl;
        }

        if (totalDisplacement == 0) {
// Not usually a problem, in fact
//            std::cerr << "WARNING: totalDisplacement == 0 (duration " << duration << ", " << df.size() << " values in df)" << std::endl;
            if (!df.empty() && adj == 0) {
                acceptableSquashRange = false;
                adj = 1;
            }
            continue;
        }

        int extremeIncrement = m_increment + lrint((toAllot * maxDisplacement) / totalDisplacement);
        if (ratio < 1.0) {
            if (extremeIncrement > lrint(ceil(m_increment * ratio))) {
                std::cerr << "ERROR: extreme increment " << extremeIncrement << " > " << m_increment * ratio << " (this should not happen)" << std::endl;
            } else if (extremeIncrement < (m_increment * ratio) / 2) {
                if (m_debugLevel > 0) {
                    std::cerr << "WARNING: extreme increment " << extremeIncrement << " < " << (m_increment * ratio) / 2 << std::endl;
                }
                acceptableSquashRange = false;
            }
        } else {
            if (extremeIncrement > m_increment * ratio * 2) {
                if (m_debugLevel > 0) {
                    std::cerr << "WARNING: extreme increment " << extremeIncrement << " > " << m_increment * ratio * 2 << std::endl;
                }
                acceptableSquashRange = false;
            } else if (extremeIncrement < lrint(floor(m_increment * ratio))) {
                std::cerr << "ERROR: extreme increment " << extremeIncrement << " < " << m_increment * ratio << " (I thought this couldn't happen?)" << std::endl;
            }
        }

        if (!acceptableSquashRange) {
            // Need to make maxDisplacement smaller as a proportion of
            // the total displacement, yet ensure that the
            // displacements still sum to the total.
            adj += maxDf/10;
        }
    }

    for (size_t i = 0; i < df.size(); ++i) {

        double displacement = maxDf - df[i];
        if (displacement < 0) displacement -= adj;
        else displacement += adj;

        if (i == 0 && phaseReset) {
            if (df.size() == 1) {
                increments.push_back(duration);
                totalIncrement += duration;
            } else {
                increments.push_back(m_increment);
                totalIncrement += m_increment;
            }
            totalDisplacement -= displacement;
            continue;
        }

        double theoreticalAllotment = 0;

        if (totalDisplacement != 0) {
            theoreticalAllotment = (toAllot * displacement) / totalDisplacement;
        }
        int allotment = lrint(theoreticalAllotment);
        if (i + 1 == df.size()) allotment = toAllot;

        int increment = m_increment + allotment;

        if (increment <= 0) {
            // this is a serious problem, the allocation is quite
            // wrong if it allows increment to diverge so far from the
            // input increment
            std::cerr << "*** WARNING: increment " << increment << " <= 0, rounding to zero" << std::endl;
            increment = 0;
            allotment = increment - m_increment;
        }

        increments.push_back(increment);
        totalIncrement += increment;

        toAllot -= allotment;
        totalDisplacement -= displacement;

        if (m_debugLevel > 2) {
            std::cerr << "df " << df[i] << ", smoothed " << df[i] << ", disp " << displacement << ", allot " << theoreticalAllotment << ", incr " << increment << ", remain " << toAllot << std::endl;
        }
    }
    
    if (m_debugLevel > 2) {
        std::cerr << "total increment: " << totalIncrement << ", left over: " << toAllot << " to allot, displacement " << totalDisplacement << std::endl;
    }

    if (totalIncrement != duration) {
        std::cerr << "*** WARNING: calculated output duration " << totalIncrement << " != expected " << duration << std::endl;
    }

    return increments;
}

void
StretchCalculator::calculateDisplacements(const std::vector<float> &df,
                                          float &maxDf,
                                          double &totalDisplacement,
                                          double &maxDisplacement,
                                          float adj) const
{
    totalDisplacement = maxDisplacement = 0;

    maxDf = 0;

    for (size_t i = 0; i < df.size(); ++i) {
        if (i == 0 || df[i] > maxDf) maxDf = df[i];
    }

    for (size_t i = 0; i < df.size(); ++i) {
        double displacement = maxDf - df[i];
        if (displacement < 0) displacement -= adj;
        else displacement += adj;
        totalDisplacement += displacement;
        if (i == 0 || displacement > maxDisplacement) {
            maxDisplacement = displacement;
        }
    }
}

}


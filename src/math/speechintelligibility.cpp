/**
 *  OSM
 *  Copyright (C) 2026  Pavel Smokotnin

 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "speechintelligibility.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace Speech {
namespace {

constexpr int ModulationCount = 14;
constexpr int RastiBandCount = 2;

constexpr double BandCenters[Calculator::BandCount] = {
    125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0
};

constexpr double ModulationFrequencies[ModulationCount] = {
    0.63, 0.8, 1.0, 1.25, 1.6, 2.0, 2.5,
    3.15, 4.0, 5.0, 6.3, 8.0, 10.0, 12.5
};

// IEC 60268-16:2020, Table A.1, male speech.
constexpr double Alpha[Calculator::BandCount] = {
    0.085, 0.127, 0.230, 0.233, 0.309, 0.224, 0.173
};
constexpr double Beta[Calculator::BandCount - 1] = {
    0.085, 0.078, 0.065, 0.011, 0.047, 0.095
};

// IEC 60268-16:2011, Table A.2, absolute speech reception threshold.
constexpr double ReceptionThreshold[Calculator::BandCount] = {
    46.0, 27.0, 12.0, 6.5, 7.5, 8.0, 12.0
};

// Annex B, Table B.1.
constexpr double StipaFrequencies[Calculator::BandCount][2] = {
    { 1.60, 8.00 },
    { 1.00, 5.00 },
    { 0.63, 3.15 },
    { 2.00, 10.00 },
    { 1.25, 6.25 },
    { 0.80, 4.00 },
    { 2.50, 12.50 }
};

// Annex D, Table D.1. The first four values apply to the 500 Hz band,
// the five values of the second row apply to the 2 kHz band.
constexpr double RastiFrequencies[RastiBandCount][5] = {
    { 1.0, 2.0, 4.0, 8.0, 0.0 },
    { 0.7, 1.4, 2.8, 5.6, 11.2 }
};
constexpr int RastiBandIndex[RastiBandCount] = { 2, 4 };

// A two-section RBJ band-pass approximates an octave filter. Its absolute
// gain is irrelevant because every modulation ratio is normalized by the
// energy in the same band.
class Biquad
{
public:
    void setBandPass(double frequency, double sampleRate, double q)
    {
        const double w0 = 2.0 * M_PI * std::min(frequency, sampleRate * 0.49) / sampleRate;
        const double alpha = std::sin(w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;

        m_b0 = alpha / a0;
        m_b1 = 0.0;
        m_b2 = -alpha / a0;
        m_a1 = -2.0 * std::cos(w0) / a0;
        m_a2 = (1.0 - alpha) / a0;
        m_z1 = 0.0;
        m_z2 = 0.0;
    }

    double process(double input) noexcept
    {
        const double output = m_b0 * input + m_z1;
        m_z1 = m_b1 * input - m_a1 * output + m_z2;
        m_z2 = m_b2 * input - m_a2 * output;
        return output;
    }

private:
    double m_b0 = 0.0, m_b1 = 0.0, m_b2 = 0.0;
    double m_a1 = 0.0, m_a2 = 0.0;
    double m_z1 = 0.0, m_z2 = 0.0;
};

double clampValue(double value, double minimum, double maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

double intensityFromLevel(double level)
{
    return std::pow(10.0, level / 10.0);
}

// IEC 60268-16:2011, Table A.1: level-dependent auditory masking in dB.
double auditoryMaskingDb(double lowerBandLevel)
{
    if (lowerBandLevel < 63.0) {
        return 0.5 * lowerBandLevel - 65.0;
    }
    if (lowerBandLevel < 67.0) {
        return 1.8 * lowerBandLevel - 146.9;
    }
    if (lowerBandLevel < 100.0) {
        return 0.5 * lowerBandLevel - 59.8;
    }
    return -10.0;
}

double correctedModulation(double modulation, int band,
                           const Calculator::Correction &correction)
{
    if (!correction.hasSignalLevels) {
        return modulation;
    }

    const double signal = intensityFromLevel(correction.signalLevels[band]);
    const double noise = correction.hasNoiseLevels
                         ? intensityFromLevel(correction.noiseLevels[band])
                         : 0.0;

    // Auditory masking only spreads upwards into the next octave band. The
    // masking factor depends on the total level (signal plus ambient noise)
    // in the lower band.
    double masking = 0.0;
    if (band > 0) {
        const double lowerLevel = intensityFromLevel(correction.signalLevels[band - 1])
                                  + (correction.hasNoiseLevels
                                     ? intensityFromLevel(correction.noiseLevels[band - 1])
                                     : 0.0);
        if (lowerLevel > 0.0 && std::isfinite(lowerLevel)) {
            const double lowerBandLevel = 10.0 * std::log10(lowerLevel);
            masking = lowerLevel * intensityFromLevel(auditoryMaskingDb(lowerBandLevel));
        }
    }

    const double threshold = intensityFromLevel(ReceptionThreshold[band]);
    const double denominator = signal + noise + masking + threshold;
    if (!(denominator > 0.0) || !std::isfinite(denominator)) {
        return 0.0;
    }

    // IEC 60268-16:2011, A.5.3, with the ambient-noise term from NOTE 2.
    return clampValue(modulation * signal / denominator, 0.0, 1.0);
}

double transmissionIndex(double modulation)
{
    if (!std::isfinite(modulation) || modulation <= 0.0) {
        return 0.0;
    }
    if (modulation >= 1.0) {
        return 1.0;
    }

    // IEC 60268-16, A.2.1: m -> apparent SNR -> TI.
    const double snr = 10.0 * std::log10(modulation / (1.0 - modulation));
    return (clampValue(snr, -15.0, 15.0) + 15.0) / 30.0;
}

double weightedIndex(const std::array<float, Calculator::BandCount> &mti)
{
    double value = 0.0;
    for (int band = 0; band < Calculator::BandCount; ++band) {
        value += Alpha[band] * mti[band];
    }
    for (int band = 0; band < Calculator::BandCount - 1; ++band) {
        value -= Beta[band] * std::sqrt(mti[band] * mti[band + 1]);
    }
    return clampValue(value, 0.0, 1.0);
}

double modulationAt(const std::vector<double> &energy, double frequency, double sampleRate)
{
    if (energy.empty() || frequency <= 0.0 || sampleRate <= 0.0) {
        return 0.0;
    }

    double cosine = 1.0;
    double sine = 0.0;
    const double step = 2.0 * M_PI * frequency / sampleRate;
    const double stepCosine = std::cos(step);
    const double stepSine = std::sin(step);
    double cosineSum = 0.0;
    double sineSum = 0.0;

    for (double value : energy) {
        if (std::isfinite(value)) {
            cosineSum += value * cosine;
            sineSum += value * sine;
        }

        const double nextCosine = cosine * stepCosine - sine * stepSine;
        sine = sine * stepCosine + cosine * stepSine;
        cosine = nextCosine;
    }

    return std::sqrt(cosineSum * cosineSum + sineSum * sineSum);
}

double median(std::vector<double> values)
{
    if (values.empty()) {
        return 0.0;
    }
    const auto middle = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}

} // namespace

QString Calculator::rating(float value)
{
    if (value >= 0.76f) return QStringLiteral("A+");
    if (value >= 0.72f) return QStringLiteral("A");
    if (value >= 0.68f) return QStringLiteral("B");
    if (value >= 0.64f) return QStringLiteral("C");
    if (value >= 0.60f) return QStringLiteral("D");
    if (value >= 0.56f) return QStringLiteral("E");
    if (value >= 0.52f) return QStringLiteral("F");
    if (value >= 0.48f) return QStringLiteral("G");
    if (value >= 0.44f) return QStringLiteral("H");
    if (value >= 0.40f) return QStringLiteral("I");
    if (value >= 0.36f) return QStringLiteral("J");
    return QStringLiteral("U");
}

Calculator::Result Calculator::fromImpulse(const std::vector<float> &values,
                                          const std::vector<float> &times,
                                          unsigned sampleRate)
{
    return fromImpulse(values, times, sampleRate, Correction{});
}

Calculator::Result Calculator::fromImpulse(const std::vector<float> &values,
                                          const std::vector<float> &times,
                                          unsigned sampleRate,
                                          const Correction &correction)
{
    Result result;
    const size_t count = std::min(values.size(), times.size());
    if (count < 32) {
        result.error = QStringLiteral("Not enough impulse-response samples.");
        return result;
    }

    if (correction.hasNoiseLevels && !correction.hasSignalLevels) {
        result.error = QStringLiteral("Signal octave-band levels are required for noise correction.");
        return result;
    }
    if (correction.hasSignalLevels || correction.hasNoiseLevels) {
        for (int band = 0; band < BandCount; ++band) {
            if (correction.hasSignalLevels) {
                const double level = correction.signalLevels[band];
                if (!std::isfinite(level) || level < -300.0 || level > 300.0) {
                    result.error = QStringLiteral("Speech octave-band levels must be finite values in dB SPL.");
                    return result;
                }
            }
            if (correction.hasNoiseLevels) {
                const double level = correction.noiseLevels[band];
                if (!std::isfinite(level) || level < -300.0 || level > 300.0) {
                    result.error = QStringLiteral("Noise octave-band levels must be finite values in dB SPL.");
                    return result;
                }
            }
        }
    }

    std::vector<std::pair<double, double>> samples;
    samples.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const double time = static_cast<double>(times[i]);
        const double value = static_cast<double>(values[i]);
        if (std::isfinite(time) && std::isfinite(value)) {
            samples.emplace_back(time, value);
        }
    }
    if (samples.size() < 32) {
        result.error = QStringLiteral("The impulse response contains no usable samples.");
        return result;
    }

    // OSM stores the impulse response circularly: t = 0 is near the middle of
    // the array and the negative-time half wraps to the beginning. Restore
    // chronological order before filtering or analyzing modulation.
    std::stable_sort(samples.begin(), samples.end(),
                     [](const std::pair<double, double> &left,
                        const std::pair<double, double> &right) {
                         return left.first < right.first;
                     });

    const double span = samples.back().first - samples.front().first;
    if (!(span > 0.0) || !std::isfinite(span)) {
        result.error = QStringLiteral("The impulse-response time axis is invalid.");
        return result;
    }

    double rate = sampleRate;
    if (!(rate > 0.0)) {
        std::vector<double> differences;
        differences.reserve(samples.size() - 1);
        for (size_t i = 1; i < samples.size(); ++i) {
            const double difference = samples[i].first - samples[i - 1].first;
            if (difference > 0.0 && std::isfinite(difference)) {
                differences.push_back(difference);
            }
        }
        const double step = median(std::move(differences));
        if (!(step > 0.0)) {
            result.error = QStringLiteral("The impulse-response time axis is invalid.");
            return result;
        }
        rate = 1000.0 / step;
    }

    if (!std::isfinite(rate) || rate < 17000.0) {
        result.error = QStringLiteral("The sample rate must be at least 17 kHz to calculate the 8 kHz STI band.");
        return result;
    }

    result.sampleRate = static_cast<float>(rate);

    double mean = 0.0;
    for (const auto &sample : samples) {
        mean += sample.second;
    }
    mean /= static_cast<double>(samples.size());

    size_t peak = 0;
    double maximum = 0.0;
    for (size_t i = 0; i < samples.size(); ++i) {
        const double value = samples[i].second - mean;
        if (std::abs(value) > maximum) {
            maximum = std::abs(value);
            peak = i;
        }
    }
    if (!(maximum > 1e-20) || !std::isfinite(maximum)) {
        result.error = QStringLiteral("The impulse response contains no energy.");
        return result;
    }

    // Anchor at the dominant arrival and keep a small leading margin for the
    // filter's pre-response. This also discards the wrapped negative-time tail.
    const double originTime = samples[peak].first;
    result.duration = static_cast<float>(std::max(0.0, samples.back().first - originTime) / 1000.0);
    result.shortImpulse = result.duration < 1.6f;

    const auto first = std::lower_bound(
        samples.begin(), samples.end(), originTime - 5.0,
        [](const std::pair<double, double> &sample, double time) {
            return sample.first < time;
        });
    const size_t firstIndex = static_cast<size_t>(first - samples.begin());

    std::vector<double> impulse;
    impulse.reserve(samples.size() - firstIndex);
    double selectedMean = 0.0;
    for (size_t i = firstIndex; i < samples.size(); ++i) {
        impulse.push_back(samples[i].second);
        selectedMean += samples[i].second;
    }
    selectedMean /= static_cast<double>(impulse.size());

    maximum = 0.0;
    for (double &value : impulse) {
        value -= selectedMean;
        maximum = std::max(maximum, std::abs(value));
    }
    if (!(maximum > 1e-20) || !std::isfinite(maximum)) {
        result.error = QStringLiteral("The impulse response contains no energy.");
        return result;
    }

    for (double &value : impulse) {
        value /= maximum;
    }

    // Leave room for the IIR filters to settle before and after the measured
    // impulse response. The trailing zeros also retain the filter's tail.
    const int warmup = std::max(512, static_cast<int>(rate / 10.0));
    const size_t filteredSize = count + static_cast<size_t>(warmup) * 2;
    std::vector<double> input(filteredSize, 0.0);
    std::copy(impulse.begin(), impulse.end(), input.begin() + warmup);

    std::array<std::vector<double>, BandCount> bandEnergy;
    bool hasEnergy = false;

    for (int band = 0; band < BandCount; ++band) {
        Biquad first;
        Biquad second;
        first.setBandPass(BandCenters[band], rate, std::sqrt(2.0));
        second.setBandPass(BandCenters[band], rate, std::sqrt(2.0));

        bandEnergy[band].resize(filteredSize - static_cast<size_t>(warmup));
        double total = 0.0;
        for (size_t i = 0; i < filteredSize; ++i) {
            const double filtered = second.process(first.process(input[i]));
            const double energy = filtered * filtered;
            if (i >= static_cast<size_t>(warmup)) {
                bandEnergy[band][i - static_cast<size_t>(warmup)] = energy;
                total += energy;
            }
        }

        if (total > 1e-18 && std::isfinite(total)) {
            hasEnergy = true;
        } else {
            // A band without energy contributes TI = 0.
            std::fill(bandEnergy[band].begin(), bandEnergy[band].end(), 0.0);
        }
    }

    if (!hasEnergy) {
        result.error = QStringLiteral("The impulse response contains no energy in the STI bands.");
        return result;
    }

    std::array<std::array<float, ModulationCount>, BandCount> fullTi{};
    std::array<std::array<float, 2>, BandCount> stipaTi{};
    const auto correctedTi = [&correction](double modulation, int band) {
        return static_cast<float>(transmissionIndex(
            correctedModulation(clampValue(modulation, 0.0, 1.0), band, correction)));
    };

    for (int band = 0; band < BandCount; ++band) {
        double total = 0.0;
        for (double value : bandEnergy[band]) {
            total += value;
        }

        if (total > 0.0) {
            for (int modulation = 0; modulation < ModulationCount; ++modulation) {
                const double normalized = modulationAt(bandEnergy[band],
                                                       ModulationFrequencies[modulation],
                                                       rate) / total;
                fullTi[band][modulation] = correctedTi(normalized, band);
            }
            for (int modulation = 0; modulation < 2; ++modulation) {
                const double normalized = modulationAt(bandEnergy[band],
                                                       StipaFrequencies[band][modulation],
                                                       rate) / total;
                stipaTi[band][modulation] = correctedTi(normalized, band);
            }
        }

        double sum = 0.0;
        for (float ti : fullTi[band]) {
            sum += ti;
        }
        result.mti[band] = static_cast<float>(sum / ModulationCount);
    }

    result.sti = static_cast<float>(weightedIndex(result.mti));

    std::array<float, BandCount> stipaMti{};
    for (int band = 0; band < BandCount; ++band) {
        stipaMti[band] = (stipaTi[band][0] + stipaTi[band][1]) * 0.5f;
    }
    result.stipa = static_cast<float>(weightedIndex(stipaMti));

    double rastiSum = 0.0;
    int rastiCount = 0;
    for (int rastiBand = 0; rastiBand < RastiBandCount; ++rastiBand) {
        const int band = RastiBandIndex[rastiBand];
        double total = 0.0;
        for (double value : bandEnergy[band]) {
            total += value;
        }
        const int modulationCount = (rastiBand == 0 ? 4 : 5);
        for (int modulation = 0; modulation < modulationCount; ++modulation) {
            double normalized = 0.0;
            if (total > 0.0) {
                normalized = modulationAt(bandEnergy[band],
                                          RastiFrequencies[rastiBand][modulation],
                                          rate) / total;
            }
            rastiSum += correctedTi(normalized, band);
            ++rastiCount;
        }
    }
    result.rasti = static_cast<float>(clampValue(rastiSum / std::max(1, rastiCount), 0.0, 1.0));
    result.noiseCorrectionApplied = correction.hasNoiseLevels;
    result.maskingCorrectionApplied = correction.hasSignalLevels;
    result.valid = true;
    return result;
}

} // namespace Speech

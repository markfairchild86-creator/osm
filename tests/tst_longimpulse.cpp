#include <QtTest>

#include <cmath>
#include <limits>
#include <random>
#include <vector>

#include "math/deconvolution.h"
#include "math/fouriertransform.h"
#include "math/speechintelligibility.h"

class LongImpulseTest : public QObject
{
    Q_OBJECT

private slots:
    void causalLongImpulse_data();
    void causalLongImpulse();
    void correctionLowersLongImpulseIntelligibility_data();
    void correctionLowersLongImpulseIntelligibility();
    void rejectsInvalidCorrectionLevels();
};

namespace {

// A deterministic room-like IR: a direct arrival plus a decaying noise tail.
std::vector<float> makeImpulse(unsigned count, unsigned sampleRate)
{
    std::vector<float> impulse(count, 0.f);
    std::mt19937 generator(20261108u);
    std::normal_distribution<float> distribution(0.f, 1.f);
    constexpr double t60 = 2.0;
    for (unsigned i = 0; i < count; ++i) {
        const double time = static_cast<double>(i) / sampleRate;
        impulse[i] = distribution(generator) *
                     static_cast<float>(std::exp(-6.907755 * time / t60));
    }
    impulse[0] += 1.f;
    return impulse;
}

// Measurement stores the deconvolution result circularly: t=0 is at N/2-1,
// the positive-time half follows, and later indices wrap to negative times.
void toMeasurementLayout(const std::vector<float> &input,
                         unsigned sampleRate,
                         std::vector<float> &values,
                         std::vector<float> &times)
{
    const unsigned count = static_cast<unsigned>(input.size());
    values.assign(count, 0.f);
    times.assign(count, 0.f);
    for (unsigned i = 0; i < count; ++i) {
        int timeIndex = static_cast<int>(i);
        int index = static_cast<int>(count / 2 - 1 + i);
        if (timeIndex > static_cast<int>(count / 2)) {
            timeIndex -= static_cast<int>(count);
            index -= static_cast<int>(count);
        }
        values[index] = input[i];
        times[index] = static_cast<float>(timeIndex * 1000.0 / sampleRate);
    }
}

// Runs the same forward-deconvolution path used by Measurement: reference
// channel B is an impulse, measured channel A is the IR.
void deconvolveImpulse(unsigned count,
                       unsigned sampleRate,
                       std::vector<float> &values,
                       std::vector<float> &times,
                       unsigned &peakIndex)
{
    const auto impulse = makeImpulse(count, sampleRate);

    FourierTransform forward;
    forward.setSize(count);
    forward.setType(FourierTransform::Fast);
    forward.setSampleRate(sampleRate);
    forward.prepareFast();
    for (unsigned i = 0; i < count; ++i) {
        forward.add(impulse[i], i == 0 ? 1.f : 0.f);
    }
    forward.transform();

    Deconvolution deconvolution;
    deconvolution.setSize(count);
    deconvolution.transform(&forward);

    std::vector<float> measured(count, 0.f);
    for (unsigned i = 0; i < count; ++i) {
        measured[i] = deconvolution.get(i);
    }

    peakIndex = deconvolution.maxIndex();
    toMeasurementLayout(measured, sampleRate, values, times);
}

void addLongImpulseRows()
{
    QTest::addColumn<unsigned>("count");
    QTest::addColumn<unsigned>("sampleRate");

    // FFT19 at 96 kHz and FFT20 at 192 kHz both provide 2.73 s of causal IR.
    QTest::newRow("FFT19 / 96 kHz") << (1u << 19) << 96000u;
    QTest::newRow("FFT20 / 192 kHz") << (1u << 20) << 192000u;
}

} // namespace

void LongImpulseTest::causalLongImpulse_data()
{
    addLongImpulseRows();
}

void LongImpulseTest::causalLongImpulse()
{
    QFETCH(unsigned, count);
    QFETCH(unsigned, sampleRate);

    std::vector<float> values;
    std::vector<float> times;
    unsigned peakIndex = 0;
    deconvolveImpulse(count, sampleRate, values, times, peakIndex);

    const auto result = Speech::Calculator::fromImpulse(values, times, sampleRate);
    qInfo() << "sampleRate" << sampleRate
            << "count" << count
            << "peakIndex" << peakIndex
            << "duration" << result.duration
            << "shortImpulse" << result.shortImpulse
            << "STI" << result.sti
            << "STIPA" << result.stipa
            << "RASTI" << result.rasti
            << "error" << result.error;

    QVERIFY2(result.valid, qPrintable(result.error));
    QVERIFY(!result.shortImpulse);
    QVERIFY2(result.duration > 1.6f, qPrintable(QString::number(result.duration)));
    QVERIFY(result.sti >= 0.f && result.sti <= 1.f);
}

void LongImpulseTest::correctionLowersLongImpulseIntelligibility_data()
{
    addLongImpulseRows();
}

void LongImpulseTest::correctionLowersLongImpulseIntelligibility()
{
    QFETCH(unsigned, count);
    QFETCH(unsigned, sampleRate);

    std::vector<float> values;
    std::vector<float> times;
    unsigned peakIndex = 0;
    deconvolveImpulse(count, sampleRate, values, times, peakIndex);

    const auto baseline = Speech::Calculator::fromImpulse(values, times, sampleRate);
    QVERIFY2(baseline.valid, qPrintable(baseline.error));

    // Masking plus the absolute reception threshold from the speech levels.
    Speech::Calculator::Correction correction;
    correction.hasSignalLevels = true;
    correction.signalLevels.fill(60.f);
    const auto masked = Speech::Calculator::fromImpulse(values, times, sampleRate, correction);

    QVERIFY2(masked.valid, qPrintable(masked.error));
    QVERIFY(masked.maskingCorrectionApplied);
    QVERIFY(!masked.noiseCorrectionApplied);
    QCOMPARE(masked.duration, baseline.duration);
    QCOMPARE(masked.shortImpulse, baseline.shortImpulse);
    // The 125 Hz band has no masking term, but still receives the absolute
    // reception threshold correction.
    QVERIFY(masked.mti[0] < baseline.mti[0]);
    QVERIFY(masked.sti < baseline.sti);
    QVERIFY(masked.sti >= 0.f && masked.sti <= 1.f);

    // Noise at the speech level adds the ambient-noise term to every band.
    correction.hasNoiseLevels = true;
    correction.noiseLevels.fill(60.f);
    const auto balanced = Speech::Calculator::fromImpulse(values, times, sampleRate, correction);
    QVERIFY2(balanced.valid, qPrintable(balanced.error));
    QVERIFY(balanced.noiseCorrectionApplied);
    QVERIFY(balanced.sti < masked.sti);
    QVERIFY(balanced.sti >= 0.f && balanced.sti <= 1.f);

    // Dominant noise must reduce intelligibility further.
    correction.noiseLevels.fill(80.f);
    const auto noisy = Speech::Calculator::fromImpulse(values, times, sampleRate, correction);
    QVERIFY2(noisy.valid, qPrintable(noisy.error));
    QVERIFY(noisy.noiseCorrectionApplied);
    QVERIFY(noisy.sti < balanced.sti);
    QVERIFY(noisy.sti >= 0.f);
    QVERIFY(noisy.stipa >= 0.f && noisy.stipa <= 1.f);
    QVERIFY(noisy.rasti >= 0.f && noisy.rasti <= 1.f);

    qInfo() << "sampleRate" << sampleRate
            << "count" << count
            << "peakIndex" << peakIndex
            << "duration" << masked.duration
            << "STI baseline" << baseline.sti
            << "masking" << masked.sti
            << "balanced" << balanced.sti
            << "noisy" << noisy.sti;
}

void LongImpulseTest::rejectsInvalidCorrectionLevels()
{
    // Validation depends only on the supplied levels, not on the transform
    // size, so one long configuration is enough here.
    const unsigned count = 1u << 19;
    const unsigned sampleRate = 96000u;

    std::vector<float> values;
    std::vector<float> times;
    unsigned peakIndex = 0;
    deconvolveImpulse(count, sampleRate, values, times, peakIndex);

    const auto baseline = Speech::Calculator::fromImpulse(values, times, sampleRate);
    QVERIFY2(baseline.valid, qPrintable(baseline.error));

    // Noise levels without speech levels have no reference signal intensity.
    Speech::Calculator::Correction correction;
    correction.hasNoiseLevels = true;
    correction.noiseLevels.fill(40.f);
    auto result = Speech::Calculator::fromImpulse(values, times, sampleRate, correction);
    QVERIFY2(result.error.contains(QStringLiteral("Signal octave-band levels")),
             qPrintable(result.error));
    QVERIFY(!result.valid);

    // A non-finite speech band level must be rejected.
    correction.hasSignalLevels = true;
    correction.signalLevels.fill(60.f);
    correction.signalLevels[3] = std::numeric_limits<float>::quiet_NaN();
    result = Speech::Calculator::fromImpulse(values, times, sampleRate, correction);
    QVERIFY2(result.error.contains(QStringLiteral("Speech octave-band levels")),
             qPrintable(result.error));
    QVERIFY(!result.valid);

    // Non-finite and out-of-range noise band levels must be rejected.
    correction.signalLevels.fill(60.f);
    correction.noiseLevels[0] = std::numeric_limits<float>::infinity();
    result = Speech::Calculator::fromImpulse(values, times, sampleRate, correction);
    QVERIFY2(result.error.contains(QStringLiteral("Noise octave-band levels")),
             qPrintable(result.error));
    QVERIFY(!result.valid);

    correction.noiseLevels.fill(60.f);
    correction.noiseLevels[6] = 301.f;
    result = Speech::Calculator::fromImpulse(values, times, sampleRate, correction);
    QVERIFY2(result.error.contains(QStringLiteral("Noise octave-band levels")),
             qPrintable(result.error));
    QVERIFY(!result.valid);

    qInfo() << "sampleRate" << sampleRate
            << "count" << count
            << "peakIndex" << peakIndex
            << "baseline STI" << baseline.sti;
}

QTEST_GUILESS_MAIN(LongImpulseTest)

#include "tst_longimpulse.moc"

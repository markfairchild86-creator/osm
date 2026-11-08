#include <QtTest>

#include <cmath>
#include <limits>
#include <random>
#include <vector>

#include "math/speechintelligibility.h"

class SpeechIntelligibilityTest : public QObject
{
    Q_OBJECT

private slots:
    void perfectImpulse();
    void centeredImpulse();
    void decayLowersIntelligibility();
    void appliesNoiseAndMaskingCorrections();
    void rejectsNoiseWithoutSignalLevels();
    void rejectsInvalidCorrectionLevels();
    void reportsShortImpulse();
    void rejectsInvalidInput();
    void ratingBands();
};

namespace {

constexpr unsigned SampleRate = 48000;

std::vector<float> makeTimes(size_t count)
{
    std::vector<float> times(count);
    for (size_t i = 0; i < count; ++i) {
        times[i] = static_cast<float>(i * 1000.0 / SampleRate);
    }
    return times;
}

// Mirrors Abstract::Data's circular impulse layout: negative times occupy the
// first half, t = 0 is at count / 2 - 1, and positive times follow.
std::vector<float> makeCenteredTimes(size_t count)
{
    std::vector<float> times(count);
    const size_t center = count / 2 - 1;
    for (size_t i = 0; i < count; ++i) {
        long long timeIndex = static_cast<long long>(i);
        if (timeIndex > static_cast<long long>(count / 2)) {
            timeIndex -= static_cast<long long>(count);
        }
        const size_t index = (center + i) % count;
        times[index] = static_cast<float>(timeIndex * 1000.0 / SampleRate);
    }
    return times;
}

std::vector<float> makeDecay(double t60, size_t count)
{
    std::vector<float> values(count, 0.f);
    std::mt19937 generator(123456u);
    std::normal_distribution<float> distribution(0.f, 1.f);
    for (size_t i = 0; i < count; ++i) {
        const double time = static_cast<double>(i) / SampleRate;
        values[i] = distribution(generator) * static_cast<float>(std::exp(-6.907755 * time / t60));
    }
    return values;
}

} // namespace

void SpeechIntelligibilityTest::perfectImpulse()
{
    const size_t count = SampleRate * 2;
    std::vector<float> values(count, 0.f);
    values[0] = 1.f;

    const auto result = Speech::Calculator::fromImpulse(values, makeTimes(count), SampleRate);

    QVERIFY2(result.valid, qPrintable(result.error));
    QVERIFY(!result.shortImpulse);
    QVERIFY(!result.noiseCorrectionApplied);
    QVERIFY(!result.maskingCorrectionApplied);
    QVERIFY2(result.sti > 0.98f, qPrintable(QString::number(result.sti)));
    QVERIFY2(result.stipa > 0.98f, qPrintable(QString::number(result.stipa)));
    QVERIFY2(result.rasti > 0.98f, qPrintable(QString::number(result.rasti)));
    for (float mti : result.mti) {
        QVERIFY(mti > 0.98f);
    }
}

void SpeechIntelligibilityTest::centeredImpulse()
{
    const size_t count = SampleRate * 2;
    const size_t center = count / 2 - 1;

    std::vector<float> values(count, 0.f);
    values[center] = 1.f;
    const auto centered = Speech::Calculator::fromImpulse(values, makeCenteredTimes(count), SampleRate);

    std::vector<float> referenceValues(count, 0.f);
    referenceValues[0] = 1.f;
    const auto reference = Speech::Calculator::fromImpulse(referenceValues, makeTimes(count), SampleRate);

    QVERIFY2(centered.valid, qPrintable(centered.error));
    QVERIFY2(reference.valid, qPrintable(reference.error));
    QVERIFY2(qAbs(centered.sti - reference.sti) < 0.001f,
             qPrintable(QString("centered STI %1, reference %2")
                            .arg(centered.sti).arg(reference.sti)));
    QVERIFY2(qAbs(centered.stipa - reference.stipa) < 0.001f,
             qPrintable(QString("centered STIPA %1, reference %2")
                            .arg(centered.stipa).arg(reference.stipa)));
    QVERIFY2(qAbs(centered.rasti - reference.rasti) < 0.001f,
             qPrintable(QString("centered RASTI %1, reference %2")
                            .arg(centered.rasti).arg(reference.rasti)));
    QVERIFY2(centered.shortImpulse, "centered layout should report only the post-arrival duration");
    QVERIFY(!reference.shortImpulse);
}

void SpeechIntelligibilityTest::decayLowersIntelligibility()
{
    const size_t count = SampleRate * 2;
    const auto times = makeTimes(count);
    const auto dry = Speech::Calculator::fromImpulse(makeDecay(0.25, count), times, SampleRate);
    const auto reverberant = Speech::Calculator::fromImpulse(makeDecay(2.0, count), times, SampleRate);

    QVERIFY2(dry.valid, qPrintable(dry.error));
    QVERIFY2(reverberant.valid, qPrintable(reverberant.error));
    QVERIFY(dry.sti > reverberant.sti);
    QVERIFY(dry.sti > 0.55f);
    QVERIFY(reverberant.sti < 0.70f);
    QVERIFY(qAbs(dry.sti - reverberant.sti) > 0.10f);

    QVERIFY(dry.stipa >= 0.f && dry.stipa <= 1.f);
    QVERIFY(dry.rasti >= 0.f && dry.rasti <= 1.f);
    QVERIFY(reverberant.stipa >= 0.f && reverberant.stipa <= 1.f);
    QVERIFY(reverberant.rasti >= 0.f && reverberant.rasti <= 1.f);
}

void SpeechIntelligibilityTest::appliesNoiseAndMaskingCorrections()
{
    const size_t count = SampleRate * 2;
    std::vector<float> values(count, 0.f);
    values[0] = 1.f;
    const auto times = makeTimes(count);
    const auto baseline = Speech::Calculator::fromImpulse(values, times, SampleRate);

    Speech::Calculator::Correction correction;
    correction.hasSignalLevels = true;
    correction.signalLevels.fill(60.f);
    const auto masked = Speech::Calculator::fromImpulse(values, times, SampleRate, correction);

    QVERIFY2(baseline.valid, qPrintable(baseline.error));
    QVERIFY2(masked.valid, qPrintable(masked.error));
    QVERIFY(masked.maskingCorrectionApplied);
    QVERIFY(!masked.noiseCorrectionApplied);
    QVERIFY(masked.sti < baseline.sti);
    QVERIFY(masked.sti > 0.90f);

    correction.hasNoiseLevels = true;
    correction.noiseLevels.fill(80.f);
    const auto noisy = Speech::Calculator::fromImpulse(values, times, SampleRate, correction);

    QVERIFY2(noisy.valid, qPrintable(noisy.error));
    QVERIFY(noisy.maskingCorrectionApplied);
    QVERIFY(noisy.noiseCorrectionApplied);
    QVERIFY(noisy.sti < masked.sti);
    QVERIFY(noisy.sti < 0.50f);

    correction.noiseLevels.fill(60.f);
    const auto balanced = Speech::Calculator::fromImpulse(values, times, SampleRate, correction);
    QVERIFY2(balanced.valid, qPrintable(balanced.error));
    QVERIFY(balanced.mti[0] > 0.45f && balanced.mti[0] < 0.55f);

    correction.hasNoiseLevels = false;
    correction.signalLevels.fill(30.f);
    const auto quiet = Speech::Calculator::fromImpulse(values, times, SampleRate, correction);
    QVERIFY2(quiet.valid, qPrintable(quiet.error));
    QVERIFY(quiet.sti < masked.sti);
}

void SpeechIntelligibilityTest::rejectsNoiseWithoutSignalLevels()
{
    const size_t count = SampleRate * 2;
    std::vector<float> values(count, 0.f);
    values[0] = 1.f;

    Speech::Calculator::Correction correction;
    correction.hasNoiseLevels = true;
    correction.noiseLevels.fill(40.f);

    const auto result = Speech::Calculator::fromImpulse(values, makeTimes(count), SampleRate, correction);
    QVERIFY(!result.valid);
    QVERIFY(!result.error.isEmpty());
}

void SpeechIntelligibilityTest::rejectsInvalidCorrectionLevels()
{
    const size_t count = SampleRate * 2;
    std::vector<float> values(count, 0.f);
    values[0] = 1.f;

    Speech::Calculator::Correction correction;
    correction.hasSignalLevels = true;
    correction.signalLevels.fill(60.f);
    correction.signalLevels[3] = std::numeric_limits<float>::quiet_NaN();

    const auto result = Speech::Calculator::fromImpulse(values, makeTimes(count), SampleRate, correction);
    QVERIFY(!result.valid);
    QVERIFY(!result.error.isEmpty());
}

void SpeechIntelligibilityTest::reportsShortImpulse()
{
    const size_t count = SampleRate / 2;
    std::vector<float> values(count, 0.f);
    values[0] = 1.f;

    const auto result = Speech::Calculator::fromImpulse(values, makeTimes(count), SampleRate);

    QVERIFY2(result.valid, qPrintable(result.error));
    QVERIFY(result.shortImpulse);
    QVERIFY(result.duration < 1.6f);
}

void SpeechIntelligibilityTest::rejectsInvalidInput()
{
    auto empty = Speech::Calculator::fromImpulse({}, {}, SampleRate);
    QVERIFY(!empty.valid);
    QVERIFY(!empty.error.isEmpty());

    std::vector<float> values(1000, 0.f);
    auto silent = Speech::Calculator::fromImpulse(values, makeTimes(values.size()), SampleRate);
    QVERIFY(!silent.valid);
    QVERIFY(!silent.error.isEmpty());

    values[0] = 1.f;
    auto lowRate = Speech::Calculator::fromImpulse(values, makeTimes(values.size()), 16000);
    QVERIFY(!lowRate.valid);
    QVERIFY(!lowRate.error.isEmpty());
}

void SpeechIntelligibilityTest::ratingBands()
{
    QCOMPARE(Speech::Calculator::rating(0.95f), QStringLiteral("A+"));
    QCOMPARE(Speech::Calculator::rating(0.74f), QStringLiteral("A"));
    QCOMPARE(Speech::Calculator::rating(0.66f), QStringLiteral("C"));
    QCOMPARE(Speech::Calculator::rating(0.54f), QStringLiteral("F"));
    QCOMPARE(Speech::Calculator::rating(0.42f), QStringLiteral("I"));
    QCOMPARE(Speech::Calculator::rating(0.20f), QStringLiteral("U"));
}

QTEST_GUILESS_MAIN(SpeechIntelligibilityTest)

#include "tst_speechintelligibility.moc"

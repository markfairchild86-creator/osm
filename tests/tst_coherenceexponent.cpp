#include <QtTest>
#include <QSettings>
#include <QStandardPaths>
#include <cmath>

#include "common/settings.h"
#include "common/workingfolder.h"
#include "meta/metameasurement.h"

class TestMeasurement : public QObject, public Meta::Measurement
{
    Q_OBJECT

public:
    TestMeasurement() = default;

    void resetAverage() noexcept override {}
    void applyAutoGain(float) override {}

signals:
    void polarityChanged(bool) override;
    void gainChanged(float) override;
    void offsetChanged(float) override;
    void modeChanged(Meta::Measurement::Mode) override;
    void averageChanged(unsigned int) override;
    void averageDepthChanged(unsigned int) override;
    void magnitudeAverageTypeChanged(Meta::Measurement::MagnitudeAverageType) override;
    void coherenceWeightingChanged(bool) override;
    void coherenceExponentChanged(float) override;
    void logWindowPointsPerOctaveChanged(unsigned int) override;
    void dataChanelChanged(unsigned int) override;
    void referenceChanelChanged(unsigned int) override;
    void averageTypeChanged(Meta::Measurement::AverageType) override;
    void windowFunctionTypeChanged(WindowFunction::Type) override;
    void filtersFrequencyChanged(Filter::Frequency) override;
    void delayChanged(int) override;
    void inputFilterChanged(Meta::Measurement::InputFilter) override;
};

class CoherenceExponentTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void clampsExponent();
    void weightsBins();
    void persistsExponent();
};

void CoherenceExponentTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("opensoundmeter-tests");
    QCoreApplication::setApplicationName("OpenSoundMeterCoherenceExponentTest");

    QSettings settings(workingfolder::settingsFilePath(), QSettings::IniFormat);
    settings.clear();
    settings.sync();
}

void CoherenceExponentTest::cleanupTestCase()
{
    QSettings settings(workingfolder::settingsFilePath(), QSettings::IniFormat);
    settings.clear();
    settings.sync();
}

void CoherenceExponentTest::clampsExponent()
{
    TestMeasurement measurement;
    QSignalSpy signal(&measurement, &TestMeasurement::coherenceExponentChanged);

    QVERIFY(qFuzzyCompare(measurement.coherenceExponent(),
                          Meta::Measurement::DEFAULT_COHERENCE_EXPONENT));

    measurement.setCoherenceExponent(0.1f);
    QVERIFY(qFuzzyCompare(measurement.coherenceExponent(),
                          Meta::Measurement::MIN_COHERENCE_EXPONENT));
    QCOMPARE(signal.count(), 1);

    measurement.setCoherenceExponent(10.f);
    QVERIFY(qFuzzyCompare(measurement.coherenceExponent(),
                          Meta::Measurement::MAX_COHERENCE_EXPONENT));
    QCOMPARE(signal.count(), 2);

    measurement.setCoherenceExponent(std::nanf(""));
    QVERIFY(qFuzzyCompare(measurement.coherenceExponent(),
                          Meta::Measurement::DEFAULT_COHERENCE_EXPONENT));
    QCOMPARE(signal.count(), 3);

    measurement.setCoherenceExponent(2.5f);
    QVERIFY(qFuzzyCompare(measurement.coherenceExponent(), 2.5f));
    QCOMPARE(signal.count(), 4);

    // Re-applying the same value must not emit another change.
    measurement.setCoherenceExponent(2.5f);
    QCOMPARE(signal.count(), 4);
}

void CoherenceExponentTest::weightsBins()
{
    using Meta::Measurement;

    const float coherence = 0.5f;
    QVERIFY(qAbs(Measurement::coherenceWeight(coherence, 1.f) - 0.5f) < 1e-6f);
    QVERIFY(qAbs(Measurement::coherenceWeight(coherence, 2.f) - 0.25f) < 1e-6f);
    QVERIFY(qAbs(Measurement::coherenceWeight(coherence, 4.f) - 0.0625f) < 1e-6f);

    // A larger exponent suppresses a partially coherent bin more strongly.
    QVERIFY(Measurement::coherenceWeight(coherence, 4.f) <
            Measurement::coherenceWeight(coherence, 2.f));
    QVERIFY(Measurement::coherenceWeight(coherence, 2.f) <
            Measurement::coherenceWeight(coherence, 1.f));

    // A smaller exponent suppresses it less.
    QVERIFY(Measurement::coherenceWeight(coherence, 0.5f) >
            Measurement::coherenceWeight(coherence, 1.f));

    // Invalid coherence values are treated as fully incoherent.
    QCOMPARE(Measurement::coherenceWeight(std::nanf(""), 2.f), 0.f);
    QCOMPARE(Measurement::coherenceWeight(-1.f, 2.f), 0.f);
    QCOMPARE(Measurement::coherenceWeight(2.f, 2.f), 1.f);

    // Non-finite and out-of-range exponents follow the setter's limits.
    QVERIFY(qAbs(Measurement::coherenceWeight(coherence, std::nanf("")) - 0.5f) < 1e-6f);
    QVERIFY(qAbs(Measurement::coherenceWeight(coherence, 10.f) - 0.0625f) < 1e-6f);
}

void CoherenceExponentTest::persistsExponent()
{
    const QString groupName = "coherence_exponent_persistence";
    const QString key = QString::fromLatin1(Meta::Measurement::coherenceExponentSettingsKey());

    {
        Settings root(groupName);
        Settings *measurementSettings = root.getSubGroup("/measurement");
        TestMeasurement first;
        first.setCoherenceExponent(
            measurementSettings->value(key, first.coherenceExponent()).toFloat());
        QVERIFY(qFuzzyCompare(first.coherenceExponent(),
                              Meta::Measurement::DEFAULT_COHERENCE_EXPONENT));

        measurementSettings->reactValue<TestMeasurement, float>(
            key, &first, &TestMeasurement::coherenceExponentChanged, first.coherenceExponent());

        first.setCoherenceExponent(2.5f);
        QVERIFY(qFuzzyCompare(measurementSettings->value(key).toFloat(), 2.5f));
        root.flush();
    }

    {
        Settings root(groupName);
        Settings *measurementSettings = root.getSubGroup("/measurement");
        TestMeasurement second;
        second.setCoherenceExponent(
            measurementSettings->value(key, second.coherenceExponent()).toFloat());
        QVERIFY(qFuzzyCompare(second.coherenceExponent(), 2.5f));
    }
}

QTEST_APPLESS_MAIN(CoherenceExponentTest)

#include "tst_coherenceexponent.moc"

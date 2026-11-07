#include <QtTest>
#include <cmath>

#include "math/fouriertransform.h"

class LtwWindowsTest : public QObject
{
    Q_OBJECT

private slots:
    void grid_data();
    void grid();
};

void LtwWindowsTest::grid_data()
{
    QTest::addColumn<unsigned int>("pointsPerOctave");
    QTest::addColumn<unsigned int>("expectedBins");

    QTest::newRow("1 ppo") << 1u << 11u;
    QTest::newRow("24 ppo") << 24u << 240u;
    QTest::newRow("48 ppo") << 48u << 479u;
    QTest::newRow("96 ppo") << 96u << 958u;
}

void LtwWindowsTest::grid()
{
    QFETCH(unsigned int, pointsPerOctave);
    QFETCH(unsigned int, expectedBins);

    FourierTransform transform;
    transform.setType(FourierTransform::Log);
    transform.setSampleRate(48000);
    transform.setLogWindowPointsPerOctave(pointsPerOctave);
    transform.prepare();

    QCOMPARE(transform.logWindowPointsPerOctave(), pointsPerOctave);
    const auto frequencies = transform.getFrequencies();
    QCOMPARE(static_cast<unsigned int>(frequencies.size()), expectedBins);

    const unsigned int intervals = expectedBins - 1;
    const float frequencyRatio = 1000.f;
    const float fFactor = std::pow(frequencyRatio, 1.f / intervals);
    const float wFactor = std::pow(fFactor, -5.f / 6.f);
    const double baseWindow = static_cast<double>(1u << 16);

    for (unsigned int i = 0; i < expectedBins; ++i) {
        const unsigned int expectedLength =
            static_cast<unsigned int>(baseWindow * std::pow(wFactor, i));
        QCOMPARE(transform.logWindowLength(i), expectedLength);
    }
}

QTEST_APPLESS_MAIN(LtwWindowsTest)

#include "tst_ltwwindows.moc"

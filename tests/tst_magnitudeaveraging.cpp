#include <QtTest>
#include <cmath>

#include "math/averaging.h"
#include "math/complex.h"

class MagnitudeAveragingTest : public QObject
{
    Q_OBJECT

private slots:
    void alignedFramesHaveSameResult();
    void phaseRotatedFramesDiverge();
    void polarUsesScalarMagnitudeMean();
    void vectorUsesComplexSum();
};

/*
 * H = 10 + 0i and H = 1 + 0i: both averaging modes should return 5.5.
 */
void MagnitudeAveragingTest::alignedFramesHaveSameResult()
{
    Averaging<float> polar;
    polar.setSize(1);
    polar.setDepth(2);
    polar.append(0, Complex(10.f, 0.f).abs());
    polar.append(0, Complex(1.f, 0.f).abs());

    Averaging<Complex> vector;
    vector.setSize(1);
    vector.setDepth(2);
    vector.append(0, Complex(10.f, 0.f));
    vector.append(0, Complex(1.f, 0.f));

    const float polarMagnitude = polar.value(0);
    const float vectorMagnitude = vector.value(0).abs();

    QVERIFY(qAbs(polarMagnitude - 5.5f) < 1e-6f);
    QVERIFY(qAbs(vectorMagnitude - 5.5f) < 1e-6f);
}

/*
 * H = 10 + 0i and H = 0 + 1i: the scalar mean is 5.5, while the vector sum
 * is 5 + 0.5i and its magnitude is sqrt(25.25), about 5.02494.
 */
void MagnitudeAveragingTest::phaseRotatedFramesDiverge()
{
    const Complex first(10.f, 0.f);
    const Complex second(0.f, 1.f);

    Averaging<float> polar;
    polar.setSize(1);
    polar.setDepth(2);
    polar.append(0, first.abs());
    polar.append(0, second.abs());

    Averaging<Complex> vector;
    vector.setSize(1);
    vector.setDepth(2);
    vector.append(0, first);
    vector.append(0, second);

    const float polarMagnitude = polar.value(0);
    const float vectorMagnitude = vector.value(0).abs();

    QVERIFY(qAbs(polarMagnitude - 5.5f) < 1e-6f);
    QVERIFY(qAbs(vectorMagnitude - std::sqrt(25.25f)) < 1e-5f);
    QVERIFY(vectorMagnitude < polarMagnitude);
}

void MagnitudeAveragingTest::polarUsesScalarMagnitudeMean()
{
    Averaging<float> polar;
    polar.setSize(1);
    polar.setDepth(2);
    polar.append(0, 10.f);
    polar.append(0, 1.f);

    QVERIFY(qAbs(polar.value(0) - 5.5f) < 1e-6f);
}

void MagnitudeAveragingTest::vectorUsesComplexSum()
{
    Averaging<Complex> vector;
    vector.setSize(1);
    vector.setDepth(2);
    vector.append(0, Complex(10.f, 0.f));
    vector.append(0, Complex(0.f, 1.f));

    const Complex mean = vector.value(0);
    QVERIFY(qAbs(mean.real - 5.f) < 1e-6f);
    QVERIFY(qAbs(mean.imag - 0.5f) < 1e-6f);
}

QTEST_APPLESS_MAIN(MagnitudeAveragingTest)

#include "tst_magnitudeaveraging.moc"

#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUrl>

#include "common/settings.h"
#include "common/weightingcurvestore.h"
#include "common/workingfolder.h"
#include "math/weightingcurve.h"

class WeightingCurveTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void builtInCurves();
    void logarithmicInterpolation();
    void parsesTextFile();
    void parsesDocumentedExample();
    void parsesSemicolonAndDecimalComma();
    void rejectsCurveWithoutTwoPoints();
    void jsonRoundTrip();
    void storeImportsAndPersistsCurve();
    void storeRenamesAndRemovesCurves();
};

static QString writeCurve(QTemporaryDir &directory, const QString &name, const QString &contents)
{
    const QString path = directory.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    QTextStream stream(&file);
    stream << contents;
    return path;
}

void WeightingCurveTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("opensoundmeter-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("OpenSoundMeterWeightingCurveTest"));

    QSettings settings(workingfolder::settingsFilePath(), QSettings::IniFormat);
    settings.clear();
    settings.sync();
}

void WeightingCurveTest::cleanupTestCase()
{
    QSettings settings(workingfolder::settingsFilePath(), QSettings::IniFormat);
    settings.clear();
    settings.sync();
}

void WeightingCurveTest::builtInCurves()
{
    const Math::WeightingCurve a = Math::WeightingCurve::builtIn(QStringLiteral("A"));
    const Math::WeightingCurve b = Math::WeightingCurve::builtIn(QStringLiteral("B"));
    const Math::WeightingCurve c = Math::WeightingCurve::builtIn(QStringLiteral("C"));
    const Math::WeightingCurve z = Math::WeightingCurve::builtIn(QStringLiteral("Z"));

    QVERIFY(a.valid());
    QVERIFY(b.valid());
    QVERIFY(c.valid());
    QVERIFY(z.valid());
    QVERIFY(qAbs(a.gain(1000.0)) < 0.01);
    QVERIFY(qAbs(b.gain(1000.0)) < 0.01);
    QVERIFY(qAbs(c.gain(1000.0)) < 0.01);
    QCOMPARE(z.gain(1000.0), 0.0);
    QVERIFY(a.gain(100.0) < -15.0);
    QVERIFY(b.gain(100.0) > a.gain(100.0));
    QVERIFY(c.gain(100.0) > a.gain(100.0));
}

void WeightingCurveTest::logarithmicInterpolation()
{
    const Math::WeightingCurve curve(
        QStringLiteral("test"),
        {
            {100.0, -6.0},
            {1000.0, 0.0},
            {10000.0, 6.0},
        });

    QVERIFY(curve.valid());
    QVERIFY(qAbs(curve.gain(316.227766) - -3.0) < 0.01);
    QVERIFY(qAbs(curve.gain(3162.27766) - 3.0) < 0.01);
    QCOMPARE(curve.gain(10.0), -6.0);
    QCOMPARE(curve.gain(20000.0), 6.0);
}

void WeightingCurveTest::parsesTextFile()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = writeCurve(
        directory,
        QStringLiteral("custom curve.txt"),
        QStringLiteral(
            "# frequency, gain\n"
            "Frequency (Hz), Gain (dB)\n"
            "20, -20.0\n"
            "100, -6.0\n"
            "1000, 0.0\n"
            "10000, 6.0\n"));

    QString error;
    const Math::WeightingCurve curve = Math::WeightingCurve::fromFile(path, &error);
    QVERIFY2(curve.valid(), qPrintable(error));
    QCOMPARE(curve.name(), QStringLiteral("custom curve"));
    QCOMPARE(curve.points().size(), 4);
    QVERIFY(qAbs(curve.gain(316.227766) - -3.0) < 0.01);
}

void WeightingCurveTest::parsesDocumentedExample()
{
    const QString path = QFINDTESTDATA("../docs/examples/weighting-curve.txt");
    QVERIFY2(!path.isEmpty(), "Could not find docs/examples/weighting-curve.txt");

    QString error;
    const Math::WeightingCurve curve = Math::WeightingCurve::fromFile(path, &error);
    QVERIFY2(curve.valid(), qPrintable(error));
    QCOMPARE(curve.name(), QStringLiteral("weighting-curve"));
    QCOMPARE(curve.points().size(), 31);
    QVERIFY(qAbs(curve.points().first().frequency - 20.0) < 0.001);
    QVERIFY(qAbs(curve.points().last().frequency - 20000.0) < 0.001);
    QVERIFY(qAbs(curve.gain(1000.0)) < 0.01);
}

void WeightingCurveTest::parsesSemicolonAndDecimalComma()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = writeCurve(
        directory,
        QStringLiteral("euro.txt"),
        QStringLiteral(
            "20; -20,0\n"
            "1000; 0,0\n"
            "10000; 6,0\n"));

    QString error;
    const Math::WeightingCurve curve = Math::WeightingCurve::fromFile(path, &error);
    QVERIFY2(curve.valid(), qPrintable(error));
    QCOMPARE(curve.points().size(), 3);
    QVERIFY(qAbs(curve.gain(1000.0)) < 0.01);
}

void WeightingCurveTest::rejectsCurveWithoutTwoPoints()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = writeCurve(
        directory,
        QStringLiteral("bad.txt"),
        QStringLiteral("Frequency (Hz), Gain (dB)\n20, -20\n"));

    QString error;
    const Math::WeightingCurve curve = Math::WeightingCurve::fromFile(path, &error);
    QVERIFY(!curve.valid());
    QVERIFY(!error.isEmpty());
}

void WeightingCurveTest::jsonRoundTrip()
{
    const Math::WeightingCurve original(
        QStringLiteral("round trip"),
        {
            {20.0, -12.0},
            {1000.0, 0.0},
            {20000.0, 4.5},
        });

    const Math::WeightingCurve restored = Math::WeightingCurve::fromJson(original.toJson());
    QVERIFY(restored.valid());
    QCOMPARE(restored.name(), original.name());
    QCOMPARE(restored.points().size(), original.points().size());
    QVERIFY(qAbs(restored.gain(1000.0) - original.gain(1000.0)) < 0.0001);
}

void WeightingCurveTest::storeImportsAndPersistsCurve()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = writeCurve(
        directory,
        QStringLiteral("imported curve.txt"),
        QStringLiteral(
            "Frequency (Hz), Gain (dB)\n"
            "20, -18\n"
            "1000, 0\n"
            "20000, 4\n"));

    WeightingCurveStore *store = WeightingCurveStore::instance();
    QVERIFY(store->contains(QStringLiteral("Z")));

    const QString imported = store->importCurve(QUrl::fromLocalFile(path));
    QCOMPARE(imported, QStringLiteral("imported curve"));
    QVERIFY(store->names().contains(imported));
    QCOMPARE(store->curve(imported).points().size(), 3);

    const QString duplicate = store->importCurve(QUrl::fromLocalFile(path));
    QCOMPARE(duplicate, QStringLiteral("imported curve (2)"));

    Settings settings(QStringLiteral("weighting"));
    const QString persisted = settings.value(QStringLiteral("curves")).toString();
    QVERIFY(persisted.contains(QStringLiteral("imported curve")));
    QVERIFY(persisted.contains(QStringLiteral("imported curve (2)")));
}

void WeightingCurveTest::storeRenamesAndRemovesCurves()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = writeCurve(
        directory,
        QStringLiteral("rename me.txt"),
        QStringLiteral(
            "Frequency (Hz), Gain (dB)\n"
            "20, -16\n"
            "1000, 0\n"
            "20000, 3\n"));

    WeightingCurveStore *store = WeightingCurveStore::instance();
    const QString imported = store->importCurve(QUrl::fromLocalFile(path));
    QCOMPARE(imported, QStringLiteral("rename me"));
    QVERIFY(store->customCurves().contains(imported));
    QVERIFY(!store->customCurves().contains(QStringLiteral("Z")));

    QVERIFY(store->renameCurve(imported, QStringLiteral("   ")).isEmpty());
    QVERIFY(!store->lastError().isEmpty());
    QVERIFY(store->renameCurve(QStringLiteral("A"), QStringLiteral("flat")).isEmpty());
    QVERIFY(store->renameCurve(imported, QStringLiteral("Z")).isEmpty());

    const QString renamed = store->renameCurve(imported, QStringLiteral("renamed curve"));
    QCOMPARE(renamed, QStringLiteral("renamed curve"));
    QVERIFY(!store->contains(imported));
    QVERIFY(store->contains(renamed));
    QCOMPARE(store->curve(renamed).points().size(), 3);

    QVERIFY(!store->removeCurve(QStringLiteral("Z")));
    QVERIFY(store->removeCurve(renamed));
    QVERIFY(!store->contains(renamed));
    QVERIFY(!store->customCurves().contains(renamed));

    Settings settings(QStringLiteral("weighting"));
    QVERIFY(!settings.value(QStringLiteral("curves")).toString().contains(renamed));
}

QTEST_MAIN(WeightingCurveTest)

#include "tst_weightingcurve.moc"

#include <QtTest>
#include <QApplication>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUrl>

#include "chart/variablechart.h"
#include "common/settings.h"
#include "common/weightingcurvestore.h"
#include "common/workingfolder.h"

class ChartSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void rangesSurviveSwitchBetweenTypes();
    void firstVisitInheritsFrequencyRange();
    void savedValuesSurviveNewChart();
    void typesWithSpecialNamesRestore();
    void speechIntelligibilityTypeRestores();
    void weightingFollowsRenameAndRemoval();

private:
    QString testGroup(const char *name) const;
    void verifyRange(QObject *plot, float xmin, float xmax, float ymin, float ymax);
};

static float value(QObject *plot, const char *property)
{
    return plot->property(property).toFloat();
}

static void setRange(QObject *plot, float xmin, float xmax, float ymin, float ymax)
{
    plot->setProperty("xmin", xmin);
    plot->setProperty("xmax", xmax);
    plot->setProperty("ymin", ymin);
    plot->setProperty("ymax", ymax);
}

QString ChartSettingsTest::testGroup(const char *name) const
{
    return QString("layout/charts/test_%1").arg(name);
}

void ChartSettingsTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName("opensoundmeter-tests");
    QCoreApplication::setApplicationName("OpenSoundMeterChartSettingsTest");

    QSettings settings(workingfolder::settingsFilePath(), QSettings::IniFormat);
    settings.clear();
    settings.sync();
}

void ChartSettingsTest::cleanupTestCase()
{
    QSettings settings(workingfolder::settingsFilePath(), QSettings::IniFormat);
    settings.clear();
    settings.sync();
}

void ChartSettingsTest::verifyRange(QObject *plot, float xmin, float xmax, float ymin, float ymax)
{
    QVERIFY2(plot, "plot is null");
    QVERIFY2(qAbs(value(plot, "xmin") - xmin) < 0.01f,
             qPrintable(QString("xmin: expected %1, got %2").arg(xmin).arg(value(plot, "xmin"))));
    QVERIFY2(qAbs(value(plot, "xmax") - xmax) < 0.01f,
             qPrintable(QString("xmax: expected %1, got %2").arg(xmax).arg(value(plot, "xmax"))));
    QVERIFY2(qAbs(value(plot, "ymin") - ymin) < 0.01f,
             qPrintable(QString("ymin: expected %1, got %2").arg(ymin).arg(value(plot, "ymin"))));
    QVERIFY2(qAbs(value(plot, "ymax") - ymax) < 0.01f,
             qPrintable(QString("ymax: expected %1, got %2").arg(ymax).arg(value(plot, "ymax"))));
}

void ChartSettingsTest::rangesSurviveSwitchBetweenTypes()
{
    Settings settings(testGroup("ranges_survive_switch"));
    Chart::VariableChart chart;
    chart.setTypeByString("Magnitude");
    chart.setSettings(&settings);

    setRange(chart.plot(), 35.f, 18000.f, -12.f, 9.f);
    verifyRange(chart.plot(), 35.f, 18000.f, -12.f, 9.f);

    chart.setTypeByString("Phase");
    QObject *phase = chart.plot();
    QVERIFY(phase);
    phase->setProperty("xmin", 45.f);
    phase->setProperty("xmax", 16000.f);
    const float phaseYmin = value(phase, "ymin");
    const float phaseYmax = value(phase, "ymax");

    chart.setTypeByString("Magnitude");
    verifyRange(chart.plot(), 35.f, 18000.f, -12.f, 9.f);

    chart.setTypeByString("Phase");
    verifyRange(chart.plot(), 45.f, 16000.f, phaseYmin, phaseYmax);
}

void ChartSettingsTest::firstVisitInheritsFrequencyRange()
{
    Settings settings(testGroup("first_visit_inherits"));
    Chart::VariableChart chart;
    chart.setTypeByString("Magnitude");
    chart.setSettings(&settings);

    setRange(chart.plot(), 50.f, 12000.f, -15.f, 6.f);

    chart.setTypeByString("Spectrum");
    QObject *spectrum = chart.plot();
    QVERIFY(spectrum);
    QVERIFY2(qAbs(value(spectrum, "xmin") - 50.f) < 0.01f, "new type should inherit current xmin");
    QVERIFY2(qAbs(value(spectrum, "xmax") - 12000.f) < 0.01f, "new type should inherit current xmax");

    setRange(spectrum, 50.f, 12000.f, -90.f, -5.f);

    chart.setTypeByString("Magnitude");
    verifyRange(chart.plot(), 50.f, 12000.f, -15.f, 6.f);

    chart.setTypeByString("Spectrum");
    verifyRange(chart.plot(), 50.f, 12000.f, -90.f, -5.f);
}

void ChartSettingsTest::savedValuesSurviveNewChart()
{
    Settings settings(testGroup("new_chart"));

    {
        Chart::VariableChart chart;
        chart.setSettings(&settings);
        chart.setTypeByString("Magnitude");
        setRange(chart.plot(), 31.f, 17000.f, -9.f, 7.f);

        chart.setTypeByString("Spectrum");
        setRange(chart.plot(), 31.f, 17000.f, -80.f, -2.f);
    }

    settings.flush();

    {
        Chart::VariableChart chart;
        chart.setTypeByString("Spectrum");
        chart.setSettings(&settings);
        verifyRange(chart.plot(), 31.f, 17000.f, -80.f, -2.f);

        chart.setTypeByString("Magnitude");
        verifyRange(chart.plot(), 31.f, 17000.f, -9.f, 7.f);
    }
}

void ChartSettingsTest::typesWithSpecialNamesRestore()
{
    Settings settings(testGroup("special_names"));
    Chart::VariableChart chart;
    chart.setTypeByString("Crest Factor");
    chart.setSettings(&settings);

    setRange(chart.plot(), 40.f, 15000.f, 3.f, 30.f);

    chart.setTypeByString("Group Delay");
    QObject *groupDelay = chart.plot();
    QVERIFY(groupDelay);
    groupDelay->setProperty("xmin", 50.f);
    groupDelay->setProperty("xmax", 14000.f);
    const float groupDelayYmin = value(groupDelay, "ymin");
    const float groupDelayYmax = value(groupDelay, "ymax");

    chart.setTypeByString("Crest Factor");
    verifyRange(chart.plot(), 40.f, 15000.f, 3.f, 30.f);

    chart.setTypeByString("Group Delay");
    verifyRange(chart.plot(), 50.f, 14000.f, groupDelayYmin, groupDelayYmax);
}

void ChartSettingsTest::speechIntelligibilityTypeRestores()
{
    Settings settings(testGroup("speech_intelligibility"));
    Chart::VariableChart chart;
    chart.setSettings(&settings);
    chart.setTypeByString(QStringLiteral("Speech Intelligibility"));

    QCOMPARE(chart.typeString(), QStringLiteral("Speech Intelligibility"));
    QVERIFY(chart.plot());
    QVERIFY(!chart.plot()->property("valid").toBool());
    QVERIFY(!chart.plot()->property("error").toString().isEmpty());

    chart.setTypeByString(QStringLiteral("Magnitude"));
    QCOMPARE(chart.typeString(), QStringLiteral("Magnitude"));
    chart.setTypeByString(QStringLiteral("Speech Intelligibility"));
    QCOMPARE(chart.typeString(), QStringLiteral("Speech Intelligibility"));
}

void ChartSettingsTest::weightingFollowsRenameAndRemoval()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString path = directory.filePath(QStringLiteral("plot curve.txt"));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << "20, -18\n"
           << "1000, 0\n"
           << "20000, 3\n";
    file.close();

    WeightingCurveStore *store = WeightingCurveStore::instance();
    const QString curve = store->importCurve(QUrl::fromLocalFile(path));
    QCOMPARE(curve, QStringLiteral("plot curve"));

    Settings settings(testGroup("weighting_follows"));
    Chart::VariableChart chart;
    chart.setTypeByString(QStringLiteral("Spectrum"));
    chart.setSettings(&settings);
    QObject *plot = chart.plot();
    QVERIFY(plot);

    plot->setProperty("curve", curve);
    QCOMPARE(plot->property("curve").toString(), curve);

    const QString renamed = store->renameCurve(curve, QStringLiteral("plot renamed"));
    QCOMPARE(renamed, QStringLiteral("plot renamed"));
    QCOMPARE(plot->property("curve").toString(), renamed);

    QVERIFY(store->removeCurve(renamed));
    QCOMPARE(plot->property("curve").toString(), QStringLiteral("Z"));
}

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }

    QApplication app(argc, argv);
    ChartSettingsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_chartsettings.moc"

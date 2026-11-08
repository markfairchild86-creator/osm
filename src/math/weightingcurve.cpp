/**
 *  OSM
 *  Copyright (C) 2026  Open Sound Meter contributors

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
#include "weightingcurve.h"

#include <algorithm>
#include <cmath>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTextStream>

namespace Math {

namespace {

constexpr double F1 = 20.598997;
constexpr double F2 = 107.65265;
constexpr double F3 = 737.86;
constexpr double F4 = 12194.217;
constexpr double F5 = 158.489032;

QString builtInName(WeightingCurve::BuiltIn type)
{
    switch (type) {
    case WeightingCurve::A:
        return QStringLiteral("A");
    case WeightingCurve::B:
        return QStringLiteral("B");
    case WeightingCurve::C:
        return QStringLiteral("C");
    case WeightingCurve::Z:
        return QStringLiteral("Z");
    case WeightingCurve::Custom:
        break;
    }
    return {};
}

WeightingCurve::BuiltIn typeByName(const QString &name)
{
    if (name == QStringLiteral("A")) {
        return WeightingCurve::A;
    }
    if (name == QStringLiteral("B")) {
        return WeightingCurve::B;
    }
    if (name == QStringLiteral("C")) {
        return WeightingCurve::C;
    }
    return WeightingCurve::Z;
}

bool parseNumber(const QString &text, double *value)
{
    static const QRegularExpression numberPattern(
        QStringLiteral("^\\s*([+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?)"));

    const auto match = numberPattern.match(text);
    if (!match.hasMatch()) {
        return false;
    }

    bool ok = false;
    const double parsed = match.captured(1).toDouble(&ok);
    if (!ok || !std::isfinite(parsed)) {
        return false;
    }

    *value = parsed;
    return true;
}

QVector<WeightingCurve::Point> normalizePoints(QVector<WeightingCurve::Point> points)
{
    points.erase(
        std::remove_if(points.begin(), points.end(), [](const auto &point) {
            return !std::isfinite(point.frequency) || !std::isfinite(point.gain) || point.frequency <= 0.0;
        }),
        points.end());

    std::sort(points.begin(), points.end(), [](const auto &left, const auto &right) {
        return left.frequency < right.frequency;
    });

    QVector<WeightingCurve::Point> normalized;
    normalized.reserve(points.size());
    for (const auto &point : points) {
        if (!normalized.isEmpty() &&
            std::abs(point.frequency - normalized.last().frequency) <=
            1e-9 * std::max(1.0, std::abs(point.frequency))) {
            normalized.last() = point;
        } else {
            normalized.append(point);
        }
    }
    return normalized;
}

} // namespace

WeightingCurve::WeightingCurve() :
    m_builtIn(Custom)
{
}

WeightingCurve::WeightingCurve(BuiltIn type) :
    m_name(builtInName(type)), m_builtIn(type)
{
    if (type == Custom) {
        m_name.clear();
    }
}

WeightingCurve::WeightingCurve(const QString &name, const QVector<Point> &points) :
    m_name(name.trimmed()), m_builtIn(Custom), m_points(normalizePoints(points))
{
}

WeightingCurve WeightingCurve::builtIn(const QString &name)
{
    const QString normalized = name.trimmed().toUpper();
    if (!isBuiltIn(normalized)) {
        return {};
    }
    return WeightingCurve(typeByName(normalized));
}

bool WeightingCurve::isBuiltIn(const QString &name)
{
    const QString normalized = name.trimmed().toUpper();
    return normalized == QStringLiteral("A") ||
           normalized == QStringLiteral("B") ||
           normalized == QStringLiteral("C") ||
           normalized == QStringLiteral("Z");
}

QVector<WeightingCurve::Point> WeightingCurve::builtInPoints(BuiltIn type)
{
    QVector<Point> points;
    points.reserve(34);
    for (int i = 0; i < 34; ++i) {
        const double frequency = std::pow(10.0, 0.1 * (i + 10));
        points.append({frequency, builtInGain(type, frequency)});
    }
    return points;
}

double WeightingCurve::builtInGain(BuiltIn type, double frequency)
{
    frequency = std::max(1e-6, frequency);
    const double f2 = frequency * frequency;

    switch (type) {
    case A: {
        const double magnitude = std::pow(10.0, 1.997 / 20.0) *
                                 (F4 * F4 * f2 * f2 /
                                  ((f2 + F1 * F1) *
                                   std::sqrt(f2 + F2 * F2) *
                                   std::sqrt(f2 + F3 * F3) *
                                   (f2 + F4 * F4)));
        return 20.0 * std::log10(magnitude);
    }
    case B: {
        const double magnitude = std::pow(10.0, 0.1696 / 20.0) *
                                 (F4 * F4 * f2 * frequency /
                                  ((f2 + F1 * F1) *
                                   std::sqrt(f2 + F5 * F5) *
                                   (f2 + F4 * F4)));
        return 20.0 * std::log10(magnitude);
    }
    case C: {
        const double magnitude = std::pow(10.0, 0.0619 / 20.0) *
                                 (F4 * F4 * f2 /
                                  ((f2 + F1 * F1) *
                                   (f2 + F4 * F4)));
        return 20.0 * std::log10(magnitude);
    }
    case Z:
    case Custom:
        break;
    }
    return 0.0;
}

WeightingCurve WeightingCurve::fromFile(const QString &fileName, QString *error)
{
    if (error) {
        error->clear();
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Could not open \"%1\": %2").arg(fileName, file.errorString());
        }
        return {};
    }

    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif

    QVector<Point> points;
    int lineNumber = 0;
    while (!stream.atEnd()) {
        ++lineNumber;
        QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('*')) {
            continue;
        }

        const int hashComment = line.indexOf('#');
        if (hashComment >= 0) {
            line.truncate(hashComment);
        }
        const int slashComment = line.indexOf(QStringLiteral("//"));
        if (slashComment >= 0) {
            line.truncate(slashComment);
        }
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        QStringList fields;
        if (line.contains(';')) {
            fields = line.split(';', Qt::SkipEmptyParts);
            for (auto &field : fields) {
                field.replace(',', '.');
            }
        } else if (line.contains(',')) {
            fields = line.split(',', Qt::SkipEmptyParts);
        } else {
            fields = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        }

        if (fields.size() < 2) {
            continue;
        }

        double frequency = 0.0;
        double gain = 0.0;
        if (!parseNumber(fields.at(0), &frequency) || !parseNumber(fields.at(1), &gain)) {
            // Header and unit rows are common in exported calibration data.
            continue;
        }

        points.append({frequency, gain});
    }

    points = normalizePoints(points);
    if (points.size() < 2) {
        if (error) {
            *error = QStringLiteral(
                "No usable weighting curve was found in \"%1\". "
                "Use two numeric columns: frequency in Hz and gain in dB."
            ).arg(QFileInfo(fileName).fileName());
        }
        return {};
    }

    QString name = QFileInfo(fileName).completeBaseName().trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("Custom");
    }
    return WeightingCurve(name, points);
}

QString WeightingCurve::name() const
{
    return m_name;
}

bool WeightingCurve::valid() const
{
    return !m_name.isEmpty() && (m_builtIn != Custom || m_points.size() >= 2);
}

bool WeightingCurve::isCustom() const
{
    return m_builtIn == Custom;
}

double WeightingCurve::gain(double frequency) const noexcept
{
    if (m_builtIn != Custom) {
        return builtInGain(m_builtIn, frequency);
    }

    if (m_points.isEmpty()) {
        return 0.0;
    }
    if (frequency <= m_points.first().frequency) {
        return m_points.first().gain;
    }
    if (frequency >= m_points.last().frequency) {
        return m_points.last().gain;
    }

    const auto upper = std::lower_bound(
        m_points.cbegin(), m_points.cend(), frequency, [](const Point &point, double value) {
            return point.frequency < value;
        });
    if (upper == m_points.cbegin()) {
        return upper->gain;
    }
    if (upper == m_points.cend()) {
        return m_points.last().gain;
    }

    const Point &right = *upper;
    const Point &left = *(upper - 1);
    const double position = std::log(frequency / left.frequency) /
                            std::log(right.frequency / left.frequency);
    return left.gain + (right.gain - left.gain) * position;
}

const QVector<WeightingCurve::Point> &WeightingCurve::points() const
{
    return m_points;
}

QJsonObject WeightingCurve::toJson() const
{
    QJsonArray points;
    for (const auto &point : m_points) {
        points.append(QJsonArray{point.frequency, point.gain});
    }

    QJsonObject object;
    object[QStringLiteral("name")] = m_name;
    object[QStringLiteral("points")] = points;
    return object;
}

WeightingCurve WeightingCurve::fromJson(const QJsonObject &object)
{
    const QString name = object.value(QStringLiteral("name")).toString().trimmed();
    const QJsonArray pointArray = object.value(QStringLiteral("points")).toArray();
    QVector<Point> points;
    points.reserve(pointArray.size());

    for (const auto &value : pointArray) {
        const QJsonArray pair = value.toArray();
        if (pair.size() < 2) {
            continue;
        }
        points.append({pair.at(0).toDouble(), pair.at(1).toDouble()});
    }

    return WeightingCurve(name, points);
}

} // namespace Math

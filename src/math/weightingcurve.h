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
#ifndef MATH_WEIGHTINGCURVE_H
#define MATH_WEIGHTINGCURVE_H

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Math {

class WeightingCurve
{
public:
    enum BuiltIn {
        A = 0,
        B = 1,
        C = 2,
        Z = 3,
        Custom = 4
    };

    struct Point {
        double frequency;
        double gain;
    };

    WeightingCurve();
    explicit WeightingCurve(BuiltIn type);
    WeightingCurve(const QString &name, const QVector<Point> &points);

    static WeightingCurve builtIn(const QString &name);
    static bool isBuiltIn(const QString &name);
    static QVector<Point> builtInPoints(BuiltIn type);
    static double builtInGain(BuiltIn type, double frequency);

    static WeightingCurve fromFile(const QString &fileName, QString *error = nullptr);

    QString name() const;
    bool valid() const;
    bool isCustom() const;
    double gain(double frequency) const noexcept;
    const QVector<Point> &points() const;

    QJsonObject toJson() const;
    static WeightingCurve fromJson(const QJsonObject &object);

private:
    QString m_name;
    BuiltIn m_builtIn;
    QVector<Point> m_points;
};

} // namespace Math

#endif // MATH_WEIGHTINGCURVE_H

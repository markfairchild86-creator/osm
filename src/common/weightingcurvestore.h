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
#ifndef WEIGHTINGCURVESTORE_H
#define WEIGHTINGCURVESTORE_H

#include <QMap>
#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QUrl>

#include "math/weightingcurve.h"

class WeightingCurveStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant availableCurves READ availableCurves NOTIFY curvesChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    static WeightingCurveStore *instance();

    QVariant availableCurves() const;
    QStringList names() const;
    QStringList customCurves() const;
    bool contains(const QString &name) const;
    Math::WeightingCurve curve(const QString &name) const;

    Q_INVOKABLE QString importCurve(const QUrl &url);
    Q_INVOKABLE QString renameCurve(const QString &oldName, const QString &newName);
    Q_INVOKABLE bool removeCurve(const QString &name);
    QString lastError() const;

signals:
    void curvesChanged();
    void curveRenamed(const QString &oldName, const QString &newName);
    void curveRemoved(const QString &name);
    void lastErrorChanged();

private:
    explicit WeightingCurveStore(QObject *parent = nullptr);

    void load();
    void save() const;
    QString uniqueName(const QString &name) const;
    void setLastError(const QString &error);

    QMap<QString, Math::WeightingCurve> m_curves;
    QString m_lastError;
};

#endif // WEIGHTINGCURVESTORE_H

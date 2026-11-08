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
#include "weightingcurvestore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "common/settings.h"

namespace {

WeightingCurveStore *s_instance = nullptr;

const QStringList builtInNames = {
    QStringLiteral("A"),
    QStringLiteral("B"),
    QStringLiteral("C"),
    QStringLiteral("Z"),
};

} // namespace

WeightingCurveStore *WeightingCurveStore::instance()
{
    if (!s_instance) {
        s_instance = new WeightingCurveStore();
    }
    return s_instance;
}

WeightingCurveStore::WeightingCurveStore(QObject *parent) :
    QObject(parent)
{
    for (const auto &name : builtInNames) {
        m_curves.insert(name, Math::WeightingCurve::builtIn(name));
    }
    load();
}

QVariant WeightingCurveStore::availableCurves() const
{
    return names();
}

QStringList WeightingCurveStore::names() const
{
    QStringList result = builtInNames;
    result.append(customCurves());
    return result;
}

QStringList WeightingCurveStore::customCurves() const
{
    QStringList result;
    for (auto it = m_curves.cbegin(); it != m_curves.cend(); ++it) {
        if (it.value().isCustom()) {
            result.append(it.key());
        }
    }
    return result;
}

bool WeightingCurveStore::contains(const QString &name) const
{
    return m_curves.contains(name);
}

Math::WeightingCurve WeightingCurveStore::curve(const QString &name) const
{
    auto it = m_curves.constFind(name);
    if (it != m_curves.cend()) {
        return it.value();
    }
    return m_curves.value(QStringLiteral("Z"));
}

QString WeightingCurveStore::importCurve(const QUrl &url)
{
    QString error;
    const QString fileName = url.isLocalFile() ? url.toLocalFile() : url.toString();
    Math::WeightingCurve curve = Math::WeightingCurve::fromFile(fileName, &error);
    if (!curve.valid()) {
        setLastError(error);
        return {};
    }

    const QString name = uniqueName(curve.name());
    curve = Math::WeightingCurve(name, curve.points());
    m_curves.insert(name, curve);
    save();

    setLastError({});
    emit curvesChanged();
    return name;
}

QString WeightingCurveStore::renameCurve(const QString &oldName, const QString &newName)
{
    const QString trimmedName = newName.trimmed();
    auto it = m_curves.find(oldName);
    if (it == m_curves.end()) {
        setLastError(QStringLiteral("The curve \"%1\" no longer exists.").arg(oldName));
        return {};
    }
    if (!it.value().isCustom()) {
        setLastError(QStringLiteral("Only imported curves can be renamed."));
        return {};
    }
    if (trimmedName.isEmpty()) {
        setLastError(QStringLiteral("Enter a name for the curve."));
        return {};
    }
    if (trimmedName == oldName) {
        setLastError({});
        return oldName;
    }
    if (Math::WeightingCurve::isBuiltIn(trimmedName)) {
        setLastError(QStringLiteral("\"%1\" is a built-in curve name.").arg(trimmedName));
        return {};
    }
    if (m_curves.contains(trimmedName)) {
        setLastError(QStringLiteral("A curve named \"%1\" already exists.").arg(trimmedName));
        return {};
    }

    const Math::WeightingCurve curve = it.value();
    m_curves.erase(it);
    m_curves.insert(trimmedName, Math::WeightingCurve(trimmedName, curve.points()));
    save();

    setLastError({});
    emit curvesChanged();
    emit curveRenamed(oldName, trimmedName);
    return trimmedName;
}

bool WeightingCurveStore::removeCurve(const QString &name)
{
    auto it = m_curves.find(name);
    if (it == m_curves.end()) {
        setLastError(QStringLiteral("The curve \"%1\" no longer exists.").arg(name));
        return false;
    }
    if (!it.value().isCustom()) {
        setLastError(QStringLiteral("Only imported curves can be removed."));
        return false;
    }

    m_curves.erase(it);
    save();

    setLastError({});
    emit curvesChanged();
    emit curveRemoved(name);
    return true;
}

QString WeightingCurveStore::lastError() const
{
    return m_lastError;
}

void WeightingCurveStore::load()
{
    Settings settings(QStringLiteral("weighting"));
    const QString json = settings.value(QStringLiteral("curves")).toString();
    if (json.isEmpty()) {
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        return;
    }

    for (const auto &value : document.array()) {
        const Math::WeightingCurve curve = Math::WeightingCurve::fromJson(value.toObject());
        if (!curve.valid() || m_curves.contains(curve.name())) {
            continue;
        }
        m_curves.insert(curve.name(), curve);
    }
}

void WeightingCurveStore::save() const
{
    QJsonArray curves;
    for (auto it = m_curves.cbegin(); it != m_curves.cend(); ++it) {
        if (it.value().isCustom()) {
            curves.append(it.value().toJson());
        }
    }

    const QJsonDocument document(curves);
    Settings settings(QStringLiteral("weighting"));
    settings.setValue(
        QStringLiteral("curves"),
        QString::fromUtf8(document.toJson(QJsonDocument::Compact)));
    settings.flush();
}

QString WeightingCurveStore::uniqueName(const QString &name) const
{
    QString base = name.trimmed();
    if (base.isEmpty()) {
        base = QStringLiteral("Custom");
    }

    QString candidate = base;
    int suffix = 2;
    while (m_curves.contains(candidate)) {
        candidate = QStringLiteral("%1 (%2)").arg(base).arg(suffix++);
    }
    return candidate;
}

void WeightingCurveStore::setLastError(const QString &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

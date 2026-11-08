/**
 *  OSM
 *  Copyright (C) 2019  Pavel Smokotnin

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
#ifndef FREQUENCYBASEDPLOT_H
#define FREQUENCYBASEDPLOT_H

#include "xyplot.h"
#include "math/weightingcurve.h"
#include <QUrl>
namespace Chart {
class FrequencyBasedPlot : public XYPlot
{
    Q_OBJECT
    Q_PROPERTY(unsigned int pointsPerOctave READ pointsPerOctave WRITE setPointsPerOctave NOTIFY
               pointsPerOctaveChanged)
    Q_PROPERTY(bool coherence READ coherence WRITE setCoherence NOTIFY coherenceChanged)
    Q_PROPERTY(float coherenceThreshold READ coherenceThreshold WRITE setCoherenceThreshold NOTIFY
               coherenceThresholdChanged)
    Q_PROPERTY(QVariant availableCurves READ getAvailableCurves NOTIFY availableCurvesChanged)
    Q_PROPERTY(QVariant customCurves READ getCustomCurves NOTIFY availableCurvesChanged)
    Q_PROPERTY(QString curve READ curveName WRITE setCurve NOTIFY curveChanged)
    Q_PROPERTY(QString curveError READ curveError NOTIFY curveErrorChanged)

public:
    FrequencyBasedPlot(Settings *settings, QQuickItem *parent = Q_NULLPTR);

    virtual void setSettings(Settings *settings) noexcept override;
    virtual void storeSettings() noexcept override;

    unsigned int pointsPerOctave() const noexcept;
    void setPointsPerOctave(unsigned int p) noexcept;

    bool coherence() const noexcept;
    void setCoherence(bool coherence) noexcept;

    float coherenceThreshold() const noexcept;
    void setCoherenceThreshold(float coherenceThreshold) noexcept;

    QVariant getAvailableCurves() const;
    QVariant getCustomCurves() const;
    QString curveName() const;
    void setCurve(const QString &curve);
    const Math::WeightingCurve &weightingCurve() const noexcept;
    Q_INVOKABLE QString importCurve(const QUrl &url);
    Q_INVOKABLE QString renameCurve(const QString &oldName, const QString &newName);
    Q_INVOKABLE bool removeCurve(const QString &name);
    QString curveError() const;

signals:
    void pointsPerOctaveChanged(unsigned int);
    void coherenceChanged(bool);
    void coherenceThresholdChanged(float);
    void availableCurvesChanged();
    void curveChanged(QString);
    void curveErrorChanged();

protected:
    void configureXAxis();
    virtual bool isPointsPerOctaveValid(unsigned int &value) const;

    unsigned int m_pointsPerOctave;
    float m_coherenceThreshold;
    bool m_coherence;
    Math::WeightingCurve m_weightingCurve;
    QString m_curveError;

private:
    void setCurveError(const QString &error);
};
}
#endif // FREQUENCYBASEDPLOT_H

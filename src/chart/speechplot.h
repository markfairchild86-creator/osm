/**
 *  OSM
 *  Copyright (C) 2026  Pavel Smokotnin

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
#ifndef CHART_SPEECHPLOT_H
#define CHART_SPEECHPLOT_H

#include <QTimer>
#include "plot.h"
#include "math/speechintelligibility.h"

class SourceList;

namespace Chart {

class SpeechPlot : public Plot
{
    Q_OBJECT

    Q_PROPERTY(QString sourceName READ sourceName NOTIFY resultChanged)
    Q_PROPERTY(QString status READ status NOTIFY resultChanged)
    Q_PROPERTY(QString error READ error NOTIFY resultChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY resultChanged)
    Q_PROPERTY(bool shortImpulse READ shortImpulse NOTIFY resultChanged)
    Q_PROPERTY(bool correctionsEnabled READ correctionsEnabled WRITE setCorrectionsEnabled NOTIFY correctionsChanged)
    Q_PROPERTY(QString signalLevels READ signalLevels WRITE setSignalLevels NOTIFY correctionsChanged)
    Q_PROPERTY(QString noiseLevels READ noiseLevels WRITE setNoiseLevels NOTIFY correctionsChanged)
    Q_PROPERTY(float sti READ sti NOTIFY resultChanged)
    Q_PROPERTY(float stipa READ stipa NOTIFY resultChanged)
    Q_PROPERTY(float rasti READ rasti NOTIFY resultChanged)
    Q_PROPERTY(QString stiRating READ stiRating NOTIFY resultChanged)
    Q_PROPERTY(QString stipaRating READ stipaRating NOTIFY resultChanged)
    Q_PROPERTY(QString rastiRating READ rastiRating NOTIFY resultChanged)

public:
    explicit SpeechPlot(Settings *settings, QQuickItem *parent = nullptr);

    void connectSources(SourceList *sourceList) override;

    QString sourceName() const;
    QString status() const;
    QString error() const;
    bool valid() const;
    bool shortImpulse() const;
    bool correctionsEnabled() const;
    QString signalLevels() const;
    QString noiseLevels() const;
    void setCorrectionsEnabled(bool enabled);
    void setSignalLevels(const QString &levels);
    void setNoiseLevels(const QString &levels);
    float sti() const;
    float stipa() const;
    float rasti() const;
    QString stiRating() const;
    QString stipaRating() const;
    QString rastiRating() const;

    const Speech::Calculator::Result &result() const;
    QColor sourceColor() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setHelper(qreal x, qreal y) noexcept override;
    Q_INVOKABLE void unsetHelper() noexcept override;
    Q_INVOKABLE qreal x2v(qreal x) const noexcept override;
    Q_INVOKABLE qreal y2v(qreal y) const noexcept override;
    Q_INVOKABLE qreal v2x(qreal value) const noexcept override;
    Q_INVOKABLE qreal v2y(qreal value) const noexcept override;
    Q_INVOKABLE void beginGesture();
    Q_INVOKABLE void applyGesture(QPointF base, QPointF move, QPointF scale);
    Q_INVOKABLE void resetAxis();

    QString xLabel() const override;
    QString yLabel() const override;
    void setSettings(Settings *settings) noexcept override;
    void storeSettings() noexcept override;
    void inheritSettings(const Plot *source) override;

signals:
    void resultChanged();
    void correctionsChanged();

protected:
    SeriesItem *createSeriesFromSource(const Shared::Source &source) override;

private:
    class Content;
    void compute();
    void updateCorrection();

    SourceList *m_sources;
    Shared::Source m_source;
    Speech::Calculator::Result m_result;
    Speech::Calculator::Correction m_correction;
    QTimer m_timer;
    Content *m_content;
    QString m_sourceName;
    QColor m_sourceColor;
    bool m_correctionsEnabled;
    QString m_signalLevels;
    QString m_noiseLevels;
    QString m_correctionError;
};

} // namespace Chart

#endif // CHART_SPEECHPLOT_H

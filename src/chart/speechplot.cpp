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
#include "speechplot.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QPainter>
#include <QRegularExpression>

#include "sourcelist.h"
#include "abstract/source.h"

using namespace Chart;

namespace {

QColor ratingColor(const QString &rating)
{
    if (rating == QStringLiteral("A+") || rating == QStringLiteral("A") || rating == QStringLiteral("B")) {
        return QColor(QStringLiteral("#43A047"));
    }
    if (rating == QStringLiteral("C") || rating == QStringLiteral("D") || rating == QStringLiteral("E")) {
        return QColor(QStringLiteral("#F9A825"));
    }
    if (rating == QStringLiteral("F") || rating == QStringLiteral("G") || rating == QStringLiteral("H")) {
        return QColor(QStringLiteral("#EF6C00"));
    }
    return QColor(QStringLiteral("#C62828"));
}

QString displayValue(float value)
{
    return QString::number(static_cast<double>(value), 'f', 2);
}

bool parseBandLevels(const QString &text,
                     std::array<float, Speech::Calculator::BandCount> &levels,
                     QString &error)
{
    const QStringList parts = text.split(QRegularExpression(QStringLiteral("[,;\\s]+")),
                                         Qt::SkipEmptyParts);
    if (parts.size() != Speech::Calculator::BandCount) {
        error = QStringLiteral("expected seven values for 125 Hz to 8 kHz");
        return false;
    }

    for (int i = 0; i < Speech::Calculator::BandCount; ++i) {
        bool ok = false;
        const float level = parts[i].toFloat(&ok);
        if (!ok || !std::isfinite(level)) {
            error = QStringLiteral("invalid number: %1").arg(parts[i]);
            return false;
        }
        levels[i] = level;
    }
    return true;
}

} // namespace

class SpeechPlot::Content : public PaintedItem
{
public:
    explicit Content(SpeechPlot *plot) : PaintedItem(plot), m_plot(plot)
    {
        setAntialiasing(true);
        setZ(5);
        setWidth(plot->width());
        setHeight(plot->height());
        QObject::connect(plot, &QQuickItem::widthChanged, this, [this, plot]() { setWidth(plot->width()); });
        QObject::connect(plot, &QQuickItem::heightChanged, this, [this, plot]() { setHeight(plot->height()); });
    }

    void paint(QPainter *painter) noexcept override
    {
        if (!painter) {
            return;
        }

        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setRenderHint(QPainter::TextAntialiasing, true);

        const auto &palette = m_plot->palette();
        const QColor text = palette.textColor();
        const QColor subtle = text;
        const QColor sourceColor = m_plot->sourceColor();
        const auto &result = m_plot->result();

        const qreal margin = 20.0;
        const qreal titleHeight = 24.0;
        const qreal headerTop = 16.0;
        const qreal statusHeight = 28.0;
        const qreal cardsTop = headerTop + titleHeight + 30.0;
        const qreal cardsBottom = std::max(cardsTop + 100.0, heightf() - statusHeight - 66.0);
        const qreal cardsHeight = cardsBottom - cardsTop;

        QFont titleFont = painter->font();
        titleFont.setPixelSize(std::max(14, static_cast<int>(heightf() / 30.0)));
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(text);
        painter->drawText(QRectF(margin, headerTop, widthf() - margin * 2.0, titleHeight),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("Speech Intelligibility"));

        QFont sourceFont = painter->font();
        sourceFont.setPixelSize(std::max(9, static_cast<int>(heightf() / 55.0)));
        sourceFont.setBold(false);
        painter->setFont(sourceFont);
        painter->setPen(subtle);
        const QString sourceLine = m_plot->sourceName().isEmpty()
                                   ? QStringLiteral("Select a measurement to evaluate")
                                   : QStringLiteral("Source: %1").arg(m_plot->sourceName());
        painter->drawText(QRectF(margin, headerTop + titleHeight + 2.0, widthf() - margin * 2.0, 20.0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          sourceLine);

        if (!result.valid) {
            QFont errorFont = painter->font();
            errorFont.setPixelSize(std::max(11, static_cast<int>(heightf() / 38.0)));
            painter->setFont(errorFont);
            painter->setPen(text);
            const QString message = m_plot->error().isEmpty()
                                    ? QStringLiteral("No impulse response available.")
                                    : m_plot->error();
            painter->drawText(QRectF(margin, cardsTop, widthf() - margin * 2.0, cardsHeight),
                              Qt::AlignCenter | Qt::TextWordWrap,
                              message);
        } else {
            const qreal gap = 12.0;
            const qreal cardWidth = (widthf() - margin * 2.0 - gap * 2.0) / 3.0;
            const QString titles[3] = { QStringLiteral("STI"), QStringLiteral("STIPA"), QStringLiteral("RASTI") };
            const QString subtitles[3] = {
                QStringLiteral("Full STI"),
                QStringLiteral("Public address"),
                QStringLiteral("Rapid (legacy)")
            };
            const float values[3] = { result.sti, result.stipa, result.rasti };
            const QString ratings[3] = { m_plot->stiRating(), m_plot->stipaRating(), m_plot->rastiRating() };

            for (int i = 0; i < 3; ++i) {
                const QRectF card(margin + i * (cardWidth + gap), cardsTop, cardWidth, cardsHeight);
                drawCard(painter, card, titles[i], subtitles[i], values[i], ratings[i],
                         (i == 0 ? sourceColor : QColor(text)));
            }

            if (heightf() > 300.0) {
                drawBands(painter, result, margin, cardsBottom + 8.0,
                          widthf() - margin * 2.0, statusHeight + 8.0, text, sourceColor);
            }
        }

        QFont statusFont = painter->font();
        statusFont.setPixelSize(std::max(8, static_cast<int>(heightf() / 62.0)));
        statusFont.setBold(false);
        painter->setFont(statusFont);
        painter->setPen(result.shortImpulse ? QColor(QStringLiteral("#EF6C00")) : subtle);
        painter->drawText(QRectF(margin, heightf() - statusHeight - 4.0, widthf() - margin * 2.0, statusHeight),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                          m_plot->status());
    }

private:
    void drawCard(QPainter *painter, const QRectF &rect, const QString &title,
                  const QString &subtitle, float value, const QString &rating,
                  const QColor &accent)
    {
        const auto &palette = m_plot->palette();
        const QColor text = palette.textColor();
        QColor background = text;
        background.setAlpha(palette.darkMode() ? 22 : 12);

        painter->setPen(Qt::NoPen);
        painter->setBrush(background);
        painter->drawRoundedRect(rect, 8.0, 8.0);

        QColor accentLine = accent;
        accentLine.setAlpha(200);
        painter->setBrush(accentLine);
        painter->drawRoundedRect(QRectF(rect.left(), rect.top(), rect.width(), 4.0), 2.0, 2.0);

        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(accentLine, 1.0));
        painter->drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);

        QFont titleFont = painter->font();
        titleFont.setPixelSize(std::max(11, static_cast<int>(rect.height() / 8.0)));
        titleFont.setBold(true);
        painter->setFont(titleFont);
        painter->setPen(text);
        painter->drawText(QRectF(rect.left() + 12.0, rect.top() + 12.0, rect.width() - 24.0, 24.0),
                          Qt::AlignLeft | Qt::AlignVCenter, title);

        QFont subFont = painter->font();
        subFont.setPixelSize(std::max(8, static_cast<int>(rect.height() / 14.0)));
        subFont.setBold(false);
        painter->setFont(subFont);
        painter->setPen(text);
        painter->drawText(QRectF(rect.left() + 12.0, rect.top() + 34.0, rect.width() - 24.0, 20.0),
                          Qt::AlignLeft | Qt::AlignVCenter, subtitle);

        QFont valueFont = painter->font();
        valueFont.setPixelSize(std::max(26, static_cast<int>(rect.height() / 3.2)));
        valueFont.setBold(true);
        painter->setFont(valueFont);
        painter->setPen(text);
        painter->drawText(QRectF(rect.left() + 12.0, rect.top() + rect.height() * 0.30,
                                 rect.width() - 24.0, rect.height() * 0.34),
                          Qt::AlignLeft | Qt::AlignVCenter, displayValue(value));

        QFont ratingFont = painter->font();
        ratingFont.setPixelSize(std::max(9, static_cast<int>(rect.height() / 13.0)));
        ratingFont.setBold(true);
        painter->setFont(ratingFont);
        painter->setPen(ratingColor(rating));
        painter->drawText(QRectF(rect.left() + 12.0, rect.top() + rect.height() - 36.0,
                                 rect.width() - 24.0, 24.0),
                          Qt::AlignRight | Qt::AlignVCenter, rating);
    }

    void drawBands(QPainter *painter, const Speech::Calculator::Result &result,
                   qreal left, qreal top, qreal width, qreal height,
                   const QColor &text, const QColor &accent)
    {
        const QString labels[7] = { "125", "250", "500", "1k", "2k", "4k", "8k" };
        const qreal labelHeight = 14.0;
        const qreal barTop = top + 16.0;
        const qreal barHeight = std::max(20.0, height - labelHeight - 18.0);
        const qreal gap = 8.0;
        const qreal barWidth = (width - gap * 6.0) / 7.0;

        QFont font = painter->font();
        font.setPixelSize(8);
        font.setBold(false);
        painter->setFont(font);
        painter->setPen(text);
        painter->drawText(QRectF(left, top, width, 14.0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("Modulation transfer index by octave band"));

        for (int i = 0; i < 7; ++i) {
            const qreal x = left + i * (barWidth + gap);
            const qreal value = std::max(0.0, std::min(1.0, static_cast<double>(result.mti[i])));
            QColor track = text;
            track.setAlpha(35);
            painter->setPen(Qt::NoPen);
            painter->setBrush(track);
            painter->drawRoundedRect(QRectF(x, barTop, barWidth, barHeight), 2.0, 2.0);

            QColor fill = accent;
            fill.setAlpha(210);
            painter->setBrush(fill);
            painter->drawRoundedRect(QRectF(x, barTop + barHeight * (1.0 - value),
                                            barWidth, barHeight * value), 2.0, 2.0);

            painter->setPen(text);
            painter->drawText(QRectF(x, barTop + barHeight + 1.0, barWidth, labelHeight),
                              Qt::AlignHCenter | Qt::AlignTop, labels[i]);
        }
    }

    SpeechPlot *m_plot;
};

SpeechPlot::SpeechPlot(Settings *settings, QQuickItem *parent) :
    Plot(settings, parent),
    m_sources(nullptr),
    m_content(nullptr),
    m_sourceName(),
    m_sourceColor(QColor(QStringLiteral("#2196F3"))),
    m_correctionsEnabled(false),
    m_signalLevels(),
    m_noiseLevels(),
    m_correctionError()
{
    m_content = new Content(this);
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &SpeechPlot::refresh);
    m_timer.start();
}

void SpeechPlot::connectSources(SourceList *sourceList)
{
    m_sources = sourceList;
    refresh();
}

void SpeechPlot::refresh()
{
    Shared::Source source;
    if (m_sources) {
        source = m_sources->selected();
        if (!source && m_sources->count() > 0) {
            source = m_sources->get(0);
        }
    }

    m_source = source;
    compute();
}

void SpeechPlot::compute()
{
    m_result = Speech::Calculator::Result{};
    m_sourceName.clear();
    m_sourceColor = QColor(QStringLiteral("#2196F3"));

    if (!m_source) {
        m_result.error = QStringLiteral("Select a measurement with an impulse response.");
    } else {
        m_sourceName = m_source->name();
        m_sourceColor = m_source->color();

        std::vector<float> values;
        std::vector<float> times;
        const unsigned sampleRate = m_source->sampleRate();

        m_source->lock();
        const unsigned count = m_source->timeDomainSize();
        values.resize(count);
        times.resize(count);
        for (unsigned i = 0; i < count; ++i) {
            values[i] = m_source->impulseValue(i);
            times[i] = m_source->impulseTime(i);
        }
        m_source->unlock();

        if (count == 0) {
            m_result.error = QStringLiteral("This source has no impulse response. Store or import an impulse measurement first.");
        } else {
            const Speech::Calculator::Correction correction =
                m_correctionsEnabled ? m_correction : Speech::Calculator::Correction{};
            m_result = Speech::Calculator::fromImpulse(values, times, sampleRate, correction);
        }
    }

    update();
    if (m_content) {
        m_content->update();
    }
    emit resultChanged();
}

QString SpeechPlot::sourceName() const
{
    return m_sourceName;
}

QString SpeechPlot::status() const
{
    if (!m_result.valid) {
        return m_result.error;
    }

    QString message = QStringLiteral("Indirect method from impulse response");
    if (m_result.shortImpulse) {
        message += QStringLiteral(" · IR %1 s (under 1.6 s)").arg(displayValue(m_result.duration));
    }
    if (!m_correctionsEnabled) {
        message += QStringLiteral(" · noise/masking corrections off");
    } else {
        if (m_result.noiseCorrectionApplied && m_result.maskingCorrectionApplied) {
            message += QStringLiteral(" · noise + masking corrections applied");
        } else if (m_result.maskingCorrectionApplied) {
            message += QStringLiteral(" · masking/threshold correction applied");
        } else if (m_result.noiseCorrectionApplied) {
            message += QStringLiteral(" · noise correction applied");
        }
        if (!m_correctionError.isEmpty()) {
            message += QStringLiteral(" · ") + m_correctionError;
        }
    }
    return message;
}

QString SpeechPlot::error() const
{
    return m_result.error;
}

bool SpeechPlot::valid() const
{
    return m_result.valid;
}

bool SpeechPlot::shortImpulse() const
{
    return m_result.shortImpulse;
}

bool SpeechPlot::correctionsEnabled() const
{
    return m_correctionsEnabled;
}

QString SpeechPlot::signalLevels() const
{
    return m_signalLevels;
}

QString SpeechPlot::noiseLevels() const
{
    return m_noiseLevels;
}

void SpeechPlot::setCorrectionsEnabled(bool enabled)
{
    if (m_correctionsEnabled == enabled) {
        return;
    }
    m_correctionsEnabled = enabled;
    emit correctionsChanged();
    updateCorrection();
}

void SpeechPlot::setSignalLevels(const QString &levels)
{
    if (m_signalLevels == levels) {
        return;
    }
    m_signalLevels = levels;
    emit correctionsChanged();
    updateCorrection();
}

void SpeechPlot::setNoiseLevels(const QString &levels)
{
    if (m_noiseLevels == levels) {
        return;
    }
    m_noiseLevels = levels;
    emit correctionsChanged();
    updateCorrection();
}

void SpeechPlot::updateCorrection()
{
    m_correction = Speech::Calculator::Correction{};
    m_correctionError.clear();

    if (m_correctionsEnabled) {
        if (m_signalLevels.trimmed().isEmpty()) {
            m_correctionError = QStringLiteral("enter seven speech octave-band levels in dB SPL");
        } else {
            QString error;
            if (parseBandLevels(m_signalLevels, m_correction.signalLevels, error)) {
                m_correction.hasSignalLevels = true;
            } else {
                m_correctionError = QStringLiteral("speech levels: %1").arg(error);
            }
        }

        if (!m_noiseLevels.trimmed().isEmpty()) {
            QString error;
            if (parseBandLevels(m_noiseLevels, m_correction.noiseLevels, error)) {
                m_correction.hasNoiseLevels = m_correction.hasSignalLevels;
            } else {
                if (!m_correctionError.isEmpty()) {
                    m_correctionError += QLatin1Char(' ');
                }
                m_correctionError += QStringLiteral("noise levels: %1").arg(error);
            }
        }
    }

    compute();
}

float SpeechPlot::sti() const
{
    return m_result.valid ? m_result.sti : 0.f;
}

float SpeechPlot::stipa() const
{
    return m_result.valid ? m_result.stipa : 0.f;
}

float SpeechPlot::rasti() const
{
    return m_result.valid ? m_result.rasti : 0.f;
}

QString SpeechPlot::stiRating() const
{
    return m_result.valid ? Speech::Calculator::rating(m_result.sti) : QStringLiteral("—");
}

QString SpeechPlot::stipaRating() const
{
    return m_result.valid ? Speech::Calculator::rating(m_result.stipa) : QStringLiteral("—");
}

QString SpeechPlot::rastiRating() const
{
    return m_result.valid ? Speech::Calculator::rating(m_result.rasti) : QStringLiteral("—");
}

const Speech::Calculator::Result &SpeechPlot::result() const
{
    return m_result;
}

QColor SpeechPlot::sourceColor() const
{
    return m_sourceColor;
}

void SpeechPlot::setHelper(qreal, qreal) noexcept
{
}

void SpeechPlot::unsetHelper() noexcept
{
}

qreal SpeechPlot::x2v(qreal) const noexcept
{
    return 0.0;
}

qreal SpeechPlot::y2v(qreal) const noexcept
{
    return 0.0;
}

qreal SpeechPlot::v2x(qreal) const noexcept
{
    return 0.0;
}

qreal SpeechPlot::v2y(qreal) const noexcept
{
    return 0.0;
}

void SpeechPlot::beginGesture()
{
}

void SpeechPlot::applyGesture(QPointF, QPointF, QPointF)
{
}

void SpeechPlot::resetAxis()
{
}

QString SpeechPlot::xLabel() const
{
    return QString();
}

QString SpeechPlot::yLabel() const
{
    return QString();
}

void SpeechPlot::setSettings(Settings *settings) noexcept
{
    Plot::setSettings(settings);
    if (!m_settings) {
        return;
    }

    setCorrectionsEnabled(m_settings->value(QStringLiteral("corrections/enabled"), false).toBool());
    setSignalLevels(m_settings->value(QStringLiteral("corrections/signalLevels")).toString());
    setNoiseLevels(m_settings->value(QStringLiteral("corrections/noiseLevels")).toString());
}

void SpeechPlot::storeSettings() noexcept
{
    if (!m_settings) {
        return;
    }

    m_settings->setValue(QStringLiteral("corrections/enabled"), m_correctionsEnabled);
    m_settings->setValue(QStringLiteral("corrections/signalLevels"), m_signalLevels);
    m_settings->setValue(QStringLiteral("corrections/noiseLevels"), m_noiseLevels);
}

void SpeechPlot::inheritSettings(const Plot *source)
{
    const auto *speechSource = dynamic_cast<const SpeechPlot *>(source);
    if (!speechSource) {
        return;
    }
    setCorrectionsEnabled(speechSource->correctionsEnabled());
    setSignalLevels(speechSource->signalLevels());
    setNoiseLevels(speechSource->noiseLevels());
}

SeriesItem *SpeechPlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

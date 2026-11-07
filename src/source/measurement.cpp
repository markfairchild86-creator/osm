/**
 *  OSM
 *  Copyright (C) 2018  Pavel Smokotnin

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
#include <QFile>
#include <QUrl>
#include <QJsonArray>
#include <QDateTime>
#include <QtMath>
#include <algorithm>
#include <utility>
#include "measurement.h"
#include "audio/client.h"
#include "generator/generatorthread.h"
#include "math/notch.h"
#include "math/bandpass.h"
#include "math/lowpassfilter.h"

Measurement::Measurement(QObject *parent) : Abstract::Source(parent), Meta::Measurement(),
    m_timer(nullptr), m_timerThread(nullptr),
    m_input(this),
    m_deviceId(audio::Client::defaultInputDeviceId()),
    m_audioStream(nullptr),
    m_settings(nullptr),//TODO: alean and remove
    m_currentMode(Mode::FFT10),
    m_currentLogWindowPointsPerOctave(0),
    m_workingDelay(0), m_delayFinderCounter(0),
    m_estimatedDelay(0),
    m_error(false), m_onReset(false),
    m_data(65536), m_reference(65536), m_loopBuffer(65536),
    m_enableCalibration(false), m_calibrationLoaded(false), m_calibrationList(), m_calibrationGain()
{
    setName("Measurement");
    setObjectName(name());

    if (m_settings) {
        setMode(        m_settings->reactValue<Measurement, Mode>(              "mode",         this,
                                                                                &Measurement::modeChanged,          mode()));
        setName(        m_settings->reactValue<Measurement, QString>(           "name",         this,
                                                                                &Measurement::nameChanged,          name()).toString());
        setColor(       m_settings->reactValue<Measurement, QColor>(            "color",        this,
                                                                                &Measurement::colorChanged,         color()).value<QColor>());
        setDelay(       m_settings->reactValue<Measurement, int>(      "delay",        this,
                                                                       &Measurement::delayChanged,         delay()).toInt());
        setGain(        m_settings->reactValue<Measurement, float>(             "gain",         this,
                                                                                &Measurement::gainChanged,          gain()).toFloat());
        setAverageType( m_settings->reactValue<Measurement, AverageType>(       "average/type", this,
                                                                                &Measurement::averageTypeChanged,   averageType()));
        setAverage(     m_settings->reactValue<Measurement, unsigned int>(      "average/fifo", this,
                                                                                &Measurement::averageChanged,       average()).toUInt());
        setAverageDepth(m_settings->reactValue<Measurement, unsigned int>(      "average/depth", this,
                                                                                &Measurement::averageDepthChanged,  averageDepth()).toUInt());
        setMagnitudeAverageType(m_settings->reactValue<Measurement, MagnitudeAverageType>("average/magnitude", this,
                                                                                &Measurement::magnitudeAverageTypeChanged, magnitudeAverageType()));
        setCoherenceWeighting(m_settings->reactValue<Measurement, bool>(        "average/coherence", this,
                                                                                &Measurement::coherenceWeightingChanged, coherenceWeighting()).toBool());
        setCoherenceExponent(m_settings->reactValue<Measurement, float>(        coherenceExponentSettingsKey(), this,
                                                                                &Measurement::coherenceExponentChanged, coherenceExponent()).toFloat());
        setLogWindowPointsPerOctave(m_settings->reactValue<Measurement, unsigned int>("ltw/pointsPerOctave", this,
                                                                                &Measurement::logWindowPointsPerOctaveChanged, logWindowPointsPerOctave()).toUInt());
        setFiltersFrequency(m_settings->reactValue<Measurement, Filter::Frequency>("average/lpf", this,
                                                                                   &Measurement::filtersFrequencyChanged, filtersFrequency()));
        setWindowFunctionType( m_settings->reactValue<Measurement, WindowFunction::Type>("window",     this,
                                                                                         &Measurement::windowFunctionTypeChanged,    m_windowFunctionType));
        setDataChanel(  m_settings->reactValue<Measurement, unsigned int>(      "route/data",   this,
                                                                                &Measurement::dataChanelChanged,    dataChanel()).toUInt());
        setReferenceChanel(m_settings->reactValue<Measurement, unsigned int>(   "route/reference", this,
                                                                                &Measurement::referenceChanelChanged, referenceChanel()).toUInt());
        setPolarity(    m_settings->reactValue<Measurement, bool>(              "polarity",     this,
                                                                                &Measurement::polarityChanged,      polarity()).toBool());
        selectDevice(   m_settings->reactValue<Measurement, QString>(           "device",       this,
                                                                                &Measurement::deviceNameChanged,        deviceName()).toString());
    }
    setTimeDomainSize(static_cast<unsigned int>(pow(2, 12)));

    updateFftPower();
    m_dataFT.setWindowFunctionType(m_windowFunctionType);
    m_moduleLPFs.resize(frequencyDomainSize());
    m_magnitudeLPFs.resize(frequencyDomainSize());
    m_phaseLPFs.resize(frequencyDomainSize());
    m_transferLPFs.resize(frequencyDomainSize());
    m_meters.resize(frequencyDomainSize());

    m_deconvolution.setSize(timeDomainSize());
    m_deconvolution.setWindowFunctionType(m_windowFunctionType);
    m_delayFinder.setSize(pow(2, 16));
    m_delayFinder.setWindowFunctionType(m_windowFunctionType);
    m_impulseData.resize(timeDomainSize());
    m_deconvLPFs.resize(timeDomainSize());
    m_deconvAvg.setSize(timeDomainSize());
    m_deconvAvg.reset();
    m_deconvTemporal.setSize(timeDomainSize());
    m_magnitudeTemporal.setSize(frequencyDomainSize());
    m_moduleTemporal.setSize(frequencyDomainSize());
    m_phaseTemporal.setSize(frequencyDomainSize());
    m_transferTemporal.setSize(frequencyDomainSize());
    m_magnitudeGate.setSize(frequencyDomainSize());
    m_moduleGate.setSize(frequencyDomainSize());
    m_phaseGate.setSize(frequencyDomainSize());
    m_transferGate.setSize(frequencyDomainSize());
    updateTemporalAveraging();
    m_coherence.setDepth(21);//Filter::BesselLPF<float>::ORDER);

    m_timer.setInterval(TIMER_INTERVAL);
    m_timer.moveToThread(&m_timerThread);
    connect(&m_timer, SIGNAL(timeout()), SLOT(transform()), Qt::DirectConnection);
    connect(&m_timerThread, SIGNAL(started()), &m_timer, SLOT(start()), Qt::DirectConnection);
    connect(&m_timerThread, SIGNAL(finished()), &m_timer, SLOT(stop()), Qt::DirectConnection);
    connect(this, &Measurement::audioFormatChanged, this, &Measurement::onSampleRateChanged);
    connect(GeneratorThread::getInstance(), &GeneratorThread::sampleOut, this, &Measurement::newSampleFromGenerator,
            Qt::DirectConnection);
    connect(GeneratorThread::getInstance(), &GeneratorThread::enabledChanged, this, &Measurement::resetLoopBuffer,
            Qt::DirectConnection);

    auto refreshDelays = [this]() {
        m_resetDelay = true;
    };
    connect(this, &Measurement::dataChanelChanged, &m_timer, refreshDelays);
    connect(this, &Measurement::referenceChanelChanged, &m_timer, refreshDelays);
    connect(this, &Measurement::deviceIdChanged, &m_timer, refreshDelays);

    connect(this, &Measurement::averageChanged, this, &Measurement::updateAverage);
    connect(this, &Measurement::averageTypeChanged, this, &Measurement::updateAverage);
    connect(this, &Measurement::averageDepthChanged, this, &Measurement::updateAverage);
    connect(this, &Measurement::coherenceWeightingChanged, this, &Measurement::updateCoherenceWeighting);
    connect(this, &Measurement::coherenceExponentChanged, this, &Measurement::updateCoherenceWeighting);
    connect(this, &Measurement::windowFunctionTypeChanged, this, &Measurement::updateWindowFunction);
    connect(this, &Measurement::filtersFrequencyChanged, this, &Measurement::updateFilterFrequency);
    connect(this, &Measurement::inputFilterChanged, this, &Measurement::applyInputFilters);

    m_timerThread.start();
    this->setActive(true);
}
Measurement::~Measurement()
{
    this->setActive(false);

    m_timerThread.quit();
    m_timerThread.wait();
}
QJsonObject Measurement::toJSON() const noexcept
{
    auto data = Abstract::Source::toJSON();
    data["delay"]           = static_cast<int>(delay());
    data["gain"]            = gain();
    data["offset"]          = offset();
    data["averageType"]     = averageType();
    data["average"]         = static_cast<int>(average());
    data["averageDepth"]    = static_cast<int>(averageDepth());
    data["magnitudeAverageType"] = static_cast<int>(magnitudeAverageType());
    data["coherenceWeighting"] = coherenceWeighting();
    data["coherenceExponent"] = coherenceExponent();
    data["logWindowPointsPerOctave"] = static_cast<int>(logWindowPointsPerOctave());
    data["filtersFrequency"] = static_cast<int>(filtersFrequency());
    data["window.type"]     = m_windowFunctionType;
    data["dataChanel"]      = static_cast<int>(dataChanel());
    data["referenceChanel"] = static_cast<int>(referenceChanel());
    data["polarity"]        = polarity();
    data["deviceName"]      = deviceName();
    data["mode"]            = mode();
    data["inputFilters"]    = static_cast<int>(inputFilter());

    QJsonObject calibration;
    calibration["enabled"] = m_enableCalibration;

    QJsonArray calibrationListJson;
    for (auto calibrationRow : m_calibrationList) {
        QJsonArray calibrationRowJson{};
        calibrationRowJson.append(static_cast<double>(calibrationRow[0]));//frequency
        calibrationRowJson.append(static_cast<double>(calibrationRow[1]));//gain
        calibrationRowJson.append(static_cast<double>(calibrationRow[2]));//phase
        calibrationListJson.append(calibrationRowJson);
    }
    calibration["list"] = calibrationListJson;


    data["calibration"] = calibration;
    return data;
}
void Measurement::fromJSON(QJsonObject data, const SourceList *list) noexcept
{
    Abstract::Source::fromJSON(data, list);

    auto castUInt = [](const QJsonValue & value, unsigned int defaultValue = 0) {
        return static_cast<unsigned int>(value.toInt(static_cast<int>(defaultValue)));
    };

    setGain(                      data["gain"].toDouble(    gain()));
    setOffset(                    data["offset"].toDouble(  offset()));
    setDelay(            castUInt(data["delay"           ], delay()));
    setAverage(          castUInt(data["average"         ], average()));
    setAverageDepth(     castUInt(data["averageDepth"    ], averageDepth()));
    setMagnitudeAverageType(static_cast<MagnitudeAverageType>(data["magnitudeAverageType"].toInt(magnitudeAverageType())));
    setCoherenceWeighting(data["coherenceWeighting"].toBool(coherenceWeighting()));
    setCoherenceExponent(static_cast<float>(data["coherenceExponent"].toDouble(static_cast<double>(coherenceExponent()))));
    setLogWindowPointsPerOctave(castUInt(data["logWindowPointsPerOctave"], logWindowPointsPerOctave()));
    setDataChanel(       castUInt(data["dataChanel"      ], dataChanel()));
    setReferenceChanel(  castUInt(data["referenceChanel" ], referenceChanel()));

    setMode(             data["mode"             ].toInt(mode()));
    setAverageType(      data["averageType"      ].toInt(averageType()));
    setFiltersFrequency( data["filtersFrequency" ].toInt(filtersFrequency()));
    setWindowFunctionType(data["window.type"     ].toInt(m_windowFunctionType));
    setPolarity(         data["polarity"         ].toBool(polarity()));
    selectDevice(        data["deviceName"       ].toString(deviceName()));
    setInputFilter(      data["inputFilters"     ].toInt(inputFilter()));

    QJsonObject calibration = data["calibration"].toObject();
    if (!calibration.isEmpty()) {
        QJsonArray calibrationListJson = calibration["list"].toArray();
        m_calibrationList.clear();
        for (auto calibrationRowRef : calibrationListJson) {
            auto calibrationRowJson = calibrationRowRef.toArray();
            float
            frequency   = static_cast<float>(calibrationRowJson[0].toDouble(0.0)),
            gain        = static_cast<float>(calibrationRowJson[1].toDouble(0.0)),
            phase       = static_cast<float>(calibrationRowJson[2].toDouble(0.0));
            QVector<float> calibrationData;
            calibrationData << frequency << gain << phase;
            m_calibrationList << calibrationData;
        }
        m_calibrationLoaded = (m_calibrationList.count() > 0);
        emit calibrationLoadedChanged(m_calibrationLoaded);
        applyCalibration();

        setCalibration(calibration["enabled"].toBool());
    }
}
float Measurement::level(const Weighting::Curve curve, const Meter::Time time) const
{
    if (m_levelMeters.m_meters.find({curve, time}) == m_levelMeters.m_meters.end()) {
        Q_ASSERT(false);
        return 0;
    }
    return m_levelMeters.m_meters.at({curve, time}).dB();
}

float Measurement::peak(const Weighting::Curve curve, const Meter::Time time) const
{
    if (m_levelMeters.m_meters.find({curve, time}) == m_levelMeters.m_meters.end()) {
        Q_ASSERT(false);
        return 0;
    }
    return m_levelMeters.m_meters.at({curve, time}).peakdB();
}
float Measurement::referenceLevel() const
{
    return m_levelMeters.m_reference.dB();
}
float Measurement::measurementPeak() const
{
    return m_levelMeters.m_meters.at({Weighting::Z, Meter::Slow}).peakdB();
}
float Measurement::referencePeak() const
{
    return m_levelMeters.m_reference.peakdB();
}
//this calls from timer thread
//should be called while mutex locked
void Measurement::updateFftPower()
{
    const unsigned int pointsPerOctave = logWindowPointsPerOctave();
    if (Q_LIKELY(m_mode == m_currentMode && pointsPerOctave == m_currentLogWindowPointsPerOctave)) return;
    m_currentMode = m_mode;
    m_currentLogWindowPointsPerOctave = pointsPerOctave;

    switch (m_currentMode) {
    case Mode::LFT:
        m_dataFT.setLogWindowPointsPerOctave(pointsPerOctave);
        m_dataFT.setType(FourierTransform::Log);
        setTimeDomainSize(pow(2, m_FFTsizes.at(FFT12)));
        break;

    default:
        m_dataFT.setSize(pow(2, m_FFTsizes.at(m_currentMode)));
        m_dataFT.setType(FourierTransform::Fast);

        setTimeDomainSize(pow(2, m_FFTsizes.at(m_currentMode)));
    }
    m_dataFT.setSampleRate(sampleRate());
    m_levelMeters.setSampleRate(sampleRate());
    m_dataFT.prepare();
    calculateDataLength();

    m_moduleAvg.setSize(frequencyDomainSize());
    m_magnitudeAvg.setSize(frequencyDomainSize());
    m_pahseAvg.setSize(frequencyDomainSize());
    m_transferAvg.setSize(frequencyDomainSize());
    m_coherence.setSize(frequencyDomainSize());

    m_moduleLPFs.resize(frequencyDomainSize());
    m_magnitudeLPFs.resize(frequencyDomainSize());
    m_phaseLPFs.resize(frequencyDomainSize());
    m_transferLPFs.resize(frequencyDomainSize());
    m_meters.resize(frequencyDomainSize());

    // Deconvolution:
    m_deconvolution.setSize(timeDomainSize());
    m_deconvLPFs.resize(timeDomainSize());
    m_deconvAvg.setSize(timeDomainSize());
    m_deconvAvg.reset();

    m_deconvTemporal.setSize(timeDomainSize());
    m_magnitudeTemporal.setSize(frequencyDomainSize());
    m_moduleTemporal.setSize(frequencyDomainSize());
    m_phaseTemporal.setSize(frequencyDomainSize());
    m_transferTemporal.setSize(frequencyDomainSize());
    m_magnitudeGate.setSize(frequencyDomainSize());
    m_moduleGate.setSize(frequencyDomainSize());
    m_phaseGate.setSize(frequencyDomainSize());
    m_transferGate.setSize(frequencyDomainSize());
    updateTemporalAveraging();
}
void Measurement::updateFilterFrequency()
{
    auto setFrequency = [&m_filtersFrequency = m_filtersFrequency](Filter::BesselLPF<float> *f) {
        f->setFrequency(m_filtersFrequency);
    };

    m_moduleLPFs.each(setFrequency);
    m_magnitudeLPFs.each(setFrequency);
    m_deconvLPFs.each(setFrequency);
    m_phaseLPFs.each([&m_filtersFrequency = m_filtersFrequency](Filter::BesselLPF<Complex> *f) {
        f->setFrequency(m_filtersFrequency);
    });
    m_transferLPFs.each([&m_filtersFrequency = m_filtersFrequency](Filter::BesselLPF<Complex> *f) {
        f->setFrequency(m_filtersFrequency);
    });
}

void Measurement::applyInputFilters()
{
    switch (m_inputFilter) {
    case InputFilter::A:
        std::atomic_store(&m_inputFilters.first,  std::shared_ptr<math::Filter>(new Weighting(Weighting::Curve::A,
                                                                                              sampleRate())));
        std::atomic_store(&m_inputFilters.second, std::shared_ptr<math::Filter>(new Weighting(Weighting::Curve::A,
                                                                                              sampleRate())));
        break;
    case InputFilter::C:
        std::atomic_store(&m_inputFilters.first,  std::shared_ptr<math::Filter>(new Weighting(Weighting::Curve::C,
                                                                                              sampleRate())));
        std::atomic_store(&m_inputFilters.second, std::shared_ptr<math::Filter>(new Weighting(Weighting::Curve::C,
                                                                                              sampleRate())));
        break;
    case InputFilter::Notch: {
        auto q = 3.f;// AES17-1998 says: 1 to 5
        std::atomic_store(&m_inputFilters.first,  std::shared_ptr<math::Filter>(new math::Notch(1000, q, sampleRate())));
        std::atomic_store(&m_inputFilters.second, std::shared_ptr<math::Filter>(new math::Notch(1000, q, sampleRate())));
        break;
    }
    case InputFilter::BP100: {
        auto q = 5.f;
        std::atomic_store(&m_inputFilters.first,  std::shared_ptr<math::Filter>(new math::BandPass(100, q, sampleRate())));
        std::atomic_store(&m_inputFilters.second, std::shared_ptr<math::Filter>(new math::BandPass(100, q, sampleRate())));
    }
    break;
    case InputFilter::LP200: {
        auto q = 0.5f;
        std::atomic_store(&m_inputFilters.first,  std::shared_ptr<math::Filter>(new math::LowPassFilter(200, q, sampleRate())));
        std::atomic_store(&m_inputFilters.second, std::shared_ptr<math::Filter>(new math::LowPassFilter(200, q, sampleRate())));
    }
    break;
    case InputFilter::Z:
        std::atomic_store(&m_inputFilters.first, std::shared_ptr<math::Filter>());
        std::atomic_store(&m_inputFilters.second, std::shared_ptr<math::Filter>());
        break;
    }

    if (m_inputFilter == InputFilter::Notch) {
        std::atomic_store(&m_levelMeters.m_filter, std::shared_ptr<math::Filter>(new math::Notch(1000, 3.f, sampleRate())));
    } else {
        std::atomic_store(&m_levelMeters.m_filter, std::shared_ptr<math::Filter>());
    }
}

void Measurement::onSampleRateChanged()
{
    std::lock_guard<std::mutex> guard(m_dataMutex);
    if (m_audioStream) {
        setSampleRate(m_audioStream->format().sampleRate);
        m_dataFT.setSampleRate(sampleRate());
        m_dataFT.prepare();
        m_levelMeters.setSampleRate(sampleRate());
        calculateDataLength();
        updateFilterFrequency();
        applyInputFilters();
    }
}
audio::DeviceInfo::Id Measurement::deviceId() const
{
    return m_deviceId;
}
void Measurement::setDeviceId(const audio::DeviceInfo::Id &deviceId)
{
    if (!deviceId.isNull() && deviceId != m_deviceId) {
        m_error = false;
        emit errorChanged(m_error);
        m_deviceId = deviceId;
        emit deviceIdChanged(m_deviceId);
        updateAudio();
    }
}
QString Measurement::deviceName() const
{
    return audio::Client::getInstance()->deviceName(m_deviceId);
}
void Measurement::selectDevice(const QString &name)
{
    auto id = audio::Client::getInstance()->deviceIdByName(name, audio::Plugin::Direction::Input);
    setDeviceId(id);
}

void Measurement::applyAutoGain(const float reference)
{
    setGain(reference - level(Weighting::A, Meter::Slow) + gain());
}

void Measurement::calculateDataLength()
{
    auto frequencyList = m_dataFT.getFrequencies();
    setFrequencyDomainSize(frequencyList.size());
    unsigned int i = 0;
    for (auto frequency : frequencyList) {
        m_ftdata[i++].frequency = frequency;
    }
    applyCalibration();
}
void Measurement::setActive(bool newActive)
{
    if (newActive == active())
        return;
    std::lock_guard<std::mutex> guard(m_dataMutex);

    Abstract::Source::setActive(newActive);
    m_error = false;
    emit errorChanged(m_error);

    updateAudio();

    m_levelMeters.reset();
    m_loopBuffer.reset();
    emit levelChanged();
    emit referenceLevelChanged();
}
void Measurement::setError()
{
    Abstract::Source::setActive(false);
    m_error = true;
    m_levelMeters.reset();
    m_input.close();
    emit errorChanged(m_error);
    emit levelChanged();
    emit referenceLevelChanged();
}
void Measurement::newSampleFromGenerator(float sample)
{
    m_loopBuffer.write(sample);
}
void Measurement::resetLoopBuffer()
{
    std::lock_guard<std::mutex> guard(m_dataMutex);
    m_loopBuffer.reset();
}
//this calls from timer thread
void Measurement::updateDelay()
{
    if (m_resetDelay) {
        m_workingDelay = 0;
        m_reference.reset();
        m_data.reset();
        m_resetDelay = false;
    }
    if (m_workingDelay != m_delay) {
        long delta = static_cast<long>(m_workingDelay) - static_cast<long>(m_delay);
        m_workingDelay = m_delay;
        bool direction = std::signbit(static_cast<double>(delta));
        delta = std::abs(delta);
        for (long i = 0; i != delta; ++i) {
            if (direction) {
                m_reference.write(0.f);
            } else {
                m_data.write(0.f);
            }
        }
    }
}
void Measurement::updateAverage()
{
    std::lock_guard<std::mutex> guard(m_dataMutex);
    m_deconvAvg.setDepth(m_average);
    m_moduleAvg.setDepth(m_average);
    m_magnitudeAvg.setDepth(m_average);
    m_pahseAvg.setDepth(m_average);
    m_transferAvg.setDepth(m_average);
    updateTemporalAveraging();
}

void Measurement::updateCoherenceWeighting()
{
    std::lock_guard<std::mutex> guard(m_dataMutex);
    m_magnitudeGate.reset();
    m_moduleGate.reset();
    m_phaseGate.reset();
    m_transferGate.reset();
}

/*
 * Per bin setup of the temporal averagers:
 * Fast/Slow are plain one-pole averages with fixed time constants, Infinite
 * averages all frames since reset, and Progressive maps depth 1-10 to one time
 * constant shared by all bins. Coherence weighting is a separate gate applied
 * on top of the selected averaging type.
 */
void Measurement::updateTemporalAveraging()
{
    const bool infinite = (averageType() == AverageType::Infinite);
    m_moduleTemporal.setInfinite(infinite);
    m_magnitudeTemporal.setInfinite(infinite);
    m_phaseTemporal.setInfinite(infinite);
    m_transferTemporal.setInfinite(infinite);
    m_deconvTemporal.setInfinite(infinite);

    m_moduleGate.setInfinite(false);
    m_magnitudeGate.setInfinite(false);
    m_phaseGate.setInfinite(false);
    m_transferGate.setInfinite(false);

    const float interval = TIMER_INTERVAL / 1000.f;
    float alpha = 0.f;
    switch (averageType()) {
    case AverageType::Fast:
        alpha = TemporalAveraging<float>::alphaFor(0.125f, interval);
        break;
    case AverageType::Slow:
        alpha = TemporalAveraging<float>::alphaFor(1.f, interval);
        break;
    case AverageType::Progressive: {
        const unsigned int depth = std::max(1u, std::min(10u, averageDepth()));
        const float timeConstant = 0.125f * std::pow(2.f, static_cast<float>(depth) - 1.f);
        alpha = TemporalAveraging<float>::alphaFor(timeConstant, interval);
        break;
    }
    default:
        break;
    }

    if (alpha > 0.f) {
        m_moduleTemporal.setAlpha(alpha);
        m_magnitudeTemporal.setAlpha(alpha);
        m_phaseTemporal.setAlpha(alpha);
        m_transferTemporal.setAlpha(alpha);
        m_deconvTemporal.setAlpha(alpha);
    }

    m_magnitudeGate.setAlpha(1.f);
    m_moduleGate.setAlpha(1.f);
    m_phaseGate.setAlpha(1.f);
    m_transferGate.setAlpha(1.f);

    m_moduleTemporal.reset();
    m_magnitudeTemporal.reset();
    m_phaseTemporal.reset();
    m_transferTemporal.reset();
    m_deconvTemporal.reset();
    m_magnitudeGate.reset();
    m_moduleGate.reset();
    m_phaseGate.reset();
    m_transferGate.reset();
}

void Measurement::updateWindowFunction()
{
    m_dataFT.setWindowFunctionType(m_windowFunctionType);
    m_deconvolution.setWindowFunctionType(m_windowFunctionType);
    m_delayFinder.setWindowFunctionType(m_windowFunctionType);
    {
        std::lock_guard<std::mutex> guard(m_dataMutex);
        m_dataFT.prepare();
    }
}
void Measurement::writeData(const char *data, qint64 len)
{
    if (!m_audioStream || m_onReset.load() || !active()) {
        return;
    }
    std::lock_guard<std::mutex> guard(m_dataMutex);
    if (!m_audioStream) {
        return;
    }
    float sample;
    auto totalChanels = m_audioStream->format().channelCount;
    unsigned int currentChanel = 0;
    bool forceRef = referenceChanel() >= totalChanels;
    bool forceData = dataChanel() >= totalChanels;
    float loopSample = 0;
    bool loopAvailable = m_loopBuffer.collected() >= m_audioStream->depth() * len / (totalChanels * sizeof(float));
    qint64 offset = 0;
    for (auto it = data; offset < len; ) {
        if (currentChanel == 0) {
            loopSample = loopAvailable ? m_loopBuffer.read() : 0;
        }

        if (currentChanel == dataChanel()) {
            memcpy(&sample, it, sizeof(float));
            m_data.write(m_polarity ? -m_gain *sample : sample * m_gain);
            m_levelMeters.add(sample * m_gain);
        }

        if (currentChanel == referenceChanel()) {
            memcpy(&sample, it, sizeof(float));
            m_reference.write(sample * m_offset);
            m_levelMeters.addToReference(sample * m_offset);
        }
        ++currentChanel;
        if (currentChanel >= totalChanels) {
            if (forceRef) {
                m_reference.write(loopSample * m_offset);
                m_levelMeters.addToReference(loopSample * m_offset);
            }
            if (forceData) {
                m_data.write(loopSample * m_gain);
                m_levelMeters.add(loopSample * m_gain);
            }
            currentChanel = 0;
        }
        it += 4;
        offset += 4;
    }

}
void Measurement::transform()
{
    if (!active() || m_error)
        return;

    lock();
    updateFftPower();
    updateDelay();

    float d, r;
    auto filterM = m_inputFilters.first;
    auto filterR = m_inputFilters.second;

    while (m_data.collected() > 0 && m_reference.collected() > 0) {
        d = m_data.read();
        r = m_reference.read();

        if (filterM) {
            d = filterM->operator()(d);
        }
        if (filterR) {
            r = filterR->operator()(r);
        }

        m_dataFT.add(d, r);
        m_deconvolution.add(d, r);
        m_delayFinder.add(d, r);
    }
    m_dataFT.transform();
    m_deconvolution.transform(&m_dataFT);
    if ((++m_delayFinderCounter % 25) == 0) {
        m_delayFinder.transform(nullptr);
        m_delayFinderCounter = 0;
    }
    averaging();
    unlock();
    emit readyRead();
    emit levelChanged();
    emit referenceLevelChanged();
}
void Measurement::averaging()
{
    Complex p;
    int j;
    m_coherence.calculate(m_ftdata.data(), &m_dataFT);
    for (unsigned int i = 0; i < frequencyDomainSize() ; i++) {

        j = static_cast<int>(i);
        float calibratedA = M_SQRT2 * m_dataFT.af(i).abs();
        float calibratedB = M_SQRT2 * m_dataFT.bf(i).abs();
        //TODO: think and do
        //if (calibratedA < someThresholdInDb ) continue;

        if (m_enableCalibration && m_calibrationGain.size() > j) {
            calibratedA /= m_calibrationGain[j];
        }
        if (m_enableCalibration && m_calibrationGain.size() > j && !qFuzzyCompare(m_calibrationPhase[j], 0)) {
            p.polar(m_dataFT.bf(i).arg() - m_dataFT.af(i).arg() - m_calibrationPhase[j]);
        } else {
            p.polar(m_dataFT.bf(i), m_dataFT.af(i));
        }

        float magnitude = calibratedA / calibratedB;
#ifdef WIN64
        if (magnitude / 0.f == magnitude) {
#else
        if (std::isnan(magnitude) || std::isinf(magnitude)) {
#endif
            magnitude = 0.f;
        }

        float weight = 1.f;
        if (m_coherenceWeighting) {
            weight = coherenceWeight(m_ftdata[i].coherence, coherenceExponent());
        }

        // Polar magnitude averaging keeps scalar magnitudes. Vector averaging
        // instead averages the complex transfer function, then derives both
        // magnitude and phase from that vector sum.
        Complex transfer = p * magnitude;
        if (!std::isfinite(transfer.real) || !std::isfinite(transfer.imag)) {
            transfer = Complex(magnitude, 0.f);
        }

        float averagedMagnitude = magnitude;
        float averagedModule = calibratedA;
        Complex averagedPhase = p;
        Complex averagedTransfer = transfer;

        switch (averageType()) {
        case AverageType::Off:
            if (m_coherenceWeighting) {
                averagedMagnitude = m_magnitudeGate.append(i, magnitude, weight);
                averagedModule = m_moduleGate.append(i, calibratedA, weight);
                averagedPhase = m_phaseGate.append(i, p, weight);
                averagedTransfer = m_transferGate.append(i, transfer, weight);
            }
            break;

        case AverageType::LPF: {
            const float filteredMagnitude = m_magnitudeLPFs[i](magnitude);
            const float filteredModule = m_moduleLPFs[i](calibratedA);
            const Complex filteredPhase = m_phaseLPFs[i](p);
            const Complex filteredTransfer = m_transferLPFs[i](transfer);
            if (m_coherenceWeighting) {
                averagedMagnitude = m_magnitudeGate.append(i, filteredMagnitude, weight);
                averagedModule = m_moduleGate.append(i, filteredModule, weight);
                averagedPhase = m_phaseGate.append(i, filteredPhase, weight);
                averagedTransfer = m_transferGate.append(i, filteredTransfer, weight);
            } else {
                averagedMagnitude = filteredMagnitude;
                averagedModule = filteredModule;
                averagedPhase = filteredPhase;
                averagedTransfer = filteredTransfer;
            }
            break;
        }

        case AverageType::FIFO: {
            m_magnitudeAvg.append(i, magnitude );
            m_moduleAvg.append(i,    calibratedA );
            m_pahseAvg.append(i,     p);
            m_transferAvg.append(i,  transfer );

            const float fifoMagnitude = m_magnitudeAvg.value(i);
            const float fifoModule = m_moduleAvg.value(i);
            const Complex fifoPhase = m_pahseAvg.value(i);
            const Complex fifoTransfer = m_transferAvg.value(i);
            if (m_coherenceWeighting) {
                averagedMagnitude = m_magnitudeGate.append(i, fifoMagnitude, weight);
                averagedModule = m_moduleGate.append(i, fifoModule, weight);
                averagedPhase = m_phaseGate.append(i, fifoPhase, weight);
                averagedTransfer = m_transferGate.append(i, fifoTransfer, weight);
            } else {
                averagedMagnitude = fifoMagnitude;
                averagedModule = fifoModule;
                averagedPhase = fifoPhase;
                averagedTransfer = fifoTransfer;
            }
            break;
        }

        case AverageType::Fast:
        case AverageType::Slow:
        case AverageType::Infinite:
        case AverageType::Progressive:
            averagedMagnitude = m_magnitudeTemporal.append(i, magnitude, weight);
            averagedModule    = m_moduleTemporal.append(i, calibratedA, weight);
            averagedPhase     = m_phaseTemporal.append(i, p, weight);
            averagedTransfer  = m_transferTemporal.append(i, transfer, weight);
            break;
        }

        if (magnitudeAverageType() == MagnitudeAverageType::Vector) {
            averagedMagnitude = averagedTransfer.abs();
            if (!std::isfinite(averagedMagnitude)) {
                averagedMagnitude = 0.f;
            }
            averagedPhase.polar(averagedTransfer.arg());
        }

        m_ftdata[i].magnitude = averagedMagnitude;
        m_ftdata[i].module    = averagedModule;
        m_ftdata[i].phase     = averagedPhase;

        m_meters[i].add(calibratedA);
        m_ftdata[i].peakSquared = m_meters[i].peakSquared();
        m_ftdata[i].meanSquared = m_meters[i].value();
    }

    int t = 0;
    float kt = 1000.f / sampleRate();
    for (unsigned int i = 0, j = timeDomainSize() / 2 - 1; i < timeDomainSize(); i++, j++, t++) {

        if (t > static_cast<int>(timeDomainSize() / 2)) {
            t -= static_cast<int>(timeDomainSize());
            j -= timeDomainSize();
        }

        switch (averageType()) {
        case AverageType::Off:
            m_impulseData[j].value.real = m_deconvolution.get(i);
            break;
        case AverageType::LPF:
            m_impulseData[j].value.real = m_deconvLPFs[i](m_deconvolution.get(i));
            break;
        case AverageType::FIFO:
            m_deconvAvg.append(i, m_deconvolution.get(i));
            m_impulseData[j].value.real = m_deconvAvg.value(i);
            break;

        case AverageType::Fast:
        case AverageType::Slow:
        case AverageType::Infinite:
        case AverageType::Progressive:
            m_impulseData[j].value.real = m_deconvTemporal.append(i, m_deconvolution.get(i));
            break;
        }
        m_impulseData[j].time  = t * kt;//ms
    }
    if (m_estimatedDelay != m_delayFinder.maxIndex()) {
        m_estimatedDelay = m_delayFinder.maxIndex();
        emit estimatedChanged();
    }
}
Shared::Source Measurement::store()
{
    auto store = std::make_shared<Stored>();
    store->build( *this );
    store->autoName(name());

    QString avg;
    switch (m_averageType) {
    case Off:
        avg = "none";
        break;
    case LPF:
        avg = "LPF ";
        switch (m_filtersFrequency) {
        case Filter::Frequency::FourthHz:
            ;
            avg += "0.25 Hz";
            break;
        case Filter::Frequency::HalfHz:
            ;
            avg += "0.5 Hz";
            break;
        case Filter::Frequency::OneHz:
            ;
            avg += "1 Hz";
            break;
        }
        break;
    case FIFO:
        avg = "FIFO " + QString::number(m_average);
        break;
    case Fast:
        avg = "Fast";
        break;
    case Slow:
        avg = "Slow";
        break;
    case Infinite:
        avg = "Infinite";
        break;
    case Progressive:
        avg = "Progressive " + QString::number(m_averageDepth);
        break;
    }
    if (m_coherenceWeighting) {
        avg += " + coherence";
        if (!qFuzzyCompare(coherenceExponent(), 1.f)) {
            avg += "^" + QString::number(coherenceExponent(), 'f', 2);
        }
    }
    avg += (m_magnitudeAverageType == MagnitudeAverageType::Vector) ? " vector" : " polar";
    QString modeNote;
    switch (mode()) {
    case LFT:
        modeNote = "FT log time window";
        break;
    default:
        modeNote = "FFT power " + modeName();
    }
    store->setNotes(
        modeNote + "\t" +
        "delay: " + QString("%1").arg(1000.0 * delay() / sampleRate(), 0, 'f', 2) + "ms " +
        "gain: " + QString("%1").arg(gain(), 0, 'f', 2) + "dB " +
        "offset: " + QString("%1").arg(offset(), 0, 'f', 2) + "dB \n" +
        deviceName() + " " +
        QString::number(sampleRate() / 1000.f, 'f', 1) + "kHz " +
        QString(" R: %1").arg(referenceChanel() + 1) +
        QString(" M: %1").arg(dataChanel() + 1) +
        (polarity() ? " polarity inversed" : "") +
        (calibration() ? " calibrated" : "") + " \n" +
        "Window: " + WindowFunction::name(m_windowFunctionType) + "\t" +
        "Average: " + avg + "\n" +
        "Date: " + QDateTime::currentDateTime().toString()

    );

    return { store };
}
Shared::Source Measurement::clone() const
{
    auto cloned = std::make_shared<Measurement>();

    cloned->setActive(false);
    cloned->setMode(mode());
    cloned->setAverageType(averageType());
    cloned->setAverage(average());
    cloned->setAverageDepth(averageDepth());
    cloned->setMagnitudeAverageType(magnitudeAverageType());
    cloned->setCoherenceWeighting(coherenceWeighting());
    cloned->setCoherenceExponent(coherenceExponent());
    cloned->setLogWindowPointsPerOctave(logWindowPointsPerOctave());
    cloned->setFiltersFrequency(filtersFrequency());
    cloned->setPolarity(polarity());
    cloned->setDataChanel(dataChanel());
    cloned->setReferenceChanel(referenceChanel());
    cloned->setWindowFunctionType(windowFunctionType());

    cloned->setCalibration(calibration());
    cloned->m_calibrationList = m_calibrationList;
    cloned->m_calibrationLoaded = m_calibrationLoaded;
    cloned->applyCalibration();

    cloned->setDelay(delay());
    cloned->setGain(gain());
    cloned->setDeviceId(deviceId());

    cloned->setName(name());
    cloned->setActive(active());

    return std::static_pointer_cast<Abstract::Source>(cloned);
}
long Measurement::estimated() const noexcept
{
    if (m_estimatedDelay > m_delayFinder.size() / 2) {
        return m_estimatedDelay - m_delayFinder.size() + static_cast<long>(m_workingDelay);
    }
    return m_estimatedDelay + static_cast<long>(m_workingDelay);
}
long Measurement::estimatedDelta() const noexcept
{
    if (m_estimatedDelay > m_delayFinder.size() / 2) {
        return m_estimatedDelay - m_delayFinder.size();
    }
    return m_estimatedDelay;
}
bool Measurement::calibration() const noexcept
{
    return m_enableCalibration;
}
bool Measurement::calibrationLoaded() const noexcept
{
    return m_calibrationLoaded;
}
void Measurement::setCalibration(bool c) noexcept
{
    if (c != m_enableCalibration) {
        m_enableCalibration = c;
        emit calibrationChanged(m_enableCalibration);
    }
}
bool Measurement::loadCalibrationFile(const QUrl &fileName) noexcept
{
    QFile loadFile(fileName.toLocalFile());
    if (!loadFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Couldn't open file");
        return false;
    }

    QTextStream in(&loadFile);
    m_calibrationList.clear();
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.at(0).isDigit())
            continue;

        QStringList row;
        qDebug() << line.indexOf("\t")  << line.indexOf(",") ;
        if (line.indexOf("\t") > 0) {
            row = line.split("\t");
        } else if (line.indexOf(",") > 0) {
            row = line.split(",");
        }

        float frequency = 0.f, gain = 0.f, phase = 0.f;
        if (row.size() > 0) frequency   = row[0].toFloat();

        if (row.size() > 1) gain        = row[1].toFloat();
        else continue;

        if (row.size() > 2) phase       = row[2].toFloat();

        QVector<float> calibrationData;
        calibrationData << frequency << gain << phase;
        m_calibrationList << calibrationData;
    }
    loadFile.close();

    m_calibrationLoaded = (m_calibrationList.count() > 0);
    emit calibrationLoadedChanged(m_calibrationLoaded);
    applyCalibration();
    return m_calibrationLoaded;
}
void Measurement::applyCalibration()
{
    if (!m_calibrationLoaded || !m_calibrationList.size())
        return;

    m_calibrationGain.resize(static_cast<int>(frequencyDomainSize()));
    m_calibrationPhase.resize(static_cast<int>(frequencyDomainSize()));

    QVector<float> last = m_calibrationList[0];
    last[0] = 0.f;

    int j = 0;
    float
    kg, bg, kp, bp,
    g1, g2, f1, f2, p1, p2,
    g, p;
    bool inList = false;
    for (int i = 0; i < static_cast<int>(frequencyDomainSize()); ++i) {

        while (m_ftdata[i].frequency > m_calibrationList[j][0]) {
            last = m_calibrationList[j];
            if (j + 1 < m_calibrationList.size()) {
                ++j;
                inList = true;
            } else {
                inList = false;
                break;
            }
        }

        f1 = last[0];
        g1 = last[1];
        p1 = last[2];
        f2 = m_calibrationList[j][0];
        g2 = m_calibrationList[j][1];
        p2 = m_calibrationList[j][2];

        if (inList) {
            kg = (g2 - g1) / (f2 - f1);
            bg = g2 - f2 * kg;

            kp = (p2 - p1) / (f2 - f1);
            bp = p2 - f2 * kp;

            g = kg * m_ftdata[i].frequency + bg;
            p = kp * m_ftdata[i].frequency + bp;
        } else {
            g = m_calibrationList[j][1];
            p = m_calibrationList[j][2];
        }

        m_calibrationGain[i] = pow(10.f, 0.05f * g);
        m_calibrationPhase[i] = p * static_cast<float>(M_PI / 180.0);
    }
}
void Measurement::updateAudio()
{
    if (m_audioStream) {
        m_input.close();
        m_audioStream->disconnect(this);
        m_audioStream->close();
    }
    m_audioStream = nullptr;
    checkChannels();
    if (active()) {
        std::async([this]() {
            audio::Format format = audio::Client::getInstance()->deviceInputFormat(m_deviceId);
            setSampleRate(format.sampleRate);
            m_input.setCallback([this](const char *buffer, qint64 size) {
                writeData(buffer, size);
            });
            m_audioStream = audio::Client::getInstance()->openInput(m_deviceId, &m_input, format);
            if (!m_audioStream) {
                setError();
                return;
            }
            connect(m_audioStream, &audio::Stream::sampleRateChanged, this, &Measurement::onSampleRateChanged);
            emit audioFormatChanged();
        });
    }
}
void Measurement::checkChannels()
{
    audio::Format format = audio::Client::getInstance()->deviceInputFormat(m_deviceId);
    if (m_referenceChanel > format.channelCount) {
        setReferenceChanel(format.channelCount - 1);
    }
    if (m_dataChanel > format.channelCount) {
        setDataChanel(format.channelCount - 1);
    }
}

void Measurement::destroy()
{
    setActive(false);
    Abstract::Source::destroy();
}
void Measurement::resetAverage() noexcept
{
    auto reset = [](auto * f) {
        f->reset();
    };

    //next operations need time and shouldn't affect audio thread
    m_onReset.store(true);
    std::lock_guard<std::mutex> guard(m_dataMutex);

    m_dataFT.reset();
    m_deconvAvg.reset();
    m_moduleAvg.reset();
    m_magnitudeAvg.reset();
    m_pahseAvg.reset();
    m_transferAvg.reset();

    m_deconvTemporal.reset();
    m_moduleTemporal.reset();
    m_magnitudeTemporal.reset();
    m_phaseTemporal.reset();
    m_transferTemporal.reset();
    m_magnitudeGate.reset();
    m_moduleGate.reset();
    m_phaseGate.reset();
    m_transferGate.reset();

    m_moduleLPFs.each(reset);
    m_magnitudeLPFs.each(reset);
    m_deconvLPFs.each(reset);
    m_phaseLPFs.each(reset);
    m_transferLPFs.each(reset);

    m_meters.each(reset);
    m_loopBuffer.reset();
    m_levelMeters.reset();

    m_onReset.store(false);
}

Measurement::Meters::Meters() : m_reference(Weighting::Z, Meter::Slow)
{
    for (auto &curve : Weighting::allCurves) {
        for (auto &time : Meter::allTimes) {
            ::Abstract::LevelsData::Key key     {curve, time};
            Meter       meter   {curve, time};
            m_meters[key] = meter;
        }
    }
}

void Measurement::Meters::addToReference(const float &value)
{
    m_reference.add(value);
}

void Measurement::Meters::setSampleRate(unsigned int sampleRate)
{
    for (auto &&meter : m_meters) {
        meter.second.setSampleRate(sampleRate);
    }
    m_reference.setSampleRate(sampleRate);
}

void Measurement::Meters::add(float value)
{
    if (auto filter = m_filter) {
        value = filter->operator()(value);
    }
    for (auto &&meter : m_meters) {
        meter.second.add(value);
    }
}

void Measurement::Meters::reset()
{
    for (auto &&meter : m_meters) {
        meter.second.reset();
    }
    m_reference.reset();
}

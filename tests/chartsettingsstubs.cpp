/*
 * Test-only stubs for the chart stack.
 *
 * The settings tests only need VariableChart, its plots, axes and settings
 * persistence. Rendering and source-list plumbing are not exercised, so they
 * are kept out of the test build to avoid pulling the whole application in.
 */

#include <QColor>
#include <QSGNode>

#include "abstract/source.h"
#include "chart/coherenceplot.h"
#include "chart/crestfactorplot.h"
#include "chart/groupdelayplot.h"
#include "chart/impulseplot.h"
#include "chart/levelplot.h"
#include "chart/magnitudeplot.h"
#include "chart/nyquistplot.h"
#include "chart/phasedelayplot.h"
#include "chart/phaseplot.h"
#include "chart/rtaplot.h"
#include "chart/seriesesitem.h"
#include "chart/spectrogramplot.h"
#include "chart/stepplot.h"
#include "sourcelist.h"

using namespace Chart;

SeriesesItem::SeriesesItem(QQuickItem *parent, Plot *plot) :
    QQuickItem(parent),
    m_plot(plot)
{
}

SeriesesItem::~SeriesesItem()
{
}

void SeriesesItem::connectSources(SourceList *)
{
}

QSGNode *SeriesesItem::updatePaintNode(QSGNode *, UpdatePaintNodeData *)
{
    return nullptr;
}

void SeriesesItem::disconnectFromParent()
{
}

void SeriesesItem::clear()
{
}

bool SeriesesItem::appendDataSource(const Shared::Source &)
{
    return false;
}

void SeriesesItem::removeDataSource(const Shared::Source &)
{
}

void SeriesesItem::setSourceZIndex(const QUuid &, int)
{
}

void SeriesesItem::setHighlighted(const QUuid &)
{
}

Plot *SeriesesItem::plot() const
{
    return m_plot;
}

QUuid SeriesesItem::groupUuid() const
{
    return m_groupUuid;
}

void SeriesesItem::setGroupUuid(const QUuid &newUuid)
{
    m_groupUuid = newUuid;
}

void SeriesesItem::parentWidthChanged()
{
}

void SeriesesItem::parentHeightChanged()
{
}

void SeriesesItem::update()
{
}

void SeriesesItem::updateZOrders()
{
}

SeriesItem *RTAPlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *MagnitudePlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *PhasePlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *CoherencePlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *GroupDelayPlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *PhaseDelayPlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *ImpulsePlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *StepPlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *SpectrogramPlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *CrestFactorPlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *NyquistPlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SeriesItem *LevelPlot::createSeriesFromSource(const Shared::Source &)
{
    return nullptr;
}

SourceList::SourceList(QObject *parent, bool)
    : QObject(parent)
    , m_colorIndex(0)
    , m_selected(0)
{
}

void SourceList::appendItem(const Shared::Source &item, bool)
{
    m_items.append(item);
}

// The settings tests never expose the source list to QML, so the real
// meta-object is not needed; provide the QObject vtable entries only.
const QMetaObject *SourceList::metaObject() const
{
    return &QObject::staticMetaObject;
}

void *SourceList::qt_metacast(const char *name)
{
    return QObject::qt_metacast(name);
}

int SourceList::qt_metacall(QMetaObject::Call call, int id, void **arguments)
{
    return QObject::qt_metacall(call, id, arguments);
}

int SourceList::count() const noexcept
{
    return m_items.size();
}

const QVector<Shared::Source> &SourceList::items() const
{
    return m_items;
}

SourceList::iterator SourceList::begin() noexcept
{
    return m_items.begin();
}

SourceList::iterator SourceList::end() noexcept
{
    return m_items.end();
}

QUrl SourceList::currentFile() const noexcept
{
    return m_currentFile;
}

QUuid SourceList::firstSource() const noexcept
{
    return {};
}

int SourceList::indexOf(const Shared::Source &item) const noexcept
{
    return m_items.indexOf(item);
}

int SourceList::selectedIndex() const
{
    return m_selected;
}

void SourceList::setSelected(int selected)
{
    m_selected = selected;
}

Shared::Source SourceList::selected() const noexcept
{
    return m_items.value(m_selected);
}

Shared::Source SourceList::get(int i) const noexcept
{
    return m_items.value(i);
}

QColor SourceList::highlightColor() const
{
    return QColor(Qt::black);
}

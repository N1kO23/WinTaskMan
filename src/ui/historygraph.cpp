#include "historygraph.h"

#include <QLocale>
#include <QPainter>
#include <QPolygon>

#include <cmath>
#include <limits>

namespace
{
// Sizes and colours measured from the Windows 7 Task Manager.
constexpr int kGridColumnWidth = 12;
constexpr int kSampleStep = 2;  // pixels between samples
constexpr int kPlotSpacing = 8; // between the plots of a multi-plot graph
constexpr int kScaleGap = 5;    // between the scale's labels and its axis
const QColor kGridColor(0x00, 0x80, 0x40);
const QColor kScaleColor(0xff, 0xff, 0x00);

// The vertical ranges auto zoom picks from, in percent.
constexpr double kZoomLevels[] = {1, 5, 10, 25, 50, 100};

// Splits `length` pixels into `count` equal parts with `spacing` between them and returns
// where part `index` starts and ends.
QPair<int, int> span(int length, int count, int spacing, int index)
{
  const int start = (index * (length + spacing)) / count;
  const int end = ((index + 1) * (length + spacing)) / count - spacing;
  return {start, end};
}

QString percentText(double percent)
{
  return QLocale::system().toString(percent) + QLatin1String(" %");
}
} // namespace

HistoryGraph::HistoryGraph(QList<Series> series, QWidget *parent)
    : QWidget(parent), m_series(std::move(series))
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void HistoryGraph::addSample(const QList<QList<double>> &values)
{
  const int plotCount = values.value(0).size();
  if (plotCount != m_plots.size())
    m_plots = QList<QList<History>>(plotCount, QList<History>(m_series.size(), History(kMaxSamples)));

  for (int plot = 0; plot < m_plots.size(); ++plot)
  {
    for (int series = 0; series < m_series.size(); ++series)
      m_plots[plot][series].append(values.value(series).value(plot));
  }
  ++m_sampleCount;
  update();
}

void HistoryGraph::clear()
{
  m_plots.clear();
  update();
}

void HistoryGraph::setSeriesVisible(int series, bool visible)
{
  m_series[series].visible = visible;
  update();
}

void HistoryGraph::setScaleVisible(bool visible)
{
  m_scaleVisible = visible;
  update();
}

void HistoryGraph::setAutoZoom(bool autoZoom)
{
  m_autoZoom = autoZoom;
  update();
}

void HistoryGraph::setGridRowHeight(int pixels)
{
  m_gridRowHeight = pixels;
  update();
}

QSize HistoryGraph::sizeHint() const
{
  return QSize(300, 150);
}

QSize HistoryGraph::minimumSizeHint() const
{
  return QSize(60, 40);
}

void HistoryGraph::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  const QList<QRect> rects = plotRects();
  for (int i = 0; i < rects.size(); ++i)
    paintPlot(painter, rects[i], i < m_plots.size() ? &m_plots[i] : nullptr);
}

QList<QRect> HistoryGraph::plotRects() const
{
  // Arrange the plots in the number of rows that makes them closest to square.
  const int count = qMax<int>(1, m_plots.size());
  int rows = 1;
  double bestScore = std::numeric_limits<double>::max();
  for (int candidate = 1; candidate <= count; ++candidate)
  {
    const int columns = (count + candidate - 1) / candidate;
    const double plotWidth = double(width() - (columns - 1) * kPlotSpacing) / columns;
    const double plotHeight = double(height() - (candidate - 1) * kPlotSpacing) / candidate;
    if (plotWidth <= 0 || plotHeight <= 0)
      continue;

    const double score = std::abs(std::log(plotWidth / plotHeight));
    if (score < bestScore)
    {
      bestScore = score;
      rows = candidate;
    }
  }

  const int columns = (count + rows - 1) / rows;
  QList<QRect> rects;
  for (int i = 0; i < count; ++i)
  {
    const auto [left, right] = span(width(), columns, kPlotSpacing, i % columns);
    const auto [top, bottom] = span(height(), rows, kPlotSpacing, i / columns);
    rects.append(QRect(QPoint(left, top), QPoint(right - 1, bottom - 1)));
  }
  return rects;
}

double HistoryGraph::rangeFor(const QList<History> *plot, int plotWidth) const
{
  if (!m_autoZoom)
    return 100.0;

  // The smallest zoom level that fits the visible part of every visible line.
  double peak = 0.0;
  const int visibleSamples = plotWidth / kSampleStep + 1;
  for (int series = 0; plot && series < m_series.size(); ++series)
  {
    if (!m_series[series].visible)
      continue;

    const History &values = plot->at(series);
    for (qsizetype i = values.lastIndex(); i >= qMax(values.firstIndex(), values.lastIndex() - visibleSamples); --i)
      peak = qMax(peak, values.at(i));
  }
  for (const double level : kZoomLevels)
  {
    if (peak <= level)
      return level;
  }
  return 100.0;
}

void HistoryGraph::paintPlot(QPainter &painter, QRect rect, const QList<History> *plot) const
{
  painter.save();
  painter.fillRect(rect, Qt::black);

  // With the scale shown, it takes a strip on the left and the plot gets the rest.
  const int stripWidth = m_scaleVisible ? fontMetrics().horizontalAdvance(percentText(100)) + 2 * kScaleGap : 0;
  const QRect strip(rect.left(), rect.top(), stripWidth, rect.height());
  if (m_scaleVisible)
    rect.setLeft(strip.right() + 2);
  const double range = rangeFor(plot, rect.width());
  if (m_scaleVisible)
    paintScale(painter, strip, range);
  painter.setClipRect(rect);

  // Horizontal grid lines rise from the bottom; vertical ones scroll left with the samples.
  painter.setPen(kGridColor);
  for (int y = rect.bottom() - m_gridRowHeight; y >= rect.top(); y -= m_gridRowHeight)
    painter.drawLine(rect.left(), y, rect.right(), y);
  const int scroll = static_cast<int>(m_sampleCount * kSampleStep % kGridColumnWidth);
  for (int x = rect.right() - scroll; x >= rect.left(); x -= kGridColumnWidth)
    painter.drawLine(x, rect.top(), x, rect.bottom());

  for (int series = 0; plot && series < m_series.size(); ++series)
  {
    if (m_series[series].visible)
      paintLine(painter, rect, plot->at(series), range, QPen(m_series[series].color, m_series[series].lineWidth));
  }
  painter.restore();
}

void HistoryGraph::paintScale(QPainter &painter, const QRect &strip, double range) const
{
  // Labels for the top, middle and bottom of the range, right-aligned against a yellow axis.
  painter.setPen(kScaleColor);
  painter.drawLine(strip.right() + 1, strip.top(), strip.right() + 1, strip.bottom());
  const QRect labels = strip.adjusted(0, 0, -kScaleGap, 0);
  painter.drawText(labels, Qt::AlignRight | Qt::AlignTop, percentText(range));
  painter.drawText(labels, Qt::AlignRight | Qt::AlignVCenter, percentText(range / 2));
  painter.drawText(labels, Qt::AlignRight | Qt::AlignBottom, percentText(0));
}

void HistoryGraph::paintLine(QPainter &painter, const QRect &rect, const History &values, double range,
                             const QPen &pen) const
{
  // The newest sample sits on the right edge, each older one kSampleStep pixels further left.
  QPolygon points;
  int x = rect.right();
  for (qsizetype i = values.lastIndex(); i >= values.firstIndex() && x > rect.left() - kSampleStep; --i)
  {
    const double fraction = qBound(0.0, values.at(i) / range, 1.0);
    points.append(QPoint(x, rect.bottom() - qRound(fraction * (rect.height() - 1))));
    x -= kSampleStep;
  }

  painter.setPen(pen);
  painter.drawPolyline(points);
}

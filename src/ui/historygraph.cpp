#include "historygraph.h"

#include <QPainter>
#include <QPolygon>

#include <cmath>
#include <limits>

namespace
{
// Sizes and colours measured from the Windows 7 Task Manager.
constexpr int kGridSize = 12;
constexpr int kSampleStep = 2;  // pixels between samples
constexpr int kPlotSpacing = 8; // between the plots of a multi-plot graph
const QColor kGridColor(0x00, 0x80, 0x40);
const QColor kKernelColor(0xff, 0x00, 0x00);

// Splits `length` pixels into `count` equal parts with `spacing` between them and returns
// where part `index` starts and ends.
QPair<int, int> span(int length, int count, int spacing, int index)
{
  const int start = (index * (length + spacing)) / count;
  const int end = ((index + 1) * (length + spacing)) / count - spacing;
  return {start, end};
}
} // namespace

HistoryGraph::HistoryGraph(const QColor &lineColor, int lineWidth, QWidget *parent)
    : QWidget(parent), m_lineColor(lineColor), m_lineWidth(lineWidth)
{
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void HistoryGraph::addSample(const QList<double> &values, const QList<double> &kernelValues)
{
  if (values.size() != m_plots.size())
    m_plots = QList<Plot>(values.size());

  for (int i = 0; i < m_plots.size(); ++i)
  {
    m_plots[i].values.append(values[i]);
    m_plots[i].kernelValues.append(kernelValues.value(i));
  }
  ++m_sampleCount;
  update();
}

void HistoryGraph::setKernelTimesVisible(bool visible)
{
  m_kernelTimesVisible = visible;
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

void HistoryGraph::paintPlot(QPainter &painter, const QRect &rect, const Plot *plot) const
{
  painter.save();
  painter.setClipRect(rect);
  painter.fillRect(rect, Qt::black);

  // Horizontal grid lines hang from the top; vertical ones scroll left with the samples.
  painter.setPen(kGridColor);
  for (int y = rect.top() + kGridSize; y <= rect.bottom(); y += kGridSize)
    painter.drawLine(rect.left(), y, rect.right(), y);
  const int scroll = static_cast<int>(m_sampleCount * kSampleStep % kGridSize);
  for (int x = rect.right() - scroll; x >= rect.left(); x -= kGridSize)
    painter.drawLine(x, rect.top(), x, rect.bottom());

  if (plot)
  {
    paintLine(painter, rect, plot->values, QPen(m_lineColor, m_lineWidth));
    if (m_kernelTimesVisible)
      paintLine(painter, rect, plot->kernelValues, QPen(kKernelColor, 1));
  }
  painter.restore();
}

void HistoryGraph::paintLine(QPainter &painter, const QRect &rect, const QContiguousCache<double> &values,
                             const QPen &pen) const
{
  // The newest sample sits on the right edge, each older one kSampleStep pixels further left.
  QPolygon points;
  int x = rect.right();
  for (qsizetype i = values.lastIndex(); i >= values.firstIndex() && x > rect.left() - kSampleStep; --i)
  {
    const double fraction = qBound(0.0, values.at(i), 100.0) / 100.0;
    points.append(QPoint(x, rect.bottom() - qRound(fraction * (rect.height() - 1))));
    x -= kSampleStep;
  }

  painter.setPen(pen);
  painter.drawPolyline(points);
}

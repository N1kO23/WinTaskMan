#pragma once

#include <QColor>
#include <QContiguousCache>
#include <QList>
#include <QWidget>

// A scrolling usage history in the style of the Windows Task Manager: black plots with a green
// grid that moves along with the data. Shows one plot per value passed to addSample().
class HistoryGraph : public QWidget
{
  Q_OBJECT

public:
  HistoryGraph(const QColor &lineColor, int lineWidth, QWidget *parent = nullptr);

  // Appends one percentage to each plot; the number of plots follows the number of values.
  // `kernelValues` holds the kernel share of each value, drawn in red when kernel times are shown.
  void addSample(const QList<double> &values, const QList<double> &kernelValues = {});
  void setKernelTimesVisible(bool visible);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  static constexpr int kMaxSamples = 2048; // enough to fill a very wide plot

  struct Plot
  {
    QContiguousCache<double> values = QContiguousCache<double>(kMaxSamples);
    QContiguousCache<double> kernelValues = QContiguousCache<double>(kMaxSamples);
  };

  QList<QRect> plotRects() const;
  void paintPlot(QPainter &painter, const QRect &rect, const Plot *plot) const;
  void paintLine(QPainter &painter, const QRect &rect, const QContiguousCache<double> &values, const QPen &pen) const;

  QColor m_lineColor;
  int m_lineWidth;
  bool m_kernelTimesVisible = false;
  QList<Plot> m_plots;
  quint64 m_sampleCount = 0; // drives the grid scrolling
};

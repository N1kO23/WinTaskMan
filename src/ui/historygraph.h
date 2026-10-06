#pragma once

#include <QColor>
#include <QContiguousCache>
#include <QList>
#include <QWidget>

// A scrolling usage history in the style of the Windows Task Manager: black plots with a green
// grid that moves along with the data. Each plot draws one line per series, and the graph shows
// as many plots as each sample has values per series.
class HistoryGraph : public QWidget
{
  Q_OBJECT

public:
  struct Series
  {
    Series(const QColor &color, int lineWidth = 1, bool visible = true)
        : color(color), lineWidth(lineWidth), visible(visible)
    {
    }

    QColor color;
    int lineWidth;
    bool visible;
  };

  explicit HistoryGraph(QList<Series> series, QWidget *parent = nullptr);

  // Appends a sample of percentages: values[series][plot].
  void addSample(const QList<QList<double>> &values);
  void clear();
  void setSeriesVisible(int series, bool visible);

  // Options of the Networking tab's graphs: a scale down the left-hand side, a vertical range
  // that zooms in to fit the data, and taller grid cells.
  void setScaleVisible(bool visible);
  void setAutoZoom(bool autoZoom);
  void setGridRowHeight(int pixels);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  static constexpr int kMaxSamples = 2048; // enough to fill a very wide plot

  using History = QContiguousCache<double>;

  QList<QRect> plotRects() const;
  double rangeFor(const QList<History> *plot, int plotWidth) const;
  void paintPlot(QPainter &painter, QRect rect, const QList<History> *plot) const;
  void paintScale(QPainter &painter, const QRect &strip, double range) const;
  void paintLine(QPainter &painter, const QRect &rect, const History &values, double range, const QPen &pen) const;

  QList<Series> m_series;
  QList<QList<History>> m_plots; // [plot][series]
  bool m_scaleVisible = false;
  bool m_autoZoom = false;
  int m_gridRowHeight = 12;
  quint64 m_sampleCount = 0; // drives the grid scrolling
};

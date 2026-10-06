#include "usagemeter.h"

#include <QPainter>

namespace
{
// Sizes and colours measured from the Windows 7 Task Manager.
constexpr int kWidth = 78;
constexpr int kBarWidth = 16;
constexpr int kBarHeight = 2;
constexpr int kBarPitch = 3; // a bar and the gap above it
constexpr int kColumnGap = 1;
constexpr int kTopMargin = 5;
constexpr int kMinimumBars = 10;
const QColor kLitColor(0x00, 0xff, 0x00);
const QColor kUnlitColor(0x00, 0x80, 0x00);
} // namespace

UsageMeter::UsageMeter(QWidget *parent)
    : QWidget(parent)
{
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
}

void UsageMeter::setValue(double percent, const QString &text)
{
  m_percent = qBound(0.0, percent, 100.0);
  m_text = text;
  update();
}

QSize UsageMeter::sizeHint() const
{
  return QSize(kWidth, 150);
}

QSize UsageMeter::minimumSizeHint() const
{
  return QSize(kWidth, kTopMargin + kMinimumBars * kBarPitch + textBandHeight());
}

int UsageMeter::textBandHeight() const
{
  return fontMetrics().height() + 8;
}

void UsageMeter::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.fillRect(rect(), Qt::black);

  // The value sits in a band along the bottom; the bars fill the space above it.
  const int barsBottom = height() - textBandHeight();
  painter.setPen(kLitColor);
  painter.drawText(QRect(0, barsBottom, width(), textBandHeight()), Qt::AlignCenter, m_text);

  // Two columns of bars, lit from the bottom up. Unlit bars are a dim checkerboard.
  const int barCount = qMax(0, (barsBottom - kTopMargin) / kBarPitch);
  const int litCount = qRound(m_percent / 100.0 * barCount);
  const int left = (width() - (2 * kBarWidth + kColumnGap)) / 2;
  const QBrush unlitBrush(kUnlitColor, Qt::Dense4Pattern);
  for (int bar = 0; bar < barCount; ++bar)
  {
    const int top = barsBottom - (bar + 1) * kBarPitch;
    for (int column = 0; column < 2; ++column)
    {
      const QRect barRect(left + column * (kBarWidth + kColumnGap), top, kBarWidth, kBarHeight);
      if (bar < litCount)
        painter.fillRect(barRect, kLitColor);
      else
        painter.fillRect(barRect, unlitBrush);
    }
  }
}

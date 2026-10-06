#pragma once

#include <QString>
#include <QWidget>

// A vertical LED bar meter in the style of the Windows Task Manager, with its value written
// underneath.
class UsageMeter : public QWidget
{
  Q_OBJECT

public:
  explicit UsageMeter(QWidget *parent = nullptr);

  void setValue(double percent, const QString &text);

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  int textBandHeight() const;

  double m_percent = 0.0;
  QString m_text;
};

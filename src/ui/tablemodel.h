#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QString>
#include <QVariant>

struct TableCell
{
  TableCell(QString text = QString(), QVariant sortKey = QVariant())
      : text(std::move(text)), sortKey(std::move(sortKey))
  {
  }

  bool operator==(const TableCell &other) const { return text == other.text && sortKey == other.sortKey; }
  bool operator!=(const TableCell &other) const { return !(*this == other); }

  QString text;
  QVariant sortKey; // sorted on instead of the text when set, e.g. the number behind "1,234 K"
};

struct TableRow
{
  QString key; // identifies the row from one update to the next
  QList<TableCell> cells;
};

struct TableColumn
{
  TableColumn(QString title, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
      : title(std::move(title)), alignment(alignment)
  {
  }

  QString title;
  Qt::Alignment alignment;
};

// A read-only table whose contents are replaced on every refresh. Rows are matched by key,
// so rows that stay (and any selection on them) survive the update.
class TableModel : public QAbstractTableModel
{
  Q_OBJECT

public:
  static constexpr int SortRole = Qt::UserRole;
  static constexpr int KeyRole = Qt::UserRole + 1;

  explicit TableModel(QList<TableColumn> columns, QObject *parent = nullptr);

  // Removes the rows whose key is missing from `rows`, updates the others in place and
  // appends the new ones. When several rows share a key, the last one wins.
  void setRows(QList<TableRow> rows);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
  QList<TableColumn> m_columns;
  QList<TableRow> m_rows;
};

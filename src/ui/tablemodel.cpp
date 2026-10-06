#include "tablemodel.h"

#include <QHash>

TableModel::TableModel(QList<TableColumn> columns, QObject *parent)
    : QAbstractTableModel(parent), m_columns(std::move(columns))
{
}

void TableModel::setRows(QList<TableRow> rows)
{
  // Map each key to the index of its last row in `rows`. Entries are taken out as they are
  // matched, so whatever remains at the end is new.
  QHash<QString, int> unmatched;
  unmatched.reserve(rows.size());
  for (int i = 0; i < rows.size(); ++i)
    unmatched.insert(rows[i].key, i);

  // Remove the rows whose key is gone, one contiguous block at a time from the bottom up.
  for (int last = m_rows.size() - 1; last >= 0; --last)
  {
    if (unmatched.contains(m_rows[last].key))
      continue;

    int first = last;
    while (first > 0 && !unmatched.contains(m_rows[first - 1].key))
      --first;
    beginRemoveRows(QModelIndex(), first, last);
    m_rows.remove(first, last - first + 1);
    endRemoveRows();
    last = first;
  }

  // Update the remaining rows in place.
  int firstChanged = -1;
  int lastChanged = -1;
  for (int row = 0; row < m_rows.size(); ++row)
  {
    TableRow &incoming = rows[unmatched.take(m_rows[row].key)];
    if (incoming.cells == m_rows[row].cells)
      continue;

    m_rows[row].cells = std::move(incoming.cells);
    if (firstChanged < 0)
      firstChanged = row;
    lastChanged = row;
  }
  if (firstChanged >= 0)
    emit dataChanged(index(firstChanged, 0), index(lastChanged, columnCount() - 1));

  // Append the rows with new keys, in the order they were given.
  QList<TableRow> added;
  for (int i = 0; i < rows.size(); ++i)
  {
    if (unmatched.value(rows[i].key, -1) == i)
      added.append(std::move(rows[i]));
  }
  if (!added.isEmpty())
  {
    beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size() + added.size() - 1);
    m_rows.append(std::move(added));
    endInsertRows();
  }
}

int TableModel::rowCount(const QModelIndex &parent) const
{
  return parent.isValid() ? 0 : m_rows.size();
}

int TableModel::columnCount(const QModelIndex &parent) const
{
  return parent.isValid() ? 0 : m_columns.size();
}

QVariant TableModel::data(const QModelIndex &index, int role) const
{
  if (!index.isValid() || index.row() >= m_rows.size() || index.column() >= m_columns.size())
    return QVariant();

  const TableRow &row = m_rows[index.row()];
  const TableCell cell = row.cells.value(index.column());
  switch (role)
  {
  case Qt::DisplayRole:
    return cell.text;
  case Qt::TextAlignmentRole:
    return static_cast<int>(m_columns[index.column()].alignment);
  case SortRole:
    return cell.sortKey.isValid() ? cell.sortKey : QVariant(cell.text);
  case KeyRole:
    return row.key;
  default:
    return QVariant();
  }
}

QVariant TableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section >= 0 && section < m_columns.size())
    return m_columns[section].title;
  return QVariant();
}

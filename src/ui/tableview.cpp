#include "tableview.h"
#include "tablemodel.h"

#include <QSortFilterProxyModel>

TableView::TableView(TableModel *model, QWidget *parent)
    : QTreeView(parent)
{
  auto *proxy = new QSortFilterProxyModel(this);
  proxy->setSourceModel(model);
  proxy->setSortRole(TableModel::SortRole);
  proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
  setModel(proxy);

  setRootIsDecorated(false);
  setUniformRowHeights(true);
  setSortingEnabled(true);
  sortByColumn(0, Qt::AscendingOrder);
  setStyleSheet(QStringLiteral("QTreeView { border: 1px solid gray; font-size: 11px; }"));
}

QString TableView::selectedKey() const
{
  const QModelIndexList rows = selectionModel()->selectedRows();
  return rows.isEmpty() ? QString() : rows.first().data(TableModel::KeyRole).toString();
}

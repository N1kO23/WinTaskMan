#pragma once

#include <QTreeView>

class TableModel;

// A sortable list in the classic task manager style, showing a TableModel through a sort proxy.
class TableView : public QTreeView
{
  Q_OBJECT

public:
  explicit TableView(TableModel *model, QWidget *parent = nullptr);

  // The key of the selected row, or an empty string when nothing is selected.
  QString selectedKey() const;
};

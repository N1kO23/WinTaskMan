#include "ui/tablemodel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

namespace
{
TableRow row(const QString &key, const QString &value)
{
  return TableRow{key, {key, value}};
}

QStringList columnText(const TableModel &model, int column)
{
  QStringList values;
  for (int row = 0; row < model.rowCount(); ++row)
    values.append(model.index(row, column).data().toString());
  return values;
}
} // namespace

class TestTableModel : public QObject
{
  Q_OBJECT

private slots:
  void addsUpdatesAndRemovesRowsByKey();
  void keepsRowsWhoseKeyRemains();
  void emitsNothingWhenNothingChanged();
  void collapsesDuplicateKeys();
  void exposesSortKeyAlignmentAndKey();
};

void TestTableModel::addsUpdatesAndRemovesRowsByKey()
{
  TableModel model({QStringLiteral("Name"), QStringLiteral("Value")});
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

  model.setRows({row("a", "1"), row("b", "2"), row("c", "3"), row("d", "4"), row("e", "5")});
  QCOMPARE(columnText(model, 0), QStringList({"a", "b", "c", "d", "e"}));

  // Removes a, c and e (three separate blocks), updates d and appends f.
  model.setRows({row("b", "2"), row("d", "40"), row("f", "6")});
  QCOMPARE(columnText(model, 0), QStringList({"b", "d", "f"}));
  QCOMPARE(columnText(model, 1), QStringList({"2", "40", "6"}));

  model.setRows({});
  QCOMPARE(model.rowCount(), 0);
}

void TestTableModel::keepsRowsWhoseKeyRemains()
{
  TableModel model({QStringLiteral("Name"), QStringLiteral("Value")});
  model.setRows({row("a", "1"), row("b", "2"), row("c", "3")});
  const QPersistentModelIndex b = model.index(1, 0);

  model.setRows({row("b", "20"), row("c", "3")});
  QVERIFY(b.isValid());
  QCOMPARE(b.row(), 0);
  QCOMPARE(b.data().toString(), QStringLiteral("b"));
  QCOMPARE(model.index(b.row(), 1).data().toString(), QStringLiteral("20"));
}

void TestTableModel::emitsNothingWhenNothingChanged()
{
  TableModel model({QStringLiteral("Name"), QStringLiteral("Value")});
  model.setRows({row("a", "1"), row("b", "2")});

  QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
  QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
  QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
  model.setRows({row("b", "2"), row("a", "1")});
  QCOMPARE(changed.count(), 0);
  QCOMPARE(inserted.count(), 0);
  QCOMPARE(removed.count(), 0);
}

void TestTableModel::collapsesDuplicateKeys()
{
  TableModel model({QStringLiteral("Name"), QStringLiteral("Value")});
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);

  model.setRows({row("local", "first"), row("other", "x"), row("local", "second")});
  QCOMPARE(columnText(model, 0), QStringList({"other", "local"}));
  QCOMPARE(columnText(model, 1), QStringList({"x", "second"}));

  model.setRows({row("local", "third"), row("local", "fourth")});
  QCOMPARE(columnText(model, 1), QStringList({"fourth"}));
}

void TestTableModel::exposesSortKeyAlignmentAndKey()
{
  TableModel model({QStringLiteral("Name"), {QStringLiteral("Memory"), Qt::AlignRight | Qt::AlignVCenter}});
  model.setRows({TableRow{QStringLiteral("42"), {QStringLiteral("init"), {QStringLiteral("1,024 K"), 1024}}}});

  const QModelIndex name = model.index(0, 0);
  const QModelIndex memory = model.index(0, 1);
  QCOMPARE(name.data(TableModel::SortRole), QVariant(QStringLiteral("init"))); // falls back to the text
  QCOMPARE(memory.data(Qt::DisplayRole), QVariant(QStringLiteral("1,024 K")));
  QCOMPARE(memory.data(TableModel::SortRole), QVariant(1024));
  QCOMPARE(memory.data(TableModel::KeyRole), QVariant(QStringLiteral("42")));
  QCOMPARE(memory.data(Qt::TextAlignmentRole).toInt(), int(Qt::AlignRight | Qt::AlignVCenter));
  QCOMPARE(model.headerData(1, Qt::Horizontal).toString(), QStringLiteral("Memory"));
}

QTEST_GUILESS_MAIN(TestTableModel)
#include "tst_tablemodel.moc"

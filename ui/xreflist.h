#pragma once

#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QModelIndex>
#include <QtWidgets/QListView>
#include <QtWidgets/QStyledItemDelegate>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QTableView>
#include <QtWidgets/QComboBox>
#include <QtGui/QImage>
#include <vector>
#include <deque>
#include <memory>

#include "binaryninjaapi.h"
#include "dockhandler.h"
#include "viewframe.h"
#include "fontsettings.h"

class XrefHeader;
class XrefItem
{
public:
	enum XrefDirection
	{
		Forward, // current address is addressing another address
		Backward // current address is being referenced by another address
	};

	enum XrefType
	{
		DataXrefType,
		CodeXrefType,
		VariableXrefType
	};

protected:
	FunctionRef m_func;
	ArchitectureRef m_arch;
	uint64_t m_addr;
	XrefType m_type;
	XrefDirection m_direction;
	mutable XrefHeader* m_parentItem;
	mutable int m_size;


public:
	explicit XrefItem();
	explicit XrefItem(XrefHeader* parent);
	explicit XrefItem(XrefHeader* parent, XrefType type);
	explicit XrefItem(BinaryNinja::ReferenceSource referenceSource, XrefType type, XrefDirection direction);
	explicit XrefItem(const XrefItem& ref);
	virtual ~XrefItem();

	XrefDirection direction() const { return m_direction; }
	FunctionRef func() const { return m_func; }
	ArchitectureRef arch() const { return m_arch; }
	uint64_t addr() const { return m_addr; }
	XrefType type() const { return m_type; }
	int size() const { return m_size; }
	void setSize(int size) const { m_size = size; }
	void setParent(XrefHeader* parent) const { m_parentItem = parent; }
	virtual XrefItem* parent() const { return (XrefItem*)m_parentItem; }
	virtual XrefItem* child(int) const { return nullptr; }
	virtual int childCount() const { return 0; }

	int row() const;
	bool operator==(const XrefItem& other);
	bool operator!=(const XrefItem& other);
};


class XrefHeader: public XrefItem
{
protected:
	QString m_name;
public:
	XrefHeader();
	XrefHeader(const QString& name, XrefItem::XrefType type, XrefHeader* parent);
	virtual ~XrefHeader() {}

	virtual QString name() const { return m_name; }
	XrefItem::XrefType type() const { return m_type; }

	virtual void appendChild(XrefItem* ref) = 0;
	virtual int row(const XrefItem* item) const = 0;
	virtual XrefItem* child(int i) const = 0;
	virtual int childCount() const = 0;
};


class XrefFunctionHeader : public XrefHeader
{
	std::deque<XrefItem*> m_refs;
	FunctionRef m_func;
public:
	XrefFunctionHeader();
	XrefFunctionHeader(FunctionRef func, XrefHeader* parent, XrefItem* child);
	virtual int childCount() const override { return m_refs.size(); }
	virtual uint64_t addr() const { return m_func->GetStart(); }
	virtual void appendChild(XrefItem* ref) override;
	virtual int row(const XrefItem* item) const override;
	virtual XrefItem* child(int i) const override;
};


class XrefCodeReferences: public XrefHeader
{
	std::map<FunctionRef, XrefFunctionHeader*> m_refs;
public:
	XrefCodeReferences(XrefHeader* parent);
	virtual ~XrefCodeReferences();
	virtual int childCount() const override { return m_refs.size(); }
	virtual void appendChild(XrefItem* ref) override;
	XrefHeader* parentOf(XrefItem* ref) const;
	virtual int row(const XrefItem* item) const override;
	virtual XrefItem* child(int i) const override;
};


class XrefDataReferences: public XrefHeader
{
	std::deque<XrefItem*> m_refs;
public:
	XrefDataReferences(XrefHeader* parent);
	virtual ~XrefDataReferences();
	virtual int childCount() const override { return m_refs.size(); };
	virtual void appendChild(XrefItem* ref) override;
	virtual int row(const XrefItem* item) const override;
	virtual XrefItem* child(int i) const override;
};


class XrefRoot: public XrefHeader
{
	mutable std::map<int, XrefHeader*> m_refs;
public:
	XrefRoot();
	XrefRoot(XrefRoot&& root);
	~XrefRoot();
	virtual int childCount() const override { return m_refs.size(); }
	void appendChild(XrefItem* ref) override;
	XrefHeader* parentOf(XrefItem* ref);
	virtual int row(const XrefItem* item) const override;
	virtual XrefHeader* child(int i) const override;
};

class BINARYNINJAUIAPI CrossReferenceTreeModel : public QAbstractItemModel
{
	Q_OBJECT

	XrefRoot* m_rootItem;
	QWidget* m_owner;
	BinaryViewRef m_data;
	std::vector<XrefItem> m_refs;

public:
	CrossReferenceTreeModel(QWidget* parent, BinaryViewRef data);
	virtual ~CrossReferenceTreeModel() {}

	virtual QModelIndex index(int row, int col, const QModelIndex& parent = QModelIndex()) const override;
	virtual QVariant data(const QModelIndex& i, int role) const override;
	virtual QModelIndex parent(const QModelIndex& i) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	virtual bool hasChildren(const QModelIndex& parent) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex nextValidIndex(const QModelIndex& current) const;
	QModelIndex prevValidIndex(const QModelIndex& current) const;
	bool selectRef(XrefItem* ref, QItemSelectionModel* selectionModel);

	bool setModelData(std::vector<XrefItem>& refs, QItemSelectionModel* selectionModel, bool& selectionUpdated);
};


class BINARYNINJAUIAPI CrossReferenceTableModel : public QAbstractTableModel
{
	Q_OBJECT

	QWidget* m_owner;
	BinaryViewRef m_data;
	std::vector<XrefItem> m_refs;
public:
	enum ColumnHeaders
	{
		Direction = 0,
		Address = 1,
		Function = 2,
		Preview = 3
	};

	CrossReferenceTableModel(QWidget* parent, BinaryViewRef data);
	virtual ~CrossReferenceTableModel() {}

	virtual QModelIndex index(int row, int col, const QModelIndex& parent = QModelIndex()) const override;
	virtual QVariant data(const QModelIndex& i, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override { (void)parent; return m_refs.size(); };
	virtual QModelIndex parent(const QModelIndex& i) const override { (void)i; return QModelIndex(); }
	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override { (void) parent; return 4;};
	virtual QVariant headerData(int column, Qt::Orientation orientation, int role) const override;
	virtual bool hasChildren(const QModelIndex&) const override { return false; }
	bool setModelData(std::vector<XrefItem>& refs, QItemSelectionModel* selectionModel, bool& selectionUpdated);
	const XrefItem& getRow(int idx);
};


class BINARYNINJAUIAPI CrossReferenceItemDelegate: public QStyledItemDelegate
{
	Q_OBJECT

	QFont m_font;
	int m_baseline, m_charWidth, m_charHeight, m_charOffset;
	QImage m_xrefTo, m_xrefFrom;
	bool m_table;

public:
	CrossReferenceItemDelegate(QWidget* parent, bool table);

	void updateFonts();

	virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& idx) const override;
	virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& idx) const override;
	virtual void paintTreeRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& idx) const;
	virtual void paintTableRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& idx) const;
	virtual QImage DrawArrow(bool direction) const;
};


class CrossReferenceFilterProxyModel : public QSortFilterProxyModel
{
	Q_OBJECT

public:
	CrossReferenceFilterProxyModel(QObject* parent = 0);
protected:
	// bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
	virtual bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
	// virtual QModelIndex index(int row, int column, const QModelIndex &parent) const override;
};

class CrossReferenceWidget;
class BINARYNINJAUIAPI CrossReferenceContainer
{
protected:
	ViewFrame* m_view;
	CrossReferenceWidget* m_parent;
	BinaryViewRef m_data;
	UIActionHandler m_actionHandler;
public:
	CrossReferenceContainer(CrossReferenceWidget* parent, ViewFrame* view, BinaryViewRef data);
	virtual QModelIndex translateIndex(const QModelIndex& idx) const { return idx; }
	virtual bool getReference(const QModelIndex& idx, FunctionRef& func, uint64_t& addr) const = 0;
	virtual QModelIndex nextIndex() = 0;
	virtual QModelIndex prevIndex() = 0;
	virtual QModelIndexList selectedIndexes() const = 0;
	virtual bool hasSelection() const = 0;
	virtual void setNewSelection(std::vector<XrefItem>& refs, bool newRefTarget) = 0;
	virtual void updateFonts() = 0;
};


class BINARYNINJAUIAPI CrossReferenceTree: public QTreeView, public CrossReferenceContainer
{
	Q_OBJECT

	CrossReferenceTreeModel* m_tree;
	CrossReferenceItemDelegate* m_itemDelegate;

protected:
	void drawBranches(QPainter *painter, const QRect &rect, const QModelIndex &index) const override;
	virtual bool getReference(const QModelIndex& idx, FunctionRef& func, uint64_t& addr) const override;

public:
	CrossReferenceTree(CrossReferenceWidget* parent, ViewFrame* view, BinaryViewRef data);
	virtual ~CrossReferenceTree();

	void setNewSelection(std::vector<XrefItem>& refs, bool newRefTarget) override;
	virtual QModelIndex nextIndex() override;
	virtual QModelIndex prevIndex() override;
	virtual bool hasSelection() const override { return selectedIndexes().size() != 0; }
	virtual void mouseMoveEvent(QMouseEvent* e) override;
	virtual void mousePressEvent(QMouseEvent* e) override;
	virtual void keyPressEvent(QKeyEvent* e) override;
	virtual QModelIndexList selectedIndexes() const override { return QTreeView::selectedIndexes(); }
	virtual void updateFonts() override;
};


class BINARYNINJAUIAPI CrossReferenceTable: public QTableView, public CrossReferenceContainer
{
	Q_OBJECT

	CrossReferenceTableModel* m_table;
	CrossReferenceItemDelegate* m_itemDelegate;
	CrossReferenceFilterProxyModel* m_model;

	int m_charWidth = 0;
	int m_charHeight = 0;
protected:
	virtual int sizeHintForRow(int row) const override;
	virtual int sizeHintForColumn(int column) const override;

public:
	CrossReferenceTable(CrossReferenceWidget* parent, ViewFrame* view, BinaryViewRef data);
	virtual ~CrossReferenceTable();

	void setNewSelection(std::vector<XrefItem>& refs, bool newRefTarget) override;
	virtual QModelIndex nextIndex() override;
	virtual QModelIndex prevIndex() override;
	virtual bool hasSelection() const override { return selectedIndexes().size() != 0; }
	virtual QModelIndexList selectedIndexes() const override { return QTableView::selectedIndexes(); }
	virtual bool getReference(const QModelIndex& idx, FunctionRef& func, uint64_t& addr) const override;
	virtual void mouseMoveEvent(QMouseEvent* e) override;
	virtual void mousePressEvent(QMouseEvent* e) override;
	virtual void keyPressEvent(QKeyEvent* e) override;
	virtual QModelIndex translateIndex(const QModelIndex& idx) const override { return m_model->mapToSource(idx); }
	virtual void updateFonts() override;
};


class BINARYNINJAUIAPI CrossReferenceWidget: public QWidget, public DockContextHandler
{
	Q_OBJECT
	Q_INTERFACES(DockContextHandler)

	ViewFrame* m_view;
	BinaryViewRef m_data;
	QAbstractItemView* m_object;
	CrossReferenceTable* m_table;
	CrossReferenceTree* m_tree;
	CrossReferenceContainer* m_container;
	bool m_useTableView;

	QTimer* m_hoverTimer;
	QPoint m_hoverPos;
	QStringList m_historyEntries;
	int m_historySize;
	QComboBox* m_combo;

	uint64_t m_curRefTarget = 0;
	uint64_t m_curRefTargetEnd = 0;
	bool m_navigating = false;
	bool m_navToNextOrPrevStarted = false;

	virtual void contextMenuEvent(QContextMenuEvent* event) override;
	virtual void wheelEvent(QWheelEvent* e) override;
public:
	CrossReferenceWidget(ViewFrame* view, BinaryViewRef data);
	virtual void notifyFontChanged() override;
	virtual bool shouldBeVisible(ViewFrame* frame) override;

	virtual void setCurrentSelection(uint64_t begin, uint64_t end);
	virtual void navigateToNext();
	virtual void navigateToPrev();
	virtual bool selectFirstRow();
	virtual bool hasSelection() const;
	virtual void goToReference(const QModelIndex& idx);

	virtual void restartHoverTimer(QMouseEvent* e);
	virtual void startHoverTimer(QMouseEvent* e);
	virtual void keyPressEvent(QKeyEvent* e) override;
	void useTableView(bool tableView, bool init);
	bool tableView() const { return m_useTableView; }

private Q_SLOTS:
	void hoverTimerEvent();

public Q_SLOTS:
	void referenceActivated(const QModelIndex& idx);
};
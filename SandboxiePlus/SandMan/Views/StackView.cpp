#include "stdafx.h"
#include "..\SandMan.h"
#include "StackView.h"
#include "..\..\MiscHelpers\Common\Common.h"

CStackView::CStackView(QWidget *parent)
	: CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	// Stack List
	m_pStackList = new QTreeWidgetEx();
	m_pStackList->setItemDelegate(new CTreeItemDelegate());
	//m_pStackList->setHeaderLabels(tr("#|Symbol|Stack address|Frame address|Control address|Return address|Stack parameters|File info").split("|"));
	m_pStackList->setHeaderLabels(tr("#|Symbol").split("|"));
	m_pStackList->setMinimumHeight(50);

	m_pStackList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pStackList->setSortingEnabled(false);

	m_pStackList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pStackList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pMainLayout->addWidget(CFinder::AddFinder(m_pStackList, this, true, &m_pFinder));
	// 

	m_bIsInvalid = false;

	//m_pMenu = new QMenu();
	AddPanelItemsToMenu();

	m_pStackList->header()->restoreState(theConf->GetBlob("MainWindow/StackView_Columns"));
}

CStackView::~CStackView()
{
	theConf->SetBlob("MainWindow/StackView_Columns", m_pStackList->header()->saveState());
}

void CStackView::Invalidate()
{
	m_bIsInvalid = true;

	for (int i = 0; i < m_pStackList->topLevelItemCount(); i++)
	{
		for(int j=0; j < m_pStackList->columnCount(); j++)
			m_pStackList->topLevelItem(i)->setForeground(j, Qt::lightGray);
	}
}

void CStackView::ShowStack(const QVector<quint64>& Stack, const CBoxedProcessPtr& pProcess)
{
	int i = 0;
	for (; i < Stack.count(); i++)
	{
		quint64 Address = Stack[i];
		
		QTreeWidgetItem* pItem;
		if (i >= m_pStackList->topLevelItemCount())
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(eStack, Qt::UserRole, i);
			pItem->setText(eStack, QString::number(i));
			m_pStackList->addTopLevelItem(pItem);
		}
		else
			pItem = m_pStackList->topLevelItem(i);

		if (m_bIsInvalid)
		{
			for (int j = 0; j < m_pStackList->columnCount(); j++)
				pItem->setForeground(j, Qt::black);
		}

		QString Symbol = pProcess->GetSymbol(Address);
		if (Symbol.isNull()) Symbol = FormatAddress(Address);
		pItem->setText(eSymbol, Symbol);
	}

	for (; i < m_pStackList->topLevelItemCount(); )
		delete m_pStackList->topLevelItem(i);

	CPanelWidgetEx::ApplyFilter(m_pStackList, m_pFinder->isVisible() ? &m_pFinder->GetSearchExp() : NULL);

	m_bIsInvalid = false;
}

void CStackView::SetFilter(const QRegularExpression& Exp, int iOptions, int Col)
{
	CPanelWidgetEx::ApplyFilter(m_pStackList, &m_pFinder->GetSearchExp());
}
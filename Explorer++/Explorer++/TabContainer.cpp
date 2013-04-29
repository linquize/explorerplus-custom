/******************************************************************
 *
 * Project: Explorer++
 * File: TabContainer.cpp
 * License: GPL - See COPYING in the top level directory
 *
 * Manages the main tab control.
 *
 * Written by David Erceg
 * www.explorerplusplus.com
 *
 *****************************************************************/

#include "stdafx.h"
#include <list>
#include "Explorer++_internal.h"
#include "TabContainer.h"


CTabContainer::CTabContainer(HWND hTabCtrl,CShellBrowser **pShellBrowsers,IExplorerplusplus *pexpp) :
m_hTabCtrl(hTabCtrl),
m_pShellBrowsers(pShellBrowsers),
m_pexpp(pexpp)
{

}

CTabContainer::~CTabContainer()
{

}

int CTabContainer::GetSelection()
{
	int Index = TabCtrl_GetCurSel(m_hTabCtrl);
	assert(Index != -1);

	return Index;
}

void CTabContainer::SetSelection(int Index)
{
	assert(Index >= 0);
	m_pexpp->SetTabSelection(Index);
}

CShellBrowser *CTabContainer::GetBrowserForTab(int Index)
{
	TCITEM tcItem;
	tcItem.mask = TCIF_PARAM;
	TabCtrl_GetItem(m_hTabCtrl,Index,&tcItem);

	return m_pShellBrowsers[tcItem.lParam];
}
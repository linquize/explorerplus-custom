/******************************************************************
 *
 * Project: Explorer++
 * File: ListView.cpp
 * License: GPL - See COPYING in the top level directory
 *
 * Manages the main listview.
 *
 * Written by David Erceg
 * www.explorerplusplus.com
 *
 *****************************************************************/

#include "stdafx.h"
#include "Explorer++_internal.h"
#include "ListView.h"
#include "../Helper/ListViewHelper.h"
#include "../Helper/Macros.h"


CListViewEdit *CListViewEdit::CreateNew(HWND hwnd,int ItemIndex,IExplorerplusplus *pexpp)
{
	return new CListViewEdit(hwnd,ItemIndex,pexpp);
}

CListViewEdit::CListViewEdit(HWND hwnd,int ItemIndex,IExplorerplusplus *pexpp) :
CBaseWindow(hwnd),
m_hEdit(hwnd),
m_ItemIndex(ItemIndex),
m_pexpp(pexpp),
m_RenameStage(RENAME_FILENAME),
m_BeginRename(true)
{
	
}

CListViewEdit::~CListViewEdit()
{
	
}

void CListViewEdit::OnEMSetSel(WPARAM &wParam,LPARAM &lParam)
{
	/* When editing an item, the listview control
	will first deselect, then select all text. If
	an item has been put into edit mode, and the
	listview attempts to select all text, modify the
	message so that only text up to the extension
	(if any) is selected. */
	if(m_BeginRename &&
		wParam == 0 &&
		lParam == -1)
	{
		int Index = GetExtensionIndex();

		if(Index != -1)
		{
			wParam = 0;
			lParam = Index;
		}

		m_BeginRename = false;
	}
}

INT_PTR CListViewEdit::OnPrivateMessage(UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_APP_KEYDOWN:
		switch(wParam)
		{
		case VK_F2:
			{
				int Index = GetExtensionIndex();

				if(Index != -1)
				{
					switch(m_RenameStage)
					{
					case RENAME_FILENAME:
						SendMessage(m_hEdit,EM_SETSEL,Index + 1,-1);
						m_RenameStage = RENAME_EXTENSION;
						break;

					case RENAME_EXTENSION:
						SendMessage(m_hEdit,EM_SETSEL,0,-1);
						m_RenameStage = RENAME_ENTIRE;
						break;

					case RENAME_ENTIRE:
						SendMessage(m_hEdit,EM_SETSEL,0,Index);
						m_RenameStage = RENAME_FILENAME;
						break;

					default:
						assert(false);
						break;
					}
				}
			}
			break;

		case VK_TAB:
			{
				HWND hListView = GetParent(m_hEdit);

				int iSel = ListView_GetNextItem(hListView,-1,LVNI_ALL|LVNI_SELECTED);
				NListView::ListView_SelectItem(hListView,iSel,FALSE);

				int nItems = ListView_GetItemCount(hListView);

				int iNewSel;

				if(iSel == (nItems - 1))
				{
					iNewSel = 0;
				}
				else
				{
					iNewSel = iSel + 1;
				}

				ListView_EditLabel(hListView,iNewSel);
			}
			break;
		}
		break;
	}

	return 0;
}

int CListViewEdit::GetExtensionIndex()
{
	TCHAR szFileName[MAX_PATH];
	GetWindowText(m_hEdit,szFileName,SIZEOF_ARRAY(szFileName));

	DWORD dwAttributes = m_pexpp->GetActiveShellBrowser()->QueryFileAttributes(m_ItemIndex);

	int Index = -1;

	if((dwAttributes & FILE_ATTRIBUTE_DIRECTORY) !=
		FILE_ATTRIBUTE_DIRECTORY)
	{
		for(int i = lstrlen(szFileName) - 1;i >= 0;i--)
		{
			if(szFileName[i] == '.')
			{
				Index = i;
				break;
			}
		}
	}

	return Index;
}
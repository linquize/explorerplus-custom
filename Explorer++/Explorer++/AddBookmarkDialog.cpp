/******************************************************************
 *
 * Project: Explorer++
 * File: AddBookmarkDialog.cpp
 * License: GPL - See COPYING in the top level directory
 *
 * Handles the 'Add Bookmark' dialog.
 *
 * Written by David Erceg
 * www.explorerplusplus.com
 *
 *****************************************************************/

#include "stdafx.h"
#include <stack>
#include "Explorer++_internal.h"
#include "AddBookmarkDialog.h"
#include "BookmarkHelper.h"
#include "MainResource.h"
#include "../Helper/Macros.h"


namespace NAddBookmarkDialog
{
	LRESULT CALLBACK TreeViewEditProcStub(HWND hwnd,UINT uMsg,
		WPARAM wParam,LPARAM lParam,UINT_PTR uIdSubclass,DWORD_PTR dwRefData);
}

const TCHAR CAddBookmarkDialogPersistentSettings::SETTINGS_KEY[] = _T("AddBookmark");

CAddBookmarkDialog::CAddBookmarkDialog(HINSTANCE hInstance,int iResource,HWND hParent,
	CBookmarkFolder &AllBookmarks,CBookmark &Bookmark) :
m_AllBookmarks(AllBookmarks),
m_Bookmark(Bookmark),
m_bNewFolderCreated(false),
m_ErrorBrush(CreateSolidBrush(ERROR_BACKGROUND_COLOR)),
CBaseDialog(hInstance,iResource,hParent,true)
{
	m_pabdps = &CAddBookmarkDialogPersistentSettings::GetInstance();

	/* If the singleton settings class has not been initialized
	yet, mark the root bookmark as selected and expanded. This
	is only needed the first time this dialog is shown, as
	selection and expansion info will be saved each time after
	that. */
	if(!m_pabdps->m_bInitialized)
	{
		m_pabdps->m_guidSelected = AllBookmarks.GetGUID();
		m_pabdps->m_setExpansion.insert(AllBookmarks.GetGUID());

		m_pabdps->m_bInitialized = true;
	}
}

CAddBookmarkDialog::~CAddBookmarkDialog()
{
	DeleteObject(m_ErrorBrush);
}

INT_PTR CAddBookmarkDialog::OnInitDialog()
{
	SetDialogIcon();

	SetDlgItemText(m_hDlg,IDC_BOOKMARK_NAME,m_Bookmark.GetName().c_str());
	SetDlgItemText(m_hDlg,IDC_BOOKMARK_LOCATION,m_Bookmark.GetLocation().c_str());

	if(m_Bookmark.GetName().size() == 0 ||
		m_Bookmark.GetLocation().size() == 0)
	{
		EnableWindow(GetDlgItem(m_hDlg,IDOK),FALSE);
	}

	HWND hTreeView = GetDlgItem(m_hDlg,IDC_BOOKMARK_TREEVIEW);

	m_pBookmarkTreeView = new CBookmarkTreeView(hTreeView,&m_AllBookmarks,
		m_pabdps->m_guidSelected,m_pabdps->m_setExpansion);

	HWND hEditName = GetDlgItem(m_hDlg,IDC_BOOKMARK_NAME);
	SendMessage(hEditName,EM_SETSEL,0,-1);
	SetFocus(hEditName);

	CBookmarkItemNotifier::GetInstance().AddObserver(this);

	m_pabdps->RestoreDialogPosition(m_hDlg,true);

	return 0;
}

void CAddBookmarkDialog::SetDialogIcon()
{
	HIMAGELIST himl = ImageList_Create(16,16,ILC_COLOR32|ILC_MASK,0,48);
	HBITMAP hBitmap = LoadBitmap(GetModuleHandle(NULL),MAKEINTRESOURCE(IDB_SHELLIMAGES));
	ImageList_Add(himl,hBitmap,NULL);

	m_hDialogIcon = ImageList_GetIcon(himl,SHELLIMAGES_ADDFAV,ILD_NORMAL);
	SetClassLongPtr(m_hDlg,GCLP_HICONSM,reinterpret_cast<LONG_PTR>(m_hDialogIcon));

	DeleteObject(hBitmap);
	ImageList_Destroy(himl);
}

void CAddBookmarkDialog::GetResizableControlInformation(CBaseDialog::DialogSizeConstraint &dsc,
	std::list<CResizableDialog::Control_t> &ControlList)
{
	dsc = CBaseDialog::DIALOG_SIZE_CONSTRAINT_NONE;

	CResizableDialog::Control_t Control;

	Control.iID = IDC_BOOKMARK_NAME;
	Control.Type = CResizableDialog::TYPE_RESIZE;
	Control.Constraint = CResizableDialog::CONSTRAINT_X;
	ControlList.push_back(Control);

	Control.iID = IDC_BOOKMARK_LOCATION;
	Control.Type = CResizableDialog::TYPE_RESIZE;
	Control.Constraint = CResizableDialog::CONSTRAINT_X;
	ControlList.push_back(Control);

	Control.iID = IDC_BOOKMARK_TREEVIEW;
	Control.Type = CResizableDialog::TYPE_RESIZE;
	Control.Constraint = CResizableDialog::CONSTRAINT_NONE;
	ControlList.push_back(Control);

	Control.iID = IDC_BOOKMARK_NEWFOLDER;
	Control.Type = CResizableDialog::TYPE_MOVE;
	Control.Constraint = CResizableDialog::CONSTRAINT_Y;
	ControlList.push_back(Control);

	Control.iID = IDOK;
	Control.Type = CResizableDialog::TYPE_MOVE;
	Control.Constraint = CResizableDialog::CONSTRAINT_NONE;
	ControlList.push_back(Control);

	Control.iID = IDCANCEL;
	Control.Type = CResizableDialog::TYPE_MOVE;
	Control.Constraint = CResizableDialog::CONSTRAINT_NONE;
	ControlList.push_back(Control);

	Control.iID = IDC_GRIPPER;
	Control.Type = CResizableDialog::TYPE_MOVE;
	Control.Constraint = CResizableDialog::CONSTRAINT_NONE;
	ControlList.push_back(Control);
}

INT_PTR CAddBookmarkDialog::OnCtlColorEdit(HWND hwnd,HDC hdc)
{
	if(hwnd == GetDlgItem(m_hDlg,IDC_BOOKMARK_NAME) ||
		hwnd == GetDlgItem(m_hDlg,IDC_BOOKMARK_LOCATION))
	{
		if(GetWindowTextLength(hwnd) == 0)
		{
			SetBkMode(hdc,TRANSPARENT);
			return reinterpret_cast<INT_PTR>(m_ErrorBrush);
		}
	}

	return FALSE;
}

INT_PTR CAddBookmarkDialog::OnCommand(WPARAM wParam,LPARAM lParam)
{
	if(HIWORD(wParam) != 0)
	{
		switch(HIWORD(wParam))
		{
		case EN_CHANGE:
			/* If either the name or location fields are empty,
			disable the ok button. */
			BOOL bEnable = (GetWindowTextLength(GetDlgItem(m_hDlg,IDC_BOOKMARK_NAME)) != 0 &&
				GetWindowTextLength(GetDlgItem(m_hDlg,IDC_BOOKMARK_LOCATION)) != 0);
			EnableWindow(GetDlgItem(m_hDlg,IDOK),bEnable);

			if(LOWORD(wParam) == IDC_BOOKMARK_NAME ||
				LOWORD(wParam) == IDC_BOOKMARK_LOCATION)
			{
				/* Used to ensure the edit controls are redrawn properly when
				changing the background color. */
				InvalidateRect(GetDlgItem(m_hDlg,LOWORD(wParam)),NULL,TRUE);
			}
			break;
		}
	}
	else
	{
		switch(LOWORD(wParam))
		{
		case IDM_ADDBOOKMARK_RLICK_RENAME:
			OnTreeViewRename();
			break;

		case IDM_ADDBOOKMARK_RLICK_NEWFOLDER:
		case IDC_BOOKMARK_NEWFOLDER:
			OnNewFolder();
			break;

		case IDOK:
			OnOk();
			break;

		case IDCANCEL:
			OnCancel();
			break;
		}
	}

	return 0;
}

INT_PTR CAddBookmarkDialog::OnNotify(NMHDR *pnmhdr)
{
	switch(pnmhdr->code)
	{
	case NM_RCLICK:
		OnRClick(pnmhdr);
		break;

	case TVN_BEGINLABELEDIT:
		OnTvnBeginLabelEdit();
		break;

	case TVN_ENDLABELEDIT:
		return OnTvnEndLabelEdit(reinterpret_cast<NMTVDISPINFO *>(pnmhdr));
		break;

	case TVN_KEYDOWN:
		OnTvnKeyDown(reinterpret_cast<NMTVKEYDOWN *>(pnmhdr));
		break;
	}

	return 0;
}

void CAddBookmarkDialog::OnNewFolder()
{
	TCHAR szTemp[64];
	LoadString(GetInstance(),IDS_BOOKMARKS_NEWBOOKMARKFOLDER,szTemp,SIZEOF_ARRAY(szTemp));
	CBookmarkFolder NewBookmarkFolder = CBookmarkFolder::Create(szTemp);

	m_bNewFolderCreated = true;
	m_NewFolderGUID = NewBookmarkFolder.GetGUID();

	HWND hTreeView = GetDlgItem(m_hDlg,IDC_BOOKMARK_TREEVIEW);
	HTREEITEM hSelectedItem = TreeView_GetSelection(hTreeView);

	assert(hSelectedItem != NULL);

	CBookmarkFolder &ParentBookmarkFolder = m_pBookmarkTreeView->GetBookmarkFolderFromTreeView(hSelectedItem);
	ParentBookmarkFolder.InsertBookmarkFolder(NewBookmarkFolder);
}

void CAddBookmarkDialog::OnRClick(NMHDR *pnmhdr)
{
	if(pnmhdr->idFrom == IDC_BOOKMARK_TREEVIEW)
	{
		HWND hTreeView = GetDlgItem(m_hDlg,IDC_BOOKMARK_TREEVIEW);

		DWORD dwCursorPos = GetMessagePos();

		POINT ptCursor;
		ptCursor.x = GET_X_LPARAM(dwCursorPos);
		ptCursor.y = GET_Y_LPARAM(dwCursorPos);

		TVHITTESTINFO tvhti;
		tvhti.pt = ptCursor;
		ScreenToClient(hTreeView,&tvhti.pt);
		HTREEITEM hItem = TreeView_HitTest(hTreeView,&tvhti);

		if(hItem != NULL)
		{
			TreeView_SelectItem(hTreeView,hItem);

			HMENU hMenu = LoadMenu(GetInstance(),MAKEINTRESOURCE(IDR_ADDBOOKMARK_RCLICK_MENU));
			TrackPopupMenu(GetSubMenu(hMenu,0),TPM_LEFTALIGN,ptCursor.x,ptCursor.y,0,m_hDlg,NULL);
			DestroyMenu(hMenu);
		}
	}
}

void CAddBookmarkDialog::OnTvnBeginLabelEdit()
{
	HWND hEdit = reinterpret_cast<HWND>(SendDlgItemMessage(m_hDlg,
		IDC_BOOKMARK_TREEVIEW,TVM_GETEDITCONTROL,0,0));
	SetWindowSubclass(hEdit,NAddBookmarkDialog::TreeViewEditProcStub,0,
		reinterpret_cast<DWORD_PTR>(this));
}

BOOL CAddBookmarkDialog::OnTvnEndLabelEdit(NMTVDISPINFO *pnmtvdi)
{
	HWND hEdit = reinterpret_cast<HWND>(SendDlgItemMessage(m_hDlg,
		IDC_BOOKMARK_TREEVIEW,TVM_GETEDITCONTROL,0,0));
	RemoveWindowSubclass(hEdit,NAddBookmarkDialog::TreeViewEditProcStub,0);

	if(pnmtvdi->item.pszText != NULL &&
		lstrlen(pnmtvdi->item.pszText) > 0)
	{
		CBookmarkFolder &BookmarkFolder = m_pBookmarkTreeView->GetBookmarkFolderFromTreeView(pnmtvdi->item.hItem);
		BookmarkFolder.SetName(pnmtvdi->item.pszText);

		SetWindowLongPtr(m_hDlg,DWLP_MSGRESULT,TRUE);
		return TRUE;
	}

	SetWindowLongPtr(m_hDlg,DWLP_MSGRESULT,FALSE);
	return FALSE;
}

LRESULT CALLBACK NAddBookmarkDialog::TreeViewEditProcStub(HWND hwnd,UINT uMsg,
	WPARAM wParam,LPARAM lParam,UINT_PTR uIdSubclass,DWORD_PTR dwRefData)
{
	CAddBookmarkDialog *pabd = reinterpret_cast<CAddBookmarkDialog *>(dwRefData);

	return pabd->TreeViewEditProc(hwnd,uMsg,wParam,lParam);
}

LRESULT CALLBACK CAddBookmarkDialog::TreeViewEditProc(HWND hwnd,UINT Msg,WPARAM wParam,LPARAM lParam)
{
	switch(Msg)
	{
	case WM_GETDLGCODE:
		switch(wParam)
		{
		case VK_RETURN:
			return DLGC_WANTALLKEYS;
			break;
		}
		break;
	}

	return DefSubclassProc(hwnd,Msg,wParam,lParam);
}

void CAddBookmarkDialog::OnTvnKeyDown(NMTVKEYDOWN *pnmtvkd)
{
	switch(pnmtvkd->wVKey)
	{
	case VK_F2:
		OnTreeViewRename();
		break;
	}
}

void CAddBookmarkDialog::OnTreeViewRename()
{
	HWND hTreeView = GetDlgItem(m_hDlg,IDC_BOOKMARK_TREEVIEW);
	HTREEITEM hSelectedItem = TreeView_GetSelection(hTreeView);
	TreeView_EditLabel(hTreeView,hSelectedItem);
}

void CAddBookmarkDialog::OnOk()
{
	HWND hName = GetDlgItem(m_hDlg,IDC_BOOKMARK_NAME);
	std::wstring strName;
	GetWindowString(hName,strName);

	HWND hLocation = GetDlgItem(m_hDlg,IDC_BOOKMARK_LOCATION);
	std::wstring strLocation;
	GetWindowString(hLocation,strLocation);

	if(strName.size() > 0 &&
		strLocation.size() > 0)
	{
		HWND hTreeView = GetDlgItem(m_hDlg,IDC_BOOKMARK_TREEVIEW);
		HTREEITEM hSelected = TreeView_GetSelection(hTreeView);
		CBookmarkFolder &BookmarkFolder = m_pBookmarkTreeView->GetBookmarkFolderFromTreeView(hSelected);

		CBookmark Bookmark(strName,strLocation,_T(""));
		BookmarkFolder.InsertBookmark(Bookmark);
	}

	EndDialog(m_hDlg,1);
}

void CAddBookmarkDialog::OnCancel()
{
	EndDialog(m_hDlg,0);
}

void CAddBookmarkDialog::SaveState()
{
	m_pabdps->SaveDialogPosition(m_hDlg);

	SaveTreeViewState();

	m_pabdps->m_bStateSaved = TRUE;
}

void CAddBookmarkDialog::SaveTreeViewState()
{
	HWND hTreeView = GetDlgItem(m_hDlg,IDC_BOOKMARK_TREEVIEW);

	HTREEITEM hSelected = TreeView_GetSelection(hTreeView);
	CBookmarkFolder &BookmarkFolder = m_pBookmarkTreeView->GetBookmarkFolderFromTreeView(hSelected);
	m_pabdps->m_guidSelected = BookmarkFolder.GetGUID();

	m_pabdps->m_setExpansion.clear();
	SaveTreeViewExpansionState(hTreeView,TreeView_GetRoot(hTreeView));
}

void CAddBookmarkDialog::SaveTreeViewExpansionState(HWND hTreeView,HTREEITEM hItem)
{
	UINT uState = TreeView_GetItemState(hTreeView,hItem,TVIS_EXPANDED);

	if(uState & TVIS_EXPANDED)
	{
		CBookmarkFolder &BookmarkFolder = m_pBookmarkTreeView->GetBookmarkFolderFromTreeView(hItem);
		m_pabdps->m_setExpansion.insert(BookmarkFolder.GetGUID());

		HTREEITEM hChild = TreeView_GetChild(hTreeView,hItem);
		SaveTreeViewExpansionState(hTreeView,hChild);

		while((hChild = TreeView_GetNextSibling(hTreeView,hChild)) != NULL)
		{
			SaveTreeViewExpansionState(hTreeView,hChild);
		}
	}
}

INT_PTR CAddBookmarkDialog::OnClose()
{
	EndDialog(m_hDlg,0);
	return 0;
}

INT_PTR CAddBookmarkDialog::OnDestroy()
{
	CBookmarkItemNotifier::GetInstance().RemoveObserver(this);
	DestroyIcon(m_hDialogIcon);

	return 0;
}

INT_PTR CAddBookmarkDialog::OnNcDestroy()
{
	delete m_pBookmarkTreeView;

	return 0;
}

void CAddBookmarkDialog::OnBookmarkAdded(const CBookmarkFolder &ParentBookmarkFolder,
	const CBookmark &Bookmark,std::size_t Position)
{

}

void CAddBookmarkDialog::OnBookmarkFolderAdded(const CBookmarkFolder &ParentBookmarkFolder,
	const CBookmarkFolder &BookmarkFolder,std::size_t Position)
{
	HTREEITEM hItem = m_pBookmarkTreeView->BookmarkFolderAdded(ParentBookmarkFolder,BookmarkFolder,Position);

	if(m_bNewFolderCreated &&
		IsEqualGUID(BookmarkFolder.GetGUID(),m_NewFolderGUID))
	{
		HWND hTreeView = GetDlgItem(m_hDlg,IDC_BOOKMARK_TREEVIEW);

		/* If a new folder has been created, it will be selected,
		as it is assumed that the user intends to place any
		new bookmark within that folder. */
		SetFocus(hTreeView);
		TreeView_SelectItem(hTreeView,hItem);
		TreeView_EditLabel(hTreeView,hItem);

		m_bNewFolderCreated = false;
	}
}

void CAddBookmarkDialog::OnBookmarkModified(const GUID &guid)
{

}

void CAddBookmarkDialog::OnBookmarkFolderModified(const GUID &guid)
{
	m_pBookmarkTreeView->BookmarkFolderModified(guid);
}

void CAddBookmarkDialog::OnBookmarkRemoved(const GUID &guid)
{

}

void CAddBookmarkDialog::OnBookmarkFolderRemoved(const GUID &guid)
{
	m_pBookmarkTreeView->BookmarkFolderRemoved(guid);
}

CAddBookmarkDialogPersistentSettings::CAddBookmarkDialogPersistentSettings() :
CDialogSettings(SETTINGS_KEY)
{
	m_bInitialized = false;
}

CAddBookmarkDialogPersistentSettings::~CAddBookmarkDialogPersistentSettings()
{
	
}

CAddBookmarkDialogPersistentSettings& CAddBookmarkDialogPersistentSettings::GetInstance()
{
	static CAddBookmarkDialogPersistentSettings abdps;
	return abdps;
}
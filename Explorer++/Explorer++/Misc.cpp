/******************************************************************
 *
 * Project: Explorer++
 * File: Misc.cpp
 * License: GPL - See COPYING in the top level directory
 *
 * Includes miscellaneous functions related to
 * the top-level GUI component.
 *
 * Written by David Erceg
 * www.explorerplusplus.com
 *
 *****************************************************************/

#include "stdafx.h"
#include <list>
#include <shobjidl.h>
#include <pantheios\inserters\integer.hpp>
#include "Explorer++.h"
#include "SelectColumnsDialog.h"
#include "MainResource.h"
#include "../DisplayWindow/DisplayWindow.h"
#include "../Helper/FileOperations.h"
#include "../Helper/Helper.h"
#include "../Helper/Controls.h"
#include "../Helper/ShellHelper.h"
#include "../Helper/ListViewHelper.h"
#include "../Helper/FolderSize.h"
#include "../Helper/Macros.h"


void Explorerplusplus::ValidateLoadedSettings(void)
{
	if(m_TreeViewWidth <= 0)
		m_TreeViewWidth = DEFAULT_TREEVIEW_WIDTH;

	if(m_DisplayWindowHeight < MINIMUM_DISPLAYWINDOW_HEIGHT)
		m_DisplayWindowHeight = DEFAULT_DISPLAYWINDOW_HEIGHT;

	ValidateColumns();
	ValidateToolbarSettings();
}

void Explorerplusplus::ValidateToolbarSettings(void)
{
	std::list<ToolbarButton_t>::iterator	itr;
	BOOL							bCorrupted = FALSE;
	int								*ButtonMap;
	int								nButtons;
	int								i = 0;

	nButtons = sizeof(ToolbarButtonSet) / sizeof(ToolbarButtonSet[0]);

	ButtonMap = (int *)malloc(sizeof(int) * nButtons);

	for(i = 0;i < nButtons;i++)
	{
		ButtonMap[i] = 0;
	}

	/* Two things to check:
	- Firstly, that all items id's are within range,
	- and secondly, that no item is duplicated (except for
	separators).
	An empty set of items (i.e. no buttons on the toolbar) is
	allowed. */
	for(itr = m_tbInitial.begin();itr != m_tbInitial.end();itr++)
	{
		if(itr->iItemID < TOOLBAR_ID_START)
		{
			bCorrupted = TRUE;
			break;
		}
	}

	if(!bCorrupted)
	{
		for(i = 0;i < nButtons;i++)
		{
			if(ButtonMap[i] > 1)
			{
				bCorrupted = TRUE;
				break;
			}
		}
	}

	if(bCorrupted)
		SetInitialToolbarButtons();

	free(ButtonMap);
}

void Explorerplusplus::ValidateColumns(void)
{
	ValidateSingleColumnSet(VALIDATE_REALFOLDER_COLUMNS,&m_RealFolderColumnList);
	ValidateSingleColumnSet(VALIDATE_CONTROLPANEL_COLUMNS,&m_ControlPanelColumnList);
	ValidateSingleColumnSet(VALIDATE_MYCOMPUTER_COLUMNS,&m_MyComputerColumnList);
	ValidateSingleColumnSet(VALIDATE_RECYCLEBIN_COLUMNS,&m_RecycleBinColumnList);
	ValidateSingleColumnSet(VALIDATE_PRINTERS_COLUMNS,&m_PrintersColumnList);
	ValidateSingleColumnSet(VALIDATE_NETWORKCONNECTIONS_COLUMNS,&m_NetworkConnectionsColumnList);
	ValidateSingleColumnSet(VALIDATE_MYNETWORKPLACES_COLUMNS,&m_MyNetworkPlacesColumnList);
}

void Explorerplusplus::ValidateSingleColumnSet(int iColumnSet,std::list<Column_t> *pColumnList)
{
	std::list<Column_t>::iterator	itr;
	Column_t					Column;
	int							*pColumnMap = NULL;
	BOOL						bFound = FALSE;
	const Column_t				*pColumns = NULL;
	unsigned int				iTotalColumnSize = 0;
	unsigned int				i = 0;

	switch(iColumnSet)
	{
		case VALIDATE_REALFOLDER_COLUMNS:
			iTotalColumnSize = SIZEOF_ARRAY(g_RealFolderColumns);
			pColumns = g_RealFolderColumns;
			break;

		case VALIDATE_CONTROLPANEL_COLUMNS:
			iTotalColumnSize = SIZEOF_ARRAY(g_ControlPanelColumns);
			pColumns = g_ControlPanelColumns;
			break;

		case VALIDATE_MYCOMPUTER_COLUMNS:
			iTotalColumnSize = SIZEOF_ARRAY(g_MyComputerColumns);
			pColumns = g_MyComputerColumns;
			break;

		case VALIDATE_RECYCLEBIN_COLUMNS:
			iTotalColumnSize = SIZEOF_ARRAY(g_RecycleBinColumns);
			pColumns = g_RecycleBinColumns;
			break;

		case VALIDATE_PRINTERS_COLUMNS:
			iTotalColumnSize = SIZEOF_ARRAY(g_PrintersColumns);
			pColumns = g_PrintersColumns;
			break;

		case VALIDATE_NETWORKCONNECTIONS_COLUMNS:
			iTotalColumnSize = SIZEOF_ARRAY(g_NetworkConnectionsColumns);
			pColumns = g_NetworkConnectionsColumns;
			break;

		case VALIDATE_MYNETWORKPLACES_COLUMNS:
			iTotalColumnSize = SIZEOF_ARRAY(g_MyNetworkPlacesColumns);
			pColumns = g_MyNetworkPlacesColumns;
			break;
	}

	pColumnMap = (int *)malloc(iTotalColumnSize * sizeof(int));

	for(i = 0;i < iTotalColumnSize;i++)
	{
		pColumnMap[i] = 0;
	}

	/* Check that every column that is supposed to appear
	is in the column list. */
	for(i = 0;i < iTotalColumnSize;i++)
	{
		bFound = FALSE;

		for(itr = pColumnList->begin();itr != pColumnList->end();itr++)
		{
			if(itr->id == pColumns[i].id)
			{
				bFound = TRUE;
				break;
			}
		}

		/* The column is not currently in the set. Add it in. */
		if(!bFound)
		{
			Column.id		= pColumns[i].id;
			Column.bChecked	= pColumns[i].bChecked;
			Column.iWidth	= DEFAULT_COLUMN_WIDTH;
			pColumnList->push_back(Column);
		}
	}

	free(pColumnMap);
}

void Explorerplusplus::ApplyLoadedSettings(void)
{
	m_pMyTreeView->SetShowHidden(m_bShowHiddenGlobal);
}

void Explorerplusplus::ApplyToolbarSettings(void)
{
	BOOL bVisible = FALSE;
	int i = 0;

	/* Set the state of the toolbars contained within
	the main rebar. */
	for(i = 0;i < NUM_MAIN_TOOLBARS;i++)
	{
		switch(m_ToolbarInformation[i].wID)
		{
		case ID_MAINTOOLBAR:
			bVisible = m_bShowMainToolbar;
			break;

		case ID_ADDRESSTOOLBAR:
			bVisible = m_bShowAddressBar;
			break;

		case ID_BOOKMARKSTOOLBAR:
			bVisible = m_bShowBookmarksToolbar;
			break;

		case ID_DRIVESTOOLBAR:
			bVisible = m_bShowDrivesToolbar;
			break;

		case ID_APPLICATIONSTOOLBAR:
			bVisible = m_bShowApplicationToolbar;
			break;
		}

		if(!bVisible)
			AddStyleToToolbar(&m_ToolbarInformation[i].fStyle,RBBS_HIDDEN);
	}

	if(m_bLockToolbars)
	{
		for(i = 0;i < NUM_MAIN_TOOLBARS;i++)
		{
			AddStyleToToolbar(&m_ToolbarInformation[i].fStyle,RBBS_NOGRIPPER);
		}
	}
}

void Explorerplusplus::AdjustFolderPanePosition(void)
{
	RECT rc;
	int IndentTop		= 0;
	int IndentBottom	= 0;
	int height;

	GetClientRect(m_hContainer,&rc);
	height = GetRectHeight(&rc);

	if(m_hMainRebar)
	{
		RECT RebarRect;

		GetWindowRect(m_hMainRebar,&RebarRect);

		IndentTop += RebarRect.bottom - RebarRect.top;
	}

	if(m_bShowStatusBar)
	{
		RECT m_hStatusBarRect;

		GetWindowRect(m_hStatusBar,&m_hStatusBarRect);

		IndentBottom += m_hStatusBarRect.bottom - m_hStatusBarRect.top;
	}

	if(m_bShowDisplayWindow)
	{
		RECT rc;

		GetWindowRect(m_hDisplayWindow,&rc);

		IndentBottom += rc.bottom - rc.top;
	}

	if(m_bShowFolders)
	{
		RECT rc;
		GetClientRect(m_hHolder,&rc);

		SetWindowPos(m_hHolder,NULL,0,IndentTop,rc.right,
		height-IndentBottom-IndentTop,SWP_SHOWWINDOW|SWP_NOZORDER);
	}
}

LRESULT Explorerplusplus::StatusBarMenuSelect(WPARAM wParam,LPARAM lParam)
{
	/* Is the menu been closed? .*/
	if(HIWORD(wParam) == 0xFFFF && lParam == 0)
	{
		m_pStatusBar->HandleStatusBarMenuClose();
	}
	else
	{
		m_pStatusBar->HandleStatusBarMenuOpen();

		TCHAR szBuffer[512];
		LoadString(m_hLanguageModule,LOWORD(wParam),
			szBuffer,SIZEOF_ARRAY(szBuffer));
		SetWindowText(m_hStatusBar,szBuffer);
	}

	return 0;
}

void Explorerplusplus::CopyToFolder(BOOL bMove)
{
	if(ListView_GetSelectedCount(m_hActiveListView) == 0)
	{
		return;
	}

	std::list<std::wstring> FullFilenameList;
	int iItem = -1;

	while((iItem = ListView_GetNextItem(m_hActiveListView,iItem,LVNI_SELECTED)) != -1)
	{
		TCHAR szFullFilename[MAX_PATH];
		m_pActiveShellBrowser->QueryFullItemName(iItem,szFullFilename);

		FullFilenameList.push_back(szFullFilename);
	}

	TCHAR szTemp[128];
	LoadString(m_hLanguageModule,IDS_GENERAL_COPY_TO_FOLDER_TITLE,szTemp,SIZEOF_ARRAY(szTemp));
	NFileOperations::CopyFilesToFolder(m_hContainer,szTemp,FullFilenameList,bMove);
}

LRESULT Explorerplusplus::OnDeviceChange(WPARAM wParam,LPARAM lParam)
{
	/* Forward this notification out to all tabs (if a
	tab is currently in my computer, it will need to
	update its contents). */
	int nTabs = TabCtrl_GetItemCount(m_hTabCtrl);

	for(int i = 0;i < nTabs;i++)
	{
		TCITEM tcItem;
		tcItem.mask = TCIF_PARAM;
		TabCtrl_GetItem(m_hTabCtrl,i,&tcItem);

		m_pShellBrowser[static_cast<int>(tcItem.lParam)]->OnDeviceChange(wParam,lParam);
	}

	/* Forward the message to the treeview, so that
	it can handle the message as well. */
	SendMessage(m_hTreeView,WM_DEVICECHANGE,wParam,lParam);

	switch(wParam)
	{
	case DBT_DEVICEARRIVAL:
		CHardwareChangeNotifier::GetInstance().NotifyDeviceArrival(reinterpret_cast<DEV_BROADCAST_HDR *>(lParam));
		break;

	case DBT_DEVICEREMOVECOMPLETE:
		CHardwareChangeNotifier::GetInstance().NotifyDeviceRemovalComplete(reinterpret_cast<DEV_BROADCAST_HDR *>(lParam));
		break;
	}

	return TRUE;
}

HRESULT Explorerplusplus::TestListViewSelectionAttributes(SFGAOF *pItemAttributes)
{
	LPITEMIDLIST	pidlDirectory = NULL;
	LPITEMIDLIST	ridl = NULL;
	LPITEMIDLIST	pidlComplete = NULL;
	HRESULT			hr = E_FAIL;
	int				iSelected;

	iSelected = ListView_GetNextItem(m_hActiveListView,-1,LVNI_SELECTED);

	if(iSelected != -1)
	{
		pidlDirectory = m_pActiveShellBrowser->QueryCurrentDirectoryIdl();

		ridl = m_pActiveShellBrowser->QueryItemRelativeIdl(iSelected);

		pidlComplete = ILCombine(pidlDirectory,ridl);

		hr = GetItemAttributes(pidlComplete,pItemAttributes);

		CoTaskMemFree(pidlComplete);
		CoTaskMemFree(ridl);
		CoTaskMemFree(pidlDirectory);
	}

	return hr;
}

HRESULT Explorerplusplus::TestTreeViewSelectionAttributes(SFGAOF *pItemAttributes)
{
	HTREEITEM		hItem;
	LPITEMIDLIST	pidl = NULL;
	HRESULT			hr = E_FAIL;

	hItem = TreeView_GetSelection(m_hTreeView);

	if(hItem != NULL)
	{
		pidl = m_pMyTreeView->BuildPath(hItem);

		hr = GetItemAttributes(pidl,pItemAttributes);

		CoTaskMemFree(pidl);
	}

	return hr;
}

BOOL Explorerplusplus::IsRenamePossible(void)
{
	SFGAOF	ItemAttributes;
	HRESULT	hr;

	ItemAttributes = SFGAO_CANRENAME;

	hr = GetSelectionAttributes(&ItemAttributes);

	if(SUCCEEDED(hr))
		return (ItemAttributes & SFGAO_CANRENAME) == SFGAO_CANRENAME;

	return FALSE;
}

BOOL Explorerplusplus::IsDeletionPossible(void)
{
	SFGAOF	ItemAttributes;
	HRESULT	hr;

	ItemAttributes = SFGAO_CANDELETE;

	hr = GetSelectionAttributes(&ItemAttributes);

	if(SUCCEEDED(hr))
		return (ItemAttributes & SFGAO_CANDELETE) == SFGAO_CANDELETE;

	return FALSE;
}

HRESULT Explorerplusplus::GetSelectionAttributes(SFGAOF *pItemAttributes)
{
	HWND	hFocus;
	HRESULT	hr = E_FAIL;

	hFocus = GetFocus();

	if(hFocus == m_hActiveListView)
		hr = TestListViewSelectionAttributes(pItemAttributes);
	else if(hFocus == m_hTreeView)
		hr = TestTreeViewSelectionAttributes(pItemAttributes);

	return hr;
}

BOOL Explorerplusplus::CanShowFileProperties(void)
{
	SFGAOF	ItemAttributes;
	HRESULT	hr;

	ItemAttributes = SFGAO_HASPROPSHEET;

	hr = GetSelectionAttributes(&ItemAttributes);

	if(SUCCEEDED(hr))
		return (ItemAttributes & SFGAO_HASPROPSHEET) == SFGAO_HASPROPSHEET;

	return FALSE;
}

BOOL Explorerplusplus::CanCutOrCopySelection(void)
{
	HWND hFocus;

	hFocus = GetFocus();

	if(hFocus == m_hActiveListView)
	{
		return m_nSelected && AreAllSelectedFilesReal();
	}
	else if(hFocus == m_hTreeView)
	{
		HTREEITEM		hItem;
		LPITEMIDLIST	pidl = NULL;
		SFGAOF			Attributes;
		HRESULT			hr;

		hItem = TreeView_GetSelection(m_hTreeView);

		if(hItem != NULL)
		{
			pidl = m_pMyTreeView->BuildPath(hItem);

			Attributes = SFGAO_CANCOPY;// | SFGAO_CANMOVE;

			hr = GetItemAttributes(pidl,&Attributes);

			CoTaskMemFree(pidl);

			if(hr == S_OK)
				return TRUE;
		}

		return FALSE;
	}

	return FALSE;
}

BOOL Explorerplusplus::CanPaste(void)
{
	HWND hFocus = GetFocus();

	std::list<FORMATETC> ftcList;
	CDropHandler::GetDropFormats(ftcList);

	BOOL bDataAvailable = FALSE;

	/* Check whether the drop source has the type of data
	that is needed for this drag operation. */
	for each(auto ftc in ftcList)
	{
		if(IsClipboardFormatAvailable(ftc.cfFormat))
		{
			bDataAvailable = TRUE;
			break;
		}
	}

	if(hFocus == m_hActiveListView)
	{
		return bDataAvailable && m_pActiveShellBrowser->CanCreate();
	}
	else if(hFocus == m_hTreeView)
	{
		HTREEITEM		hItem;
		LPITEMIDLIST	pidl = NULL;
		SFGAOF			Attributes;
		HRESULT			hr;

		hItem = TreeView_GetSelection(m_hTreeView);

		if(hItem != NULL)
		{
			pidl = m_pMyTreeView->BuildPath(hItem);

			Attributes = SFGAO_FILESYSTEM;

			hr = GetItemAttributes(pidl,&Attributes);

			CoTaskMemFree(pidl);

			if(hr == S_OK)
				return bDataAvailable;
		}
	}

	return FALSE;
}

BOOL Explorerplusplus::AreAllSelectedFilesReal(void)
{
	int iItem = -1;

	if(m_nSelected == 0)
		return FALSE;

	while((iItem = ListView_GetNextItem(m_hActiveListView,iItem,LVNI_SELECTED)) != -1)
	{
		if(!m_pActiveShellBrowser->IsFileReal(iItem))
			return FALSE;
	}

	return TRUE;
}

HRESULT Explorerplusplus::HandleStatusText(void)
{
	FolderInfo_t	FolderInfo;
	int				nTotal;
	int				nFilesSelected;
	int				nFoldersSelected;
	TCHAR			szItemsSelected[64];
	TCHAR			lpszSizeBuffer[32];
	TCHAR			szBuffer[64];
	TCHAR			szTemp[64];
	TCHAR			*szNumSelected = NULL;
	int				res;

	nTotal				= m_pActiveShellBrowser->QueryNumItems();
	nFilesSelected		= m_pActiveShellBrowser->QueryNumSelectedFiles();
	nFoldersSelected	= m_pActiveShellBrowser->QueryNumSelectedFolders();

	if((nFilesSelected + nFoldersSelected) != 0)
	{
		szNumSelected = PrintComma(nFilesSelected + nFoldersSelected);

		if((nFilesSelected + nFoldersSelected) == 1)
		{
			LoadString(m_hLanguageModule,IDS_GENERAL_SELECTED_ONEITEM,szTemp,
				SIZEOF_ARRAY(szTemp));

			/* One item selected. Form:
			1 item selected */
			StringCchPrintf(szItemsSelected,SIZEOF_ARRAY(szItemsSelected),
			_T("%s %s"),szNumSelected,szTemp);
		}
		else
		{
			LoadString(m_hLanguageModule,IDS_GENERAL_SELECTED_MOREITEMS,szTemp,
				SIZEOF_ARRAY(szTemp));

			/* More than one item selected. Form:
			n items selected */
			StringCchPrintf(szItemsSelected,SIZEOF_ARRAY(szItemsSelected),
			_T("%s %s"),szNumSelected,szTemp);
		}
	}
	else
	{
		szNumSelected = PrintComma(nTotal);

		if(nTotal == 1)
		{
			LoadString(m_hLanguageModule,IDS_GENERAL_ONEITEM,szTemp,
				SIZEOF_ARRAY(szTemp));

			/* Text: '1 item' */
			StringCchPrintf(szItemsSelected,SIZEOF_ARRAY(szItemsSelected),
			_T("%s %s"),szNumSelected,szTemp);
		}
		else
		{
			LoadString(m_hLanguageModule,IDS_GENERAL_MOREITEMS,szTemp,
				SIZEOF_ARRAY(szTemp));

			/* Text: 'n Items' */
			StringCchPrintf(szItemsSelected,SIZEOF_ARRAY(szItemsSelected),
			_T("%s %s"),szNumSelected,szTemp);
		}
	}

	SendMessage(m_hStatusBar,SB_SETTEXT,(WPARAM)0|0,(LPARAM)szItemsSelected);

	if(m_pActiveShellBrowser->InVirtualFolder())
	{
		LoadString(m_hLanguageModule,IDS_GENERAL_VIRTUALFOLDER,lpszSizeBuffer,
			SIZEOF_ARRAY(lpszSizeBuffer));
	}
	else
	{
		m_pActiveShellBrowser->QueryFolderInfo(&FolderInfo);

		if((nFilesSelected + nFoldersSelected) == 0)
		{
			/* No items(files or folders) selected. */
			FormatSizeString(FolderInfo.TotalFolderSize,lpszSizeBuffer,
				SIZEOF_ARRAY(lpszSizeBuffer),m_bForceSize,m_SizeDisplayFormat);
		}
		else
		{
			if(nFilesSelected == 0)
			{
				/* Only folders selected. Don't show any size in the status bar. */
				StringCchCopy(lpszSizeBuffer,SIZEOF_ARRAY(lpszSizeBuffer),EMPTY_STRING);
			}
			else
			{
				/* Mixture of files and folders selected. Show size of currently
				selected files. */
				FormatSizeString(FolderInfo.TotalSelectionSize,lpszSizeBuffer,
					SIZEOF_ARRAY(lpszSizeBuffer),m_bForceSize,m_SizeDisplayFormat);
			}
		}
	}

	SendMessage(m_hStatusBar,SB_SETTEXT,(WPARAM)1|0,(LPARAM)lpszSizeBuffer);

	res = CreateDriveFreeSpaceString(m_CurrentDirectory,szBuffer,SIZEOF_ARRAY(szBuffer));

	if(res == -1)
		StringCchCopy(szBuffer,SIZEOF_ARRAY(szBuffer),EMPTY_STRING);

	SendMessage(m_hStatusBar,SB_SETTEXT,(WPARAM)2|0,(LPARAM)szBuffer);

	return S_OK;
}

BOOL Explorerplusplus::CompareVirtualFolders(UINT uFolderCSIDL)
{
	return CompareVirtualFolders(m_CurrentDirectory,uFolderCSIDL);
}

BOOL Explorerplusplus::CompareVirtualFolders(TCHAR *szDirectory,UINT uFolderCSIDL)
{
	TCHAR szParsingPath[MAX_PATH];

	GetVirtualFolderParsingPath(uFolderCSIDL,szParsingPath);

	if(StrCmp(szDirectory,szParsingPath) == 0)
		return TRUE;

	return FALSE;
}

/*
RUNS IN CONTEXT OF DIRECTORY MOINTORING WORKER THREAD.
Possible bugs:
 - The tab may exist at the point of call that checks
   whether or not the tab index has been freed. However,
   it's possible it may not exist directly after.
   Therefore, use a critical section to ensure a tab cannot
   be freed until at least this call completes.

   If this runs before the tab is freed, the tab existence
   check will succeed, the shell browser function will be called
   and this function will exit.
   If this runs after the tab is freed, the tab existence
   check will fail, and the shell browser function won't be called.
*/
void Explorerplusplus::DirectoryAlteredCallback(TCHAR *szFileName,DWORD dwAction,
void *pData)
{
	DirectoryAltered_t	*pDirectoryAltered = NULL;
	Explorerplusplus			*pContainer = NULL;

	EnterCriticalSection(&g_csDirMonCallback);

	pDirectoryAltered = (DirectoryAltered_t *)pData;
	pContainer = (Explorerplusplus *)pDirectoryAltered->pData;

	/* Does this tab still exist? */
	if(pContainer->m_uTabMap[pDirectoryAltered->iIndex] == 1)
	{
		TCHAR szDirectory[MAX_PATH];
		pContainer->m_pShellBrowser[pDirectoryAltered->iIndex]->QueryCurrentDirectory(SIZEOF_ARRAY(szDirectory),szDirectory);
		pantheios::log(pantheios::debug,_T("Directory change notification received for \""),szDirectory,_T("\", Action = "),
			pantheios::integer(dwAction),_T(", Filename = \""),szFileName,_T("\""));

		pContainer->m_pShellBrowser[pDirectoryAltered->iIndex]->FilesModified(dwAction,
			szFileName,pDirectoryAltered->iIndex,pDirectoryAltered->iFolderIndex);
	}

	LeaveCriticalSection(&g_csDirMonCallback);
}

void Explorerplusplus::HandleFileSelectionDisplay(void)
{
	int nSelected;

	DisplayWindow_ClearTextBuffer(m_hDisplayWindow);

	nSelected = m_pActiveShellBrowser->QueryNumSelected();

	if(nSelected == 0)
		HandleFileSelectionDisplayZero();
	else if(nSelected == 1)
		HandleFileSelectionDisplayOne();
	else if(nSelected > 1)
		HandleFileSelectionDisplayMore();
}

void Explorerplusplus::HandleFileSelectionDisplayZero(void)
{
	/* Clear out any previous data shown in the display window. */
	DisplayWindow_ClearTextBuffer(m_hDisplayWindow);
	DisplayWindow_SetThumbnailFile(m_hDisplayWindow,EMPTY_STRING,FALSE);

	TCHAR szCurrentDirectory[MAX_PATH];
	m_pActiveShellBrowser->QueryCurrentDirectory(MAX_PATH,szCurrentDirectory);
	LPITEMIDLIST pidlDirectory = m_pActiveShellBrowser->QueryCurrentDirectoryIdl();

	LPITEMIDLIST pidlComputer = NULL;
	SHGetFolderLocation(NULL,CSIDL_DRIVES,NULL,0,&pidlComputer);

	if(CompareIdls(pidlDirectory,pidlComputer))
	{
		TCHAR szDisplay[512];
		DWORD dwSize = SIZEOF_ARRAY(szDisplay);
		GetComputerName(szDisplay,&dwSize);
		DisplayWindow_BufferText(m_hDisplayWindow,szDisplay);

		char szCPUBrand[64];
		GetCPUBrandString(szCPUBrand,SIZEOF_ARRAY(szCPUBrand));

		TCHAR szTemp[512];
		WCHAR wszCPUBrand[64];
		MultiByteToWideChar(CP_ACP,0,szCPUBrand,-1,wszCPUBrand,SIZEOF_ARRAY(wszCPUBrand));
		LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAY_WINDOW_PROCESSOR,szTemp,SIZEOF_ARRAY(szTemp));
		StringCchPrintf(szDisplay,SIZEOF_ARRAY(szDisplay),szTemp,wszCPUBrand);
		DisplayWindow_BufferText(m_hDisplayWindow,szDisplay);

		MEMORYSTATUSEX msex;
		msex.dwLength = sizeof(msex);
		GlobalMemoryStatusEx(&msex);

		ULARGE_INTEGER lTotalPhysicalMem;
		lTotalPhysicalMem.QuadPart = msex.ullTotalPhys;

		TCHAR szMemorySize[32];
		FormatSizeString(lTotalPhysicalMem,szMemorySize,SIZEOF_ARRAY(szMemorySize));
		LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAY_WINDOW_MEMORY,szTemp,SIZEOF_ARRAY(szTemp));
		StringCchPrintf(szDisplay,SIZEOF_ARRAY(szDisplay),szTemp,szMemorySize);
		DisplayWindow_BufferText(m_hDisplayWindow,szDisplay);
	}
	else
	{
		/* Folder name. */
		TCHAR szFolderName[MAX_PATH];
		GetDisplayName(szCurrentDirectory,szFolderName,SHGDN_INFOLDER);
		DisplayWindow_BufferText(m_hDisplayWindow,szFolderName);

		/* Folder type. */
		SHFILEINFO shfi;
		SHGetFileInfo(reinterpret_cast<LPCTSTR>(pidlDirectory),NULL,&shfi,sizeof(shfi),SHGFI_PIDL|SHGFI_TYPENAME);
		DisplayWindow_BufferText(m_hDisplayWindow,shfi.szTypeName);
	}

	CoTaskMemFree(pidlComputer);
	CoTaskMemFree(pidlDirectory);
}

void Explorerplusplus::HandleFileSelectionDisplayOne(void)
{
	WIN32_FIND_DATA	*pwfd = NULL;
	SHFILEINFO		shfi;
	TCHAR			szFullItemName[MAX_PATH];
	TCHAR			szFileDate[256];
	TCHAR			szDisplayDate[MAX_STRING_LENGTH];
	TCHAR			szDisplayName[MAX_PATH];
	TCHAR			szDateModified[256];
	int				iSelected;

	iSelected = ListView_GetNextItem(m_hActiveListView,-1,LVNI_SELECTED);

	if(iSelected != -1)
	{
		m_pActiveShellBrowser->QueryDisplayName(iSelected,
			MAX_PATH,szDisplayName);

		/* File name. */
		DisplayWindow_BufferText(m_hDisplayWindow,szDisplayName);

		m_pActiveShellBrowser->QueryFullItemName(iSelected,szFullItemName);

		if(!m_pActiveShellBrowser->InVirtualFolder())
		{
			DWORD dwAttributes;

			m_pActiveShellBrowser->QueryFullItemName(iSelected,szFullItemName);

			pwfd = m_pActiveShellBrowser->QueryFileFindData(iSelected);

			dwAttributes = GetFileAttributes(szFullItemName);

			if(((dwAttributes & FILE_ATTRIBUTE_DIRECTORY) ==
				FILE_ATTRIBUTE_DIRECTORY) && m_bShowFolderSizes)
			{
				FolderSize_t	*pfs = NULL;
				FolderSizeExtraInfo_t	*pfsei = NULL;
				DWFolderSize_t	DWFolderSize;
				TCHAR			szDisplayText[256];
				TCHAR			szTotalSize[64];
				TCHAR			szCalculating[64];
				DWORD			ThreadId;

				pfs = (FolderSize_t *)malloc(sizeof(FolderSize_t));

				if(pfs != NULL)
				{
					pfsei = (FolderSizeExtraInfo_t *)malloc(sizeof(FolderSizeExtraInfo_t));

					if(pfsei != NULL)
					{
						pfsei->pContainer	= (void *)this;
						pfsei->uId			= m_iDWFolderSizeUniqueId;
						pfs->pData			= (LPVOID)pfsei;

						pfs->pfnCallback	= FolderSizeCallbackStub;

						StringCchCopy(pfs->szPath,MAX_PATH,szFullItemName);

						LoadString(m_hLanguageModule,IDS_GENERAL_TOTALSIZE,
							szTotalSize,SIZEOF_ARRAY(szTotalSize));
						LoadString(m_hLanguageModule,IDS_GENERAL_CALCULATING,
							szCalculating,SIZEOF_ARRAY(szCalculating));
						StringCchPrintf(szDisplayText,SIZEOF_ARRAY(szDisplayText),
							_T("%s: %s"),szTotalSize,szCalculating);
						DisplayWindow_BufferText(m_hDisplayWindow,szDisplayText);

						/* Maintain a global list of folder size operations. */
						DWFolderSize.uId	= m_iDWFolderSizeUniqueId;
						DWFolderSize.iTabId	= m_iObjectIndex;
						DWFolderSize.bValid	= TRUE;
						m_DWFolderSizes.push_back(DWFolderSize);

						HANDLE hThread = CreateThread(NULL,0,Thread_CalculateFolderSize,(LPVOID)pfs,0,&ThreadId);
						CloseHandle(hThread);

						m_iDWFolderSizeUniqueId++;
					}
					else
					{
						free(pfs);
					}
				}
			}
			else
			{
				SHGetFileInfo(szFullItemName,pwfd->dwFileAttributes,
					&shfi,sizeof(shfi),SHGFI_TYPENAME|SHGFI_USEFILEATTRIBUTES);

				DisplayWindow_BufferText(m_hDisplayWindow,shfi.szTypeName);
			}

			CreateFileTimeString(&pwfd->ftLastWriteTime,
				szFileDate,SIZEOF_ARRAY(szFileDate),
				m_bShowFriendlyDatesGlobal);

			LoadString(m_hLanguageModule,IDS_GENERAL_DATEMODIFIED,szDateModified,
				SIZEOF_ARRAY(szDateModified));

			StringCchPrintf(szDisplayDate,
			SIZEOF_ARRAY(szDisplayDate),
			_T("%s: %s"),szDateModified,szFileDate);

			/* File (modified) date. */
			DisplayWindow_BufferText(m_hDisplayWindow,szDisplayDate);

			if(IsImage(szFullItemName))
			{
				TCHAR szOutput[256];
				TCHAR szTemp[64];
				UINT uWidth;
				UINT uHeight;
				Gdiplus::Image *pimg = NULL;

				pimg = new Gdiplus::Image(szFullItemName,FALSE);

				if(pimg->GetLastStatus() == Gdiplus::Ok)
				{
					uWidth = pimg->GetWidth();
					LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAYWINDOW_IMAGEWIDTH,szTemp,SIZEOF_ARRAY(szTemp));
					StringCchPrintf(szOutput,SIZEOF_ARRAY(szOutput),szTemp,uWidth);
					DisplayWindow_BufferText(m_hDisplayWindow,szOutput);

					uHeight = pimg->GetHeight();
					LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAYWINDOW_IMAGEHEIGHT,szTemp,SIZEOF_ARRAY(szTemp));
					StringCchPrintf(szOutput,SIZEOF_ARRAY(szOutput),szTemp,uHeight);
					DisplayWindow_BufferText(m_hDisplayWindow,szOutput);

					Gdiplus::PixelFormat format;
					UINT uBitDepth;

					format = pimg->GetPixelFormat();

					switch(format)
					{
					case PixelFormat1bppIndexed:
						uBitDepth = 1;
						break;

					case PixelFormat4bppIndexed:
						uBitDepth = 4;
						break;

					case PixelFormat8bppIndexed:
						uBitDepth = 8;
						break;

					case PixelFormat16bppARGB1555:
					case PixelFormat16bppGrayScale:
					case PixelFormat16bppRGB555:
					case PixelFormat16bppRGB565:
						uBitDepth = 16;
						break;

					case PixelFormat24bppRGB:
						uBitDepth = 24;
						break;

					case PixelFormat32bppARGB:
					case PixelFormat32bppPARGB:
					case PixelFormat32bppRGB:
						uBitDepth = 32;
						break;

					case PixelFormat48bppRGB:
						uBitDepth = 48;
						break;

					case PixelFormat64bppARGB:
					case PixelFormat64bppPARGB:
						uBitDepth = 64;
						break;

					default:
						uBitDepth = 0;
						break;
					}

					if(uBitDepth == -1)
					{
						LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAYWINDOW_BITDEPTHUNKNOWN,szTemp,SIZEOF_ARRAY(szTemp));
						StringCchCopy(szOutput,SIZEOF_ARRAY(szOutput),szTemp);
					}
					else
					{
						LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAYWINDOW_BITDEPTH,szTemp,SIZEOF_ARRAY(szTemp));
						StringCchPrintf(szOutput,SIZEOF_ARRAY(szOutput),szTemp,uBitDepth);
					}

					DisplayWindow_BufferText(m_hDisplayWindow,szOutput);

					Gdiplus::REAL res;

					res = pimg->GetHorizontalResolution();
					LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAYWINDOW_HORIZONTALRESOLUTION,szTemp,SIZEOF_ARRAY(szTemp));
					StringCchPrintf(szOutput,SIZEOF_ARRAY(szOutput),szTemp,res);
					DisplayWindow_BufferText(m_hDisplayWindow,szOutput);

					res = pimg->GetVerticalResolution();
					LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAYWINDOW_VERTICALRESOLUTION,szTemp,SIZEOF_ARRAY(szTemp));
					StringCchPrintf(szOutput,SIZEOF_ARRAY(szOutput),szTemp,res);
					DisplayWindow_BufferText(m_hDisplayWindow,szOutput);
				}

				delete pimg;
			}

			/* Only attempt to show file previews for files (not folders). Also, only
			attempt to show a preview if the display window is actually active. */
			if(((dwAttributes & FILE_ATTRIBUTE_DIRECTORY) !=
				FILE_ATTRIBUTE_DIRECTORY) && m_bShowFilePreviews
				&& m_bShowDisplayWindow)
			{
				DisplayWindow_SetThumbnailFile(m_hDisplayWindow,szFullItemName,TRUE);
			}
			else
			{
				DisplayWindow_SetThumbnailFile(m_hDisplayWindow,EMPTY_STRING,FALSE);
			}
		}
		else
		{
			m_pActiveShellBrowser->QueryFullItemName(iSelected,szFullItemName);

			if(PathIsRoot(szFullItemName))
			{
				TCHAR szMsg[64];
				TCHAR szTemp[64];
				ULARGE_INTEGER ulTotalNumberOfBytes;
				ULARGE_INTEGER ulTotalNumberOfFreeBytes;
				BOOL bRet = GetDiskFreeSpaceEx(szFullItemName,NULL,&ulTotalNumberOfBytes,&ulTotalNumberOfFreeBytes);

				if(bRet)
				{
					TCHAR szSize[32];
					FormatSizeString(ulTotalNumberOfFreeBytes,szSize,SIZEOF_ARRAY(szSize));
					LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAY_WINDOW_FREE_SPACE,szTemp,SIZEOF_ARRAY(szTemp));
					StringCchPrintf(szMsg,SIZEOF_ARRAY(szMsg),szTemp,szSize);
					DisplayWindow_BufferText(m_hDisplayWindow,szMsg);

					FormatSizeString(ulTotalNumberOfBytes,szSize,SIZEOF_ARRAY(szSize));
					LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAY_WINDOW_TOTAL_SIZE,szTemp,SIZEOF_ARRAY(szTemp));
					StringCchPrintf(szMsg,SIZEOF_ARRAY(szMsg),szTemp,szSize);
					DisplayWindow_BufferText(m_hDisplayWindow,szMsg);
				}

				TCHAR szFileSystem[MAX_PATH + 1];
				bRet = GetVolumeInformation(szFullItemName,NULL,0,NULL,NULL,NULL,szFileSystem,SIZEOF_ARRAY(szFileSystem));

				if(bRet)
				{
					LoadString(m_hLanguageModule,IDS_GENERAL_DISPLAY_WINDOW_FILE_SYSTEM,szTemp,SIZEOF_ARRAY(szTemp));
					StringCchPrintf(szMsg,SIZEOF_ARRAY(szMsg),szTemp,szFileSystem);
					DisplayWindow_BufferText(m_hDisplayWindow,szMsg);
				}
			}
		}
	}
}

void Explorerplusplus::HandleFileSelectionDisplayMore(void)
{
	TCHAR			szNumSelected[64] = EMPTY_STRING;
	TCHAR			szTotalSize[64] = EMPTY_STRING;
	TCHAR			szTotalSizeFragment[32] = EMPTY_STRING;
	TCHAR			szMore[64];
	TCHAR			szTotalSizeString[64];
	FolderInfo_t	FolderInfo;
	int				nSelected;

	DisplayWindow_SetThumbnailFile(m_hDisplayWindow,EMPTY_STRING,FALSE);

	nSelected = m_pActiveShellBrowser->QueryNumSelected();

	LoadString(m_hLanguageModule,IDS_GENERAL_SELECTED_MOREITEMS,
		szMore,SIZEOF_ARRAY(szMore));

	StringCchPrintf(szNumSelected,SIZEOF_ARRAY(szNumSelected),
	_T("%d %s"),nSelected,szMore);

	DisplayWindow_BufferText(m_hDisplayWindow,szNumSelected);

	if(!m_pActiveShellBrowser->InVirtualFolder())
	{
		m_pActiveShellBrowser->QueryFolderInfo(&FolderInfo);

		FormatSizeString(FolderInfo.TotalSelectionSize,szTotalSizeFragment,
			SIZEOF_ARRAY(szTotalSizeFragment),m_bForceSize,m_SizeDisplayFormat);

		LoadString(m_hLanguageModule,IDS_GENERAL_TOTALFILESIZE,
		szTotalSizeString,SIZEOF_ARRAY(szTotalSizeString));

		StringCchPrintf(szTotalSize,SIZEOF_ARRAY(szTotalSize),
		_T("%s: %s"),szTotalSizeString,szTotalSizeFragment);
	}

	DisplayWindow_BufferText(m_hDisplayWindow,szTotalSize);
}

void FolderSizeCallbackStub(int nFolders,int nFiles,PULARGE_INTEGER lTotalFolderSize,LPVOID pData)
{
	Explorerplusplus::FolderSizeExtraInfo_t *pfsei = reinterpret_cast<Explorerplusplus::FolderSizeExtraInfo_t *>(pData);
	reinterpret_cast<Explorerplusplus *>(pfsei->pContainer)->FolderSizeCallback(pfsei,nFolders,nFiles,lTotalFolderSize);
	free(pfsei);
}

void Explorerplusplus::FolderSizeCallback(FolderSizeExtraInfo_t *pfsei,
int nFolders,int nFiles,PULARGE_INTEGER lTotalFolderSize)
{
	DWFolderSizeCompletion_t *pDWFolderSizeCompletion = NULL;

	pDWFolderSizeCompletion = (DWFolderSizeCompletion_t *)malloc(sizeof(DWFolderSizeCompletion_t));

	pDWFolderSizeCompletion->liFolderSize = *lTotalFolderSize;
	pDWFolderSizeCompletion->uId = pfsei->uId;

	/* Queue the result back to the main thread, so that
	the folder size can be displayed. It is up to the main
	thread to determine whether the folder size should actually
	be shown. */
	PostMessage(m_hContainer,WM_APP_FOLDERSIZECOMPLETED,
		(WPARAM)pDWFolderSizeCompletion,0);
}

void Explorerplusplus::CreateViewsMenu(POINT *ptOrigin)
{
	UINT	uViewMode;
	int		ItemToCheck;

	m_pShellBrowser[m_iObjectIndex]->GetCurrentViewMode(&uViewMode);

	ItemToCheck = GetViewModeMenuId(uViewMode);
	CheckMenuRadioItem(m_hViewsMenu,IDM_VIEW_THUMBNAILS,IDM_VIEW_EXTRALARGEICONS,
		ItemToCheck,MF_BYCOMMAND);

	TrackPopupMenu(m_hViewsMenu,TPM_LEFTALIGN,ptOrigin->x,ptOrigin->y,
		0,m_hContainer,NULL);
}

int Explorerplusplus::CreateDriveFreeSpaceString(TCHAR *szPath,TCHAR *szBuffer,int nBuffer)
{
	ULARGE_INTEGER	TotalNumberOfBytes;
	ULARGE_INTEGER	TotalNumberOfFreeBytes;
	ULARGE_INTEGER	BytesAvailableToCaller;
	TCHAR			szFreeSpace[32];
	TCHAR			szFree[16];
	TCHAR			szFreeSpaceString[512];

	if(GetDiskFreeSpaceEx(szPath,&BytesAvailableToCaller,
	&TotalNumberOfBytes,&TotalNumberOfFreeBytes) == 0)
	{
		szBuffer = NULL;
		return -1;
	}

	FormatSizeString(TotalNumberOfFreeBytes,szFreeSpace,
		SIZEOF_ARRAY(szFreeSpace));

	LoadString(m_hLanguageModule,IDS_GENERAL_FREE,szFree,SIZEOF_ARRAY(szFree));

	StringCchPrintf(szFreeSpaceString,SIZEOF_ARRAY(szFreeSpace),
	_T("%s %s (%.0f%%)"),szFreeSpace,szFree,TotalNumberOfFreeBytes.QuadPart * 100.0 / TotalNumberOfBytes.QuadPart);

	if(nBuffer > lstrlen(szFreeSpaceString))
		StringCchCopy(szBuffer,nBuffer,szFreeSpaceString);
	else
		szBuffer = NULL;

	return lstrlen(szFreeSpaceString);
}

/*
 * Returns TRUE if there are any selected items
 * in the window that currently has focus; FALSE
 * otherwise.
 */
BOOL Explorerplusplus::CheckItemSelection(void)
{
	HWND hFocus;

	hFocus = GetFocus();

	if(hFocus == m_hActiveListView)
	{
		if(ListView_GetSelectedCount(m_hActiveListView) > 0)
			return TRUE;
	}
	else if(hFocus == m_hTreeView)
	{
		if(TreeView_GetSelection(m_hTreeView) != NULL)
			return TRUE;
	}

	return FALSE;
}

BOOL Explorerplusplus::VerifyLanguageVersion(TCHAR *szLanguageModule) const
{
	TCHAR szImageName[MAX_PATH];
	DWORD dwpvProcessLS;
	DWORD dwpvProcessMS;
	DWORD dwpvLanguageLS;
	DWORD dwpvLanguageMS;
	DWORD dwRet;
	BOOL bSuccess1;
	BOOL bSuccess2;

	dwRet = GetCurrentProcessImageName(szImageName,SIZEOF_ARRAY(szImageName));

	if(dwRet != 0)
	{
		bSuccess1 = GetFileProductVersion(szImageName,&dwpvProcessLS,&dwpvProcessMS);
		bSuccess2 = GetFileProductVersion(szLanguageModule,&dwpvLanguageLS,&dwpvLanguageMS);

		if(bSuccess1 && bSuccess2)
		{
			/* For the version of the language DLL to match
			the version of the executable, the major version,
			minor version and micro version must match. The
			build version is ignored. */
			if(HIWORD(dwpvLanguageMS) == HIWORD(dwpvProcessMS) &&
				LOWORD(dwpvLanguageMS) == LOWORD(dwpvProcessMS) &&
				HIWORD(dwpvLanguageLS) == HIWORD(dwpvProcessLS))
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

int Explorerplusplus::LookupColumnNameStringIndex(int iColumnId)
{
	switch(iColumnId)
	{
	case CM_NAME:
		return IDS_COLUMN_NAME_NAME;
		break;

	case CM_TYPE:
		return IDS_COLUMN_NAME_TYPE;
		break;

	case CM_SIZE:
		return IDS_COLUMN_NAME_SIZE;
		break;

	case CM_DATEMODIFIED:
		return IDS_COLUMN_NAME_DATEMODIFIED;
		break;

	case CM_ATTRIBUTES:
		return IDS_COLUMN_NAME_ATTRIBUTES;
		break;

	case CM_REALSIZE:
		return IDS_COLUMN_NAME_REALSIZE;
		break;

	case CM_SHORTNAME:
		return IDS_COLUMN_NAME_SHORTNAME;
		break;

	case CM_OWNER:
		return IDS_COLUMN_NAME_OWNER;
		break;

	case CM_PRODUCTNAME:
		return IDS_COLUMN_NAME_PRODUCTNAME;
		break;

	case CM_COMPANY:
		return IDS_COLUMN_NAME_COMPANY;
		break;

	case CM_DESCRIPTION:
		return IDS_COLUMN_NAME_DESCRIPTION;
		break;

	case CM_FILEVERSION:
		return IDS_COLUMN_NAME_FILEVERSION;
		break;

	case CM_PRODUCTVERSION:
		return IDS_COLUMN_NAME_PRODUCTVERSION;
		break;

	case CM_SHORTCUTTO:
		return IDS_COLUMN_NAME_SHORTCUTTO;
		break;

	case CM_HARDLINKS:
		return IDS_COLUMN_NAME_HARDLINKS;
		break;

	case CM_EXTENSION:
		return IDS_COLUMN_NAME_EXTENSION;
		break;

	case CM_CREATED:
		return IDS_COLUMN_NAME_CREATED;
		break;

	case CM_ACCESSED:
		return IDS_COLUMN_NAME_ACCESSED;
		break;

	case CM_TITLE:
		return IDS_COLUMN_NAME_TITLE;
		break;

	case CM_SUBJECT:
		return IDS_COLUMN_NAME_SUBJECT;
		break;

	case CM_AUTHOR:
		return IDS_COLUMN_NAME_AUTHOR;
		break;

	case CM_KEYWORDS:
		return IDS_COLUMN_NAME_KEYWORDS;
		break;

	case CM_COMMENT:
		return IDS_COLUMN_NAME_COMMENT;
		break;

	case CM_CAMERAMODEL:
		return IDS_COLUMN_NAME_CAMERAMODEL;
		break;

	case CM_DATETAKEN:
		return IDS_COLUMN_NAME_DATETAKEN;
		break;

	case CM_WIDTH:
		return IDS_COLUMN_NAME_WIDTH;
		break;

	case CM_HEIGHT:
		return IDS_COLUMN_NAME_HEIGHT;
		break;

	case CM_VIRTUALCOMMENTS:
		return IDS_COLUMN_NAME_VIRTUALCOMMENTS;
		break;

	case CM_TOTALSIZE:
		return IDS_COLUMN_NAME_TOTALSIZE;
		break;

	case CM_FREESPACE:
		return IDS_COLUMN_NAME_FREESPACE;
		break;

	case CM_FILESYSTEM:
		return IDS_COLUMN_NAME_FILESYSTEM;
		break;

	case CM_ORIGINALLOCATION:
		return IDS_COLUMN_NAME_ORIGINALLOCATION;
		break;

	case CM_DATEDELETED:
		return IDS_COLUMN_NAME_DATEDELETED;
		break;

	case CM_NUMPRINTERDOCUMENTS:
		return IDS_COLUMN_NAME_NUMPRINTERDOCUMENTS;
		break;

	case CM_PRINTERSTATUS:
		return IDS_COLUMN_NAME_PRINTERSTATUS;
		break;

	case CM_PRINTERCOMMENTS:
		return IDS_COLUMN_NAME_PRINTERCOMMENTS;
		break;

	case CM_PRINTERLOCATION:
		return IDS_COLUMN_NAME_PRINTERLOCATION;
		break;

	case CM_PRINTERMODEL:
		return IDS_COLUMN_NAME_PRINTERMODEL;
		break;

	case CM_NETWORKADAPTER_STATUS:
		return IDS_COLUMN_NAME_NETWORKADAPTER_STATUS;
		break;

	case CM_MEDIA_BITRATE:
		return IDS_COLUMN_NAME_BITRATE;
		break;

	case CM_MEDIA_COPYRIGHT:
		return IDS_COLUMN_NAME_COPYRIGHT;
		break;

	case CM_MEDIA_DURATION:
		return IDS_COLUMN_NAME_DURATION;
		break;

	case CM_MEDIA_PROTECTED:
		return IDS_COLUMN_NAME_PROTECTED;
		break;

	case CM_MEDIA_RATING:
		return IDS_COLUMN_NAME_RATING;
		break;

	case CM_MEDIA_ALBUMARTIST:
		return IDS_COLUMN_NAME_ALBUMARTIST;
		break;

	case CM_MEDIA_ALBUM:
		return IDS_COLUMN_NAME_ALBUM;
		break;

	case CM_MEDIA_BEATSPERMINUTE:
		return IDS_COLUMN_NAME_BEATSPERMINUTE;
		break;

	case CM_MEDIA_COMPOSER:
		return IDS_COLUMN_NAME_COMPOSER;
		break;

	case CM_MEDIA_CONDUCTOR:
		return IDS_COLUMN_NAME_CONDUCTOR;
		break;

	case CM_MEDIA_DIRECTOR:
		return IDS_COLUMN_NAME_DIRECTOR;
		break;

	case CM_MEDIA_GENRE:
		return IDS_COLUMN_NAME_GENRE;
		break;

	case CM_MEDIA_LANGUAGE:
		return IDS_COLUMN_NAME_LANGUAGE;
		break;

	case CM_MEDIA_BROADCASTDATE:
		return IDS_COLUMN_NAME_BROADCASTDATE;
		break;

	case CM_MEDIA_CHANNEL:
		return IDS_COLUMN_NAME_CHANNEL;
		break;

	case CM_MEDIA_STATIONNAME:
		return IDS_COLUMN_NAME_STATIONNAME;
		break;

	case CM_MEDIA_MOOD:
		return IDS_COLUMN_NAME_MOOD;
		break;

	case CM_MEDIA_PARENTALRATING:
		return IDS_COLUMN_NAME_PARENTALRATING;
		break;

	case CM_MEDIA_PARENTALRATINGREASON:
		return IDS_COLUMN_NAME_PARENTALRATINGREASON;
		break;

	case CM_MEDIA_PERIOD:
		return IDS_COLUMN_NAME_PERIOD;
		break;

	case CM_MEDIA_PRODUCER:
		return IDS_COLUMN_NAME_PRODUCER;
		break;

	case CM_MEDIA_PUBLISHER:
		return IDS_COLUMN_NAME_PUBLISHER;
		break;

	case CM_MEDIA_WRITER:
		return IDS_COLUMN_NAME_WRITER;
		break;

	case CM_MEDIA_YEAR:
		return IDS_COLUMN_NAME_YEAR;
		break;
	}

	return 0;
}

int Explorerplusplus::LookupColumnDescriptionStringIndex(int iColumnId)
{
	switch(iColumnId)
	{
		case CM_NAME:
			return IDS_COLUMN_DESCRIPTION_NAME;
			break;

		case CM_TYPE:
			return IDS_COLUMN_DESCRIPTION_TYPE;
			break;

		case CM_SIZE:
			return IDS_COLUMN_DESCRIPTION_SIZE;
			break;

		case CM_DATEMODIFIED:
			return IDS_COLUMN_DESCRIPTION_MODIFIED;
			break;

		case CM_ATTRIBUTES:
			return IDS_COLUMN_DESCRIPTION_ATTRIBUTES;
			break;

		case CM_REALSIZE:
			return IDS_COLUMN_DESCRIPTION_REALSIZE;
			break;

		case CM_SHORTNAME:
			return IDS_COLUMN_DESCRIPTION_SHORTNAME;
			break;

		case CM_OWNER:
			return IDS_COLUMN_DESCRIPTION_OWNER;
			break;

		case CM_PRODUCTNAME:
			return IDS_COLUMN_DESCRIPTION_PRODUCTNAME;
			break;

		case CM_COMPANY:
			return IDS_COLUMN_DESCRIPTION_COMPANY;
			break;

		case CM_DESCRIPTION:
			return IDS_COLUMN_DESCRIPTION_DESCRIPTION;
			break;

		case CM_FILEVERSION:
			return IDS_COLUMN_DESCRIPTION_FILEVERSION;
			break;

		case CM_PRODUCTVERSION:
			return IDS_COLUMN_DESCRIPTION_PRODUCTVERSION;
			break;

		case CM_SHORTCUTTO:
			return IDS_COLUMN_DESCRIPTION_SHORTCUTTO;
			break;

		case CM_HARDLINKS:
			return IDS_COLUMN_DESCRIPTION_HARDLINKS;
			break;

		case CM_EXTENSION:
			return IDS_COLUMN_DESCRIPTION_EXTENSION;
			break;
			
		case CM_CREATED:
			return IDS_COLUMN_DESCRIPTION_CREATED;
			break;
			
		case CM_ACCESSED:
			return IDS_COLUMN_DESCRIPTION_ACCESSED;
			break;

		case CM_TITLE:
			return IDS_COLUMN_DESCRIPTION_TITLE;
			break;

		case CM_SUBJECT:
			return IDS_COLUMN_DESCRIPTION_SUBJECT;
			break;

		case CM_AUTHOR:
			return IDS_COLUMN_DESCRIPTION_AUTHOR;
			break;

		case CM_KEYWORDS:
			return IDS_COLUMN_DESCRIPTION_KEYWORDS;
			break;

		case CM_COMMENT:
			return IDS_COLUMN_DESCRIPTION_COMMENT;
			break;

		case CM_CAMERAMODEL:
			return IDS_COLUMN_DESCRIPTION_CAMERAMODEL;
			break;

		case CM_DATETAKEN:
			return IDS_COLUMN_DESCRIPTION_DATETAKEN;
			break;

		case CM_WIDTH:
			return IDS_COLUMN_DESCRIPTION_WIDTH;
			break;

		case CM_HEIGHT:
			return IDS_COLUMN_DESCRIPTION_HEIGHT;
			break;

		case CM_VIRTUALCOMMENTS:
			return IDS_COLUMN_DESCRIPTION_COMMENT;
			break;

		case CM_TOTALSIZE:
			return IDS_COLUMN_DESCRIPTION_TOTALSIZE;
			break;

		case CM_FREESPACE:
			return IDS_COLUMN_DESCRIPTION_FREESPACE;
			break;

		case CM_FILESYSTEM:
			return IDS_COLUMN_DESCRIPTION_FILESYSTEM;
			break;

		case CM_NUMPRINTERDOCUMENTS:
			return IDS_COLUMN_DESCRIPTION_NUMPRINTERDOCUMENTS;
			break;

		case CM_PRINTERCOMMENTS:
			return IDS_COLUMN_DESCRIPTION_PRINTERCOMMENTS;
			break;

		case CM_PRINTERLOCATION:
			return IDS_COLUMN_DESCRIPTION_PRINTERLOCATION;
			break;

		case CM_NETWORKADAPTER_STATUS:
			return IDS_COLUMN_DESCRIPTION_NETWORKADAPTER_STATUS;
			break;

		case CM_MEDIA_BITRATE:
			return IDS_COLUMN_DESCRIPTION_BITRATE;
			break;
	}

	return 0;
}

void Explorerplusplus::OnSelectColumns()
{
	CSelectColumnsDialog SelectColumnsDialog(m_hLanguageModule,IDD_SELECTCOLUMNS,m_hContainer,this);
	SelectColumnsDialog.ShowModalDialog();

	UpdateArrangeMenuItems();
}

CStatusBar *Explorerplusplus::GetStatusBar()
{
	return m_pStatusBar;
}
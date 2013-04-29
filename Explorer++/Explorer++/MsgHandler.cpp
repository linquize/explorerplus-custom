/******************************************************************
 *
 * Project: Explorer++
 * File: MsgHandler.cpp
 * License: GPL - See COPYING in the top level directory
 *
 * Handles messages passed back from the main GUI components.
 *
 * Written by David Erceg
 * www.explorerplusplus.com
 *
 *****************************************************************/

#include "stdafx.h"
#include "Explorer++.h"
#include "Explorer++_internal.h"
#include "WildcardSelectDialog.h"
#include "MainResource.h"
#include "../Helper/ShellHelper.h"
#include "../Helper/ListViewHelper.h"
#include "../Helper/Controls.h"
#include "../Helper/RegistrySettings.h"
#include "../Helper/Macros.h"


DWORD WINAPI Thread_IconFinder(LPVOID pParam);
void CALLBACK IconThreadInitialization(ULONG_PTR dwParam);
void CALLBACK QuitIconAPC(ULONG_PTR dwParam);

DWORD WINAPI Thread_IconFinder(LPVOID pParam)
{
	/* OLE initialization is no longer done from within
	this function. This is because of the fact that the
	first APC may run BEFORE this thread initialization
	function. If this occurs, OLE will not be initialized,
	and possible errors may occur.
	OLE is now initialized using an APC that is queued
	immediately after this thread is created. As APC's
	are run sequentially, it is guaranteed that the
	initialization APC will run before any other APC,
	thus acting like this initialization function. */

	/* WARNING: Warning C4127 (conditional expression is
	constant) temporarily disabled for this function. */
	#pragma warning(push)
	#pragma warning(disable:4127)
	while(TRUE)
	{
		SleepEx(INFINITE,TRUE);
	}
	#pragma warning(pop)

	return 0;
}

void CALLBACK IconThreadInitialization(ULONG_PTR dwParam)
{
	/* This will be balanced out by a corresponding
	CoUninitialize() when the thread is ended.
	It must be apartment threaded, or some icons (such
	as those used for XML files) may not load properly.
	It *may* be due to the fact that one or more of
	the other threads in use do not initialize COM/
	use the same threading model. */
	CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
}

void CALLBACK QuitIconAPC(ULONG_PTR dwParam)
{
	CoUninitialize();
}

/*
 * Main window creation.
 *
 * Settings are loaded very early on. Any
 * initial settings must be in place before
 * this.
 */
void Explorerplusplus::OnWindowCreate(void)
{
	ILoadSave *pLoadSave = NULL;

	InitializeTaskbarThumbnails();

	LoadAllSettings(&pLoadSave);
	ApplyToolbarSettings();

	SetLanguageModule();

	InitializeBookmarks();

	m_hIconThread = CreateThread(NULL,0,Thread_IconFinder,NULL,0,NULL);
	SetThreadPriority(m_hIconThread,THREAD_PRIORITY_BELOW_NORMAL);
	QueueUserAPC(IconThreadInitialization,m_hIconThread,NULL);

	m_hTreeViewIconThread = CreateThread(NULL,0,Thread_IconFinder,NULL,0,NULL);
	SetThreadPriority(m_hTreeViewIconThread,THREAD_PRIORITY_BELOW_NORMAL);
	QueueUserAPC(IconThreadInitialization,m_hTreeViewIconThread,NULL);

	m_hFolderSizeThread = CreateThread(NULL,0,Thread_IconFinder,NULL,0,NULL);
	SetThreadPriority(m_hFolderSizeThread,THREAD_PRIORITY_BELOW_NORMAL);
	QueueUserAPC(IconThreadInitialization,m_hFolderSizeThread,NULL);

	/* These need to occur after the language module
	has been initialized, but before the tabs are
	restored. */
	SetMenu(m_hContainer,LoadMenu(m_hLanguageModule,MAKEINTRESOURCE(IDR_MAINMENU)));
	m_hArrangeSubMenu				= GetSubMenu(LoadMenu(m_hLanguageModule,MAKEINTRESOURCE(IDR_ARRANGEMENU)),0);
	m_hArrangeSubMenuRClick			= GetSubMenu(LoadMenu(m_hLanguageModule,MAKEINTRESOURCE(IDR_ARRANGEMENU)),0);
	m_hGroupBySubMenu				= GetSubMenu(LoadMenu(m_hLanguageModule,MAKEINTRESOURCE(IDR_GROUPBY_MENU)),0);
	m_hGroupBySubMenuRClick			= GetSubMenu(LoadMenu(m_hLanguageModule,MAKEINTRESOURCE(IDR_GROUPBY_MENU)),0);
	m_hTabRightClickMenu			= GetSubMenu(LoadMenu(m_hLanguageModule,MAKEINTRESOURCE(IDR_TAB_RCLICK)),0);
	m_hToolbarRightClickMenu		= GetSubMenu(LoadMenu(m_hLanguageModule,MAKEINTRESOURCE(IDR_TOOLBAR_MENU)),0);
	m_hDisplayWindowRightClickMenu	= GetSubMenu(LoadMenu(m_hLanguageModule,MAKEINTRESOURCE(IDR_DISPLAYWINDOW_RCLICK)),0);
	m_hViewsMenu					= GetSubMenu(LoadMenu(m_hLanguageModule,MAKEINTRESOURCE(IDR_VIEWS_MENU)),0);

	HBITMAP hb;

	/* Large and small image lists for the main toolbar. */
	m_himlToolbarSmall = ImageList_Create(TOOLBAR_IMAGE_SIZE_SMALL_X,TOOLBAR_IMAGE_SIZE_SMALL_Y,ILC_COLOR32|ILC_MASK,0,47);
	hb = LoadBitmap(GetModuleHandle(0),MAKEINTRESOURCE(IDB_SHELLIMAGES));
	ImageList_Add(m_himlToolbarSmall,hb,NULL);
	DeleteObject(hb);

	m_himlToolbarLarge = ImageList_Create(TOOLBAR_IMAGE_SIZE_LARGE_X,TOOLBAR_IMAGE_SIZE_LARGE_Y,ILC_COLOR32|ILC_MASK,0,47);
	hb = LoadBitmap(GetModuleHandle(0),MAKEINTRESOURCE(IDB_SHELLIMAGES_LARGE));
	ImageList_Add(m_himlToolbarLarge,hb,NULL);
	DeleteObject(hb);

	CreateDirectoryMonitor(&m_pDirMon);

	CreateStatusBar();
	CreateMainControls();
	InitializeDisplayWindow();
	InitializeTabs();
	CreateFolderControls();

	/* All child windows MUST be resized before
	any listview changes take place. If auto arrange
	is turned off in the listview, when it is
	initially sized, all current items will lock
	to the current width. The only was to unlock
	them from this width is to turn auto arrange back on.
	Therefore, the listview MUST be set to the correct
	size initially. */
	ResizeWindows();

	/* Settings cannot be applied until
	all child windows have been created. */
	ApplyLoadedSettings();

	/* Taskbar thumbnails can only be shown in
	Windows 7, so we'll set the internal setting to
	false if we're running on an earlier version
	of Windows. */
	if(!(m_dwMajorVersion == WINDOWS_VISTA_SEVEN_MAJORVERSION &&
		m_dwMinorVersion >= 1))
	{
		m_bShowTaskbarThumbnails = FALSE;
	}

	/* The internal variable that controls whether or not
	taskbar thumbnails are shown in Windows 7 should only
	be set once during execution (i.e. when Explorer++
	starts up).
	Therefore, we'll only ever show the user a provisional
	setting, to stop them from changing the actual value. */
	m_bShowTaskbarThumbnailsProvisional	= m_bShowTaskbarThumbnails;

	RestoreTabs(pLoadSave);
	delete pLoadSave;




	SHChangeNotifyEntry shcne;

	/* Don't need to specify any file for this notification. */
	shcne.fRecursive	= TRUE;
	shcne.pidl			= NULL;

	/* Register for any shell changes. This should
	be done after the tabs have been created. */
	SHChangeNotifyRegister(m_hContainer,SHCNRF_ShellLevel,SHCNE_ASSOCCHANGED,
		WM_APP_ASSOCCHANGED,1,&shcne);




	/* Mark the main menus as owner drawn. */
	InitializeMenus();

	InitializeArrangeMenuItems();

	/* Place the main window in the clipboard chain. This
	will allow the 'Paste' button to be enabled/disabled
	dynamically. */
	m_hNextClipboardViewer = SetClipboardViewer(m_hContainer);

	SetFocus(m_hActiveListView);
}

void Explorerplusplus::TestConfigFile(void)
{
	m_bLoadSettingsFromXML = TestConfigFileInternal();
}

BOOL TestConfigFileInternal(void)
{
	HANDLE	hConfigFile;
	TCHAR	szConfigFile[MAX_PATH];
	BOOL	bLoadSettingsFromXML = FALSE;

	/* To ensure the configuration file is loaded from the same directory
	as the executable, determine the fully qualified path of the executable,
	then save the configuration file in that directory. */
	GetCurrentProcessImageName(szConfigFile,SIZEOF_ARRAY(szConfigFile));

	PathRemoveFileSpec(szConfigFile);
	PathAppend(szConfigFile,NExplorerplusplus::XML_FILENAME);

	hConfigFile = CreateFile(szConfigFile,GENERIC_READ,FILE_SHARE_READ,NULL,
		OPEN_EXISTING,0,NULL);

	if(hConfigFile != INVALID_HANDLE_VALUE)
	{
		bLoadSettingsFromXML = TRUE;

		CloseHandle(hConfigFile);
	}

	return bLoadSettingsFromXML;
}

void Explorerplusplus::LoadAllSettings(ILoadSave **pLoadSave)
{
	/* Tests for the existence of the configuration
	file. If the file is present, a flag is set
	indicating that the config file should be used
	to load settings. */
	TestConfigFile();

	/* Initialize the LoadSave interface. Note
	that this interface must be regenerated when
	saving, as it's possible for the save/load
	methods to be different. */
	if(m_bLoadSettingsFromXML)
	{
		*pLoadSave = new CLoadSaveXML(this,TRUE);

		/* When loading from the config file, also
		set the option to save back to it on exit. */
		m_bSavePreferencesToXMLFile = TRUE;
	}
	else
	{
		*pLoadSave = new CLoadSaveRegistry(this);
	}

	(*pLoadSave)->LoadBookmarks();
	(*pLoadSave)->LoadGenericSettings();
	(*pLoadSave)->LoadDefaultColumns();
	(*pLoadSave)->LoadApplicationToolbar();
	(*pLoadSave)->LoadToolbarInformation();
	(*pLoadSave)->LoadColorRules();
	(*pLoadSave)->LoadDialogStates();

	ValidateLoadedSettings();
}

/*
 * Selects which language resource DLL based
 * on user preferences and system language.
 * The default language is English.
 */
void Explorerplusplus::SetLanguageModule(void)
{
	HANDLE			hFindFile;
	WIN32_FIND_DATA	wfd;
	LANGID			LanguageID;
	TCHAR			szLanguageModule[MAX_PATH];
	TCHAR			szNamePattern[MAX_PATH];
	TCHAR			szFullFileName[MAX_PATH];
	TCHAR			szName[MAX_PATH];
	WORD			wLanguage;

	if(g_bForceLanguageLoad)
	{
		/* Language has been forced on the command
		line by the user. Attempt to find the
		corresponding DLL. */
		GetCurrentProcessImageName(szLanguageModule,SIZEOF_ARRAY(szLanguageModule));
		PathRemoveFileSpec(szLanguageModule);
		StringCchPrintf(szName,SIZEOF_ARRAY(szName),_T("Explorer++%2s.dll"),g_szLang);
		PathAppend(szLanguageModule,szName);

		wLanguage = GetFileLanguage(szLanguageModule);

		m_Language = wLanguage;
	}
	else
	{
		if(!m_bLanguageLoaded)
		{
			/* No previous language loaded. Try and use the system
			default language. */
			LanguageID = GetUserDefaultUILanguage();

			m_Language = PRIMARYLANGID(LanguageID);
		}
	}

	if(m_Language == LANG_ENGLISH)
	{
		m_hLanguageModule = GetModuleHandle(NULL);
	}
	else
	{
		GetCurrentProcessImageName(szLanguageModule,SIZEOF_ARRAY(szLanguageModule));
		PathRemoveFileSpec(szLanguageModule);

		StringCchCopy(szNamePattern,SIZEOF_ARRAY(szNamePattern),szLanguageModule);
		PathAppend(szNamePattern,_T("Explorer++??.dll"));

		hFindFile = FindFirstFile(szNamePattern,&wfd);

		/* Loop through the current translation DLL's to
		try and find one that matches the specified
		language. */
		if(hFindFile != INVALID_HANDLE_VALUE)
		{
			StringCchCopy(szFullFileName,SIZEOF_ARRAY(szFullFileName),szLanguageModule);
			PathAppend(szFullFileName,wfd.cFileName);
			wLanguage = GetFileLanguage(szFullFileName);

			BOOL bLanguageMismatch = FALSE;

			if(wLanguage == m_Language)
			{
				/* Using translation DLL's built for other versions of
				the executable will most likely crash the program due
				to incorrect/missing resources.
				Therefore, only load the specified translation DLL
				if it matches the current internal version. */
				if(VerifyLanguageVersion(szFullFileName))
				{
					m_hLanguageModule = LoadLibrary(szFullFileName);
				}
				else
				{
					bLanguageMismatch = TRUE;
				}
			}
			else
			{
				while(FindNextFile(hFindFile,&wfd) != 0)
				{
					StringCchCopy(szFullFileName,SIZEOF_ARRAY(szFullFileName),szLanguageModule);
					PathAppend(szFullFileName,wfd.cFileName);
					wLanguage = GetFileLanguage(szFullFileName);

					if(wLanguage == m_Language)
					{
						if(VerifyLanguageVersion(szFullFileName))
						{
							m_hLanguageModule = LoadLibrary(szFullFileName);
						}
						else
						{
							bLanguageMismatch = TRUE;
						}

						break;
					}
				}
			}

			FindClose(hFindFile);

			if(bLanguageMismatch)
			{
				TCHAR szTemp[256];

				/* Attempt to show an error message in the language
				that was specified. */
				switch(wLanguage)
				{
				case LANG_CHINESE_SIMPLIFIED:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_CHINESE_SIMPLIFIED,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_CZECH:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_CZECH,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_DANISH:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_DANISH,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_DUTCH:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_DUTCH,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_FRENCH:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_FRENCH,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_GERMAN:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_GERMAN,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_ITALIAN:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_ITALIAN,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_JAPANESE:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_JAPANESE,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_KOREAN:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_KOREAN,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_NORWEGIAN:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_NORWEGIAN,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_PORTUGUESE:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_PORTUGUESE,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_ROMANIAN:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_ROMANIAN,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_RUSSIAN:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_RUSSIAN,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				case LANG_SPANISH:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH_SPANISH,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;

				default:
					LoadString(GetModuleHandle(NULL),IDS_GENERAL_TRANSLATION_DLL_VERSION_MISMATCH,
						szTemp,SIZEOF_ARRAY(szTemp));
					break;
				}

				/* Main window hasn't been constructed yet, so this
				message box doesn't have any owner window. */
				MessageBox(NULL,szTemp,NExplorerplusplus::WINDOW_NAME,MB_ICONWARNING);
			}
		}
	}

	/* The language DLL was not found/could not be loaded.
	Use the default internal resource set. */
	if(m_hLanguageModule == NULL)
	{
		m_hLanguageModule = GetModuleHandle(NULL);

		m_Language = LANG_ENGLISH;
	}
}

void Explorerplusplus::SetMenuOwnerDraw(HMENU hMenu)
{
	int nTopLevelMenus;

	nTopLevelMenus = GetMenuItemCount(hMenu);

	SetMenuOwnerDrawInternal(hMenu,nTopLevelMenus);
}

/*
 * Marks the specified menu as owner drawn.
 */
void Explorerplusplus::SetMenuOwnerDrawInternal(HMENU hMenu,
int nMenus)
{
	MENUITEMINFO		mi;
	int					i = 0;

	for(i = 0;i < nMenus;i++)
	{
		SetMenuItemOwnerDrawn(hMenu,i);

		mi.cbSize		= sizeof(mi);
		mi.fMask		= MIIM_SUBMENU;
		GetMenuItemInfo(hMenu,i,TRUE,&mi);

		if(mi.hSubMenu != NULL)
			SetMenuOwnerDraw(mi.hSubMenu);
	}
}

void Explorerplusplus::SetMenuItemOwnerDrawn(HMENU hMenu,int iItem)
{
	MENUITEMINFO		mi;
	CustomMenuInfo_t	*pcmi = NULL;

	mi.cbSize		= sizeof(mi);
	mi.fMask		= MIIM_FTYPE|MIIM_ID;

	GetMenuItemInfo(hMenu,iItem,TRUE,&mi);

	if(!(mi.fType & MFT_OWNERDRAW))
		mi.fType |= MFT_OWNERDRAW;

	pcmi = (CustomMenuInfo_t *)malloc(sizeof(CustomMenuInfo_t));

	pcmi->bUseImage		= FALSE;
	pcmi->dwItemData	= 0;

	mi.fMask		|= MIIM_DATA;
	mi.dwItemData	= (ULONG_PTR)pcmi;
	SetMenuItemInfo(hMenu,iItem,TRUE,&mi);
}

/*
 * Sets the bitmap for an owner drawn menu.
 * The menu MUST have already been marked as
 * owner drawn (so that the owner drawn menu
 * structure is in place).
 */
void Explorerplusplus::SetMenuItemBitmap(HMENU hMenu,UINT ItemID,int iBitmap)
{
	MENUITEMINFO		mi;
	CustomMenuInfo_t	*pcmi = NULL;

	BOOL bRes;

	mi.cbSize		= sizeof(mi);
	mi.fMask		= MIIM_DATA;
	bRes = GetMenuItemInfo(hMenu,ItemID,FALSE,&mi);

	if(bRes)
	{
		pcmi = (CustomMenuInfo_t *)mi.dwItemData;

		pcmi->bUseImage = TRUE;
		pcmi->iImage	= iBitmap;
	}
}

/*
 * Creates a new tab. If a folder is selected,
 * that folder is opened in a new tab, else
 * the default directory is opened.
 */
void Explorerplusplus::OnNewTab(void)
{
	int		iSelected;
	HRESULT	hr;
	BOOL	bFolderSelected = FALSE;

	iSelected = ListView_GetNextItem(m_hActiveListView,
	-1,LVNI_FOCUSED|LVNI_SELECTED);

	if(iSelected != -1)
	{
		TCHAR FullItemPath[MAX_PATH];

		/* An item is selected, so get its full pathname. */
		m_pActiveShellBrowser->QueryFullItemName(iSelected,FullItemPath);

		/* If the selected item is a folder, open that folder
		in a new tab, else just use the default new tab directory. */
		if(PathIsDirectory(FullItemPath))
		{
			bFolderSelected = TRUE;
			BrowseFolder(FullItemPath,SBSP_ABSOLUTE,TRUE,TRUE,FALSE);
		}
	}

	/* Either no items are selected, or the focused + selected
	item was not a folder; open the default tab directory. */
	if(!bFolderSelected)
	{
		hr = BrowseFolder(m_DefaultTabDirectory,SBSP_ABSOLUTE,TRUE,TRUE,FALSE);

		if(FAILED(hr))
			BrowseFolder(m_DefaultTabDirectoryStatic,SBSP_ABSOLUTE,TRUE,TRUE,FALSE);
	}
}

/*
 * Called when a key is pressed in the main combobox
 * (i.e. the address bar).
 */
void Explorerplusplus::OnComboBoxKeyDown(WPARAM wParam)
{
	switch(wParam)
	{
		case VK_RETURN:
			OnAddressBarGo();
			break;
	}
}

/*
 * Navigates to the folder specified by the incoming
 * csidl.
 */
void Explorerplusplus::GotoFolder(int FolderCSIDL)
{
	LPITEMIDLIST	pidl = NULL;
	HRESULT			hr;

	hr = SHGetFolderLocation(NULL,FolderCSIDL,NULL,0,&pidl);

	/* Don't use SUCCEEDED(hr). */
	if(hr == S_OK)
	{
		BrowseFolder(pidl,SBSP_SAMEBROWSER|SBSP_ABSOLUTE);

		CoTaskMemFree(pidl);
	}
}

void Explorerplusplus::OpenListViewItem(int iItem,BOOL bOpenInNewTab,BOOL bOpenInNewWindow)
{
	LPITEMIDLIST	pidlComplete = NULL;
	LPITEMIDLIST	pidl = NULL;
	LPITEMIDLIST	ridl = NULL;

	pidl = m_pActiveShellBrowser->QueryCurrentDirectoryIdl();
	ridl = m_pActiveShellBrowser->QueryItemRelativeIdl(iItem);

	if(ridl != NULL)
	{
		pidlComplete = ILCombine(pidl,ridl);

		OpenItem(pidlComplete,bOpenInNewTab,bOpenInNewWindow);

		CoTaskMemFree(pidlComplete);
		CoTaskMemFree(ridl);
	}

	CoTaskMemFree(pidl);
}

void Explorerplusplus::OpenItem(TCHAR *szItem,BOOL bOpenInNewTab,BOOL bOpenInNewWindow)
{
	LPITEMIDLIST	pidlItem = NULL;
	HRESULT			hr;

	hr = GetIdlFromParsingName(szItem,&pidlItem);

	if(SUCCEEDED(hr))
	{
		OpenItem(pidlItem,bOpenInNewTab,bOpenInNewWindow);

		CoTaskMemFree(pidlItem);
	}
}

void Explorerplusplus::OpenItem(LPITEMIDLIST pidlItem,BOOL bOpenInNewTab,BOOL bOpenInNewWindow)
{
	SFGAOF uAttributes = SFGAO_FOLDER|SFGAO_STREAM|SFGAO_LINK;
	LPITEMIDLIST pidlControlPanel = NULL;
	HRESULT	hr;
	BOOL bControlPanelParent = FALSE;

	hr = SHGetFolderLocation(NULL,CSIDL_CONTROLS,NULL,0,&pidlControlPanel);

	if(SUCCEEDED(hr))
	{
		/* Check if the parent of the item is the control panel.
		If it is, pass it to the shell to open, rather than
		opening it in-place. */
		if(ILIsParent(pidlControlPanel,pidlItem,FALSE) &&
			!CompareIdls(pidlControlPanel,pidlItem))
		{
			bControlPanelParent = TRUE;
		}

		CoTaskMemFree(pidlControlPanel);
	}

	/* On Vista and later, the Control Panel was split into
	two completely separate views:
	 - Icon View
	 - Category View
	Icon view is essentially the same view provided in
	Windows XP and earlier (i.e. a simple, flat listing of
	all the items in the control panel).
	Category view, on the other hand, groups similar
	Control Panel items under several broad categories.
	It is important to note that both these 'views' are
	represented by different GUID's, and are NOT the
	same folder.
	 - Icon View:
	   ::{21EC2020-3AEA-1069-A2DD-08002B30309D} (Vista and Win 7)
	   ::{26EE0668-A00A-44D7-9371-BEB064C98683}\0 (Win 7)
	 - Category View:
	   ::{26EE0668-A00A-44D7-9371-BEB064C98683} (Vista and Win 7)
	*/
	if(m_dwMajorVersion >= WINDOWS_VISTA_SEVEN_MAJORVERSION)
	{
		if(!bControlPanelParent)
		{
			hr = GetIdlFromParsingName(CONTROL_PANEL_CATEGORY_VIEW,&pidlControlPanel);

			if(SUCCEEDED(hr))
			{
				/* Check if the parent of the item is the control panel.
				If it is, pass it to the shell to open, rather than
				opening it in-place. */
				if(ILIsParent(pidlControlPanel,pidlItem,FALSE) &&
					!CompareIdls(pidlControlPanel,pidlItem))
				{
					bControlPanelParent = TRUE;
				}

				CoTaskMemFree(pidlControlPanel);
			}
		}
	}

	hr = GetItemAttributes(pidlItem,&uAttributes);

	if(SUCCEEDED(hr))
	{
		if((uAttributes & SFGAO_FOLDER) && (uAttributes & SFGAO_STREAM))
		{
			/* Zip file. */
			if(m_bHandleZipFiles)
			{
				OpenFolderItem(pidlItem,bOpenInNewTab,bOpenInNewWindow);
			}
			else
			{
				OpenFileItem(pidlItem,EMPTY_STRING);
			}
		}
		else if(((uAttributes & SFGAO_FOLDER) && !bControlPanelParent))
		{
			/* Open folders. */
			OpenFolderItem(pidlItem,bOpenInNewTab,bOpenInNewWindow);
		}
		else if(uAttributes & SFGAO_LINK && !bControlPanelParent)
		{
			/* This item is a shortcut. */
			TCHAR	szItemPath[MAX_PATH];
			TCHAR	szTargetPath[MAX_PATH];

			GetDisplayName(pidlItem,szItemPath,SHGDN_FORPARSING);

			hr = NFileOperations::ResolveLink(m_hContainer,0,szItemPath,szTargetPath,SIZEOF_ARRAY(szTargetPath));

			if(hr == S_OK)
			{
				/* The target of the shortcut was found
				successfully. Query it to determine whether
				it is a folder or not. */
				uAttributes = SFGAO_FOLDER|SFGAO_STREAM;
				hr = GetItemAttributes(szTargetPath,&uAttributes);

				/* Note this is functionally equivalent to
				recursively calling this function again.
				However, the link may be arbitrarily deep
				(or point to itself). Therefore, DO NOT
				call this function recursively with itself
				without some way of stopping. */
				if(SUCCEEDED(hr))
				{
					/* Is this a link to a folder or zip file? */
					if(((uAttributes & SFGAO_FOLDER) && !(uAttributes & SFGAO_STREAM)) ||
						((uAttributes & SFGAO_FOLDER) && (uAttributes & SFGAO_STREAM) && m_bHandleZipFiles))
					{
						LPITEMIDLIST	pidlTarget = NULL;

						hr = GetIdlFromParsingName(szTargetPath,&pidlTarget);

						if(SUCCEEDED(hr))
						{
							OpenFolderItem(pidlTarget,bOpenInNewTab,bOpenInNewWindow);

							CoTaskMemFree(pidlTarget);
						}
					}
					else
					{
						hr = E_FAIL;
					}
				}
			}

			if(FAILED(hr))
			{
				/* It is possible the target may not resolve,
				yet the shortcut is still valid. This is the
				case with shortcut URL's for example.
				Also, even if the shortcut points to a dead
				folder, it should still attempted to be
				opened. */
				OpenFileItem(pidlItem,EMPTY_STRING);
			}
		}
		else if(bControlPanelParent && (uAttributes & SFGAO_FOLDER))
		{
			TCHAR szParsingPath[MAX_PATH];
			TCHAR szExplorerPath[MAX_PATH];

			GetDisplayName(pidlItem,szParsingPath,SHGDN_FORPARSING);

			MyExpandEnvironmentStrings(_T("%windir%\\explorer.exe"),
				szExplorerPath,SIZEOF_ARRAY(szExplorerPath));

			/* Invoke Windows Explorer directly. Note that only folder
			items need to be passed directly to Explorer. Two central
			reasons:
			1. Explorer can only open folder items.
			2. Non-folder items can be opened directly (regardless of
			whether or not they're children of the control panel). */
			ShellExecute(m_hContainer,_T("open"),szExplorerPath,
				szParsingPath,NULL,SW_SHOWNORMAL);
		}
		else
		{
			/* File item. */
			OpenFileItem(pidlItem,EMPTY_STRING);
		}
	}
}

void Explorerplusplus::OpenFolderItem(LPITEMIDLIST pidlItem,BOOL bOpenInNewTab,BOOL bOpenInNewWindow)
{
	if(bOpenInNewWindow)
		BrowseFolder(pidlItem,SBSP_SAMEBROWSER,FALSE,FALSE,TRUE);
	else if(m_bAlwaysOpenNewTab || bOpenInNewTab)
		BrowseFolder(pidlItem,SBSP_SAMEBROWSER,TRUE,TRUE,FALSE);
	else
		BrowseFolder(pidlItem,SBSP_SAMEBROWSER);
}

void Explorerplusplus::OpenFileItem(LPITEMIDLIST pidlItem,const TCHAR *szParameters)
{
	TCHAR			szItemDirectory[MAX_PATH];
	LPITEMIDLIST	pidlParent = NULL;

	pidlParent = ILClone(pidlItem);

	ILRemoveLastID(pidlParent);

	GetDisplayName(pidlParent,szItemDirectory,SHGDN_FORPARSING);

	ExecuteFileAction(m_hContainer,EMPTY_STRING,szParameters,szItemDirectory,(LPCITEMIDLIST)pidlItem);

	CoTaskMemFree(pidlParent);
}

void Explorerplusplus::OnMainToolbarRClick(void)
{
	POINT ptCursor;
	DWORD dwPos;

	lCheckMenuItem(m_hToolbarRightClickMenu,IDM_TOOLBARS_ADDRESSBAR,m_bShowAddressBar);
	lCheckMenuItem(m_hToolbarRightClickMenu,IDM_TOOLBARS_MAINTOOLBAR,m_bShowMainToolbar);
	lCheckMenuItem(m_hToolbarRightClickMenu,IDM_TOOLBARS_BOOKMARKSTOOLBAR,m_bShowBookmarksToolbar);
	lCheckMenuItem(m_hToolbarRightClickMenu,IDM_TOOLBARS_DRIVES,m_bShowDrivesToolbar);
	lCheckMenuItem(m_hToolbarRightClickMenu,IDM_TOOLBARS_APPLICATIONTOOLBAR,m_bShowApplicationToolbar);
	lCheckMenuItem(m_hToolbarRightClickMenu,IDM_TOOLBARS_LOCKTOOLBARS,m_bLockToolbars);

	SetFocus(m_hMainToolbar);
	dwPos = GetMessagePos();
	ptCursor.x = GET_X_LPARAM(dwPos);
	ptCursor.y = GET_Y_LPARAM(dwPos);

	TrackPopupMenu(m_hToolbarRightClickMenu,TPM_LEFTALIGN,
		ptCursor.x,ptCursor.y,0,m_hMainRebar,NULL);
}

void Explorerplusplus::OnSaveFileSlack(void)
{
	HANDLE	hFile;
	TCHAR	pszSlack[4096];
	TCHAR	szSlackFileName[MAX_PATH];
	TCHAR	szSaveFileName[MAX_PATH] = EMPTY_STRING;
	DWORD	nBytesWritten;
	BOOL	bSaveNameRetrieved;
	int		iItem;
	int		nBytesRetrieved;

	bSaveNameRetrieved = GetFileNameFromUser(m_hContainer,
	szSaveFileName,m_CurrentDirectory);

	if(bSaveNameRetrieved)
	{
		iItem = ListView_GetNextItem(m_hActiveListView,-1,LVNI_FOCUSED);

		if(iItem != -1)
		{
			m_pActiveShellBrowser->QueryFullItemName(iItem,szSlackFileName);

			nBytesRetrieved = ReadFileSlack(szSlackFileName,pszSlack,SIZEOF_ARRAY(pszSlack));

			if(nBytesRetrieved != -1)
			{
				hFile = CreateFile(szSaveFileName,GENERIC_WRITE,0,NULL,
				OPEN_ALWAYS,0,NULL);

				if(hFile != INVALID_HANDLE_VALUE)
				{
					WriteFile(hFile,(LPVOID)pszSlack,nBytesRetrieved,&nBytesWritten,NULL);

					CloseHandle(hFile);
				}
			}
		}
	}
}

void Explorerplusplus::OnWildcardSelect(BOOL bSelect)
{
	CWildcardSelectDialog WilcardSelectDialog(m_hLanguageModule,
		IDD_WILDCARDSELECT,m_hContainer,bSelect,this);

	WilcardSelectDialog.ShowModalDialog();
}

BOOL Explorerplusplus::OnSize(int MainWindowWidth,int MainWindowHeight)
{
	RECT			rc;
	TCITEM			tcItem;
	UINT			uFlags;
	int				IndentBottom = 0;
	int				IndentTop = 0;
	int				IndentLeft = 0;
	int				iIndentRebar = 0;
	int				iHolderWidth;
	int				iHolderHeight;
	int				iHolderTop;
	int				iTabBackingWidth;
	int				iTabBackingLeft;
	int				nTabs;
	int				i = 0;

	if(m_hMainRebar)
	{
		GetWindowRect(m_hMainRebar,&rc);
		iIndentRebar += GetRectHeight(&rc);
	}

	if(m_bShowStatusBar)
	{
		GetWindowRect(m_hStatusBar,&rc);
		IndentBottom += GetRectHeight(&rc);
	}

	if(m_bShowDisplayWindow)
	{
		IndentBottom += m_DisplayWindowHeight;
	}

	if(m_bShowFolders)
	{
		GetClientRect(m_hHolder,&rc);
		IndentLeft = GetRectWidth(&rc);
	}

	IndentTop = iIndentRebar;

	if(m_bShowTabBar)
	{
		if(!m_bShowTabBarAtBottom)
		{
			IndentTop += TAB_WINDOW_HEIGHT;
		}
	}

	/* <---- Tab control + backing ----> */

	if(m_bExtendTabControl)
	{
		iTabBackingLeft = 0;
		iTabBackingWidth = MainWindowWidth;
	}
	else
	{
		iTabBackingLeft = IndentLeft;
		iTabBackingWidth = MainWindowWidth - IndentLeft;
	}

	uFlags = m_bShowTabBar?SWP_SHOWWINDOW:SWP_HIDEWINDOW;

	int iTabTop;

	if(!m_bShowTabBarAtBottom)
	{
		iTabTop = iIndentRebar;
	}
	else
	{
		iTabTop = MainWindowHeight - IndentBottom - TAB_WINDOW_HEIGHT;
	}

	/* If we're showing the tab bar at the bottom of the listview,
	the only thing that will change is the top coordinate. */
	SetWindowPos(m_hTabBacking,m_hDisplayWindow,iTabBackingLeft,
		iTabTop,iTabBackingWidth,
		TAB_WINDOW_HEIGHT,uFlags);

	SetWindowPos(m_hTabCtrl,NULL,0,0,iTabBackingWidth - 25,
		TAB_WINDOW_HEIGHT,SWP_SHOWWINDOW|SWP_NOZORDER);

	/* Tab close button. */
	SetWindowPos(m_hTabWindowToolbar,NULL,iTabBackingWidth + TAB_TOOLBAR_X_OFFSET,
	TAB_TOOLBAR_Y_OFFSET,TAB_TOOLBAR_WIDTH,TAB_TOOLBAR_HEIGHT,SWP_SHOWWINDOW|SWP_NOZORDER);

	if(m_bExtendTabControl &&
		!m_bShowTabBarAtBottom)
	{
		iHolderTop = IndentTop;
	}
	else
	{
		iHolderTop = iIndentRebar;
	}

	/* <---- Holder window + child windows ----> */

	if(m_bExtendTabControl &&
		m_bShowTabBarAtBottom &&
		m_bShowTabBar)
	{
		iHolderHeight = MainWindowHeight - IndentBottom - iHolderTop - TAB_WINDOW_HEIGHT;
	}
	else
	{
		iHolderHeight = MainWindowHeight - IndentBottom - iHolderTop;
	}

	iHolderWidth = m_TreeViewWidth;

	SetWindowPos(m_hHolder,NULL,0,iHolderTop,
		iHolderWidth,iHolderHeight,SWP_NOZORDER);

	/* The treeview is only slightly smaller than the holder
	window, in both the x and y-directions. */
	SetWindowPos(m_hTreeView,NULL,TREEVIEW_X_CLEARANCE,TREEVIEW_Y_CLEARANCE,
		iHolderWidth - TREEVIEW_HOLDER_CLEARANCE - TREEVIEW_X_CLEARANCE,
		iHolderHeight - TREEVIEW_Y_CLEARANCE,SWP_NOZORDER);

	SetWindowPos(m_hFoldersToolbar,NULL,
		iHolderWidth + FOLDERS_TOOLBAR_X_OFFSET,FOLDERS_TOOLBAR_Y_OFFSET,
		FOLDERS_TOOLBAR_WIDTH,FOLDERS_TOOLBAR_HEIGHT,SWP_SHOWWINDOW|SWP_NOZORDER);


	/* <---- Display window ----> */

	SetWindowPos(m_hDisplayWindow,NULL,0,MainWindowHeight - IndentBottom,
		MainWindowWidth,m_DisplayWindowHeight,SWP_SHOWWINDOW|SWP_NOZORDER);


	/* <---- ALL listview windows ----> */

	nTabs = TabCtrl_GetItemCount(m_hTabCtrl);

	for(i = 0;i < nTabs;i++)
	{
		tcItem.mask = TCIF_PARAM;
		TabCtrl_GetItem(m_hTabCtrl,i,&tcItem);

		uFlags = SWP_NOZORDER;

		if((int)tcItem.lParam == m_iObjectIndex)
			uFlags |= SWP_SHOWWINDOW;

		if(!m_bShowTabBarAtBottom)
		{
			SetWindowPos(m_hListView[(int)tcItem.lParam],NULL,IndentLeft,IndentTop,
				MainWindowWidth - IndentLeft,MainWindowHeight - IndentBottom - IndentTop,
				uFlags);
		}
		else
		{
			if(m_bShowTabBar)
			{
				SetWindowPos(m_hListView[(int)tcItem.lParam],NULL,IndentLeft,IndentTop,
					MainWindowWidth - IndentLeft,MainWindowHeight - IndentBottom - IndentTop - TAB_WINDOW_HEIGHT,
					uFlags);
			}
			else
			{
				SetWindowPos(m_hListView[(int)tcItem.lParam],NULL,IndentLeft,IndentTop,
					MainWindowWidth - IndentLeft,MainWindowHeight - IndentBottom - IndentTop,
					uFlags);
			}
		}
	}


	/* <---- Status bar ----> */

	ResizeStatusBar(m_hStatusBar,MainWindowWidth,MainWindowHeight);
	SetStatusBarParts(MainWindowWidth);


	/* <---- Main rebar + child windows ----> */

	/* Ensure that the main rebar keeps its width in line with the main
	window (its height will not change). */
	MoveWindow(m_hMainRebar,0,0,MainWindowWidth,0,FALSE);

	SetFocus(m_hLastActiveWindow);

	return TRUE;
}

int Explorerplusplus::OnDestroy(void)
{
	if(m_pClipboardDataObject != NULL)
	{
		if(OleIsCurrentClipboard(m_pClipboardDataObject) == S_OK)
		{
			/* Ensure that any data that was copied to the clipboard
			remains there after we exit. */
			OleFlushClipboard();
		}
	}

	QueueUserAPC(QuitIconAPC,m_hIconThread,NULL);

	ImageList_Destroy(m_himlToolbarSmall);
	ImageList_Destroy(m_himlToolbarLarge);

	delete m_pStatusBar;

	ChangeClipboardChain(m_hContainer,m_hNextClipboardViewer);
	PostQuitMessage(0);

	return 0;
}

int Explorerplusplus::OnClose(void)
{
	if(m_bConfirmCloseTabs && (TabCtrl_GetItemCount(m_hTabCtrl) > 1))
	{
		TCHAR szTemp[128];
		LoadString(m_hLanguageModule,IDS_GENERAL_CLOSE_ALL_TABS,szTemp,SIZEOF_ARRAY(szTemp));
		int response = MessageBox(m_hContainer,szTemp,NExplorerplusplus::WINDOW_NAME,MB_ICONINFORMATION|MB_YESNO);

		/* If the user clicked no, return without
		closing. */
		if(response == IDNO)
			return 1;
	}

	m_iLastSelectedTab = m_iTabSelectedItem;

	m_bShowTaskbarThumbnails = m_bShowTaskbarThumbnailsProvisional;

	SaveAllSettings();

	RevokeDragDrop(m_hTabCtrl);

	DestroyWindow(m_hContainer);

	return 0;
}

void Explorerplusplus::OnDirChanged(int iTabId)
{
	m_pActiveShellBrowser->QueryCurrentDirectory(SIZEOF_ARRAY(m_CurrentDirectory),
		m_CurrentDirectory);
	SetCurrentDirectory(m_CurrentDirectory);

	HandleDirectoryMonitoring(iTabId);

	UpdateArrangeMenuItems();

	m_nSelected = 0;

	/* Set the focus back to the first item. */
	ListView_SetItemState(m_hActiveListView,0,LVIS_FOCUSED,LVIS_FOCUSED);

	UpdateWindowStates();

	InvalidateTaskbarThumbnailBitmap(iTabId);

	SetTabIcon();
}

void Explorerplusplus::OnResolveLink(void)
{
	TCHAR	ShortcutFileName[MAX_PATH];
	TCHAR	szFullFileName[MAX_PATH];
	TCHAR	szPath[MAX_PATH];
	HRESULT	hr;
	int		iItem;

	iItem = ListView_GetNextItem(m_hActiveListView,-1,LVNI_FOCUSED);

	if(iItem != -1)
	{
		m_pActiveShellBrowser->QueryFullItemName(iItem,ShortcutFileName);

		hr = NFileOperations::ResolveLink(m_hContainer,0,ShortcutFileName,szFullFileName,SIZEOF_ARRAY(szFullFileName));

		if(hr == S_OK)
		{
			/* Strip the filename, just leaving the path component. */
			StringCchCopy(szPath,SIZEOF_ARRAY(szPath),szFullFileName);
			PathRemoveFileSpec(szPath);

			hr = BrowseFolder(szPath,SBSP_ABSOLUTE,TRUE,TRUE,FALSE);

			if(SUCCEEDED(hr))
			{
				/* Strip off the path, and select the shortcut target
				in the listview. */
				PathStripPath(szFullFileName);
				m_pActiveShellBrowser->SelectFiles(szFullFileName);

				SetFocus(m_hActiveListView);
			}
		}
	}
}

void Explorerplusplus::OnSaveDirectoryListing(void)
{
	TCHAR FileName[MAX_PATH];
	LoadString(m_hLanguageModule,IDS_GENERAL_DIRECTORY_LISTING_FILENAME,FileName,SIZEOF_ARRAY(FileName));
	StringCchCat(FileName,SIZEOF_ARRAY(FileName),_T(".txt"));
	BOOL bSaveNameRetrieved = GetFileNameFromUser(m_hContainer,FileName,m_CurrentDirectory);

	if(bSaveNameRetrieved)
	{
		NFileOperations::SaveDirectoryListing(m_CurrentDirectory,FileName);
	}
}

void Explorerplusplus::OnTabCtrlGetDispInfo(LPARAM lParam)
{
	HWND			ToolTipControl;
	LPNMTTDISPINFO	lpnmtdi;
	NMHDR			*nmhdr = NULL;
	static TCHAR	szTabToolTip[512];
	TCITEM			tcItem;

	lpnmtdi = (LPNMTTDISPINFO)lParam;
	nmhdr = &lpnmtdi->hdr;

	ToolTipControl = (HWND)SendMessage(m_hTabCtrl,TCM_GETTOOLTIPS,0,0);

	if(nmhdr->hwndFrom == ToolTipControl)
	{
		tcItem.mask = TCIF_PARAM;
		TabCtrl_GetItem(m_hTabCtrl,nmhdr->idFrom,&tcItem);

		m_pShellBrowser[(int)tcItem.lParam]->QueryCurrentDirectory(SIZEOF_ARRAY(szTabToolTip),
			szTabToolTip);
		lpnmtdi->lpszText = szTabToolTip;
	}
}

void Explorerplusplus::OnSetFocus(void)
{
	SetFocus(m_hLastActiveWindow);
}

/*
 * Called when the contents of the clipboard change.
 * All cut items are deghosted, and the 'Paste' button
 * is enabled/disabled.
 */
void Explorerplusplus::OnDrawClipboard(void)
{
	if(m_pClipboardDataObject != NULL)
	{
		if(OleIsCurrentClipboard(m_pClipboardDataObject) == S_FALSE)
		{
			/* Deghost all items that have been 'cut'. */
			for each(auto strFile in m_CutFileNameList)
			{
				/* Only deghost the items if the tab they
				are/were in still exists. */
				if(CheckTabIdStatus(m_iCutTabInternal))
				{
					int iItem = m_pShellBrowser[m_iCutTabInternal]->LocateFileItemIndex(strFile.c_str());

					/* It is possible that the ghosted file
					does NOT exist within the current folder.
					This is the case when (for example), a file
					is cut, and the folder is changed, in which
					case the item is no longer available. */
					if(iItem != -1)
						m_pShellBrowser[m_iCutTabInternal]->DeghostItem(iItem);
				}
			}

			m_CutFileNameList.clear();

			/* Deghost any cut treeview items. */
			if(m_hCutTreeViewItem != NULL)
			{
				TVITEM tvItem;

				tvItem.mask			= TVIF_HANDLE|TVIF_STATE;
				tvItem.hItem		= m_hCutTreeViewItem;
				tvItem.state		= 0;
				tvItem.stateMask	= TVIS_CUT;
				TreeView_SetItem(m_hTreeView,&tvItem);

				m_hCutTreeViewItem = NULL;
			}

			m_pClipboardDataObject->Release();
			m_pClipboardDataObject = NULL;
		}
	}

	SendMessage(m_hMainToolbar,TB_ENABLEBUTTON,(WPARAM)TOOLBAR_PASTE,
		!m_pActiveShellBrowser->InVirtualFolder() && IsClipboardFormatAvailable(CF_HDROP));

	/* Forward the message to the next window in the chain. */
	SendMessage(m_hNextClipboardViewer,WM_DRAWCLIPBOARD,0,0);
}

/*
 * Called when the clipboard chain is changed (i.e. a window
 * is added/removed).
 */
void Explorerplusplus::OnChangeCBChain(WPARAM wParam,LPARAM lParam)
{
	if((HWND)wParam == m_hNextClipboardViewer)
		m_hNextClipboardViewer = (HWND)lParam;
	else if(m_hNextClipboardViewer != NULL)
		SendMessage(m_hNextClipboardViewer,WM_CHANGECBCHAIN,wParam,lParam);
}

void Explorerplusplus::HandleDirectoryMonitoring(int iTabId)
{
	DirectoryAltered_t	*pDirectoryAltered = NULL;
	TCHAR				szDirectoryToWatch[MAX_PATH];
	TCHAR				szRecycleBin[MAX_PATH];
	int					iDirMonitorId;

	iDirMonitorId		= m_pShellBrowser[iTabId]->GetDirMonitorId();
			
	/* Stop monitoring the directory that was browsed from. */
	m_pDirMon->StopDirectoryMonitor(iDirMonitorId);

	m_pShellBrowser[iTabId]->QueryCurrentDirectory(SIZEOF_ARRAY(szDirectoryToWatch),
		szDirectoryToWatch);

	GetVirtualFolderParsingPath(CSIDL_BITBUCKET,szRecycleBin);

	/* Don't watch virtual folders (the 'recycle bin' may be an
	exception to this). */
	if(m_pShellBrowser[iTabId]->InVirtualFolder())
	{
		iDirMonitorId = -1;
	}
	else
	{
		pDirectoryAltered = (DirectoryAltered_t *)malloc(sizeof(DirectoryAltered_t));

		pDirectoryAltered->iIndex		= iTabId;
		pDirectoryAltered->iFolderIndex	= m_pShellBrowser[iTabId]->GetFolderIndex();
		pDirectoryAltered->pData		= this;

		/* Start monitoring the directory that was opened. */
		pantheios::log(pantheios::debug,_T("Starting directory monitoring for \""),szDirectoryToWatch,_T("\""));
		iDirMonitorId = m_pDirMon->WatchDirectory(szDirectoryToWatch,FILE_NOTIFY_CHANGE_FILE_NAME|
			FILE_NOTIFY_CHANGE_SIZE|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_ATTRIBUTES|
			FILE_NOTIFY_CHANGE_LAST_WRITE|FILE_NOTIFY_CHANGE_LAST_ACCESS|FILE_NOTIFY_CHANGE_CREATION|
			FILE_NOTIFY_CHANGE_SECURITY,DirectoryAlteredCallback,FALSE,(void *)pDirectoryAltered);
	}

	m_pShellBrowser[iTabId]->SetDirMonitorId(iDirMonitorId);
}

void Explorerplusplus::OnTbnDropDown(LPARAM lParam)
{
	NMTOOLBAR		*nmTB = NULL;
	LPITEMIDLIST	pidl = NULL;
	POINT			ptOrigin;
	RECT			rc;
	HRESULT			hr;

	nmTB = (NMTOOLBAR *)lParam;

	GetWindowRect(m_hMainToolbar,&rc);

	ptOrigin.x = rc.left;
	ptOrigin.y = rc.bottom - 4;

	if(nmTB->iItem == TOOLBAR_BACK)
	{
		hr = m_pActiveShellBrowser->CreateHistoryPopup(m_hContainer,&pidl,&ptOrigin,TRUE);

		if(SUCCEEDED(hr))
		{
			BrowseFolder(pidl,SBSP_ABSOLUTE|SBSP_WRITENOHISTORY);

			CoTaskMemFree(pidl);
		}
	}
	else if(nmTB->iItem == TOOLBAR_FORWARD)
	{
		SendMessage(m_hMainToolbar,TB_GETRECT,(WPARAM)TOOLBAR_BACK,(LPARAM)&rc);

		ptOrigin.x += rc.right;

		hr = m_pActiveShellBrowser->CreateHistoryPopup(m_hContainer,&pidl,&ptOrigin,FALSE);

		if(SUCCEEDED(hr))
		{
			BrowseFolder(pidl,SBSP_ABSOLUTE|SBSP_WRITENOHISTORY);

			CoTaskMemFree(pidl);
		}
	}
	else if(nmTB->iItem == TOOLBAR_VIEWS)
	{
		ShowToolbarViewsDropdown();
	}
}

void Explorerplusplus::OnDisplayWindowResized(WPARAM wParam)
{
	RECT	rc;

	if((int)wParam >= MINIMUM_DISPLAYWINDOW_HEIGHT)
		m_DisplayWindowHeight = (int)wParam;

	GetClientRect(m_hContainer,&rc);

	SendMessage(m_hContainer,WM_SIZE,SIZE_RESTORED,(LPARAM)MAKELPARAM(rc.right,rc.bottom));
}

void Explorerplusplus::OnStartedBrowsing(int iTabId,TCHAR *szFolderPath)
{
	TCHAR	szLoadingText[512];

	if(iTabId == m_iObjectIndex)
	{
		TCHAR szTemp[64];
		LoadString(m_hLanguageModule,IDS_GENERAL_LOADING,szTemp,SIZEOF_ARRAY(szTemp));
		StringCchPrintf(szLoadingText,SIZEOF_ARRAY(szLoadingText),szTemp,szFolderPath);

		/* Browsing of a folder has started. Set the status bar text to indicate that
		the folder is been loaded. */
		SendMessage(m_hStatusBar,SB_SETTEXT,(WPARAM)0|0,(LPARAM)szLoadingText);

		/* Clear the text in all other parts of the status bar. */
		SendMessage(m_hStatusBar,SB_SETTEXT,(WPARAM)1|0,(LPARAM)EMPTY_STRING);
		SendMessage(m_hStatusBar,SB_SETTEXT,(WPARAM)2|0,(LPARAM)EMPTY_STRING);
	}
}

/*
 * Sizes all columns in the active listview
 * based on their text.
 */
void Explorerplusplus::OnAutoSizeColumns(void)
{
	size_t	nColumns;
	UINT	iCol = 0;

	nColumns = m_pActiveShellBrowser->QueryNumActiveColumns();

	for(iCol = 0;iCol < nColumns;iCol++)
	{
		ListView_SetColumnWidth(m_hActiveListView,iCol,LVSCW_AUTOSIZE);
	}
}

BOOL Explorerplusplus::OnMeasureItem(WPARAM wParam,LPARAM lParam)
{
	MEASUREITEMSTRUCT	*pMeasureItem = NULL;

	pMeasureItem = (MEASUREITEMSTRUCT *)lParam;

	if(pMeasureItem->CtlType == ODT_MENU)
	{
		return m_pCustomMenu->OnMeasureItem(wParam,lParam);
	}

	return TRUE;
}

BOOL Explorerplusplus::OnDrawItem(WPARAM wParam,LPARAM lParam)
{
	DRAWITEMSTRUCT	*pDrawItem = NULL;

	pDrawItem = (DRAWITEMSTRUCT *)lParam;

	if(pDrawItem->CtlType == ODT_MENU)
	{
		return m_pCustomMenu->OnDrawItem(wParam,lParam);
	}

	return TRUE;
}

/* Cycle through the current views. */
void Explorerplusplus::OnToolbarViews(void)
{
	CycleViewState(TRUE);
}

void Explorerplusplus::CycleViewState(BOOL bCycleForward)
{
	UINT	uViewMode;
	UINT	uNewViewMode;

	m_pShellBrowser[m_iObjectIndex]->GetCurrentViewMode(&uViewMode);

	std::list<ViewMode_t>::iterator itr;

	for(itr = m_ViewModes.begin();itr != m_ViewModes.end();itr++)
	{
		if(itr->uViewMode == uViewMode)
			break;
	}

	if(bCycleForward)
	{
		std::list<ViewMode_t>::iterator itrEnd;

		itrEnd = m_ViewModes.end();
		itrEnd--;

		if(itr == itrEnd)
			itr = m_ViewModes.begin();
		else
			itr++;
	}
	else
	{
		if(itr == m_ViewModes.begin())
		{
			itr = m_ViewModes.end();
			itr--;
		}
		else
		{
			itr--;
		}
	}

	uNewViewMode = itr->uViewMode;

	m_pShellBrowser[m_iObjectIndex]->SetCurrentViewMode(uNewViewMode);
}

void Explorerplusplus::ShowToolbarViewsDropdown(void)
{
	POINT	ptOrigin;
	RECT	rcButton;

	SendMessage(m_hMainToolbar,TB_GETRECT,(WPARAM)TOOLBAR_VIEWS,(LPARAM)&rcButton);

	ptOrigin.x	= rcButton.left;
	ptOrigin.y	= rcButton.bottom;

	ClientToScreen(m_hMainToolbar,&ptOrigin);

	CreateViewsMenu(&ptOrigin);
}

void Explorerplusplus::OnSortByAscending(BOOL bSortAscending)
{
	UINT	SortMode;

	if(bSortAscending != m_pActiveShellBrowser->GetSortAscending())
	{
		m_pActiveShellBrowser->SetSortAscending(bSortAscending);

		m_pShellBrowser[m_iObjectIndex]->GetSortMode(&SortMode);

		/* It is quicker to re-sort the folder than refresh it. */
		m_pShellBrowser[m_iObjectIndex]->SortFolder(SortMode);
	}
}

void Explorerplusplus::OnPreviousWindow(void)
{
	HWND	hFocus;

	hFocus = GetFocus();

	if(hFocus == m_hActiveListView)
	{
		if(m_bShowFolders)
		{
			SetFocus(m_hTreeView);
		}
		else
		{
			if(m_bShowAddressBar)
			{
				SetFocus(m_hAddressBar);
			}
		}
	}
	else if(hFocus == m_hTreeView)
	{
		if(m_bShowAddressBar)
		{
			SetFocus(m_hAddressBar);
		}
		else
		{
			/* Always shown. */
			SetFocus(m_hActiveListView);
		}
	}
	else if(hFocus == (HWND)SendMessage(m_hAddressBar,CBEM_GETEDITCONTROL,0,0))
	{
		/* Always shown. */
		SetFocus(m_hActiveListView);
	}
}

/*
 * Shifts focus to the next internal
 * window in the chain.
 */
void Explorerplusplus::OnNextWindow(void)
{
	HWND	hFocus;

	if(m_bListViewRenaming)
	{
		SendMessage(ListView_GetEditControl(m_hActiveListView),
			WM_APP_KEYDOWN,VK_TAB,0);
	}
	else
	{
		hFocus = GetFocus();

		/* Check if the next target window is visible.
		If it is, select it, else select the next
		window in the chain. */
		if(hFocus == m_hActiveListView)
		{
			if(m_bShowAddressBar)
			{
				SetFocus(m_hAddressBar);
			}
			else
			{
				if(m_bShowFolders)
				{
					SetFocus(m_hTreeView);
				}
			}
		}
		else if(hFocus == m_hTreeView)
		{
			/* Always shown. */
			SetFocus(m_hActiveListView);
		}
		else if(hFocus == (HWND)SendMessage(m_hAddressBar,CBEM_GETEDITCONTROL,0,0))
		{
			if(m_bShowFolders)
			{
				SetFocus(m_hTreeView);
			}
			else
			{
				SetFocus(m_hActiveListView);
			}
		}
	}
}

void Explorerplusplus::SetGoMenuName(HMENU hMenu,UINT uMenuID,UINT csidl)
{
	MENUITEMINFO	mii;
	LPITEMIDLIST	pidl = NULL;
	TCHAR			szFolderName[MAX_PATH];
	HRESULT			hr;

	hr = SHGetFolderLocation(NULL,csidl,NULL,0,&pidl);

	/* Don't use SUCCEEDED(hr). */
	if(hr == S_OK)
	{
		GetDisplayName(pidl,szFolderName,SHGDN_INFOLDER);

		mii.cbSize		= sizeof(mii);
		mii.fMask		= MIIM_STRING;
		mii.dwTypeData	= szFolderName;
		SetMenuItemInfo(hMenu,uMenuID,FALSE,&mii);

		CoTaskMemFree(pidl);
	}
	else
	{
		mii.cbSize		= sizeof(mii);
		mii.fMask		= MIIM_DATA;
		GetMenuItemInfo(hMenu,uMenuID,FALSE,&mii);

		/* Free the custom menu information. */
		free((CustomMenuInfo_t *)mii.dwItemData);

		/* Now, delete the menu .*/
		DeleteMenu(hMenu,uMenuID,MF_BYCOMMAND);
	}
}

/*
Browses to the specified folder within the _current_
tab. Also performs path expansion, meaning paths with
embedded environment variables will be handled automatically.

NOTE: All user-facing functions MUST send their paths
through here, rather than converting them to an idl
themselves (so that path expansion and any other required
processing can occur here).

The ONLY times an idl should be sent are:
 - When loading directories on startup
 - When navigating to a folder on the 'Go' menu
*/
HRESULT Explorerplusplus::BrowseFolder(const TCHAR *szPath,UINT wFlags)
{
	return BrowseFolder(szPath,wFlags,FALSE,FALSE,FALSE);
}

HRESULT Explorerplusplus::BrowseFolder(const TCHAR *szPath,UINT wFlags,
BOOL bOpenInNewTab,BOOL bSwitchToNewTab,BOOL bOpenInNewWindow)
{
	LPITEMIDLIST	pidl = NULL;
	HRESULT			hr = S_FALSE;

	/* Doesn't matter if we can't get the pidl here,
	as some paths will be relative, or will be filled
	by the shellbrowser (e.g. when browsing back/forward). */
	hr = GetIdlFromParsingName(szPath,&pidl);

	BrowseFolder(pidl,wFlags,bOpenInNewTab,bSwitchToNewTab,bOpenInNewWindow);

	if(SUCCEEDED(hr))
	{
		CoTaskMemFree(pidl);
	}

	return hr;
}

/* ALL calls to browse a folder in the current tab MUST
pass through this function. This ensures that tabs that
have their addresses locked will not change directory. */
HRESULT Explorerplusplus::BrowseFolder(LPITEMIDLIST pidlDirectory,UINT wFlags)
{
	HRESULT hr = E_FAIL;
	int iTabObjectIndex = -1;

	if(!m_TabInfo[m_iObjectIndex].bAddressLocked)
	{
		hr = m_pActiveShellBrowser->BrowseFolder(pidlDirectory,wFlags);

		if(SUCCEEDED(hr))
		{
			PlayNavigationSound();
		}

		iTabObjectIndex = m_iObjectIndex;
	}
	else
	{
		hr = CreateNewTab(pidlDirectory,NULL,NULL,TRUE,&iTabObjectIndex);
	}

	if(SUCCEEDED(hr))
	{
		OnDirChanged(iTabObjectIndex);
	}

	return hr;
}

HRESULT Explorerplusplus::BrowseFolder(LPITEMIDLIST pidlDirectory,UINT wFlags,
BOOL bOpenInNewTab,BOOL bSwitchToNewTab,BOOL bOpenInNewWindow)
{
	HRESULT hr = E_FAIL;
	int iTabObjectIndex = -1;

	if(bOpenInNewWindow)
	{
		/* Create a new instance of this program, with the
		specified path as an argument. */
		SHELLEXECUTEINFO sei;
		TCHAR szCurrentProcess[MAX_PATH];
		TCHAR szPath[MAX_PATH];
		TCHAR szParameters[512];

		GetCurrentProcessImageName(szCurrentProcess,SIZEOF_ARRAY(szCurrentProcess));

		GetDisplayName(pidlDirectory,szPath,SHGDN_FORPARSING);
		StringCchPrintf(szParameters,SIZEOF_ARRAY(szParameters),_T("\"%s\""),szPath);

		sei.cbSize			= sizeof(sei);
		sei.fMask			= SEE_MASK_DEFAULT;
		sei.lpVerb			= _T("open");
		sei.lpFile			= szCurrentProcess;
		sei.lpParameters	= szParameters;
		sei.lpDirectory		= NULL;
		sei.nShow			= SW_SHOW;
		ShellExecuteEx(&sei);
	}
	else
	{
		if(!bOpenInNewTab && !m_TabInfo[m_iObjectIndex].bAddressLocked)
		{
			hr = m_pActiveShellBrowser->BrowseFolder(pidlDirectory,wFlags);

			if(SUCCEEDED(hr))
			{
				PlayNavigationSound();
			}

			iTabObjectIndex = m_iObjectIndex;
		}
		else
		{
			if(m_TabInfo[m_iObjectIndex].bAddressLocked)
				hr = CreateNewTab(pidlDirectory,NULL,NULL,TRUE,&iTabObjectIndex);
			else
				hr = CreateNewTab(pidlDirectory,NULL,NULL,bSwitchToNewTab,&iTabObjectIndex);
		}

		if(SUCCEEDED(hr))
			OnDirChanged(iTabObjectIndex);
	}

	return hr;
}

void Explorerplusplus::SetAllDefaultColumns(void)
{
	/* Set the default columns as the initial set. When the
	settings are loaded, these columns may be overwritten. */
	SetDefaultRealFolderColumns(&m_RealFolderColumnList);
	SetDefaultControlPanelColumns(&m_ControlPanelColumnList);
	SetDefaultMyComputerColumns(&m_MyComputerColumnList);
	SetDefaultRecycleBinColumns(&m_RecycleBinColumnList);
	SetDefaultPrintersColumns(&m_PrintersColumnList);
	SetDefaultNetworkConnectionsColumns(&m_NetworkConnectionsColumnList);
	SetDefaultMyNetworkPlacesColumns(&m_MyNetworkPlacesColumnList);
}

void Explorerplusplus::SetDefaultRealFolderColumns(std::list<Column_t> *pColumns)
{
	Column_t Column;
	int i = 0;

	pColumns->clear();

	for(i = 0;i <SIZEOF_ARRAY(g_RealFolderColumns);i++)
	{
		Column.id		= g_RealFolderColumns[i].id;
		Column.bChecked	= g_RealFolderColumns[i].bChecked;
		Column.iWidth	= g_RealFolderColumns[i].iWidth;
		pColumns->push_back(Column);
	}
}

void Explorerplusplus::SetDefaultControlPanelColumns(std::list<Column_t> *pColumns)
{
	Column_t Column;
	int i = 0;

	pColumns->clear();

	for(i = 0;i <SIZEOF_ARRAY(g_ControlPanelColumns);i++)
	{
		Column.id		= g_ControlPanelColumns[i].id;
		Column.bChecked	= g_ControlPanelColumns[i].bChecked;
		Column.iWidth	= g_RealFolderColumns[i].iWidth;
		pColumns->push_back(Column);
	}
}

void Explorerplusplus::SetDefaultMyComputerColumns(std::list<Column_t> *pColumns)
{
	Column_t Column;
	int i = 0;

	pColumns->clear();

	for(i = 0;i <SIZEOF_ARRAY(g_MyComputerColumns);i++)
	{
		Column.id		= g_MyComputerColumns[i].id;
		Column.bChecked	= g_MyComputerColumns[i].bChecked;
		Column.iWidth	= g_RealFolderColumns[i].iWidth;
		pColumns->push_back(Column);
	}
}

void Explorerplusplus::SetDefaultRecycleBinColumns(std::list<Column_t> *pColumns)
{
	Column_t Column;
	int i = 0;

	pColumns->clear();

	for(i = 0;i <SIZEOF_ARRAY(g_RecycleBinColumns);i++)
	{
		Column.id		= g_RecycleBinColumns[i].id;
		Column.bChecked	= g_RecycleBinColumns[i].bChecked;
		Column.iWidth	= g_RealFolderColumns[i].iWidth;
		pColumns->push_back(Column);
	}
}

void Explorerplusplus::SetDefaultPrintersColumns(std::list<Column_t> *pColumns)
{
	Column_t Column;
	int i = 0;

	pColumns->clear();

	for(i = 0;i <SIZEOF_ARRAY(g_PrintersColumns);i++)
	{
		Column.id		= g_PrintersColumns[i].id;
		Column.bChecked	= g_PrintersColumns[i].bChecked;
		Column.iWidth	= g_RealFolderColumns[i].iWidth;
		pColumns->push_back(Column);
	}
}

void Explorerplusplus::SetDefaultNetworkConnectionsColumns(std::list<Column_t> *pColumns)
{
	Column_t Column;
	int i = 0;

	pColumns->clear();

	for(i = 0;i <SIZEOF_ARRAY(g_NetworkConnectionsColumns);i++)
	{
		Column.id		= g_NetworkConnectionsColumns[i].id;
		Column.bChecked	= g_NetworkConnectionsColumns[i].bChecked;
		Column.iWidth	= g_RealFolderColumns[i].iWidth;
		pColumns->push_back(Column);
	}
}

void Explorerplusplus::SetDefaultMyNetworkPlacesColumns(std::list<Column_t> *pColumns)
{
	Column_t Column;
	int i = 0;

	pColumns->clear();

	for(i = 0;i <SIZEOF_ARRAY(g_MyNetworkPlacesColumns);i++)
	{
		Column.id		= g_MyNetworkPlacesColumns[i].id;
		Column.bChecked	= g_MyNetworkPlacesColumns[i].bChecked;
		Column.iWidth	= g_RealFolderColumns[i].iWidth;
		pColumns->push_back(Column);
	}
}

void Explorerplusplus::OnLockToolbars(void)
{
	REBARBANDINFO	rbbi;
	UINT			nBands;
	UINT			i = 0;

	m_bLockToolbars = !m_bLockToolbars;

	nBands = (UINT)SendMessage(m_hMainRebar,RB_GETBANDCOUNT,0,0);

	for(i = 0;i < nBands;i++)
	{
		/* First, retrieve the current style for this band. */
		rbbi.cbSize	= sizeof(REBARBANDINFO);
		rbbi.fMask	= RBBIM_STYLE;
		SendMessage(m_hMainRebar,RB_GETBANDINFO,i,(LPARAM)&rbbi);

		/* Add the gripper style. */
		AddGripperStyle(&rbbi.fStyle,!m_bLockToolbars);

		/* Now, set the new style. */
		SendMessage(m_hMainRebar,RB_SETBANDINFO,i,(LPARAM)&rbbi);
	}

	/* If the rebar is locked, prevent items from
	been rearranged. */
	AddWindowStyle(m_hMainRebar,RBS_FIXEDORDER,m_bLockToolbars);
}

void Explorerplusplus::OnShellNewItemCreated(LPARAM lParam)
{
	HWND	hEdit;
	int		iRenamedItem;

	iRenamedItem = (int)lParam;

	if(iRenamedItem != -1)
	{
		/* Start editing the label for this item. */
		hEdit = ListView_EditLabel(m_hActiveListView,iRenamedItem);
	}
}

void Explorerplusplus::OnCreateNewFolder(void)
{
	TCHAR			szNewFolderName[32768];
	LPITEMIDLIST	pidlItem = NULL;
	HRESULT			hr;

	hr = CreateNewFolder(m_CurrentDirectory,szNewFolderName,SIZEOF_ARRAY(szNewFolderName));

	if(SUCCEEDED(hr))
	{
		m_bCountingDown = TRUE;
		NListView::ListView_SelectAllItems(m_hActiveListView,FALSE);
		SetFocus(m_hActiveListView);

		GetIdlFromParsingName(szNewFolderName,&pidlItem);
		m_pActiveShellBrowser->QueueRename((LPITEMIDLIST)pidlItem);

		CoTaskMemFree(pidlItem);
	}
	else
	{
		TCHAR	szTemp[512];

		LoadString(m_hLanguageModule,IDS_NEWFOLDERERROR,szTemp,
		SIZEOF_ARRAY(szTemp));

		MessageBox(m_hContainer,szTemp,NExplorerplusplus::WINDOW_NAME,
			MB_ICONERROR|MB_OK);
	}
}

void Explorerplusplus::OnAppCommand(WPARAM wParam,LPARAM lParam)
{
	UINT	cmd;

	cmd = GET_APPCOMMAND_LPARAM(lParam);

	switch(cmd)
	{
	case APPCOMMAND_BROWSER_BACKWARD:
		/* This will cancel any menu that may be shown
		at the moment. */
		SendMessage(m_hContainer,WM_CANCELMODE,0,0);

		OnBrowseBack();
		break;

	case APPCOMMAND_BROWSER_FORWARD:
		SendMessage(m_hContainer,WM_CANCELMODE,0,0);
		OnBrowseForward();
		break;

	case APPCOMMAND_BROWSER_FAVORITES:
		break;

	case APPCOMMAND_BROWSER_REFRESH:
		SendMessage(m_hContainer,WM_CANCELMODE,0,0);
		OnRefresh();
		break;

	case APPCOMMAND_BROWSER_SEARCH:
		break;

	case APPCOMMAND_CLOSE:
		SendMessage(m_hContainer,WM_CANCELMODE,0,0);
		OnCloseTab();
		break;

	case APPCOMMAND_COPY:
		OnCopy(TRUE);
		break;

	case APPCOMMAND_CUT:
		OnCopy(FALSE);
		break;

	case APPCOMMAND_HELP:
		break;

	case APPCOMMAND_NEW:
		break;

	case APPCOMMAND_PASTE:
		OnPaste();
		break;

	case APPCOMMAND_UNDO:
		m_FileActionHandler.Undo();
		break;

	case APPCOMMAND_REDO:
		break;
	}
}

void Explorerplusplus::OnBrowseBack(void)
{
	BrowseFolder(EMPTY_STRING,
		SBSP_NAVIGATEBACK|SBSP_SAMEBROWSER);
}

void Explorerplusplus::OnBrowseForward(void)
{
	BrowseFolder(EMPTY_STRING,
		SBSP_NAVIGATEFORWARD|SBSP_SAMEBROWSER);
}

void Explorerplusplus::OnRefresh(void)
{
	RefreshTab(m_iObjectIndex);
}

void Explorerplusplus::CopyColumnInfoToClipboard(void)
{
	std::list<Column_t> Columns;
	m_pActiveShellBrowser->ExportCurrentColumns(&Columns);

	std::wstring strColumnInfo;
	int nActiveColumns = 0;

	for each(auto Column in Columns)
	{
		if(Column.bChecked)
		{
			TCHAR szText[64];
			LoadString(m_hLanguageModule,LookupColumnNameStringIndex(Column.id),szText,SIZEOF_ARRAY(szText));

			strColumnInfo += std::wstring(szText) + _T("\t");

			nActiveColumns++;
		}
	}

	/* Remove the trailing tab. */
	strColumnInfo = strColumnInfo.substr(0,strColumnInfo.size() - 1);

	strColumnInfo += _T("\r\n");

	int iItem = -1;

	while((iItem = ListView_GetNextItem(m_hActiveListView,iItem,LVNI_SELECTED)) != -1)
	{
		for(int i = 0;i < nActiveColumns;i++)
		{
			TCHAR szText[64];
			ListView_GetItemText(m_hActiveListView,iItem,i,szText,
				SIZEOF_ARRAY(szText));

			strColumnInfo += std::wstring(szText) + _T("\t");
		}

		strColumnInfo = strColumnInfo.substr(0,strColumnInfo.size() - 1);

		strColumnInfo += _T("\r\n");
	}

	/* Remove the trailing newline. */
	strColumnInfo = strColumnInfo.substr(0,strColumnInfo.size() - 2);

	CopyTextToClipboard(strColumnInfo);
}

void Explorerplusplus::SetFilterStatus(void)
{
	m_pActiveShellBrowser->SetFilterStatus(!m_pActiveShellBrowser->GetFilterStatus());
}

void Explorerplusplus::OnDirectoryModified(int iTabId)
{
	/* This message is sent when one of the
	tab directories is modified.
	Two cases to handle:
	 1. Tab that sent the notification DOES NOT
	    have focus.
	 2. Tab that sent the notification DOES have
	    focus.

	Case 1 (Tab DOES NOT have focus):
	No updates will be applied. When the tab
	selection changes to the updated tab, the
	view will be synchronized anyhow (since all
	windows are updated when the tab selection
	changes).

	Case 2 (Tab DOES have focus):
	In this case, only the following updates
	need to be applied:
	 - Updated status bar text
	 - Handle file selection display (i.e. update
	   the display window)
	*/

	if(iTabId == m_iObjectIndex)
	{
		HandleStatusText();
		HandleFileSelectionDisplay();
	}
}

void Explorerplusplus::OnIdaRClick(void)
{
	/* Show the context menu (if any)
	for the window that currently has
	the focus.
	Note: The edit box within the address
	bar already handles the r-click menu
	key. */

	HWND hFocus;

	hFocus = GetFocus();

	if(hFocus == m_hActiveListView)
	{
		/* The behaviour of the listview is
		slightly different when compared to
		normal right-clicking.
		If any item(s) in the listview are
		selected when they key is pressed,
		the context menu for those items will
		be shown, rather than the background
		context menu.
		The context menu will be anchored to
		the item that currently has selection.
		If no item is selected, the background
		context menu will be shown (and anchored
		at the current mouse position). */
		POINT ptMenuOrigin = {0,0};

		/* If no items are selected, pass the current mouse
		position. If items are selected, take the one with
		focus, and pass its center point. */
		if(ListView_GetSelectedCount(m_hActiveListView) == 0)
		{
			GetCursorPos(&ptMenuOrigin);
		}
		else
		{
			HIMAGELIST himl;
			POINT ptItem;
			UINT uViewMode;
			int iItem;
			int cx;
			int cy;

			iItem = ListView_GetNextItem(m_hActiveListView,-1,LVNI_FOCUSED);

			if(iItem != -1)
			{
				ListView_GetItemPosition(m_hActiveListView,iItem,&ptItem);

				ClientToScreen(m_hActiveListView,&ptItem);

				m_pShellBrowser[m_iObjectIndex]->GetCurrentViewMode(&uViewMode);

				if(uViewMode == VM_SMALLICONS || uViewMode == VM_LIST ||
					uViewMode == VM_DETAILS)
					himl = ListView_GetImageList(m_hActiveListView,LVSIL_SMALL);
				else
					himl = ListView_GetImageList(m_hActiveListView,LVSIL_NORMAL);

				ImageList_GetIconSize(himl,&cx,&cy);

				/* DON'T free the image list. */

				/* The origin of the menu will be fixed at the centre point
				of the items icon. */
				ptMenuOrigin.x = ptItem.x + cx / 2;
				ptMenuOrigin.y = ptItem.y + cy / 2;
			}
		}

		OnListViewRClick(m_hActiveListView,&ptMenuOrigin);
	}
	else if(hFocus == m_hTreeView)
	{
		HTREEITEM hSelection;
		RECT rcItem;
		POINT ptOrigin;

		hSelection = TreeView_GetSelection(m_hTreeView);

		TreeView_GetItemRect(m_hTreeView,hSelection,&rcItem,TRUE);

		ptOrigin.x = rcItem.left;
		ptOrigin.y = rcItem.top;

		ClientToScreen(m_hTreeView,&ptOrigin);

		ptOrigin.y += (rcItem.bottom - rcItem.top) / 2;

		if(hSelection != NULL)
		{
			OnTreeViewRightClick((WPARAM)hSelection,(LPARAM)&ptOrigin);
		}
	}
}

/* A file association has changed. Rather
than refreshing all tabs, just find all
icons again.

To refresh system image list:
1. Call FileIconInit(TRUE)
2. Change "Shell Icon Size" in "Control Panel\\Desktop\\WindowMetrics"
3. Call FileIconInit(FALSE)

Note that refreshing the system image list affects
the WHOLE PROGRAM. This means that the treeview
needs to have its icons refreshed as well.

References:
http://tech.groups.yahoo.com/group/wtl/message/13911
http://www.eggheadcafe.com/forumarchives/platformsdkshell/Nov2005/post24294253.asp
*/
void Explorerplusplus::OnAssocChanged(void)
{
	typedef BOOL (WINAPI *FII_PROC)(BOOL);
	FII_PROC FileIconInit;
	HKEY hKey;
	TCITEM tcItem;
	HMODULE hShell32;
	TCHAR szShellIconSize[32];
	TCHAR szTemp[32];
	DWORD dwShellIconSize;
	LONG res;
	int i = 0;
	int nTabs;
	int iIndex;

	hShell32 = LoadLibrary(_T("shell32.dll"));

	FileIconInit = (FII_PROC)GetProcAddress(hShell32,(LPCSTR)660);

	res = RegOpenKeyEx(HKEY_CURRENT_USER,
		_T("Control Panel\\Desktop\\WindowMetrics"),
		0,KEY_READ|KEY_WRITE,&hKey);

	if(res == ERROR_SUCCESS)
	{
		NRegistrySettings::ReadStringFromRegistry(hKey,_T("Shell Icon Size"),
			szShellIconSize,SIZEOF_ARRAY(szShellIconSize));

		dwShellIconSize = _wtoi(szShellIconSize);

		/* Increment the value by one, and save it back to the registry. */
		StringCchPrintf(szTemp,SIZEOF_ARRAY(szTemp),_T("%d"),dwShellIconSize + 1);
		NRegistrySettings::SaveStringToRegistry(hKey,_T("Shell Icon Size"),szTemp);

		if(FileIconInit != NULL)
			FileIconInit(TRUE);

		/* Now, set it back to the original value. */
		NRegistrySettings::SaveStringToRegistry(hKey,_T("Shell Icon Size"),szShellIconSize);

		if(FileIconInit != NULL)
			FileIconInit(FALSE);

		RegCloseKey(hKey);
	}

	/* DO NOT free shell32.dll. Doing so will release
	the image lists (among other things). */

	nTabs = TabCtrl_GetItemCount(m_hTabCtrl);

	/* When the system image list is refresh, ALL previous
	icons will be discarded. This means that SHGetFileInfo()
	needs to be called to get each files icon again. */

	/* Now, go through each tab, and refresh each icon. */
	for(i = 0;i < nTabs;i++)
	{
		tcItem.mask = TCIF_PARAM;
		TabCtrl_GetItem(m_hTabCtrl,i,&tcItem);

		iIndex = (int)tcItem.lParam;

		m_pShellBrowser[iIndex]->RefreshAllIcons();
	}

	/* Now, refresh the treeview. */
	m_pMyTreeView->RefreshAllIcons();

	/* Address bar. */
	HandleAddressBarText();
}

void Explorerplusplus::OnCloneWindow(void)
{
	TCHAR szExecutable[MAX_PATH];
	TCHAR szCurrentDirectory[MAX_PATH];
	TCHAR szQuotedCurrentDirectory[MAX_PATH];
	SHELLEXECUTEINFO sei;

	GetCurrentProcessImageName(szExecutable,
		SIZEOF_ARRAY(szExecutable));

	m_pActiveShellBrowser->QueryCurrentDirectory(SIZEOF_ARRAY(szCurrentDirectory),
		szCurrentDirectory);

	StringCchPrintf(szQuotedCurrentDirectory,
		SIZEOF_ARRAY(szQuotedCurrentDirectory),
		_T("\"%s\""),szCurrentDirectory);

	sei.cbSize			= sizeof(sei);
	sei.fMask			= 0;
	sei.lpVerb			= _T("open");
	sei.lpFile			= szExecutable;
	sei.lpParameters	= szQuotedCurrentDirectory;
	sei.lpDirectory		= NULL;
	sei.hwnd			= NULL;
	sei.nShow			= SW_SHOW;
	ShellExecuteEx(&sei);
}

void Explorerplusplus::ShowMainRebarBand(HWND hwnd,BOOL bShow)
{
	REBARBANDINFO rbi;
	LRESULT lResult;
	UINT nBands;
	UINT i = 0;

	nBands = (UINT)SendMessage(m_hMainRebar,RB_GETBANDCOUNT,0,0);

	for(i = 0;i < nBands;i++)
	{
		rbi.cbSize	= sizeof(rbi);
		rbi.fMask	= RBBIM_CHILD;
		lResult = SendMessage(m_hMainRebar,RB_GETBANDINFO,i,(LPARAM)&rbi);

		if(lResult)
		{
			if(hwnd == rbi.hwndChild)
			{
				SendMessage(m_hMainRebar,RB_SHOWBAND,i,bShow);
				break;
			}
		}
	}
}

void Explorerplusplus::OnNdwIconRClick(WPARAM wParam,LPARAM lParam)
{
	LPITEMIDLIST pidlDirectory	= NULL;
	POINT pt;

	pidlDirectory = m_pActiveShellBrowser->QueryCurrentDirectoryIdl();

	pt.x = GET_X_LPARAM(lParam);
	pt.y = GET_Y_LPARAM(lParam);
	ClientToScreen(m_hDisplayWindow,&pt);

	OnListViewRClick(m_hDisplayWindow,&pt);
}

void Explorerplusplus::OnNdwRClick(WPARAM wParam,LPARAM lParam)
{
	POINT pt;

	pt.x = GET_X_LPARAM(lParam);
	pt.y = GET_Y_LPARAM(lParam);

	ClientToScreen(m_hDisplayWindow,&pt);

	TrackPopupMenu(m_hDisplayWindowRightClickMenu,
		TPM_LEFTALIGN|TPM_RIGHTBUTTON|TPM_VERTICAL,
		pt.x,pt.y,0,m_hContainer,NULL);
}

LRESULT Explorerplusplus::OnCustomDraw(LPARAM lParam)
{
	NMLVCUSTOMDRAW *pnmlvcd = NULL;
	NMCUSTOMDRAW *pnmcd = NULL;

	pnmlvcd = (NMLVCUSTOMDRAW *)lParam;

	if(pnmlvcd->nmcd.hdr.hwndFrom == m_hActiveListView)
	{
		pnmcd = &pnmlvcd->nmcd;

		switch(pnmcd->dwDrawStage)
		{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			break;

		case CDDS_ITEMPREPAINT:
			{
				DWORD dwAttributes = m_pActiveShellBrowser->QueryFileAttributes(static_cast<int>(pnmcd->dwItemSpec));

				TCHAR szFileName[MAX_PATH];
				m_pActiveShellBrowser->QueryFullItemName(static_cast<int>(pnmcd->dwItemSpec),szFileName);
				PathStripPath(szFileName);

				/* Loop through each filter. Decide whether to change the font of the
				current item based on its filename and/or attributes. */
				for each(auto ColorRule in m_ColorRules)
				{
					BOOL bMatchFileName = FALSE;
					BOOL bMatchAttributes = FALSE;

					/* Only match against the filename if it's not empty. */
					if(ColorRule.strFilterPattern.size() > 0)
					{
						if(CheckWildcardMatch(ColorRule.strFilterPattern.c_str(),szFileName,!ColorRule.caseInsensitive) == 1)
						{
							bMatchFileName = TRUE;
						}
					}
					else
					{
						bMatchFileName = TRUE;
					}

					if(ColorRule.dwFilterAttributes != 0)
					{
						if(ColorRule.dwFilterAttributes & dwAttributes)
						{
							bMatchAttributes = TRUE;
						}
					}
					else
					{
						bMatchAttributes = TRUE;
					}

					if(bMatchFileName && bMatchAttributes)
					{
						pnmlvcd->clrText = ColorRule.rgbColour;
						return CDRF_NEWFONT;
					}
				}
			}
			break;
		}

		return CDRF_NOTIFYITEMDRAW;
	}

	return 0;
}

int Explorerplusplus::GetViewModeMenuId(UINT uViewMode)
{
	switch(uViewMode)
	{
		case VM_THUMBNAILS:
			return IDM_VIEW_THUMBNAILS;
			break;

		case VM_TILES:
			return IDM_VIEW_TILES;
			break;

		case VM_EXTRALARGEICONS:
			return IDM_VIEW_EXTRALARGEICONS;
			break;

		case VM_LARGEICONS:
			return IDM_VIEW_LARGEICONS;
			break;

		case VM_ICONS:
			return IDM_VIEW_ICONS;
			break;

		case VM_SMALLICONS:
			return IDM_VIEW_SMALLICONS;
			break;

		case VM_LIST:
			return IDM_VIEW_LIST;
			break;

		case VM_DETAILS:
			return IDM_VIEW_DETAILS;
			break;
	}

	return -1;
}

int Explorerplusplus::GetViewModeMenuStringId(UINT uViewMode)
{
	switch(uViewMode)
	{
		case VM_THUMBNAILS:
			return IDS_VIEW_THUMBNAILS;
			break;

		case VM_TILES:
			return IDS_VIEW_TILES;
			break;

		case VM_EXTRALARGEICONS:
			return IDS_VIEW_EXTRALARGEICONS;
			break;

		case VM_LARGEICONS:
			return IDS_VIEW_LARGEICONS;
			break;

		case VM_ICONS:
			if(m_dwMajorVersion >= WINDOWS_VISTA_SEVEN_MAJORVERSION)
				return IDS_VIEW_MEDIUMICONS;
			else if(m_dwMajorVersion >= WINDOWS_XP_MAJORVERSION)
				return IDS_VIEW_ICONS;
			break;

		case VM_SMALLICONS:
			return IDS_VIEW_SMALLICONS;
			break;

		case VM_LIST:
			return IDS_VIEW_LIST;
			break;

		case VM_DETAILS:
			return IDS_VIEW_DETAILS;
			break;
	}

	return -1;
}

void Explorerplusplus::OnSortBy(UINT uSortMode)
{
	UINT uCurrentSortMode;

	m_pShellBrowser[m_iObjectIndex]->GetSortMode(&uCurrentSortMode);

	if(!m_pShellBrowser[m_iObjectIndex]->IsGroupViewEnabled() &&
		uSortMode == uCurrentSortMode)
	{
		m_pActiveShellBrowser->ToggleSortAscending();
	}
	else if(m_pShellBrowser[m_iObjectIndex]->IsGroupViewEnabled())
	{
		m_pActiveShellBrowser->SetGrouping(FALSE);
	}

	m_pShellBrowser[m_iObjectIndex]->SortFolder(uSortMode);
}

void Explorerplusplus::OnGroupBy(UINT uSortMode)
{
	UINT uCurrentSortMode;

	m_pShellBrowser[m_iObjectIndex]->GetSortMode(&uCurrentSortMode);

	/* If group view is already enabled, and the current sort
	mode matches the supplied sort mode, toggle the ascending/
	descending flag. */
	if(m_pShellBrowser[m_iObjectIndex]->IsGroupViewEnabled() &&
		uSortMode == uCurrentSortMode)
	{
		m_pActiveShellBrowser->ToggleSortAscending();
	}
	else if(!m_pShellBrowser[m_iObjectIndex]->IsGroupViewEnabled())
	{
		m_pActiveShellBrowser->SetGroupingFlag(TRUE);
	}

	m_pShellBrowser[m_iObjectIndex]->SortFolder(uSortMode);
}

void Explorerplusplus::OnHome(void)
{
	HRESULT hr;

	hr = BrowseFolder(m_DefaultTabDirectory,SBSP_ABSOLUTE);

	if(FAILED(hr))
	{
		BrowseFolder(m_DefaultTabDirectoryStatic,SBSP_ABSOLUTE);
	}
}

void Explorerplusplus::OnNavigateUp(void)
{
	TCHAR szDirectory[MAX_PATH];
	m_pActiveShellBrowser->QueryCurrentDirectory(SIZEOF_ARRAY(szDirectory),
		szDirectory);
	PathStripPath(szDirectory);

	BrowseFolder(EMPTY_STRING,SBSP_PARENT|SBSP_SAMEBROWSER);

	m_pActiveShellBrowser->SelectFiles(szDirectory);
}

void Explorerplusplus::SaveAllSettings(void)
{
	ILoadSave *pLoadSave = NULL;

	if(m_bSavePreferencesToXMLFile)
		pLoadSave = new CLoadSaveXML(this,FALSE);
	else
		pLoadSave = new CLoadSaveRegistry(this);

	pLoadSave->SaveGenericSettings();
	pLoadSave->SaveTabs();
	pLoadSave->SaveDefaultColumns();
	pLoadSave->SaveBookmarks();
	pLoadSave->SaveApplicationToolbar();
	pLoadSave->SaveToolbarInformation();
	pLoadSave->SaveColorRules();
	pLoadSave->SaveDialogStates();

	delete pLoadSave;
}

/* Saves directory settings for a particular
tab. */
void Explorerplusplus::SaveDirectorySpecificSettings(int iTab)
{
	TCITEM tcItem;
	BOOL bRet;

	tcItem.mask = TCIF_PARAM;
	bRet = TabCtrl_GetItem(m_hTabCtrl,iTab,&tcItem);

	if(bRet)
	{
		int iIndexInternal = (int)tcItem.lParam;

		DirectorySettings_t ds;

		/* TODO: First check if there are already settings held for this
		tab. If there are, delete them first. */

		ds.pidlDirectory = m_pShellBrowser[iIndexInternal]->QueryCurrentDirectoryIdl();

		m_pShellBrowser[iIndexInternal]->GetSortMode(&ds.dsi.SortMode);
		m_pShellBrowser[iIndexInternal]->GetCurrentViewMode(&ds.dsi.ViewMode);

		ColumnExport_t ce;

		m_pShellBrowser[iIndexInternal]->ExportAllColumns(&ce);

		ds.dsi.ControlPanelColumnList		= ce.ControlPanelColumnList;
		ds.dsi.MyComputerColumnList			= ce.MyComputerColumnList;
		ds.dsi.MyNetworkPlacesColumnList	= ce.MyNetworkPlacesColumnList;
		ds.dsi.NetworkConnectionsColumnList	= ce.NetworkConnectionsColumnList;
		ds.dsi.PrintersColumnList			= ce.PrintersColumnList;
		ds.dsi.RealFolderColumnList			= ce.RealFolderColumnList;
		ds.dsi.RecycleBinColumnList			= ce.RecycleBinColumnList;

		m_DirectorySettingsList.push_back(ds);
	}
}

/* TODO: This needs to be moved into the actual shell browser. Can't change
settings until it's known that the folder has successfully changed. */
void Explorerplusplus::SetDirectorySpecificSettings(int iTab,LPITEMIDLIST pidlDirectory)
{
	if(m_DirectorySettingsList.size() > 0)
	{
		for each(auto ds in m_DirectorySettingsList)
		{
			if(CompareIdls(pidlDirectory,ds.pidlDirectory))
			{
				TCITEM tcItem;
				BOOL bRet;

				tcItem.mask = TCIF_PARAM;
				bRet = TabCtrl_GetItem(m_hTabCtrl,iTab,&tcItem);

				if(bRet)
				{
					int iIndexInternal = (int)tcItem.lParam;

					m_pShellBrowser[iIndexInternal]->SetSortMode(ds.dsi.SortMode);
					m_pShellBrowser[iIndexInternal]->SetCurrentViewMode(ds.dsi.ViewMode);

					ColumnExport_t ce;

					ce.ControlPanelColumnList = ds.dsi.ControlPanelColumnList;
					ce.MyComputerColumnList = ds.dsi.MyComputerColumnList;
					ce.MyNetworkPlacesColumnList = ds.dsi.MyNetworkPlacesColumnList;
					ce.NetworkConnectionsColumnList = ds.dsi.NetworkConnectionsColumnList;
					ce.PrintersColumnList = ds.dsi.PrintersColumnList;
					ce.RealFolderColumnList = ds.dsi.RealFolderColumnList;
					ce.RecycleBinColumnList = ds.dsi.RecycleBinColumnList;

					m_pShellBrowser[iIndexInternal]->ImportAllColumns(&ce);
				}
			}
		}
	}
}

void Explorerplusplus::PlayNavigationSound(void)
{
	if(m_bPlayNavigationSound)
	{
		PlaySound(MAKEINTRESOURCE(IDR_WAVE_NAVIGATIONSTART),NULL,
			SND_RESOURCE|SND_ASYNC);
	}
}

HWND Explorerplusplus::GetActiveListView() const
{
	return m_hActiveListView;
}

CShellBrowser *Explorerplusplus::GetActiveShellBrowser() const
{
	return m_pActiveShellBrowser;
}

void Explorerplusplus::OnShowHiddenFiles(void)
{
	m_pActiveShellBrowser->ToggleShowHidden();

	RefreshTab(m_iObjectIndex);
}
#ifndef MANAGEBOOKMARKSDIALOG_INCLUDED
#define MANAGEBOOKMARKSDIALOG_INCLUDED

#include "BookmarkHelper.h"
#include "../Helper/BaseDialog.h"
#include "../Helper/ResizableDialog.h"
#include "../Helper/DialogSettings.h"
#include "../Helper/Bookmark.h"

class CManageBookmarksDialog;

class CManageBookmarksDialogPersistentSettings : public CDialogSettings
{
public:

	~CManageBookmarksDialogPersistentSettings();

	static CManageBookmarksDialogPersistentSettings &GetInstance();

private:

	friend CManageBookmarksDialog;

	enum ColumnType_t
	{
		COLUMN_TYPE_NAME = 1,
		COLUMN_TYPE_LOCATION = 2,
		COLUMN_TYPE_VISIT_DATE = 3,
		COLUMN_TYPE_VISIT_COUNT = 4,
		COLUMN_TYPE_ADDED = 5,
		COLUMN_TYPE_LAST_MODIFIED = 6
	};

	struct ColumnInfo_t
	{
		ColumnType_t	ColumnType;
		int				iWidth;
		bool			bActive;
	};

	static const TCHAR SETTINGS_KEY[];
	static const int DEFAULT_MANAGE_BOOKMARKS_COLUMN_WIDTH = 180;

	CManageBookmarksDialogPersistentSettings();

	CManageBookmarksDialogPersistentSettings(const CManageBookmarksDialogPersistentSettings &);
	CManageBookmarksDialogPersistentSettings & operator=(const CManageBookmarksDialogPersistentSettings &);

	void SetupDefaultColumns();

	std::vector<ColumnInfo_t>		m_vectorColumnInfo;

	bool							m_bInitialized;
	GUID							m_guidSelected;
	NBookmarkHelper::setExpansion_t	m_setExpansion;

	NBookmarkHelper::SortMode_t		m_SortMode;
	bool							m_bSortAscending;
};

class CManageBookmarksDialog : public CBaseDialog, public NBookmark::IBookmarkItemNotification
{
public:

	CManageBookmarksDialog(HINSTANCE hInstance,int iResource,HWND hParent,CBookmarkFolder &AllBookmarks);
	~CManageBookmarksDialog();

	int CALLBACK		SortBookmarks(LPARAM lParam1,LPARAM lParam2);
	LRESULT CALLBACK	EditSearchProc(HWND hwnd,UINT Msg,WPARAM wParam,LPARAM lParam);

	void	OnBookmarkAdded(const CBookmarkFolder &ParentBookmarkFolder,const CBookmark &Bookmark,std::size_t Position);
	void	OnBookmarkFolderAdded(const CBookmarkFolder &ParentBookmarkFolder,const CBookmarkFolder &BookmarkFolder,std::size_t Position);
	void	OnBookmarkModified(const GUID &guid);
	void	OnBookmarkFolderModified(const GUID &guid);
	void	OnBookmarkRemoved(const GUID &guid);
	void	OnBookmarkFolderRemoved(const GUID &guid);

protected:

	INT_PTR	OnInitDialog();
	INT_PTR	OnCtlColorEdit(HWND hwnd,HDC hdc);
	INT_PTR	OnAppCommand(HWND hwnd,UINT uCmd,UINT uDevice,DWORD dwKeys);
	INT_PTR	OnCommand(WPARAM wParam,LPARAM lParam);
	INT_PTR	OnNotify(NMHDR *pnmhdr);
	INT_PTR	OnClose();
	INT_PTR	OnDestroy();
	INT_PTR	OnNcDestroy();

	void	SaveState();

private:

	static const COLORREF SEARCH_TEXT_COLOR = RGB(120,120,120);

	static const int TOOLBAR_ID_BACK			= 10000;
	static const int TOOLBAR_ID_FORWARD			= 10001;
	static const int TOOLBAR_ID_ORGANIZE		= 10002;
	static const int TOOLBAR_ID_VIEWS			= 10003;
	static const int TOOLBAR_ID_IMPORTEXPORT	= 10004;

	CManageBookmarksDialog & operator = (const CManageBookmarksDialog &mbd);

	void		SetDialogIcon();

	void		SetupSearchField();
	void		SetupToolbar();
	void		SetupTreeView();
	void		SetupListView();

	void		SortListViewItems(NBookmarkHelper::SortMode_t SortMode);

	void		GetColumnString(CManageBookmarksDialogPersistentSettings::ColumnType_t ColumnType,TCHAR *szColumn,UINT cchBuf);
	void		GetBookmarkItemColumnInfo(const NBookmarkHelper::variantBookmark_t variantBookmark,CManageBookmarksDialogPersistentSettings::ColumnType_t ColumnType,TCHAR *szColumn,size_t cchBuf);
	void		GetBookmarkColumnInfo(const CBookmark &Bookmark,CManageBookmarksDialogPersistentSettings::ColumnType_t ColumnType,TCHAR *szColumn,size_t cchBuf);
	void		GetBookmarkFolderColumnInfo(const CBookmarkFolder &BookmarkFolder,CManageBookmarksDialogPersistentSettings::ColumnType_t ColumnType,TCHAR *szColumn,size_t cchBuf);

	void		SetSearchFieldDefaultState();
	void		RemoveSearchFieldDefaultState();

	void		BrowseBack();
	void		BrowseForward();
	void		BrowseBookmarkFolder(const CBookmarkFolder &BookmarkFolder);

	void		UpdateToolbarState();

	void		OnNewFolder();
	void		OnDeleteBookmark(const GUID &guid);

	void		OnEnChange(HWND hEdit);
	void		OnDblClk(NMHDR *pnmhdr);
	void		OnRClick(NMHDR *pnmhdr);

	void		OnTbnDropDown(NMTOOLBAR *nmtb);
	void		ShowViewMenu();
	void		ShowOrganizeMenu();

	void		OnTvnSelChanged(NMTREEVIEW *pnmtv);

	void		OnListViewRClick();
	void		OnListViewHeaderRClick();
	BOOL		OnLvnEndLabelEdit(NMLVDISPINFO *pnmlvdi);
	void		OnLvnKeyDown(NMLVKEYDOWN *pnmlvkd);
	void		OnListViewRename();

	void		OnOk();
	void		OnCancel();

	HICON						m_hDialogIcon;

	HWND						m_hToolbar;
	HIMAGELIST					m_himlToolbar;

	CBookmarkFolder				&m_AllBookmarks;

	GUID						m_guidCurrentFolder;

	bool						m_bNewFolderAdded;
	GUID						m_guidNewFolder;

	std::stack<GUID>			m_stackBack;
	std::stack<GUID>			m_stackForward;
	bool						m_bSaveHistory;

	CBookmarkTreeView			*m_pBookmarkTreeView;

	bool						m_bListViewInitialized;
	CBookmarkListView			*m_pBookmarkListView;

	HFONT						m_hEditSearchFont;
	bool						m_bSearchFieldBlank;
	bool						m_bEditingSearchField;

	CManageBookmarksDialogPersistentSettings	*m_pmbdps;
};

#endif
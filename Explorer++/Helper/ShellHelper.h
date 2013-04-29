#ifndef SHELLHELPER_INCLUDED
#define SHELLHELPER_INCLUDED

#include <list>

#define CONTROL_PANEL_CATEGORY_VIEW	_T("::{26EE0668-A00A-44D7-9371-BEB064C98683}")

struct JumpListTaskInformation
{
	const TCHAR	*pszName;
	const TCHAR	*pszPath;
	const TCHAR	*pszArguments;
	const TCHAR	*pszIconPath;
	int			iIcon;
};

struct ContextMenuHandler_t
{
	HMODULE		hDLL;
	IUnknown	*pUnknown;
};

void			DecodePath(TCHAR *szInitialPath,TCHAR *szCurrentDirectory,TCHAR *szParsingPath,size_t cchDest);
HRESULT			GetIdlFromParsingName(const TCHAR *szParsingName,LPITEMIDLIST *pidl);
HRESULT			GetDisplayName(TCHAR *szParsingPath,TCHAR *szDisplayName,DWORD uFlags);
HRESULT			GetDisplayName(LPCITEMIDLIST pidlDirectory,TCHAR *szDisplayName,DWORD uFlags);
BOOL			CheckIdl(LPITEMIDLIST pidl);
BOOL			IsIdlDirectory(LPITEMIDLIST pidl);
void			GetVirtualFolderParsingPath(UINT uFolderCSIDL,TCHAR *szParsingPath);
HRESULT			GetVirtualParentPath(LPITEMIDLIST pidlDirectory,LPITEMIDLIST *pidlParent);
BOOL			IsNamespaceRoot(LPCITEMIDLIST pidl);
HRESULT			GetItemInfoTip(const TCHAR *szItemPath,TCHAR *szInfoTip,int cchMax);
HRESULT			GetItemInfoTip(LPITEMIDLIST pidlComplete,TCHAR *szInfoTip,int cchMax);
HRESULT			GetCsidlFolderName(UINT csidl,TCHAR *szFolderName,DWORD uParsingFlags);
BOOL			MyExpandEnvironmentStrings(const TCHAR *szSrc,TCHAR *szExpandedPath,DWORD nSize);
HRESULT			BuildHDropList(OUT FORMATETC *pftc,OUT STGMEDIUM *pstg,IN std::list<std::wstring> FilenameList);
HRESULT			BuildShellIDList(OUT FORMATETC *pftc,OUT STGMEDIUM *pstg,IN LPCITEMIDLIST pidlDirectory,IN std::list<LPITEMIDLIST> pidlList);
HRESULT			BindToShellFolder(LPCITEMIDLIST pidlDirectory,IShellFolder **pShellFolder);
BOOL			IsPathGUID(TCHAR *szPath);
BOOL			CompareIdls(LPCITEMIDLIST pidl1,LPCITEMIDLIST pidl2);
void			SetFORMATETC(FORMATETC *pftc,CLIPFORMAT cfFormat,DVTARGETDEVICE *ptd,DWORD dwAspect,LONG lindex,DWORD tymed);
HRESULT			AddJumpListTasks(std::list<JumpListTaskInformation> TaskList);
BOOL			LoadContextMenuHandlers(IN const TCHAR *szRegKey,OUT std::list<ContextMenuHandler_t> *pContextMenuHandlers);
BOOL			LoadIUnknownFromCLSID(IN TCHAR *szCLSID,OUT ContextMenuHandler_t *pContextMenuHandler);
BOOL			CopyTextToClipboard(const std::wstring &str);

#endif
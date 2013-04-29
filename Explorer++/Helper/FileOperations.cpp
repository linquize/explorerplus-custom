/******************************************************************
 *
 * Project: Helper
 * File: FileOperations.cpp
 * License: GPL - See COPYING in the top level directory
 *
 * Provides a set of file operation functions.
 *
 * Written by David Erceg
 * www.explorerplusplus.com
 *
 *****************************************************************/

#include "stdafx.h"
#include <list>
#include <sstream>
#include "FileOperations.h"
#include "Helper.h"
#include "iDataObject.h"
#include "ShellHelper.h"
#include "Macros.h"


#define PASTE_CLIPBOARD_LINK		0
#define PASTE_CLIPBOARD_HARDLINK	1

int PasteFilesFromClipboardSpecial(TCHAR *szDestination,UINT fPasteType);

BOOL NFileOperations::RenameFile(const std::wstring &strOldFilename,
	const std::wstring &strNewFilename)
{
	TCHAR *pszOldFilename = new TCHAR[strOldFilename.size() + 2];
	TCHAR *pszNewFilename = new TCHAR[strNewFilename.size() + 2];

	StringCchCopy(pszOldFilename,strOldFilename.size() + 2,strOldFilename.c_str());
	pszOldFilename[lstrlen(pszOldFilename) + 1] = '\0';
	StringCchCopy(pszNewFilename,strNewFilename.size() + 2,strNewFilename.c_str());
	pszNewFilename[lstrlen(pszNewFilename) + 1] = '\0';

	SHFILEOPSTRUCT shfo;
	shfo.hwnd	= NULL;
	shfo.wFunc	= FO_RENAME;
	shfo.pFrom	= pszOldFilename;
	shfo.pTo	= pszNewFilename;
	shfo.fFlags	= FOF_ALLOWUNDO;
	BOOL bRes = (!SHFileOperation(&shfo) && !shfo.fAnyOperationsAborted);

	delete[] pszOldFilename;
	delete[] pszNewFilename;

	return bRes;
}

BOOL NFileOperations::DeleteFiles(HWND hwnd,const std::list<std::wstring> &FullFilenameList,
	BOOL bPermanent)
{
	TCHAR *pszFullFilenames = NFileOperations::BuildFilenameList(FullFilenameList);

	FILEOP_FLAGS fFlags = 0;

	if(!bPermanent)
	{
		fFlags = FOF_ALLOWUNDO;
	}

	SHFILEOPSTRUCT shfo;
	shfo.hwnd					= hwnd;
	shfo.wFunc					= FO_DELETE;
	shfo.pFrom					= pszFullFilenames;
	shfo.pTo					= NULL;
	shfo.fFlags					= fFlags;
	shfo.fAnyOperationsAborted	= NULL;
	shfo.hNameMappings			= NULL;
	shfo.lpszProgressTitle		= NULL;
	BOOL bRes = (!SHFileOperation(&shfo) && !shfo.fAnyOperationsAborted);

	free(pszFullFilenames);

	return bRes;
}

BOOL NFileOperations::CopyFilesToFolder(HWND hOwner,const std::wstring &strTitle,
	const std::list<std::wstring> &FullFilenameList,BOOL bMove)
{
	std::wstring strOutputFilename;
	BOOL bRes = NFileOperations::CreateBrowseDialog(hOwner,strTitle.c_str(),strOutputFilename);

	if(!bRes)
	{
		return FALSE;
	}

	TCHAR *pszFullFilenames = NFileOperations::BuildFilenameList(FullFilenameList);

	/* TODO: Pass off to copy function. */
	SHFILEOPSTRUCT shfo;

	if(bMove)
	{
		shfo.wFunc = FO_MOVE;
	}
	else
	{
		shfo.wFunc = FO_COPY;
	}

	shfo.hwnd	= hOwner;
	shfo.pFrom	= pszFullFilenames;
	shfo.pTo	= strOutputFilename.c_str();
	shfo.fFlags	= FOF_ALLOWUNDO;
	bRes = (!SHFileOperation(&shfo) && !shfo.fAnyOperationsAborted);

	free(pszFullFilenames);

	return bRes;
}

TCHAR *NFileOperations::BuildFilenameList(const std::list<std::wstring> &FilenameList)
{
	TCHAR *pszFilenames = NULL;
	int iTotalSize = 0;

	for each(auto Filename in FilenameList)
	{
		pszFilenames = reinterpret_cast<TCHAR *>(realloc(pszFilenames,
			(iTotalSize + Filename.size() + 1) * sizeof(TCHAR)));
		memcpy(pszFilenames + iTotalSize,Filename.c_str(),(Filename.size() + 1) * sizeof(TCHAR));
		iTotalSize += static_cast<int>(Filename.size() + 1);
	}

	/* The list of strings must end with a second
	terminating NULL character, so add it now. */
	pszFilenames = reinterpret_cast<TCHAR *>(realloc(pszFilenames,(iTotalSize + 1) * sizeof(TCHAR)));
	pszFilenames[iTotalSize] = '\0';

	/* Note that it is up to the caller to free this. */
	return pszFilenames;
}

HRESULT CreateNewFolder(TCHAR *Directory,TCHAR *szNewFolderName,int cchMax)
{
	WIN32_FIND_DATA	wfd;
	HANDLE			hFirstFile;
	WCHAR			szLongPath[32768];
	TCHAR			FolderName[32768];
	BOOL			res;
	int				i = 2;

	if(Directory == NULL)
		return E_INVALIDARG;

	if(Directory[lstrlen(Directory) - 1] == '\\')
	{
		/* DON'T add a backslash to a path that already has
		one. Since it is assumed that ALL paths this function
		handles may be longer than MAX_PATH, don't use any
		of the Path* functions. */
		StringCchPrintf(FolderName,SIZEOF_ARRAY(FolderName),
			_T("%sNew Folder"),Directory);
	}
	else
	{
		StringCchPrintf(FolderName,SIZEOF_ARRAY(FolderName),
			_T("%s\\New Folder"),Directory);
	}

	StringCchPrintf(szLongPath,SIZEOF_ARRAY(szLongPath),
		L"\\\\?\\%s",FolderName);

	while((hFirstFile = FindFirstFile(szLongPath,&wfd))
	!= INVALID_HANDLE_VALUE)
	{
		FindClose(hFirstFile);

		if(Directory[lstrlen(Directory) - 1] == '\\')
		{
			/* DON'T add a backslash to a path that already has
			one. Since it is assumed that ALL paths this function
			handles may be longer than MAX_PATH, don't use any
			of the Path* functions. */
			StringCchPrintf(FolderName,SIZEOF_ARRAY(FolderName),
				_T("%sNew Folder (%d)"),Directory,i);
		}
		else
		{
			StringCchPrintf(FolderName,SIZEOF_ARRAY(FolderName),
				_T("%s\\New Folder (%d)"),Directory,i);
		}

		StringCchPrintf(szLongPath,SIZEOF_ARRAY(szLongPath),
			L"\\\\?\\%s",FolderName);

		i++;
	}

	res = CreateDirectory(szLongPath,NULL);

	if(!res)
	{
		return E_FAIL;
	}

	StringCchCopy(szNewFolderName,cchMax,FolderName);

	return S_OK;
}

BOOL NFileOperations::SaveDirectoryListing(const std::wstring &strDirectory,const std::wstring &strFilename)
{
	std::wstring strContents = _T("Directory\r\n---------\r\n") + strDirectory + _T("\r\n\r\n");

	SYSTEMTIME st;
	FILETIME ft;
	FILETIME lft;
	GetLocalTime(&st);
	SystemTimeToFileTime(&st,&ft);
	LocalFileTimeToFileTime(&ft,&lft);

	TCHAR szTime[128];
	CreateFileTimeString(&lft,szTime,SIZEOF_ARRAY(szTime),FALSE);
	strContents += _T("Date\r\n----\r\n") + std::wstring(szTime) + _T("\r\n\r\n");

	std::wstring strSearch = strDirectory + _T("\\*");

	WIN32_FIND_DATA wfd;
	HANDLE hFirstFile = FindFirstFile(strSearch.c_str(),&wfd);

	std::list<std::wstring> FolderList;
	std::list<std::wstring> FileList;
	ULARGE_INTEGER ulTotalSize;

	ulTotalSize.QuadPart = 0;

	if(hFirstFile != INVALID_HANDLE_VALUE)
	{
		ULARGE_INTEGER ulFileSize;

		if(lstrcmpi(wfd.cFileName,_T(".")) != 0 &&
			lstrcmpi(wfd.cFileName,_T("..")) != 0)
		{
			if((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ==
			FILE_ATTRIBUTE_DIRECTORY)
			{
				FolderList.push_back(wfd.cFileName);
			}
			else
			{
				FileList.push_back(wfd.cFileName);

				ulFileSize.LowPart = wfd.nFileSizeLow;
				ulFileSize.HighPart = wfd.nFileSizeHigh;

				ulTotalSize.QuadPart += ulFileSize.QuadPart;
			}
		}

		while(FindNextFile(hFirstFile,&wfd) != 0)
		{
			if(lstrcmpi(wfd.cFileName,_T(".")) != 0 &&
				lstrcmpi(wfd.cFileName,_T("..")) != 0)
			{
				if((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ==
					FILE_ATTRIBUTE_DIRECTORY)
				{
					FolderList.push_back(wfd.cFileName);
				}
				else
				{
					FileList.push_back(wfd.cFileName);

					ulFileSize.LowPart = wfd.nFileSizeLow;
					ulFileSize.HighPart = wfd.nFileSizeHigh;

					ulTotalSize.QuadPart += ulFileSize.QuadPart;
				}
			}
		}

		FindClose(hFirstFile);
	}

	std::wstringstream ss;
	ss.imbue(std::locale(""));
	ss.precision(0);

	strContents += _T("Statistics\r\n----------\r\n");

	ss << std::fixed << FolderList.size();
	strContents += _T("Number of folders: ") + ss.str() + std::wstring(_T("\r\n"));

	ss.str(_T(""));
	ss << std::fixed << FileList.size();
	strContents += _T("Number of files: ") + ss.str() + std::wstring(_T("\r\n"));

	TCHAR szTotalSize[32];
	FormatSizeString(ulTotalSize,szTotalSize,SIZEOF_ARRAY(szTotalSize));
	strContents += _T("Total size (not including subfolders): ") + std::wstring(szTotalSize) + std::wstring(_T("\r\n"));

	strContents += _T("\r\nFolders\r\n-------\r\n");

	for each(auto Folder in FolderList)
	{
		strContents += Folder + _T("\r\n");
	}

	strContents += _T("\r\nFiles\r\n-----\r\n");

	for each(auto File in FileList)
	{
		strContents += File + _T("\r\n");
	}

	/* Remove the trailing newline. */
	strContents = strContents.substr(0,strContents.size() - 2);

	HANDLE hFile = CreateFile(strFilename.c_str(),FILE_WRITE_DATA,0,NULL,
		CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);

	if(hFile != INVALID_HANDLE_VALUE)
	{
		DWORD nBytesWritten;

		/* Write out the BOM for UTF-16 LE data.
		See http://en.wikipedia.org/wiki/Byte-order_mark */
		WriteFile(hFile,reinterpret_cast<LPCVOID>("\xFF\xFE"),2,&nBytesWritten,NULL);

		WriteFile(hFile,reinterpret_cast<LPCVOID>(strContents.c_str()),static_cast<DWORD>(strContents.size() * sizeof(WCHAR)),
			&nBytesWritten,NULL);

		if(nBytesWritten == strContents.size())
		{
			return TRUE;
		}

		CloseHandle(hFile);
	}

	return FALSE;
}

HRESULT CopyFiles(std::list<std::wstring> FileNameList,IDataObject **pClipboardDataObject)
{
	return CopyFilesToClipboard(FileNameList,FALSE,pClipboardDataObject);
}

HRESULT CutFiles(std::list<std::wstring> FileNameList,IDataObject **pClipboardDataObject)
{
	return CopyFilesToClipboard(FileNameList,TRUE,pClipboardDataObject);
}

HRESULT CopyFilesToClipboard(std::list<std::wstring> FileNameList,
BOOL bMove,IDataObject **pClipboardDataObject)
{
	FORMATETC ftc[2];
	STGMEDIUM stg[2];
	HRESULT hr;

	BuildHDropList(&ftc[0],&stg[0],FileNameList);

	ftc[1].cfFormat			= (CLIPFORMAT)RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
	ftc[1].ptd				= NULL;
	ftc[1].dwAspect			= DVASPECT_CONTENT;
	ftc[1].lindex			= -1;
	ftc[1].tymed			= TYMED_HGLOBAL;
	
	HGLOBAL hglb = GlobalAlloc(GMEM_MOVEABLE,sizeof(DWORD));

	DWORD *pdwCopyEffect = static_cast<DWORD *>(GlobalLock(hglb));

	if(bMove)
		*pdwCopyEffect = DROPEFFECT_MOVE;
	else
		*pdwCopyEffect = DROPEFFECT_COPY;

	GlobalUnlock(hglb);

	stg[1].pUnkForRelease	= 0;

	stg[1].hGlobal			= hglb;
	stg[1].tymed			= TYMED_HGLOBAL;

	hr = CreateDataObject(ftc,stg,pClipboardDataObject,2);

	IAsyncOperation *pAsyncOperation = NULL;

	(*pClipboardDataObject)->QueryInterface(IID_IAsyncOperation,(void **)&pAsyncOperation);

	pAsyncOperation->SetAsyncMode(TRUE);
	pAsyncOperation->Release();

	if(SUCCEEDED(hr))
	{
		hr = OleSetClipboard(*pClipboardDataObject);
	}

	return hr;
}

struct HANDLETOMAPPINGS
{
	UINT			uNumberOfMappings;
	LPSHNAMEMAPPING	lpSHNameMapping;
};

int PasteLinksToClipboardFiles(TCHAR *szDestination)
{
	return PasteFilesFromClipboardSpecial(szDestination,PASTE_CLIPBOARD_LINK);
}

int PasteHardLinks(TCHAR *szDestination)
{
	return PasteFilesFromClipboardSpecial(szDestination,PASTE_CLIPBOARD_HARDLINK);
}

/* TODO: Use CDropHandler. */
int PasteFilesFromClipboardSpecial(TCHAR *szDestination,UINT fPasteType)
{
	IDataObject	*ClipboardObject = NULL;
	DROPFILES	*pdf = NULL;
	FORMATETC	ftc;
	STGMEDIUM	stg;
	HRESULT		hr;
	TCHAR		szFileName[MAX_PATH];
	TCHAR		szLinkFileName[MAX_PATH];
	TCHAR		szOldFileName[MAX_PATH];
	int			nFilesCopied = -1;
	int			i = 0;

	hr = OleGetClipboard(&ClipboardObject);

	if(SUCCEEDED(hr))
	{
		ftc.cfFormat	= CF_HDROP;
		ftc.ptd			= NULL;
		ftc.dwAspect	= DVASPECT_CONTENT;
		ftc.lindex		= -1;
		ftc.tymed		= TYMED_HGLOBAL;

		hr = ClipboardObject->GetData(&ftc,&stg);

		if(SUCCEEDED(hr))
		{
			pdf = (DROPFILES *)GlobalLock(stg.hGlobal);

			if(pdf != NULL)
			{
				nFilesCopied = DragQueryFile((HDROP)pdf,
					0xFFFFFFFF,NULL,0);

				for(i = 0;i < nFilesCopied;i++)
				{
					DragQueryFile((HDROP)pdf,i,szOldFileName,SIZEOF_ARRAY(szOldFileName));

					StringCchCopy(szLinkFileName,SIZEOF_ARRAY(szLinkFileName),szDestination);

					StringCchCopy(szFileName,SIZEOF_ARRAY(szFileName),szOldFileName);
					PathStripPath(szFileName);

					PathAppend(szLinkFileName,szFileName);

					switch(fPasteType)
					{
					case PASTE_CLIPBOARD_LINK:
						PathRenameExtension(szLinkFileName,_T(".lnk"));
						NFileOperations::CreateLinkToFile(szOldFileName,szLinkFileName,EMPTY_STRING);
						break;

					case PASTE_CLIPBOARD_HARDLINK:
						CreateHardLink(szLinkFileName,szOldFileName,NULL);
						break;
					}
				}

				GlobalUnlock(stg.hGlobal);
			}

			ReleaseStgMedium(&stg);
		}
		ClipboardObject->Release();
	}

	return nFilesCopied;
}

HRESULT NFileOperations::CreateLinkToFile(const std::wstring &strTargetFilename,
	const std::wstring &strLinkFilename,const std::wstring &strLinkDescription)
{
	IShellLink *pShellLink = NULL;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,
		IID_IShellLink,reinterpret_cast<LPVOID *>(&pShellLink));

	if(SUCCEEDED(hr))
	{
		pShellLink->SetPath(strTargetFilename.c_str());
		pShellLink->SetDescription(strLinkDescription.c_str());

		IPersistFile *pPersistFile = NULL;
		hr = pShellLink->QueryInterface(IID_IPersistFile,reinterpret_cast<LPVOID *>(&pPersistFile));

		if(SUCCEEDED(hr))
		{
			pPersistFile->Save(strLinkFilename.c_str(),TRUE);
			pPersistFile->Release();
		}

		pShellLink->Release();
	}

	return hr;
}

HRESULT NFileOperations::ResolveLink(HWND hwnd,DWORD fFlags,TCHAR *szLinkFilename,TCHAR *szResolvedPath,int nBufferSize)
{
	SHFILEINFO shfi;
	DWORD_PTR dwRet = SHGetFileInfo(szLinkFilename,NULL,&shfi,sizeof(shfi),SHGFI_ATTRIBUTES);

	if(dwRet == 0 ||
		!(shfi.dwAttributes & SFGAO_LINK))
	{
		return E_FAIL;
	}

	IShellLink *pShellLink = NULL;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,
		IID_IShellLink,reinterpret_cast<LPVOID *>(&pShellLink));

	if(hr == S_OK)
	{
		IPersistFile *pPersistFile = NULL;
		hr = pShellLink->QueryInterface(IID_IPersistFile,reinterpret_cast<LPVOID *>(&pPersistFile));

		if(hr == S_OK)
		{
			hr = pPersistFile->Load(szLinkFilename,STGM_READ);

			if(hr == S_OK)
			{
				pShellLink->Resolve(hwnd,fFlags);

				TCHAR szResolvedPathInternal[MAX_PATH];
				pShellLink->GetPath(szResolvedPathInternal,SIZEOF_ARRAY(szResolvedPathInternal),NULL,SLGP_UNCPRIORITY);

				StringCchCopy(szResolvedPath,nBufferSize,szResolvedPathInternal);
			}

			pPersistFile->Release();
		}

		pShellLink->Release();
	}

	return hr;
}

BOOL NFileOperations::CreateBrowseDialog(HWND hOwner,const std::wstring &strTitle,std::wstring &strOutputFilename)
{
	LPITEMIDLIST pidl = NULL;
	BOOL bRes = NFileOperations::CreateBrowseDialog(hOwner,strTitle,&pidl);

	if(bRes)
	{
		TCHAR szOutputFilename[MAX_PATH];
		SHGetPathFromIDList(pidl,szOutputFilename);
		strOutputFilename = szOutputFilename;
		CoTaskMemFree(pidl);
	}

	return bRes;
}

BOOL NFileOperations::CreateBrowseDialog(HWND hOwner,const std::wstring &strTitle,LPITEMIDLIST *ppidl)
{
	CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);

	TCHAR szDisplayName[MAX_PATH];

	BROWSEINFO bi;
	bi.hwndOwner		= hOwner;
	bi.pidlRoot			= NULL;
	bi.pszDisplayName	= szDisplayName;
	bi.lpszTitle		= strTitle.c_str();
	bi.ulFlags			= BIF_NEWDIALOGSTYLE;
	bi.lpfn				= NULL;
	*ppidl = SHBrowseForFolder(&bi);

	BOOL bSuccessful = (*ppidl != NULL);

	CoUninitialize();

	return bSuccessful;
}

void NFileOperations::DeleteFileSecurely(const std::wstring &strFilename,OverwriteMethod_t uOverwriteMethod)
{
	HANDLE			hFile;
	WIN32_FIND_DATA	wfd;
	HANDLE			hFindFile;
	HCRYPTPROV		hProv;
	LARGE_INTEGER	lRealFileSize;
	BYTE			Pass1Data;
	BYTE			Pass2Data;
	BYTE			Pass3Data;
	DWORD			nBytesWritten;
	BOOL			bFolder;
	int				i = 0;

	hFindFile = FindFirstFile(strFilename.c_str(),&wfd);

	if(hFindFile == INVALID_HANDLE_VALUE)
		return;

	bFolder = (wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY;

	FindClose(hFindFile);

	if(bFolder)
	{
		/* TODO: Recurse. */
		return;
	}

	/* Determine the actual size of the file on disk
	(i.e. how many clusters it is allocated). */
	GetRealFileSize(strFilename,&lRealFileSize);

	/* Open the file, block any sharing mode, to stop the file
	been opened while it is overwritten. */
	hFile = CreateFile(strFilename.c_str(),FILE_WRITE_DATA,0,NULL,OPEN_EXISTING,
	NULL,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return;

	/* Extend the file out to the end of its last sector. */
	SetFilePointerEx(hFile,lRealFileSize,NULL,FILE_BEGIN);
	SetEndOfFile(hFile);

	/* Start at the beginning of the file, and
	write in the first-pass data, 0x00 over
	the length of the whole file. */
	SetFilePointer(hFile,0,NULL,FILE_BEGIN);
	Pass1Data = 0x00;

	for(i = 0;i < lRealFileSize.QuadPart;i++)
	{
		WriteFile(hFile,(LPVOID)&Pass1Data,1,&nBytesWritten,NULL);
	}

	if(uOverwriteMethod == OVERWRITE_THREEPASS)
	{
		/* Start at the beginning of the file, and
		write in the second-pass data, 0xFF over
		the length of the whole file. */
		SetFilePointer(hFile,0,NULL,FILE_BEGIN);
		Pass2Data = 0xFF;

		for(i = 0;i < lRealFileSize.QuadPart;i++)
		{
			WriteFile(hFile,(LPVOID)&Pass2Data,1,&nBytesWritten,NULL);
		}

		SetFilePointer(hFile,0,NULL,FILE_BEGIN);

		CryptAcquireContext(&hProv,_T("SecureDelete"),NULL,PROV_RSA_AES,CRYPT_NEWKEYSET);

		for(i = 0;i < lRealFileSize.QuadPart;i++)
		{
			CryptGenRandom(hProv,1,(LPBYTE)&Pass3Data);
			WriteFile(hFile,(LPVOID)&Pass3Data,1,&nBytesWritten,NULL);
		}

		CryptAcquireContext(&hProv,_T("SecureDelete"),NULL,PROV_RSA_AES,CRYPT_DELETEKEYSET);
	}

	FlushFileBuffers(hFile);

	CloseHandle(hFile);

	DeleteFile(strFilename.c_str());
}
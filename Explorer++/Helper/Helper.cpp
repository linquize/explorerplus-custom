/******************************************************************
 *
 * Project: Helper
 * File: Helper.cpp
 * License: GPL - See COPYING in the top level directory
 *
 * Contains various helper functions.
 *
 * Written by David Erceg
 * www.explorerplusplus.com
 *
 *****************************************************************/

#include "stdafx.h"
#include <sstream>
#include "Helper.h"
#include "FileOperations.h"
#include "ShellHelper.h"
#include "Macros.h"


/* Local helpers. */
void	EnterAttributeIntoString(BOOL bEnter,TCHAR *String,int Pos,TCHAR chAttribute);
BOOL	GetFileAllocationInfo(TCHAR *lpszFileName,STARTING_VCN_INPUT_BUFFER *pStartingVcn,
							  RETRIEVAL_POINTERS_BUFFER *pRetrievalPointers,DWORD BufSize);
BOOL	GetNtfsVolumeInfo(TCHAR *lpszDrive,NTFS_VOLUME_DATA_BUFFER *pNtfsVolumeInfo,DWORD BufSize);
TCHAR	*GetPartitionName(LARGE_INTEGER StartingOffset);

void FormatSizeString(ULARGE_INTEGER lFileSize,TCHAR *pszFileSize,
size_t cchBuf)
{
	FormatSizeString(lFileSize,pszFileSize,cchBuf,FALSE,SIZE_FORMAT_NONE);
}

void FormatSizeString(ULARGE_INTEGER lFileSize,TCHAR *pszFileSize,
size_t cchBuf,BOOL bForceSize,SizeDisplayFormat_t sdf)
{
	TCHAR *pszSizeTypes[] = {_T("bytes"),_T("KB"),_T("MB"),_T("GB"),_T("TB"),_T("PB")};

	double fFileSize = static_cast<double>(lFileSize.QuadPart);
	int iSizeIndex = 0;

	if(bForceSize)
	{
		switch(sdf)
		{
		case SIZE_FORMAT_BYTES:
			iSizeIndex = 0;
			break;

		case SIZE_FORMAT_KBYTES:
			iSizeIndex = 1;
			break;

		case SIZE_FORMAT_MBYTES:
			iSizeIndex = 2;
			break;

		case SIZE_FORMAT_GBYTES:
			iSizeIndex = 3;
			break;

		case SIZE_FORMAT_TBYTES:
			iSizeIndex = 4;
			break;

		case SIZE_FORMAT_PBYTES:
			iSizeIndex = 5;
			break;
		}

		for(int i = 0;i < iSizeIndex;i++)
		{
			fFileSize /= 1024;
		}
	}
	else
	{
		while((fFileSize / 1024) >= 1)
		{
			fFileSize /= 1024;

			iSizeIndex++;
		}

		if(iSizeIndex > (SIZEOF_ARRAY(pszSizeTypes) - 1))
		{
			StringCchCopy(pszFileSize,cchBuf,EMPTY_STRING);
			return;
		}
	}

	int iPrecision;

	if(iSizeIndex == 0)
	{
		iPrecision = 0;
	}
	else
	{
		if(fFileSize < 10)
		{
			iPrecision = 2;
		}
		else if(fFileSize < 100)
		{
			iPrecision = 1;
		}
		else
		{
			iPrecision = 0;
		}
	}

	int iLeast = static_cast<int>((fFileSize - static_cast<int>(fFileSize)) *
		pow(10.0,iPrecision + 1));

	/* Setting the precision will cause automatic rounding. Therefore,
	if the least significant digit to be dropped is greater than 0.5,
	reduce it to below 0.5. */
	if(iLeast >= 5)
	{
		fFileSize -= 5.0 * pow(10.0,-(iPrecision + 1));
	}

	std::wstringstream ss;
	ss.imbue(std::locale(""));
	ss.precision(iPrecision);

	ss << std::fixed << fFileSize << _T(" ") << pszSizeTypes[iSizeIndex];
	std::wstring str = ss.str();
	StringCchCopy(pszFileSize,cchBuf,str.c_str());
}

int CreateFileTimeString(FILETIME *FileTime,
TCHAR *Buffer,int MaxCharacters,BOOL bFriendlyDate)
{
	SYSTEMTIME SystemTime;
	FILETIME LocalFileTime;
	TCHAR TempBuffer[MAX_STRING_LENGTH];
	TCHAR DateBuffer[MAX_STRING_LENGTH];
	TCHAR TimeBuffer[MAX_STRING_LENGTH];
	SYSTEMTIME CurrentTime;
	int iReturn1 = 0;
	int iReturn2 = 0;

	if(FileTime == NULL)
	{
		Buffer = NULL;
		return -1;
	}

	FileTimeToLocalFileTime(FileTime,&LocalFileTime);
	FileTimeToSystemTime(&LocalFileTime,&SystemTime);
	
	GetLocalTime(&CurrentTime);

	if(bFriendlyDate)
	{
		if((CurrentTime.wYear == SystemTime.wYear) &&
		(CurrentTime.wMonth == SystemTime.wMonth))
		{
			if(CurrentTime.wDay == SystemTime.wDay)
			{
				StringCchCopy(DateBuffer,SIZEOF_ARRAY(DateBuffer),_T("Today"));

				iReturn1 = 1;
			}
			else if(CurrentTime.wDay == (SystemTime.wDay + 1))
			{
				StringCchCopy(DateBuffer,SIZEOF_ARRAY(DateBuffer),_T("Yesterday"));

				iReturn1 = 1;
			}
			else
			{
				iReturn1 = GetDateFormat(LOCALE_USER_DEFAULT,LOCALE_USE_CP_ACP,&SystemTime,
				NULL,DateBuffer,MAX_STRING_LENGTH);
			}
		}
		else
		{
			iReturn1 = GetDateFormat(LOCALE_USER_DEFAULT,LOCALE_USE_CP_ACP,&SystemTime,
			NULL,DateBuffer,MAX_STRING_LENGTH);
		}
	}
	else
	{
		iReturn1 = GetDateFormat(LOCALE_USER_DEFAULT,LOCALE_USE_CP_ACP,&SystemTime,
		NULL,DateBuffer,MAX_STRING_LENGTH);
	}

	iReturn2 = GetTimeFormat(LOCALE_USER_DEFAULT,LOCALE_USE_CP_ACP,&SystemTime,
	NULL,TimeBuffer,MAX_STRING_LENGTH);
	
	if(iReturn1 && iReturn2)
	{
		StringCchPrintf(TempBuffer,SIZEOF_ARRAY(TempBuffer),
			_T("%s, %s"),DateBuffer,TimeBuffer);

		if(MaxCharacters < (lstrlen(TempBuffer) + 1))
		{
			Buffer = NULL;

			return lstrlen(TempBuffer) + 1;
		}
		else
		{
			StringCchCopy(Buffer,MaxCharacters,TempBuffer);

			return lstrlen(TempBuffer) + 1;
		}
	}

	Buffer = NULL;

	return -1;
}

HRESULT GetBitmapDimensions(TCHAR *FileName,SIZE *BitmapSize)
{
	HANDLE hFile;
	HANDLE hMappedFile;
	BITMAPFILEHEADER *BitmapFileHeader;
	BITMAPINFOHEADER *BitmapInfoHeader;
	LPVOID Address;

	if((FileName == NULL) || (BitmapSize == NULL))
		return E_INVALIDARG;

	hFile = CreateFile(FileName,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return E_FAIL;

	hMappedFile = CreateFileMapping(hFile,NULL,PAGE_READONLY,0,0,_T("MappedFile"));

	if(hMappedFile == NULL)
	{
		CloseHandle(hFile);
		return E_FAIL;
	}

	Address = MapViewOfFile(hMappedFile,FILE_MAP_READ,0,0,0);

	if(Address == NULL)
	{
		CloseHandle(hFile);
		CloseHandle(hMappedFile);
		return E_FAIL;
	}

	BitmapInfoHeader = (BITMAPINFOHEADER *)(LPBYTE)((TCHAR *)Address + sizeof(BITMAPFILEHEADER));

	BitmapFileHeader = (BITMAPFILEHEADER *)(LPBYTE)Address;

	BitmapSize->cx = BitmapInfoHeader->biWidth;
	BitmapSize->cy = BitmapInfoHeader->biHeight;

	CloseHandle(hFile);
	UnmapViewOfFile(Address);
	CloseHandle(hMappedFile);

	return S_OK;
}

HINSTANCE StartCommandPrompt(TCHAR *Directory,bool Elevated)
{
	HINSTANCE hNewInstance = NULL;

	TCHAR SystemPath[MAX_PATH];
	BOOL bRes = SHGetSpecialFolderPath(NULL,SystemPath,CSIDL_SYSTEM,0);

	if(bRes)
	{
		TCHAR CommandPath[MAX_PATH];
		PathCombine(CommandPath,SystemPath,_T("cmd.exe"));

		TCHAR Operation[32];

		if(Elevated)
		{
			StringCchCopy(Operation,SIZEOF_ARRAY(Operation),_T("runas"));
		}
		else
		{
			StringCchCopy(Operation,SIZEOF_ARRAY(Operation),_T("open"));
		}

		hNewInstance = ShellExecute(NULL,Operation,CommandPath,NULL,Directory,
		SW_SHOWNORMAL);
	}

	return hNewInstance;
}

BOOL lCheckMenuItem(HMENU hMenu,UINT ItemID,BOOL bCheck)
{
	if(bCheck)
	{
		CheckMenuItem(hMenu,ItemID,MF_CHECKED);
		return TRUE;
	}
	else
	{
		CheckMenuItem(hMenu,ItemID,MF_UNCHECKED);
		return FALSE;
	}
}

BOOL lEnableMenuItem(HMENU hMenu,UINT ItemID,BOOL bEnable)
{
	if(bEnable)
	{
		EnableMenuItem(hMenu,ItemID,MF_ENABLED);
		return TRUE;
	}
	else
	{
		EnableMenuItem(hMenu,ItemID,MF_GRAYED);
		return FALSE;
	}
}

BOOL GetRealFileSize(const std::wstring &strFilename,PLARGE_INTEGER lpRealFileSize)
{
	LARGE_INTEGER lFileSize;
	LONG ClusterSize;
	HANDLE hFile;
	TCHAR szRoot[MAX_PATH];

	/* Get a handle to the file. */
	hFile = CreateFile(strFilename.c_str(),GENERIC_READ,
	FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,NULL,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	/* Get the files size (count of number of actual
	number of bytes in file). */
	GetFileSizeEx(hFile,&lFileSize);

	*lpRealFileSize = lFileSize;

	if(lFileSize.QuadPart != 0)
	{
		StringCchCopy(szRoot,SIZEOF_ARRAY(szRoot),strFilename.c_str());
		PathStripToRoot(szRoot);

		/* Get the cluster size of the drive the file resides on. */
		ClusterSize = GetClusterSize(szRoot);

		if((lpRealFileSize->QuadPart % ClusterSize) != 0)
		{
			/* The real size is the logical file size rounded up to the end of the
			nearest cluster. */
			lpRealFileSize->QuadPart += ClusterSize - (lpRealFileSize->QuadPart % ClusterSize);
		}
	}

	CloseHandle(hFile);

	return TRUE;
}

LONG GetFileSectorSize(TCHAR *FileName)
{
	LONG SectorSize;
	LONG FileSize;
	LONG SectorFileSize;
	HANDLE hFile;
	int SectorCount = 0;
	TCHAR Root[MAX_PATH];

	if(FileName == NULL)
		return -1;

	/* Get a handle to the file. */
	hFile = CreateFile(FileName,GENERIC_READ,
	FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,NULL,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return -1;

	/* Get the files size (count of number of actual
	number of bytes in file). */
	FileSize = GetFileSize(hFile,NULL);

	StringCchCopy(Root,SIZEOF_ARRAY(Root),FileName);
	PathStripToRoot(Root);

	/* Get the sector size of the drive the file resides on. */
	SectorSize = GetSectorSize(Root);

	SectorFileSize = 0;
	while(SectorFileSize < FileSize)
	{
		SectorFileSize += SectorSize;
		SectorCount++;
	}

	CloseHandle(hFile);

	return SectorCount;
}

BOOL FileTimeToLocalSystemTime(LPFILETIME lpFileTime,LPSYSTEMTIME lpLocalTime)
{
	SYSTEMTIME SystemTime;

	FileTimeToSystemTime(lpFileTime,&SystemTime);

	return SystemTimeToTzSpecificLocalTime(NULL,&SystemTime,lpLocalTime);
}

BOOL LocalSystemTimeToFileTime(LPSYSTEMTIME lpLocalTime,LPFILETIME lpFileTime)
{
	SYSTEMTIME SystemTime;

	TzSpecificLocalTimeToSystemTime(NULL,lpLocalTime,&SystemTime);

	return SystemTimeToFileTime(&SystemTime,lpFileTime);
}

BOOL SetProcessTokenPrivilege(DWORD ProcessId,TCHAR *PrivilegeName,BOOL bEnablePrivilege)
{
	HANDLE hProcess;
	HANDLE hToken;
	TOKEN_PRIVILEGES tp;
	LUID luid;

	hProcess = OpenProcess(PROCESS_ALL_ACCESS,FALSE,ProcessId);

	if(hProcess == NULL)
		return FALSE;

	OpenProcessToken(hProcess,TOKEN_ALL_ACCESS,&hToken);

	CloseHandle(hProcess);

	LookupPrivilegeValue(NULL,PrivilegeName,&luid);

	tp.PrivilegeCount				= 1;
	tp.Privileges[0].Luid			= luid;

	if(bEnablePrivilege)
		tp.Privileges[0].Attributes	= SE_PRIVILEGE_ENABLED;
	else
		tp.Privileges[0].Attributes	= 0;

	BOOL Res = AdjustTokenPrivileges(hToken,FALSE,&tp,0,NULL,NULL);

	CloseHandle(hToken);

	return Res;
}

BOOL CompareFileTypes(TCHAR *pszFile1,TCHAR *pszFile2)
{
	SHFILEINFO shfi1;
	SHFILEINFO shfi2;

	SHGetFileInfo(pszFile1,NULL,&shfi1,sizeof(shfi1),SHGFI_TYPENAME);
	SHGetFileInfo(pszFile2,NULL,&shfi2,sizeof(shfi2),SHGFI_TYPENAME);

	if(StrCmp(shfi1.szTypeName,shfi2.szTypeName) == 0)
		return TRUE;

	return FALSE;
}

BOOL SetFileSparse(TCHAR *szFileName)
{
	HANDLE hFile;
	DWORD NumBytesReturned;
	BOOL res;

	hFile = CreateFile(szFileName,FILE_WRITE_DATA,0,
	NULL,OPEN_EXISTING,NULL,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	res = DeviceIoControl(hFile,FSCTL_SET_SPARSE,NULL,0,
	NULL,0,&NumBytesReturned,NULL);

	CloseHandle(hFile);

	return res;
}

void CheckItem(HWND hwnd,BOOL bCheck)
{
	DWORD CheckState;

	if(bCheck)
		CheckState = BST_CHECKED;
	else
		CheckState = BST_UNCHECKED;

	SendMessage(hwnd,BM_SETCHECK,(WPARAM)CheckState,(LPARAM)0);
}

BOOL IsItemChecked(HWND hwnd)
{
	LRESULT CheckState;

	CheckState = SendMessage(hwnd,BM_GETCHECK,0,0);

	return (CheckState == BST_CHECKED) ? TRUE : FALSE;
}

TCHAR *PrintComma(unsigned long nPrint)
{
	LARGE_INTEGER lPrint;

	lPrint.LowPart = nPrint;
	lPrint.HighPart = 0;

	return PrintCommaLargeNum(lPrint);
}

TCHAR *PrintCommaLargeNum(LARGE_INTEGER lPrint)
{
	static TCHAR szBuffer[14];
	TCHAR *p = &szBuffer[SIZEOF_ARRAY(szBuffer) - 1];
	static TCHAR chComma = ',';
	unsigned long long nTemp = (unsigned long long)(lPrint.LowPart + (lPrint.HighPart * pow(2.0,32.0)));
	int i = 0;

	if(nTemp == 0)
	{
		StringCchPrintf(szBuffer,SIZEOF_ARRAY(szBuffer),_T("%d"),0);
		return szBuffer;
	}

	*p = (TCHAR)'\0';

	while(nTemp != 0)
	{
		if(i%3 == 0 && i != 0)
			*--p = chComma;

		*--p = '0' + (TCHAR)(nTemp % 10);

		nTemp /= 10;

		i++;
	}

	return p;
}

BOOL lShowWindow(HWND hwnd,BOOL bShowWindow)
{
	int WindowShowState;

	if(bShowWindow)
		WindowShowState = SW_SHOW;
	else
		WindowShowState = SW_HIDE;

	return ShowWindow(hwnd,WindowShowState);
}

int GetRectHeight(RECT *rc)
{
	return rc->bottom - rc->top;
}

int GetRectWidth(RECT *rc)
{
	return rc->right - rc->left;
}

DWORD BuildFileAttributeString(TCHAR *lpszFileName,TCHAR *Buffer,DWORD BufSize)
{
	HANDLE hFindFile;
	WIN32_FIND_DATA wfd;

	/* FindFirstFile is used instead of GetFileAttributes() or
	GetFileAttributesEx() because of its behaviour
	in relation to system files that normally
	won't have their attributes given (such as the
	pagefile, which neither of the two functions
	above can retrieve the attributes of). */
	hFindFile = FindFirstFile(lpszFileName,&wfd);

	if(hFindFile == INVALID_HANDLE_VALUE)
	{
		StringCchCopy(Buffer,BufSize,EMPTY_STRING);
		return 0;
	}

	BuildFileAttributeStringInternal(wfd.dwFileAttributes,Buffer,BufSize);

	FindClose(hFindFile);

	return wfd.dwFileAttributes;
}

void BuildFileAttributeStringInternal(DWORD dwFileAttributes,TCHAR *szOutput,DWORD cchMax)
{
	TCHAR szAttributes[8];
	int i = 0;

	EnterAttributeIntoString(dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE,szAttributes,i++,'A');
	EnterAttributeIntoString(dwFileAttributes & FILE_ATTRIBUTE_HIDDEN,szAttributes,i++,'H');
	EnterAttributeIntoString(dwFileAttributes & FILE_ATTRIBUTE_READONLY,szAttributes,i++,'R');
	EnterAttributeIntoString(dwFileAttributes & FILE_ATTRIBUTE_SYSTEM,szAttributes,i++,'S');
	EnterAttributeIntoString((dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY,
		szAttributes,i++,'D');
	EnterAttributeIntoString(dwFileAttributes & FILE_ATTRIBUTE_COMPRESSED,szAttributes,i++,'C');
	EnterAttributeIntoString(dwFileAttributes & FILE_ATTRIBUTE_ENCRYPTED,szAttributes,i++,'E');

	szAttributes[i] = '\0';

	StringCchCopy(szOutput,cchMax,szAttributes);
}

void EnterAttributeIntoString(BOOL bEnter,TCHAR *String,int Pos,TCHAR chAttribute)
{
	if(bEnter)
		String[Pos] = chAttribute;
	else
		String[Pos] = '-';
}

size_t GetFileOwner(TCHAR *szFile,TCHAR *szOwner,DWORD BufSize)
{
	HANDLE hFile;
	PSID pSid;
	PSECURITY_DESCRIPTOR pSecurityDescriptor;
	DWORD dwRes;
	TCHAR szAccountName[512];
	DWORD dwAccountName = SIZEOF_ARRAY(szAccountName);
	TCHAR szDomainName[512];
	DWORD dwDomainName = SIZEOF_ARRAY(szDomainName);
	size_t ReturnLength = 0;
	SID_NAME_USE eUse;
	LPTSTR StringSid;
	BOOL bRes;

	/* The SE_SECURITY_NAME privilege is needed to call GetSecurityInfo on the given file. */
	bRes = SetProcessTokenPrivilege(GetCurrentProcessId(),SE_SECURITY_NAME,TRUE);

	if(!bRes)
		return 0;

	hFile = CreateFile(szFile,STANDARD_RIGHTS_READ|ACCESS_SYSTEM_SECURITY,FILE_SHARE_READ,NULL,OPEN_EXISTING,
	FILE_FLAG_BACKUP_SEMANTICS,NULL);

	if(hFile != INVALID_HANDLE_VALUE)
	{
		pSid = (PSID)GlobalAlloc(GMEM_FIXED,sizeof(PSID));

		pSecurityDescriptor = (PSECURITY_DESCRIPTOR)GlobalAlloc(GMEM_FIXED,sizeof(PSECURITY_DESCRIPTOR));

		dwRes = GetSecurityInfo(hFile,SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION,&pSid,
			NULL,NULL,NULL,&pSecurityDescriptor);

		if(dwRes != ERROR_SUCCESS)
		{
			CloseHandle(hFile);
			return 0;
		}

		bRes = LookupAccountSid(NULL,pSid,szAccountName,&dwAccountName,
			szDomainName,&dwDomainName,&eUse);

		/* LookupAccountSid failed. */
		if(bRes == 0)
		{
			bRes = ConvertSidToStringSid(pSid,&StringSid);

			if(bRes != 0)
			{
				StringCchCopy(szOwner,BufSize,StringSid);

				LocalFree(StringSid);
				ReturnLength = lstrlen(StringSid);
			}
		}
		else
		{
			StringCchPrintf(szOwner,BufSize,_T("%s\\%s"),szDomainName,szAccountName);

			ReturnLength = lstrlen(szAccountName);
		}

		LocalFree(&pSecurityDescriptor);
		CloseHandle(hFile);
	}

	/* Reset the privilege. */
	SetProcessTokenPrivilege(GetCurrentProcessId(),SE_SECURITY_NAME,FALSE);

	return ReturnLength;
}

BOOL GetProcessOwner(TCHAR *szOwner,DWORD BufSize)
{
	HANDLE hProcess;
	HANDLE hToken;
	TOKEN_USER *pTokenUser = NULL;
	SID_NAME_USE eUse;
	LPTSTR StringSid;
	TCHAR szAccountName[512];
	DWORD dwAccountName = SIZEOF_ARRAY(szAccountName);
	TCHAR szDomainName[512];
	DWORD dwDomainName = SIZEOF_ARRAY(szDomainName);
	DWORD ReturnLength;
	DWORD dwSize = 0;
	BOOL bRes;
	BOOL bReturn = FALSE;

	hProcess = OpenProcess(PROCESS_ALL_ACCESS,FALSE,GetCurrentProcessId());

	if(hProcess != NULL)
	{
		bRes = OpenProcessToken(hProcess,TOKEN_ALL_ACCESS,&hToken);

		if(bRes)
		{
			GetTokenInformation(hToken,TokenUser,NULL,0,&dwSize);

			pTokenUser = (PTOKEN_USER)GlobalAlloc(GMEM_FIXED,dwSize);

			if(pTokenUser != NULL)
			{
				GetTokenInformation(hToken,TokenUser,(LPVOID)pTokenUser,dwSize,&ReturnLength);

				bRes = LookupAccountSid(NULL,pTokenUser->User.Sid,szAccountName,&dwAccountName,
					szDomainName,&dwDomainName,&eUse);

				/* LookupAccountSid failed. */
				if(bRes == 0)
				{
					bRes = ConvertSidToStringSid(pTokenUser->User.Sid,&StringSid);

					if(bRes != 0)
					{
						StringCchCopy(szOwner,BufSize,StringSid);

						LocalFree(StringSid);

						bReturn = TRUE;
					}
				}
				else
				{
					StringCchPrintf(szOwner,BufSize,_T("%s\\%s"),szDomainName,szAccountName);

					bReturn = TRUE;
				}

				GlobalFree(pTokenUser);
			}
		}
		CloseHandle(hProcess);
	}

	if(!bReturn)
		StringCchCopy(szOwner,BufSize,EMPTY_STRING);

	return bReturn;
}

BOOL CheckGroupMembership(GroupType_t GroupType)
{
	SID_IDENTIFIER_AUTHORITY sia = SECURITY_NT_AUTHORITY;
	PSID psid;
	DWORD dwGroup = 0;
	BOOL bMember = FALSE;
	BOOL bRet;

	switch(GroupType)
	{
	case GROUP_ADMINISTRATORS:
		dwGroup = DOMAIN_ALIAS_RID_ADMINS;
		break;

	case GROUP_POWERUSERS:
		dwGroup = DOMAIN_ALIAS_RID_POWER_USERS;
		break;

	case GROUP_USERS:
		dwGroup = DOMAIN_ALIAS_RID_USERS;
		break;

	case GROUP_USERSRESTRICTED:
		dwGroup = DOMAIN_ALIAS_RID_GUESTS;
		break;
	}

	bRet = AllocateAndInitializeSid(&sia,2,SECURITY_BUILTIN_DOMAIN_RID,
		dwGroup,0,0,0,0,0,0,&psid);

	if(bRet)
	{
		CheckTokenMembership(NULL,psid,&bMember);

		FreeSid(psid);
	}

	return bMember;
}

struct LANGCODEPAGE
{
	WORD wLanguage;
	WORD wCodePage;
} *lpTranslate;

BOOL GetVersionInfoString(const TCHAR *szFileName,const TCHAR *szVersionInfo,TCHAR *szBuffer,UINT cbBufLen)
{
	LPVOID lpData;
	TCHAR szSubBlock[64];
	TCHAR *lpszLocalBuf = NULL;
	LANGID UserLangId;
	DWORD dwLen;
	DWORD dwHandle = NULL;
	UINT cbTranslate;
	UINT cbLen;
	BOOL bRet = FALSE;
	unsigned int i = 0;

	dwLen = GetFileVersionInfoSize(szFileName,&dwHandle);

	if(dwLen > 0)
	{
		lpData = malloc(dwLen);

		if(lpData != NULL)
		{
			if(GetFileVersionInfo(szFileName,0,dwLen,lpData) != 0)
			{
				UserLangId = GetUserDefaultLangID();

				VerQueryValue(lpData,_T("\\VarFileInfo\\Translation"),(LPVOID *)&lpTranslate,&cbTranslate);

				for(i = 0;i < (cbTranslate / sizeof(LANGCODEPAGE));i++)
				{
					/* If the bottom eight bits of the language id's match, use this
					version information (since this means that the version information
					and the users default language are the same). Also use this version
					information if the language is not specified (i.e. wLanguage is 0). */
					if((lpTranslate[i].wLanguage & 0xFF) == (UserLangId & 0xFF) ||
						lpTranslate[i].wLanguage == 0)
					{
						StringCchPrintf(szSubBlock,SIZEOF_ARRAY(szSubBlock),
							_T("\\StringFileInfo\\%04X%04X\\%s"),lpTranslate[i].wLanguage,
							lpTranslate[i].wCodePage,szVersionInfo);

						if(VerQueryValue(lpData,szSubBlock,(LPVOID *)&lpszLocalBuf,&cbLen) != 0)
						{
							/* The buffer may be NULL if the specified data was not found
							within the file. */
							if(lpszLocalBuf != NULL)
							{
								StringCchCopy(szBuffer,cbBufLen,lpszLocalBuf);

								bRet = TRUE;
								break;
							}
						}
					}
				}
			}
			free(lpData);
		}
	}

	return bRet;
}

DWORD GetNumFileHardLinks(TCHAR *lpszFileName)
{
	HANDLE hFile;
	BY_HANDLE_FILE_INFORMATION FileInfo;
	BOOL bRes;

	hFile = CreateFile(lpszFileName,FILE_READ_ATTRIBUTES,FILE_SHARE_READ,NULL,
	OPEN_EXISTING,NULL,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return 0;

	bRes = GetFileInformationByHandle(hFile,&FileInfo);

	CloseHandle(hFile);

	if(bRes == 0)
	{
		return 0;
	}

	return FileInfo.nNumberOfLinks;
}

int ReadFileProperty(TCHAR *lpszFileName,DWORD dwPropertyType,TCHAR *lpszPropertyBuf,DWORD dwBufLen)
{
	HANDLE hFile;
	TCHAR szCommentStreamName[512];
	LPCSTR lpszProperty;
	DWORD dwNumBytesRead;
	DWORD dwSectionLength;
	DWORD dwPropertyCount;
	DWORD dwPropertyId;
	DWORD dwPropertyOffset = 0;
	DWORD dwPropertyLength;
	DWORD dwPropertyMarker;
	DWORD dwSectionOffset;
	DWORD dwCodepageOffset = 0;
	UINT uCodepage = CP_ACP;
	BOOL bFound = FALSE;
	unsigned int i = 0;

	StringCchPrintf(szCommentStreamName,SIZEOF_ARRAY(szCommentStreamName),
	_T("%s:%cSummaryInformation"),lpszFileName,0x5);
	hFile = CreateFile(szCommentStreamName,GENERIC_READ,FILE_SHARE_READ,NULL,
	OPEN_EXISTING,NULL,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return -1;

	/* Constant offset. */
	SetFilePointer(hFile,0x2C,0,FILE_CURRENT);

	ReadFile(hFile,(LPBYTE)&dwSectionOffset,sizeof(dwSectionOffset),&dwNumBytesRead,NULL);

	/* The section offset is from the start of the stream. */
	SetFilePointer(hFile,dwSectionOffset,0,FILE_BEGIN);

	/* Since this is the only section, the section length gives the length from the
	start of the section to the end of the stream. The property count gives the
	number of properties associated with this file (author, comments, etc). */
	ReadFile(hFile,(LPBYTE)&dwSectionLength,sizeof(DWORD),&dwNumBytesRead,NULL);
	ReadFile(hFile,(LPBYTE)&dwPropertyCount,sizeof(DWORD),&dwNumBytesRead,NULL);

	for(i = 0;i < dwPropertyCount;i++)
	{
		ReadFile(hFile,(LPBYTE)&dwPropertyId,sizeof(dwPropertyId),&dwNumBytesRead,NULL);

		if(dwPropertyId == 1)
		{
			/* Property id 1 is the codepage that any string properties are encoded in.
			If present, it occurs before the strings to which it refers. */
			ReadFile(hFile,(LPBYTE)&dwCodepageOffset,sizeof(dwCodepageOffset),&dwNumBytesRead,NULL);
		}
		else if(dwPropertyId == dwPropertyType)
		{
			bFound = TRUE;

			/* The offset is from the start of this section. */
			ReadFile(hFile,(LPBYTE)&dwPropertyOffset,sizeof(dwPropertyOffset),&dwNumBytesRead,NULL);
		}
		else
		{
			/* Skip past the offset. */
			SetFilePointer(hFile,0x04,0,FILE_CURRENT);
		}
	}

	if(!bFound)
	{
		CloseHandle(hFile);
		return -1;
	}

	if(dwCodepageOffset)
	{
		SetFilePointer(hFile,dwSectionOffset + dwCodepageOffset + 4,0,FILE_BEGIN);
		ReadFile(hFile,(LPBYTE)&uCodepage,sizeof(uCodepage),&dwNumBytesRead,NULL);
	}

	/* Property offsets are given from the start of the section. */
	SetFilePointer(hFile,dwSectionOffset + dwPropertyOffset,0,FILE_BEGIN);

	/* Read the property marker. If this is not equal to 0x1E, then this is not a valid property
	(1E indicates a NULL terminated string prepended by dword string length). */
	ReadFile(hFile,(LPBYTE)&dwPropertyMarker,sizeof(dwPropertyMarker),&dwNumBytesRead,NULL);

	//if(dwPropertyMarker != 0x1E)
		//return -1;

	/* Read the length of the property (if the property is a string, this length includes the
	NULL byte). */
	ReadFile(hFile,(LPBYTE)&dwPropertyLength,sizeof(dwPropertyLength),&dwNumBytesRead,NULL);

	lpszProperty = (LPCSTR)malloc(dwPropertyLength);

	/* Read out the property of interest. */
	ReadFile(hFile,(LPBYTE)lpszProperty,dwPropertyLength,&dwNumBytesRead,NULL);
	
	if(dwNumBytesRead != dwPropertyLength)
	{
		/* This stream is not valid, and has probably been altered or damaged. */
		CloseHandle(hFile);
		return -1;
	}

	#ifdef UNICODE
	/* Convert the property string from its current codepage to UTF-16, as expected by our caller. */
	MultiByteToWideChar(uCodepage,MB_PRECOMPOSED,
		lpszProperty,dwPropertyLength,
		lpszPropertyBuf,dwBufLen);
	dwPropertyLength = lstrlen(lpszPropertyBuf) + 1;
	#else
	StringCchCopy(lpszPropertyBuf,dwBufLen,lpszProperty);
	#endif

	free((LPVOID)lpszProperty);
	CloseHandle(hFile);

	return dwPropertyLength;
}

typedef struct
{
	DWORD Id;
	DWORD Offset;
} PropertyDeclarations_t;

int SetFileProperty(TCHAR *lpszFileName,DWORD dwPropertyType,TCHAR *szNewValue)
{
	HANDLE hFile;
	PropertyDeclarations_t *pPropertyDeclarations = NULL;
	TCHAR szCommentStreamName[512];
	DWORD dwNumBytesRead;
	DWORD dwSectionLength;
	DWORD dwPropertyCount;
	DWORD dwPropertyMarker;
	DWORD dwSectionOffset;
	DWORD dwNumBytesWritten;
	BOOL bFound = FALSE;
	unsigned int i = 0;
	TCHAR szBuf[512];
	int iPropertyNumber;

	StringCchPrintf(szCommentStreamName,SIZEOF_ARRAY(szCommentStreamName),
	_T("%s:%cSummaryInformation"),lpszFileName,0x5);
	hFile = CreateFile(szCommentStreamName,GENERIC_READ,FILE_SHARE_READ,NULL,
	OPEN_EXISTING,NULL,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return -1;

	/*Constant offset.*/
	SetFilePointer(hFile,0x2C,0,FILE_CURRENT);

	/*Read out the section offset (the SummaryInformation stream only has one offset anyway...).*/
	ReadFile(hFile,(LPBYTE)&dwSectionOffset,sizeof(dwSectionOffset),&dwNumBytesRead,NULL);

	/*The secion offset is from the start of the stream. Go back to the start of the stream,
	and then go to the start of the section.*/
	SetFilePointer(hFile,0,0,FILE_BEGIN);

	/* Read header and section declarations. */
	ReadFile(hFile,(LPBYTE)szBuf,512,&dwNumBytesRead,NULL);

	SetFilePointer(hFile,dwSectionOffset,0,FILE_BEGIN);

	/*Since this is the only section, the section length gives the length from the
	start of the section to the end of the stream. The property count gives the
	number of properties assocciated with this file (author, comments, etc).*/
	ReadFile(hFile,(LPBYTE)&dwSectionLength,sizeof(DWORD),&dwNumBytesRead,NULL);
	ReadFile(hFile,(LPBYTE)&dwPropertyCount,sizeof(DWORD),&dwNumBytesRead,NULL);

	pPropertyDeclarations = (PropertyDeclarations_t *)malloc(dwPropertyCount * sizeof(PropertyDeclarations_t));

	/*Go through each property, try to find the one that was asked for.
	This is in the property declarations part.*/
	for(i = 0;i < dwPropertyCount;i++)
	{
		ReadFile(hFile,(LPBYTE)&pPropertyDeclarations[i],sizeof(PropertyDeclarations_t),&dwNumBytesRead,NULL);

		if(pPropertyDeclarations[i].Id == dwPropertyType)
		{
			bFound = TRUE;
			iPropertyNumber = i;
		}
	}

	if(!bFound)
	{
		free(pPropertyDeclarations);
		CloseHandle(hFile);
		return -1;
	}

	TCHAR szPropertyBuf[512];
	int iCurrentSize;
	int iSizeDifferential;
	ReadFileProperty(lpszFileName,dwPropertyType,szPropertyBuf,512);
	iCurrentSize = lstrlen(szPropertyBuf);

	iSizeDifferential = lstrlen(szNewValue) - iCurrentSize;

	dwSectionLength += iSizeDifferential;

	/* New section length (offset 0x30). */
	SetFilePointer(hFile,dwSectionOffset,0,FILE_BEGIN);
	WriteFile(hFile,(LPCVOID)&dwSectionLength,sizeof(dwSectionLength),&dwNumBytesWritten,NULL);

	/* Beginning of the property declarations table. */
	SetFilePointer(hFile,0x38,0,FILE_BEGIN);

	/* Property declarations after specfied one. */
	SetFilePointer(hFile,(iPropertyNumber + 1) * sizeof(PropertyDeclarations_t),0,FILE_CURRENT);

	for(i = iPropertyNumber + 1;i < dwPropertyCount;i++)
	{
		WriteFile(hFile,(LPCVOID)&pPropertyDeclarations[i],sizeof(PropertyDeclarations_t),&dwNumBytesWritten,NULL);
		pPropertyDeclarations[i].Offset += iSizeDifferential; 
	}

	/*Go back to the start of the offset, then to the property that is desired (property offsets
	are given from the start of the section).*/
	/*SetFilePointer(hFile,0,0,FILE_BEGIN);
	SetFilePointer(hFile,dwSectionOffset,0,FILE_BEGIN);
	SetFilePointer(hFile,dwPropertyOffset,0,FILE_CURRENT);*/

	/*Read the property marker. If this is not equal to 0x1E, then this is not a valid property
	(1E indicates a NULL terminated string prepended by dword string length).*/
	ReadFile(hFile,(LPBYTE)&dwPropertyMarker,sizeof(dwPropertyMarker),&dwNumBytesRead,NULL);
	
	free(pPropertyDeclarations);
	CloseHandle(hFile);

	return 0;
}

BOOL ReadImageProperty(TCHAR *lpszImage,UINT PropertyId,void *pPropBuffer,DWORD dwBufLen)
{
	Gdiplus::GdiplusStartupInput	StartupInput;
	WCHAR				wszImage[MAX_PATH];
	Gdiplus::PropertyItem	*pPropItems = NULL;
	char				pTempBuffer[512];
	ULONG_PTR			Token;
	UINT				Size;
	UINT				NumProperties;
	Gdiplus::Status		res;
	BOOL				bFound = FALSE;
	unsigned int		i = 0;

	GdiplusStartup(&Token,&StartupInput,NULL);

	#ifndef UNICODE
	MultiByteToWideChar(CP_ACP,0,lpszImage,
	-1,wszImage,SIZEOF_ARRAY(wszImage));
	#else
	StringCchCopy(wszImage,SIZEOF_ARRAY(wszImage),lpszImage);
	#endif

	Gdiplus::Image *image = new Gdiplus::Image(wszImage,FALSE);

	if(image->GetLastStatus() != Gdiplus::Ok)
	{
		delete image;
		return FALSE;
	}

	if(PropertyId == PropertyTagImageWidth)
	{
		UINT uWidth;

		uWidth = image->GetWidth();

		bFound = TRUE;

		StringCchPrintf((LPWSTR)pPropBuffer,dwBufLen,L"%u pixels",uWidth);
	}
	else if(PropertyId == PropertyTagImageHeight)
	{
		UINT uHeight;

		uHeight = image->GetHeight();

		bFound = TRUE;

		StringCchPrintf((LPWSTR)pPropBuffer,dwBufLen,L"%u pixels",uHeight);
	}
	else
	{
		image->GetPropertySize(&Size,&NumProperties);

		pPropItems = (Gdiplus::PropertyItem *)malloc(Size);
		res = image->GetAllPropertyItems(Size,NumProperties,pPropItems);

		if(res == Gdiplus::Ok)
		{
			for(i = 0;i < NumProperties;i++)
			{
				if(pPropItems[i].id == PropertyId)
				{
					bFound = TRUE;
					break;
				}
			}
		}

		if(!bFound && (PropertyId == PropertyTagExifDTOrig))
		{
			/* If the specified tag is PropertyTagExifDTOrig, we'll
			transparently fall back on PropertyTagDateTime. */
			for(i = 0;i < NumProperties;i++)
			{
				if(pPropItems[i].id == PropertyTagDateTime)
				{
					bFound = TRUE;
					break;
				}
			}
		}

		if(bFound)
			memcpy(pTempBuffer,pPropItems[i].value,pPropItems[i].length);
		else
			pPropBuffer = NULL;

		/* All property strings are ANSI. */
		#ifndef UNICODE
		StringCchCopy((char *)pPropBuffer,dwBufLen,pTempBuffer);
		#else
		MultiByteToWideChar(CP_ACP,0,pTempBuffer,
			-1,(WCHAR *)pPropBuffer,dwBufLen);
		#endif

		free(pPropItems);
	}

	delete image;
	Gdiplus::GdiplusShutdown(Token);

	return bFound;
}

int DumpSummaryInformationStream(TCHAR *lpszInputFile,TCHAR *lpszOutputFile)
{
	HANDLE hInputFile;
	HANDLE hOutputFile;
	TCHAR szCommentStreamName[512];
	DWORD dwNumBytesRead;
	DWORD NumBytesWritten;
	LPVOID Buffer[512];
	DWORD FileSize;

	StringCchPrintf(szCommentStreamName,SIZEOF_ARRAY(szCommentStreamName),
	_T("%s:%cDocumentSummaryInformation"),lpszInputFile,0x5);

	hInputFile = CreateFile(szCommentStreamName,GENERIC_READ,FILE_SHARE_READ,NULL,
	OPEN_EXISTING,NULL,NULL);

	if(hInputFile == INVALID_HANDLE_VALUE)
		return -1;

	FileSize = GetFileSize(hInputFile,NULL);

	ReadFile(hInputFile,Buffer,FileSize,&dwNumBytesRead,NULL);

	hOutputFile = CreateFile(lpszOutputFile,GENERIC_WRITE,0,NULL,OPEN_EXISTING,
	NULL,NULL);

	WriteFile(hOutputFile,Buffer,dwNumBytesRead,&NumBytesWritten,NULL);

	CloseHandle(hInputFile);
	CloseHandle(hOutputFile);

	return NumBytesWritten;
}

int EnumFileStreams(TCHAR *lpszFileName)
{
	HANDLE hFile;
	WIN32_STREAM_ID sid;
	WCHAR wszStreamName[MAX_PATH];
	LPVOID lpContext = NULL;
	DWORD dwNumBytesRead;
	DWORD dwNumBytesSeekedLow;
	DWORD dwNumBytesSeekedHigh;
	int iNumStreams = 0;

	hFile = CreateFile(lpszFileName,GENERIC_READ,FILE_SHARE_READ,NULL,
	OPEN_EXISTING,NULL,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return -1;

	BackupRead(hFile,(LPBYTE)&sid,20,&dwNumBytesRead,FALSE,FALSE,&lpContext);

	BackupRead(hFile,(LPBYTE)wszStreamName,sid.dwStreamNameSize,
	&dwNumBytesRead,FALSE,FALSE,&lpContext);

	/*Seek to the end of this stream (this is the default data
	stream for the file).*/
	BackupSeek(hFile,sid.Size.LowPart,sid.Size.HighPart,
	&dwNumBytesSeekedLow,&dwNumBytesSeekedHigh,&lpContext);

	BackupRead(hFile,(LPBYTE)&sid,20,&dwNumBytesRead,FALSE,FALSE,&lpContext);
	while(dwNumBytesRead != 0)
	{
		BackupRead(hFile,(LPBYTE)wszStreamName,sid.dwStreamNameSize,
		&dwNumBytesRead,FALSE,FALSE,&lpContext);

		BackupSeek(hFile,sid.Size.LowPart,sid.Size.HighPart,
		&dwNumBytesSeekedLow,&dwNumBytesSeekedHigh,&lpContext);

		if(sid.dwStreamId == BACKUP_ALTERNATE_DATA)
			iNumStreams++;

		BackupRead(hFile,(LPBYTE)&sid,20,&dwNumBytesRead,FALSE,FALSE,&lpContext);
	}
	
	BackupRead(hFile,NULL,0,NULL,TRUE,FALSE,&lpContext);

	CloseHandle(hFile);

	return 1;
}

void WriteFileSlack(TCHAR *szFileName,void *pData,int iDataSize)
{
	HANDLE hFile;
	DWORD FileSize;
	DWORD NumBytesWritten;
	DWORD SectorsPerCluster;
	DWORD BytesPerSector;
	LARGE_INTEGER lRealFileSize;
	TCHAR Root[MAX_PATH];
	LONG FileSectorSize;

	/*The SE_MANAGE_VOLUME_NAME privilege is needed to set
	the valid data length of a file.*/
	if(!SetProcessTokenPrivilege(GetCurrentProcessId(),SE_MANAGE_VOLUME_NAME,TRUE))
		return;

	hFile = CreateFile(szFileName,GENERIC_WRITE,
	FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,
	NULL,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return;

	StringCchCopy(Root,SIZEOF_ARRAY(Root),szFileName);
	PathStripToRoot(Root);

	GetDiskFreeSpace(Root,&SectorsPerCluster,&BytesPerSector,NULL,NULL);

	/*Ask for the files logical size.*/
	FileSize = GetFileSize(hFile,NULL);

	/*Determine the files actual size (i.e. the
	size it physically takes up on disk).*/
	GetRealFileSize(szFileName,&lRealFileSize);

	if((FileSize % GetClusterSize(Root)) == 0)
	{
		/*This file takes up an exact number of clusters,
		thus there is no free space at the end of the file
		to write data.*/
		CloseHandle(hFile);
		return;
	}
	
	FileSectorSize = GetFileSectorSize(szFileName);
	if((FileSectorSize % SectorsPerCluster) == 0)
	{
		/*This file has data in all the sectors of its clusters.
		Data written to the end of a sector in use is wiped when
		the file is shrunk. Therefore, cannot write data to the end
		of this file.*/
		CloseHandle(hFile);
		return;
	}

	/* Extend the file to the physical end of file. */
	SetFilePointerEx(hFile,lRealFileSize,NULL,FILE_BEGIN);
	SetEndOfFile(hFile);
	SetFileValidData(hFile,lRealFileSize.QuadPart);

	/*Move back to the first spare sector.*/
	SetFilePointer(hFile,FileSectorSize * BytesPerSector,NULL,FILE_BEGIN);

	/*Write the data to be hidden into the file.*/
	WriteFile(hFile,pData,iDataSize,&NumBytesWritten,NULL);

	/*The data that was written must be flushed.
	If it is not, the os will not physically
	write the data to disk before the file is
	shrunk.*/
	FlushFileBuffers(hFile);

	/*Now shrink the file back to its original size.*/
	SetFilePointer(hFile,FileSize,NULL,FILE_BEGIN);
	SetEndOfFile(hFile);
	SetFileValidData(hFile,FileSize);

	CloseHandle(hFile);
}

int ReadFileSlack(TCHAR *FileName,TCHAR *pszSlack,int iBufferLen)
{
	HANDLE			hFile;
	DWORD			FileSize;
	DWORD			nBytesRead = 0;
	DWORD			FileSectorSize;
	TCHAR			*pszSlackTemp = NULL;
	DWORD			BytesPerSector;
	LARGE_INTEGER	lRealFileSize;
	TCHAR			Root[MAX_PATH];
	int				SpareSectors;
	BOOL			res;

	/* The SE_MANAGE_VOLUME_NAME privilege is needed to set
	the valid data length of a file. */
	SetProcessTokenPrivilege(GetCurrentProcessId(),SE_MANAGE_VOLUME_NAME,TRUE);

	if(GetLastError() != ERROR_SUCCESS)
		return -1;

	hFile = CreateFile(FileName,GENERIC_READ|GENERIC_WRITE,
	FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,
	NULL,NULL);

	if(hFile == INVALID_HANDLE_VALUE)
		return -1;

	StringCchCopy(Root,SIZEOF_ARRAY(Root),FileName);
	PathStripToRoot(Root);

	res = GetDiskFreeSpace(Root,NULL,&BytesPerSector,NULL,NULL);

	if(res)
	{
		/* Get the file's logical size. */
		FileSize = GetFileSize(hFile,NULL);

		/* Determine the files actual size (i.e. the
		size it physically takes up on disk). */
		GetRealFileSize(FileName,&lRealFileSize);

		if(lRealFileSize.QuadPart > FileSize)
		{
			FileSectorSize = GetFileSectorSize(FileName);

			/* Determine the number of sectors at the end of the file
			that are currently not in use. */
			SpareSectors = (int)((lRealFileSize.QuadPart / BytesPerSector) - FileSectorSize);

			/* Extend the file to the physical end of file. */
			SetFilePointerEx(hFile,lRealFileSize,NULL,FILE_BEGIN);
			SetEndOfFile(hFile);
			SetFileValidData(hFile,lRealFileSize.QuadPart);

			if((FileSectorSize * BytesPerSector) > FileSize)
			{
				/* Move the file pointer back to the logical end of file, so that all data
				after the logical end of file can be read. */
				SetFilePointer(hFile,FileSectorSize * BytesPerSector,NULL,FILE_BEGIN);

				pszSlackTemp = (TCHAR *)malloc(SpareSectors * BytesPerSector);

				if(pszSlackTemp != NULL)
				{
					/* Read out the data contained after the logical end of file. */
					ReadFile(hFile,(LPVOID)pszSlackTemp,SpareSectors * BytesPerSector,&nBytesRead,NULL);

					memcpy_s(pszSlack,iBufferLen,pszSlackTemp,nBytesRead);
				}
			}

			/* Now shrink the file back to its original size. */
			SetFilePointer(hFile,FileSize,NULL,FILE_BEGIN);
			SetEndOfFile(hFile);
			SetFileValidData(hFile,FileSize);
		}
	}

	CloseHandle(hFile);

	return nBytesRead;
}

BOOL GetFileNameFromUser(HWND hwnd,TCHAR *FullFileName,TCHAR *InitialDirectory)
{
	TCHAR *Filter = _T("Text Document (*.txt)\0*.txt\0All Files\0*.*\0\0");
	OPENFILENAME ofn;
	BOOL bRet;

	ofn.lStructSize			= sizeof(ofn);
	ofn.hwndOwner			= hwnd;
	ofn.lpstrFilter			= Filter;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nMaxCustFilter		= 0;
	ofn.nFilterIndex		= 0;
	ofn.lpstrFile			= FullFileName;
	ofn.nMaxFile			= MAX_PATH;
	ofn.lpstrFileTitle		= NULL;
	ofn.nMaxFileTitle		= 0;
	ofn.lpstrInitialDir		= InitialDirectory;
	ofn.lpstrTitle			= NULL;
	ofn.Flags				= OFN_ENABLESIZING|OFN_OVERWRITEPROMPT|OFN_EXPLORER;
	ofn.lpstrDefExt			= _T("txt");
	ofn.lCustData			= NULL;
	ofn.lpfnHook			= NULL;
	ofn.pvReserved			= NULL;
	ofn.dwReserved			= NULL;
	ofn.FlagsEx				= NULL;

	bRet = GetSaveFileName(&ofn);

	return bRet;
}

void TabCtrl_SwapItems(HWND hTabCtrl,int iItem1,int iItem2)
{
	TCITEM tcItem;
	LPARAM lParam1;
	LPARAM lParam2;
	TCHAR szText1[512];
	TCHAR szText2[512];
	int iImage1;
	int iImage2;
	BOOL res;

	tcItem.mask			= TCIF_TEXT|TCIF_PARAM|TCIF_IMAGE;
	tcItem.pszText		= szText1;
	tcItem.cchTextMax	= SIZEOF_ARRAY(szText1);

	res = TabCtrl_GetItem(hTabCtrl,iItem1,&tcItem);

	if(!res)
		return;

	lParam1 = tcItem.lParam;
	iImage1 = tcItem.iImage;

	tcItem.mask			= TCIF_TEXT|TCIF_PARAM|TCIF_IMAGE;
	tcItem.pszText		= szText2;
	tcItem.cchTextMax	= SIZEOF_ARRAY(szText2);

	res = TabCtrl_GetItem(hTabCtrl,iItem2,&tcItem);

	if(!res)
		return;

	lParam2 = tcItem.lParam;
	iImage2 = tcItem.iImage;

	tcItem.mask		= TCIF_TEXT|TCIF_PARAM|TCIF_IMAGE;
	tcItem.pszText	= szText1;
	tcItem.lParam	= lParam1;
	tcItem.iImage	= iImage1;

	TabCtrl_SetItem(hTabCtrl,iItem2,&tcItem);

	tcItem.mask		= TCIF_TEXT|TCIF_PARAM|TCIF_IMAGE;
	tcItem.pszText	= szText2;
	tcItem.lParam	= lParam2;
	tcItem.iImage	= iImage2;

	TabCtrl_SetItem(hTabCtrl,iItem1,&tcItem);
}

void TabCtrl_SetItemText(HWND Tab,int iTab,TCHAR *Text)
{
	TCITEM tcItem;

	if(Text == NULL)
		return;

	tcItem.mask			= TCIF_TEXT;
	tcItem.pszText		= Text;

	SendMessage(Tab,TCM_SETITEM,(WPARAM)(int)iTab,(LPARAM)&tcItem);
}

BOOL CheckWildcardMatch(const TCHAR *szWildcard,const TCHAR *szString,BOOL bCaseSensitive)
{
	/* Handles multiple wildcard patterns. If the wildcard pattern contains ':', 
	split the pattern into multiple subpatterns.
	For example "*.h: *.cpp" would match against "*.h" and "*.cpp" */
	BOOL bMultiplePattern = FALSE;

	for(int i = 0; i < lstrlen(szWildcard); i++)
	{
		if(szWildcard[i] == ':')
		{
			bMultiplePattern = TRUE;
			break;
		}
	}

	if(!bMultiplePattern)
	{
		return CheckWildcardMatchInternal(szWildcard,szString,bCaseSensitive);
	}
	else
	{
		TCHAR szWildcardPattern[512];
		TCHAR *szSinglePattern = NULL;
		TCHAR *szSearchPattern = NULL;
		TCHAR *szRemainingPattern = NULL;

		StringCchCopy(szWildcardPattern,SIZEOF_ARRAY(szWildcardPattern),szWildcard);

		szSinglePattern = cstrtok_s(szWildcardPattern,_T(":"),&szRemainingPattern);
		PathRemoveBlanks(szSinglePattern);

		while(szSinglePattern != NULL)
		{
			if(CheckWildcardMatchInternal(szSinglePattern,szString,bCaseSensitive))
			{
				return TRUE;
			}

			szSearchPattern = szRemainingPattern;
			szSinglePattern = cstrtok_s(szSearchPattern,_T(":"),&szRemainingPattern);
			PathRemoveBlanks(szSinglePattern);
		}
	}

	return FALSE;
}

BOOL CheckWildcardMatchInternal(const TCHAR *szWildcard,const TCHAR *szString,BOOL bCaseSensitive)
{
	BOOL bMatched;
	BOOL bCurrentMatch = TRUE;

	while(*szWildcard != '\0' && *szString != '\0' && bCurrentMatch)
	{
		switch(*szWildcard)
		{
		/* Match against the next part of the wildcard string.
		If there is a match, then return true, else consume
		the next character, and check again. */
		case '*':
			bMatched = FALSE;

			if(*(szWildcard + 1) != '\0')
			{
				bMatched = CheckWildcardMatch(++szWildcard,szString,bCaseSensitive);
			}

			while(*szWildcard != '\0' && *szString != '\0' && !bMatched)
			{
				/* Consume one more character on the input string,
				and keep (recursively) trying to match. */
				bMatched = CheckWildcardMatch(szWildcard,++szString,bCaseSensitive);
			}

			if(bMatched)
			{
				while(*szWildcard != '\0')
					szWildcard++;

				szWildcard--;

				while(*szString != '\0')
					szString++;
			}

			bCurrentMatch = bMatched;
			break;

		case '?':
			szString++;
			break;

		default:
			if(bCaseSensitive)
			{
				bCurrentMatch = (*szWildcard == *szString);
			}
			else
			{
				TCHAR szCharacter1[1];
				LCMapString(LOCALE_USER_DEFAULT,LCMAP_LOWERCASE,szWildcard,1,szCharacter1,SIZEOF_ARRAY(szCharacter1));

				TCHAR szCharacter2[1];
				LCMapString(LOCALE_USER_DEFAULT,LCMAP_LOWERCASE,szString,1,szCharacter2,SIZEOF_ARRAY(szCharacter2));

				bCurrentMatch = (szCharacter1[0] == szCharacter2[0]);
			}

			szString++;
			break;
		}

		szWildcard++;
	}

	/* Skip past any trailing wildcards. */
	while(*szWildcard == '*')
		szWildcard++;

	if(*szWildcard == '\0' && *szString == '\0' && bCurrentMatch)
		return TRUE;

	return FALSE;
}

TCHAR *DecodePrinterStatus(DWORD dwStatus)
{
	if(dwStatus == 0)
		return _T("Ready");
	else if(dwStatus & PRINTER_STATUS_BUSY)
		return _T("Busy");
	else if(dwStatus & PRINTER_STATUS_ERROR)
		return _T("Error");
	else if(dwStatus & PRINTER_STATUS_INITIALIZING)
		return _T("Initializing");
	else if(dwStatus & PRINTER_STATUS_IO_ACTIVE)
		return _T("Active");
	else if(dwStatus & PRINTER_STATUS_NOT_AVAILABLE)
		return _T("Unavailable");
	else if(dwStatus & PRINTER_STATUS_OFFLINE)
		return _T("Offline");
	else if(dwStatus & PRINTER_STATUS_OUT_OF_MEMORY)
		return _T("Out of memory");
	else if(dwStatus & PRINTER_STATUS_NO_TONER)
		return _T("Out of toner");

	return NULL;
}

void RetrieveAdapterInfo(void)
{
	IP_ADAPTER_ADDRESSES *pAdapterAddresses	= NULL;
	ULONG ulOutBufLen						= 0;

	GetAdaptersAddresses(AF_UNSPEC,0,NULL,NULL,&ulOutBufLen);

	pAdapterAddresses = (IP_ADAPTER_ADDRESSES *)malloc(ulOutBufLen);

	if(pAdapterAddresses != NULL)
	{
		GetAdaptersAddresses(AF_UNSPEC,0,NULL,pAdapterAddresses,&ulOutBufLen);

		free(pAdapterAddresses);
	}
}

BOOL IsImage(TCHAR *szFileName)
{
	static TCHAR *ImageExts[10] = {_T("bmp"),_T("ico"),
	_T("gif"),_T("jpg"),_T("exf"),_T("png"),_T("tif"),_T("wmf"),_T("emf"),_T("tiff")};
	TCHAR *ext;
	int i = 0;

	if(szFileName != NULL)
	{
		ext = PathFindExtension(szFileName);

		if(ext == NULL || (ext + 1) == NULL)
			return FALSE;

		ext++;

		for(i = 0;i < 10;i++)
		{
			if(lstrcmpi(ext,ImageExts[i]) == 0)
				return TRUE;
		}
	}

	return FALSE;
}

void ReplaceCharacters(TCHAR *str,char ch,char replacement)
{
	int  i = 0;

	for(i = 0;i < lstrlen(str);i++)
	{
		if(str[i] == ch)
			str[i] = replacement;
	}
}

void ShowLastError(void)
{
	LPVOID ErrorMessage;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|
	FORMAT_MESSAGE_FROM_SYSTEM,NULL,GetLastError(),
	MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),(LPTSTR)&ErrorMessage,
	0,NULL);

	MessageBox(NULL,(LPTSTR)ErrorMessage,EMPTY_STRING,MB_OK|MB_ICONQUESTION);
}

TCHAR *GetToken(TCHAR *ptr,TCHAR *Buffer,TCHAR *BufferLength)
{
	TCHAR *p;
	int i = 0;

	if(ptr == NULL || *ptr == '\0')
	{
		*Buffer = NULL;
		return NULL;
	}

	p = ptr;

	while(*p == ' ' || *p == '\t')
		p++;

	if(*p == '\"')
	{
		p++;
		while(*p != '\0' && *p != '\"')
		{
			Buffer[i++] = *p;
			p++;
		}
		p++;
	}
	else
	{
		while(*p != '\0' && *p != ' ' && *p != '\t')
		{
			Buffer[i++] = *p;
			p++;
		}
	}

	Buffer[i] = '\0';

	while(*p == ' ' || *p == '\t')
		p++;

	return p;
}

HRESULT GetItemInfoTip(const TCHAR *szItemPath,TCHAR *szInfoTip,int cchMax)
{
	LPITEMIDLIST	pidlItem = NULL;
	HRESULT			hr;

	hr = GetIdlFromParsingName(szItemPath,&pidlItem);

	if(SUCCEEDED(hr))
	{
		hr = GetItemInfoTip(pidlItem,szInfoTip,cchMax);

		CoTaskMemFree(pidlItem);
	}

	return hr;
}

HRESULT GetItemInfoTip(LPITEMIDLIST pidlComplete,TCHAR *szInfoTip,int cchMax)
{
	IShellFolder	*pShellFolder = NULL;
	IQueryInfo		*pQueryInfo = NULL;
	LPITEMIDLIST	pidlRelative = NULL;
	LPCWSTR			ppwszTip = NULL;
	HRESULT			hr;

	hr = SHBindToParent(pidlComplete,IID_IShellFolder,(void **)&pShellFolder,(LPCITEMIDLIST *)&pidlRelative);

	if(SUCCEEDED(hr))
	{
		hr = pShellFolder->GetUIObjectOf(NULL,1,(LPCITEMIDLIST *)&pidlRelative,
			IID_IQueryInfo,0,(void **)&pQueryInfo);

		if(SUCCEEDED(hr))
		{
			hr = pQueryInfo->GetInfoTip(QITIPF_USENAME,(WCHAR **)&ppwszTip);

			if(SUCCEEDED(hr))
			{
				#ifndef UNICODE
				WideCharToMultiByte(CP_ACP,0,ppwszTip,-1,szInfoTip,
					cchMax,NULL,NULL);
				#else
				StringCchCopy(szInfoTip,cchMax,ppwszTip);
				#endif
			}

			pQueryInfo->Release();
		}
		pShellFolder->Release();
	}

	return hr;
}

void AddGripperStyle(UINT *fStyle,BOOL bAddGripper)
{
	if(bAddGripper)
	{
		/* Remove the no-gripper style (if present). */
		if((*fStyle & RBBS_NOGRIPPER) == RBBS_NOGRIPPER)
			*fStyle &= ~RBBS_NOGRIPPER;

		/* Only add the gripper style if it isn't already present. */
		if((*fStyle & RBBS_GRIPPERALWAYS) != RBBS_GRIPPERALWAYS)
			*fStyle |= RBBS_GRIPPERALWAYS;
	}
	else
	{
		/* Remove the gripper style (if present). */
		if((*fStyle & RBBS_GRIPPERALWAYS) == RBBS_GRIPPERALWAYS)
			*fStyle &= ~RBBS_GRIPPERALWAYS;

		/* Only add the gripper style if it isn't already present. */
		if((*fStyle & RBBS_NOGRIPPER) != RBBS_NOGRIPPER)
			*fStyle |= RBBS_NOGRIPPER;
	}
}

/* Adds or removes the specified window
style from a window. */
void AddWindowStyle(HWND hwnd,UINT fStyle,BOOL bAdd)
{
	LONG_PTR fCurrentStyle;

	fCurrentStyle = GetWindowLongPtr(hwnd,GWL_STYLE);

	if(bAdd)
	{
		/* Only add the style if it isn't already present. */
		if((fCurrentStyle & fStyle) != fStyle)
			fCurrentStyle |= fStyle;
	}
	else
	{
		/* Only remove the style if it is present. */
		if((fCurrentStyle & fStyle) == fStyle)
			fCurrentStyle &= ~fStyle;
	}

	SetWindowLongPtr(hwnd,GWL_STYLE,fCurrentStyle);
}

DWORD GetCurrentProcessImageName(TCHAR *szImageName,DWORD nSize)
{
	HANDLE	hProcess;
	DWORD	dwProcessId;
	DWORD	dwRet = 0;

	dwProcessId = GetCurrentProcessId();
	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,dwProcessId);

	if(hProcess != NULL)
	{
		dwRet = GetModuleFileNameEx(hProcess,NULL,szImageName,nSize);
		CloseHandle(hProcess);
	}

	return dwRet;
}

WORD GetFileLanguage(TCHAR *szFullFileName)
{
	LANGANDCODEPAGE	*plcp = NULL;
	DWORD			dwLen;
	DWORD			dwHandle;
	WORD			wLanguage = 0;
	UINT			uLen;
	void			*pTranslateInfo = NULL;

	dwLen = GetFileVersionInfoSize(szFullFileName,&dwHandle);

	if(dwLen > 0)
	{
		pTranslateInfo = malloc(dwLen);

		if(pTranslateInfo != NULL)
		{
			GetFileVersionInfo(szFullFileName,NULL,dwLen,pTranslateInfo);
			VerQueryValue(pTranslateInfo,_T("\\VarFileInfo\\Translation"),
				(LPVOID *)&plcp,&uLen);

			if(uLen >= sizeof(LANGANDCODEPAGE))
				wLanguage = PRIMARYLANGID(plcp[0].wLanguage);

			free(pTranslateInfo);
		}
	}

	return wLanguage;
}

BOOL GetFileProductVersion(TCHAR *szFullFileName,
DWORD *pdwProductVersionLS,DWORD *pdwProductVersionMS)
{
	VS_FIXEDFILEINFO	*pvsffi = NULL;
	DWORD			dwLen;
	DWORD			dwHandle;
	UINT			uLen;
	BOOL			bSuccess = FALSE;
	void			*pData = NULL;

	*pdwProductVersionLS = 0;
	*pdwProductVersionMS = 0;

	dwLen = GetFileVersionInfoSize(szFullFileName,&dwHandle);

	if(dwLen > 0)
	{
		pData = malloc(dwLen);

		if(pData != NULL)
		{
			GetFileVersionInfo(szFullFileName,NULL,dwLen,pData);
			VerQueryValue(pData,_T("\\"),
				(LPVOID *)&pvsffi,&uLen);

			/* To retrieve the product version numbers:
			HIWORD(pvsffi->dwProductVersionMS);
			LOWORD(pvsffi->dwProductVersionMS);
			HIWORD(pvsffi->dwProductVersionLS);
			LOWORD(pvsffi->dwProductVersionLS); */

			if(uLen > 0)
			{
				*pdwProductVersionLS = pvsffi->dwProductVersionLS;
				*pdwProductVersionMS = pvsffi->dwProductVersionMS;

				bSuccess = TRUE;
			}

			free(pData);
		}
	}

	return bSuccess;
}

void GetCPUBrandString(char *pszCPUBrand,UINT cchBuf)
{
	int CPUInfo[4] = {-1};
	char szCPUBrand[64];

	/* Refer to cpuid documentation at:
	ms-help://MS.VSCC.v90/MS.MSDNQTR.v90.en/dv_vclang/html/f8c344d3-91bf-405f-8622-cb0e337a6bdc.htm */
	__cpuid(CPUInfo,0x80000002);
	memcpy(szCPUBrand,CPUInfo,sizeof(CPUInfo));
	__cpuid(CPUInfo,0x80000003);
	memcpy(szCPUBrand + 16,CPUInfo,sizeof(CPUInfo));
	__cpuid(CPUInfo,0x80000004);
	memcpy(szCPUBrand + 32,CPUInfo,sizeof(CPUInfo));

	StringCchCopyA(pszCPUBrand,cchBuf,szCPUBrand);
}

void ReplaceCharacterWithString(TCHAR *szBaseString,TCHAR *szOutput,
UINT cchMax,TCHAR chToReplace,TCHAR *szReplacement)
{
	TCHAR szNewString[1024];
	int iBase = 0;
	int i = 0;

	szNewString[0] = '\0';
	for(i = 0;i < lstrlen(szBaseString);i++)
	{
		if(szBaseString[i] == '&')
		{
			StringCchCatN(szNewString,SIZEOF_ARRAY(szNewString),
				&szBaseString[iBase],i - iBase);
			StringCchCat(szNewString,SIZEOF_ARRAY(szNewString),szReplacement);

			iBase = i + 1;
		}
	}

	StringCchCatN(szNewString,SIZEOF_ARRAY(szNewString),
		&szBaseString[iBase],i - iBase);

	StringCchCopy(szOutput,cchMax,szNewString);
}

/* Centers one window (hChild) with respect to
another (hParent), as per the Windows UX
Guidelines (2009).
This means placing the child window 45% of the
way from the top of the parent window (with 55%
of the space left between the bottom of the
child window and the bottom of the parent window).*/
void CenterWindow(HWND hParent,HWND hChild)
{
	RECT rcParent;
	RECT rcChild;
	POINT ptOrigin;

	GetClientRect(hParent,&rcParent);
	GetClientRect(hChild,&rcChild);

	/* Take the offset between the two windows, and map it back to the
	desktop. */
	ptOrigin.x = (GetRectWidth(&rcParent) - GetRectWidth(&rcChild)) / 2;
	ptOrigin.y = (LONG)((GetRectHeight(&rcParent) - GetRectHeight(&rcChild)) * 0.45);
	MapWindowPoints(hParent,HWND_DESKTOP,&ptOrigin,1);

	SetWindowPos(hChild,NULL,ptOrigin.x,ptOrigin.y,
		0,0,SWP_NOSIZE|SWP_SHOWWINDOW|SWP_NOZORDER);
}

TCHAR *ReplaceSubString(TCHAR *szString,TCHAR *szSubString,TCHAR *szReplacement)
{
	static TCHAR szDest[1024];
	TCHAR szTemp[1024];
	TCHAR *pSub = NULL;

	StringCchCopy(szDest,SIZEOF_ARRAY(szDest),szString);

	while((pSub = StrStr(szDest,szSubString)) != NULL)
	{
		StringCchCopy(szTemp,SIZEOF_ARRAY(szTemp),szDest);

		StringCchCopyN(szDest,SIZEOF_ARRAY(szDest),szTemp,
			pSub - szDest);
		StringCchCat(szDest,SIZEOF_ARRAY(szDest),szReplacement);
		StringCchCat(szDest,SIZEOF_ARRAY(szDest),&szTemp[pSub - szDest + lstrlen(szSubString)]);
	}

	return szDest;
}

HRESULT GetMediaMetadata(TCHAR *szFileName,LPCWSTR szAttribute,BYTE **pszOutput)
{
	typedef HRESULT (WINAPI *WMCREATEEDITOR_PROC)(IWMMetadataEditor **);
	WMCREATEEDITOR_PROC pWMCreateEditor = NULL;
	HMODULE hWMVCore;
	IWMMetadataEditor *pEditor = NULL;
	IWMHeaderInfo *pWMHeaderInfo = NULL;
	HRESULT hr = E_FAIL;

	hWMVCore = LoadLibrary(_T("wmvcore.dll"));

	if(hWMVCore != NULL)
	{
		pWMCreateEditor = (WMCREATEEDITOR_PROC)GetProcAddress(hWMVCore,"WMCreateEditor");

		if(pWMCreateEditor != NULL)
		{
			hr = pWMCreateEditor(&pEditor);

			if(SUCCEEDED(hr))
			{
				hr = pEditor->Open(szFileName);

				if(SUCCEEDED(hr))
				{
					hr = pEditor->QueryInterface(IID_IWMHeaderInfo,(void **)&pWMHeaderInfo);

					if(SUCCEEDED(hr))
					{
						WORD wStreamNum;
						WMT_ATTR_DATATYPE Type;
						WORD cbLength;

						/* Any stream. Should be zero for MP3 files. */
						wStreamNum = 0;

						hr = pWMHeaderInfo->GetAttributeByName(&wStreamNum,szAttribute,&Type,NULL,&cbLength);

						if(SUCCEEDED(hr))
						{
							*pszOutput = (BYTE *)malloc(cbLength);

							if(*pszOutput != NULL)
							{
								hr = pWMHeaderInfo->GetAttributeByName(&wStreamNum,szAttribute,&Type,
									*pszOutput,&cbLength);
							}
						}

						pWMHeaderInfo->Release();
					}
				}

				pEditor->Release();
			}
		}

		FreeLibrary(hWMVCore);
	}

	return hr;
}

void UpdateToolbarBandSizing(HWND hRebar,HWND hToolbar)
{
	REBARBANDINFO rbbi;
	SIZE sz;
	int nBands;
	int iBand = -1;
	int i = 0;

	nBands = (int)SendMessage(hRebar,RB_GETBANDCOUNT,0,0);

	for(i = 0;i < nBands;i++)
	{
		rbbi.cbSize	= sizeof(rbbi);
		rbbi.fMask	= RBBIM_CHILD;
		SendMessage(hRebar,RB_GETBANDINFO,i,(LPARAM)&rbbi);

		if(rbbi.hwndChild == hToolbar)
		{
			iBand = i;
			break;
		}
	}

	if(iBand != -1)
	{
		SendMessage(hToolbar,TB_GETMAXSIZE,0,(LPARAM)&sz);

		rbbi.cbSize		= sizeof(rbbi);
		rbbi.fMask		= RBBIM_IDEALSIZE;
		rbbi.cxIdeal	= sz.cx;
		SendMessage(hRebar,RB_SETBANDINFO,iBand,(LPARAM)&rbbi);
	}
}

void MergeDateTime(SYSTEMTIME *pstOutput,SYSTEMTIME *pstDate,SYSTEMTIME *pstTime)
{
	/* Date fields. */
	pstOutput->wYear		= pstDate->wYear;
	pstOutput->wMonth		= pstDate->wMonth;
	pstOutput->wDayOfWeek	= pstDate->wDayOfWeek;
	pstOutput->wDay			= pstDate->wDay;

	/* Time fields. */
	pstOutput->wHour			= pstTime->wHour;
	pstOutput->wMinute			= pstTime->wMinute;
	pstOutput->wSecond			= pstTime->wSecond;
	pstOutput->wMilliseconds	= pstTime->wMilliseconds;
}

void GetWindowString(HWND hwnd,std::wstring &str)
{
	int iLen = GetWindowTextLength(hwnd);

	TCHAR *szTemp = new TCHAR[iLen + 1];
	GetWindowText(hwnd,szTemp,iLen + 1);

	str = szTemp;

	delete[] szTemp;
}

BOOL lCheckDlgButton(HWND hDlg,int ButtonId,BOOL bCheck)
{
	UINT uCheck;

	if(bCheck)
	{
		uCheck = BST_CHECKED;
	}
	else
	{
		uCheck = BST_UNCHECKED;
	}

	return CheckDlgButton(hDlg,ButtonId,uCheck);
}

void TrimStringLeft(std::wstring &str,const std::wstring &strWhitespace)
{
	size_t pos = str.find_first_not_of(strWhitespace);
	str.erase(0,pos);
}

void TrimStringRight(std::wstring &str,const std::wstring &strWhitespace)
{
	size_t pos = str.find_last_not_of(strWhitespace);
	str.erase(pos + 1);
}

void TrimString(std::wstring &str,const std::wstring &strWhitespace)
{
	TrimStringLeft(str,strWhitespace);
	TrimStringRight(str,strWhitespace);
}

void AddStyleToToolbar(UINT *fStyle,UINT fStyleToAdd)
{
	if((*fStyle & fStyleToAdd) != fStyleToAdd)
		*fStyle |= fStyleToAdd;
}
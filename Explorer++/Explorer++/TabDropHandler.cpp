/******************************************************************
 *
 * Project: Explorer++
 * File: TabDropHandler.cpp
 * License: GPL - See COPYING in the top level directory
 *
 * Manages drag and drop for the main tab control.
 *
 * Written by David Erceg
 * www.explorerplusplus.com
 *
 *****************************************************************/

#include "stdafx.h"
#include <list>
#include "TabDropHandler.h"
#include "../Helper/Macros.h"


CTabDropHandler::CTabDropHandler(HWND hTabCtrl,CTabContainer *pTabContainer) :
m_hTabCtrl(hTabCtrl),
m_pTabContainer(pTabContainer),
m_RefCount(1)
{
	SetWindowSubclass(m_hTabCtrl,TabCtrlProcStub,SUBCLASS_ID,reinterpret_cast<DWORD_PTR>(this));

	CoCreateInstance(CLSID_DragDropHelper,NULL,CLSCTX_INPROC_SERVER,
		IID_IDragSourceHelper,reinterpret_cast<LPVOID *>(&m_pDragSourceHelper));

	m_pDragSourceHelper->QueryInterface(IID_IDropTargetHelper,reinterpret_cast<void **>(&m_pDropTargetHelper));
}

CTabDropHandler::~CTabDropHandler()
{
	m_pDropTargetHelper->Release();
	m_pDragSourceHelper->Release();
}

HRESULT __stdcall CTabDropHandler::QueryInterface(REFIID iid,void **ppvObject)
{
	*ppvObject = NULL;

	if(iid == IID_IUnknown)
	{
		*ppvObject = static_cast<IUnknown *>(this);
	}
	else if(iid == IID_IDropTarget)
	{
		*ppvObject = static_cast<IDropTarget *>(this);
	}

	if(*ppvObject)
	{
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG __stdcall CTabDropHandler::AddRef(void)
{
	return ++m_RefCount;
}

ULONG __stdcall CTabDropHandler::Release(void)
{
	m_RefCount--;
	
	if(m_RefCount == 0)
	{
		delete this;
		return 0;
	}

	return m_RefCount;
}

LRESULT CALLBACK TabCtrlProcStub(HWND hwnd,UINT uMsg,
	WPARAM wParam,LPARAM lParam,UINT_PTR uIdSubclass,DWORD_PTR dwRefData)
{
	CTabDropHandler *ptdh = reinterpret_cast<CTabDropHandler *>(dwRefData);

	return ptdh->TabCtrlProc(hwnd,uMsg,wParam,lParam);
}

LRESULT CALLBACK CTabDropHandler::TabCtrlProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_TIMER:
		if(wParam == TIMER_ID)
		{
			m_pTabContainer->SetSelection(m_TabHoverIndex);

			return 0;
		}
		break;

	case WM_NCDESTROY:
		RemoveWindowSubclass(hwnd,TabCtrlProcStub,SUBCLASS_ID);
		break;
	}

	return DefSubclassProc(hwnd,uMsg,wParam,lParam);
}

HRESULT __stdcall CTabDropHandler::DragEnter(IDataObject *pDataObject,DWORD grfKeyState,POINTL pt,DWORD *pdwEffect)
{
	m_AcceptData = false;
	m_TabHoverIndex = m_pTabContainer->GetSelection();

	std::list<FORMATETC> ftcList;
	CDropHandler::GetDropFormats(ftcList);

	for each(auto ftc in ftcList)
	{
		if(pDataObject->QueryGetData(&ftc) == S_OK)
		{
			m_AcceptData = true;
			GetRepresentativeSourceDrive(pDataObject,ftc.cfFormat);

			break;
		}
	}

	TCHITTESTINFO tchti;
	tchti.pt.x = pt.x;
	tchti.pt.y = pt.y;
	ScreenToClient(m_hTabCtrl,&tchti.pt);
	int iTab = TabCtrl_HitTest(m_hTabCtrl,&tchti);
	*pdwEffect = DetermineCurrentDragEffect(iTab,grfKeyState,*pdwEffect);

	if(grfKeyState & MK_LBUTTON)
	{
		m_DragType = DRAG_TYPE_LEFTCLICK;
	}
	else
	{
		m_DragType = DRAG_TYPE_RIGHTCLICK;
	}

	m_pDropTargetHelper->DragEnter(m_hTabCtrl,pDataObject,reinterpret_cast<POINT *>(&pt),*pdwEffect);

	return S_OK;
}

void CTabDropHandler::GetRepresentativeSourceDrive(IDataObject *pDataObject,CLIPFORMAT Format)
{
	switch(Format)
	{
	case CF_HDROP:
		GetRepresentativeSourceDriveHDrop(pDataObject);
		break;

	default:
		m_RepresentativeDrive = EMPTY_STRING;
		break;
	}
}

void CTabDropHandler::GetRepresentativeSourceDriveHDrop(IDataObject *pDataObject)
{
	FORMATETC ftc = {CF_HDROP,NULL,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};
	STGMEDIUM stg;

	HRESULT hr = pDataObject->GetData(&ftc,&stg);

	if(hr == S_OK)
	{
		DROPFILES *pdf = reinterpret_cast<DROPFILES *>(GlobalLock(stg.hGlobal));

		if(pdf != NULL)
		{
			int nDroppedFiles = DragQueryFile(reinterpret_cast<HDROP>(pdf),0xFFFFFFFF,NULL,NULL);

			if(nDroppedFiles >= 1)
			{
				TCHAR szFileName[MAX_PATH];
				DragQueryFile(reinterpret_cast<HDROP>(pdf),0,szFileName,
					SIZEOF_ARRAY(szFileName));

				PathStripToRoot(szFileName);
				m_RepresentativeDrive = szFileName;
			}

			GlobalUnlock(stg.hGlobal);
		}

		ReleaseStgMedium(&stg);
	}
}

DWORD CTabDropHandler::DetermineCurrentDragEffect(int iTab,DWORD grfKeyState,DWORD CurrentDropEffect)
{
	DWORD DropEffect = DROPEFFECT_NONE;

	if(iTab != -1)
	{
		if(m_pTabContainer->GetBrowserForTab(iTab)->CanCreate())
		{
			TCHAR szDestDirectory[MAX_PATH];
			m_pTabContainer->GetBrowserForTab(iTab)->QueryCurrentDirectory(SIZEOF_ARRAY(szDestDirectory),szDestDirectory);

			BOOL bOnSameDrive = PathIsSameRoot(szDestDirectory,m_RepresentativeDrive.c_str());
			DropEffect = ::DetermineCurrentDragEffect(grfKeyState,CurrentDropEffect,m_AcceptData,bOnSameDrive);
		}
	}

	return DropEffect;
}

HRESULT __stdcall CTabDropHandler::DragOver(DWORD grfKeyState,POINTL pt,DWORD *pdwEffect)
{
	TCHITTESTINFO tchti;
	tchti.pt.x = pt.x;
	tchti.pt.y = pt.y;
	ScreenToClient(m_hTabCtrl,&tchti.pt);
	int iTab = TabCtrl_HitTest(m_hTabCtrl,&tchti);

	/* Set a timer. If the mouse does not move
	outside this tab before the timer expires,
	and the item is still been dragged, switch
	focus to this tab. */
	if(iTab != -1 &&
		iTab != m_pTabContainer->GetSelection() &&
		iTab != m_TabHoverIndex)
	{
		SetTimer(m_hTabCtrl,TIMER_ID,TIMEOUT_VALUE,NULL);
	}
	else if(iTab == -1 ||
		iTab != m_TabHoverIndex)
	{
		KillTimer(m_hTabCtrl,TIMER_ID);
	}

	m_TabHoverIndex = iTab;

	*pdwEffect = DetermineCurrentDragEffect(iTab,grfKeyState,*pdwEffect);

	m_pDropTargetHelper->DragOver(reinterpret_cast<POINT *>(&pt),*pdwEffect);

	return S_OK;
}

HRESULT __stdcall CTabDropHandler::DragLeave(void)
{
	KillTimer(m_hTabCtrl,TIMER_ID);
	m_pDropTargetHelper->DragLeave();

	return S_OK;
}

HRESULT __stdcall CTabDropHandler::Drop(IDataObject *pDataObject,DWORD grfKeyState,POINTL pt,DWORD *pdwEffect)
{
	KillTimer(m_hTabCtrl,TIMER_ID);

	TCHITTESTINFO tchti;
	tchti.pt.x = pt.x;
	tchti.pt.y = pt.y;
	ScreenToClient(m_hTabCtrl,&tchti.pt);
	int iTab = TabCtrl_HitTest(m_hTabCtrl,&tchti);

	if(iTab != -1 &&
		m_AcceptData)
	{
		TCHAR szDestDirectory[MAX_PATH];
		m_pTabContainer->GetBrowserForTab(iTab)->QueryCurrentDirectory(
			SIZEOF_ARRAY(szDestDirectory),szDestDirectory);

		CDropHandler *pDropHandler = CDropHandler::CreateNew();
		pDropHandler->Drop(pDataObject,grfKeyState,pt,pdwEffect,m_hTabCtrl,
			m_DragType,szDestDirectory,NULL,FALSE);
		pDropHandler->Release();
	}

	m_pDropTargetHelper->Drop(pDataObject,reinterpret_cast<POINT *>(&pt),*pdwEffect);

	return S_OK;
}
#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "UnsharpMaskPanelCtl.h"
#include "UnsharpMaskPanel.h"
#include "NavigationPanelCtl.h"
#include "SettingsProvider.h"
#include "NLS.h"

CUnsharpMaskPanelCtl::CUnsharpMaskPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) : CPanelController(pMainDlg, true) {
	m_bVisible = false;
	m_bOldShowNavPanel = false;
	m_dAlternateUSMAmount = 0;
	m_pUnsharpMaskParams = new CUnsharpMaskParams(CSettingsProvider::This().UnsharpMaskParams());
	m_pPanel = m_pUnsharpMaskPanel = new CUnsharpMaskPanel(pMainDlg->GetHWND(), this, pImageProcPanel, m_pUnsharpMaskParams);
	m_pUnsharpMaskPanel->GetBtnCancel()->SetButtonPressedHandler(&OnCancelUnsharpMask, this);
	m_pUnsharpMaskPanel->GetBtnApply()->SetButtonPressedHandler(&OnApplyUnsharpMask, this);
}

CUnsharpMaskPanelCtl::~CUnsharpMaskPanelCtl() { 
	delete m_pUnsharpMaskPanel;
	m_pUnsharpMaskPanel = NULL;
	delete m_pUnsharpMaskParams;
	m_pUnsharpMaskParams = NULL;
}

bool CUnsharpMaskPanelCtl::IsVisible() {
	return m_bVisible;
}

bool CUnsharpMaskPanelCtl::IsActive() {
	return m_bVisible;
}

void CUnsharpMaskPanelCtl::SetVisible(bool bVisible) {
	if (bVisible != m_bVisible) {
		if (bVisible) {
			StartUnsharpMaskPanel();
		} else {
			TerminateUnsharpMaskPanel();
		}
	}
}

void CUnsharpMaskPanelCtl::SetActive(bool bActive) {
	SetVisible(bActive);
}

void* CUnsharpMaskPanelCtl::GetUSMDIBForPreview(CSize clippingSize, CPoint offset,
												const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {
	return CurrentImage()->GetDIBUnsharpMasked(clippingSize, offset, imageProcParams, eProcFlags, *m_pUnsharpMaskParams);
}

bool CUnsharpMaskPanelCtl::OnKeyDown(unsigned int nVirtualKey, bool bShift, bool bAlt, bool bCtrl) {
	if (nVirtualKey == VK_ESCAPE) {
		TerminateUnsharpMaskPanel();
		return true;
	}
	if (nVirtualKey == 'A' && bCtrl) {
		double dTemp = m_dAlternateUSMAmount;
		m_dAlternateUSMAmount = m_pUnsharpMaskParams->Amount;
		m_pUnsharpMaskParams->Amount = dTemp;
		InvalidateMainDlg();
		return true;
	}
	return true; // all other keys are eaten up in this panel without doing anything
}

void CUnsharpMaskPanelCtl::StartUnsharpMaskPanel() {
	if (CurrentImage() == NULL) {
		return;
	}
	m_pMainDlg->ResetZoomTo100Percents(false);
	m_bVisible = true;
	m_bOldShowNavPanel = m_pMainDlg->PrepareForModalPanel();
	InvalidateMainDlg();
}

void CUnsharpMaskPanelCtl::TerminateUnsharpMaskPanel() {
	m_bVisible = false;
	m_pMainDlg->GetNavPanelCtl()->SetActive(m_bOldShowNavPanel);
	CurrentImage()->FreeUnsharpMaskResources();
	InvalidateMainDlg();
}

void CUnsharpMaskPanelCtl::OnCancelUnsharpMask(void* pContext, int nParameter, CButtonCtrl & sender) {
	((CUnsharpMaskPanelCtl*)pContext)->TerminateUnsharpMaskPanel();
}

void CUnsharpMaskPanelCtl::OnApplyUnsharpMask(void* pContext, int nParameter, CButtonCtrl & sender) {
	CUnsharpMaskPanelCtl* pThis = (CUnsharpMaskPanelCtl*)pContext;
	HCURSOR hOldCursor = ::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_WAIT)));
	if (!pThis->CurrentImage()->ApplyUnsharpMaskToOriginalPixels(*(pThis->m_pUnsharpMaskParams))) {
		::MessageBox(pThis->m_pMainDlg->GetHWND(), CNLS::GetString(_T("The operation failed because not enough memory is available!")),
			_T("JPEGView"), MB_ICONSTOP | MB_OK);
	}
	::SetCursor(hOldCursor);
	pThis->TerminateUnsharpMaskPanel();
}
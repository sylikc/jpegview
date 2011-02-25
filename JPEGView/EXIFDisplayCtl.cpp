#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "EXIFDisplayCtl.h"
#include "EXIFDisplay.h"
#include "SettingsProvider.h"
#include "NLS.h"

CEXIFDisplayCtl::CEXIFDisplayCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) : CPanelController(pMainDlg, false) {
	m_bVisible = CSettingsProvider::This().ShowFileInfo();
	m_pImageProcPanel = pImageProcPanel;
	m_pPanel = m_pEXIFDisplay = new CEXIFDisplay(pMainDlg->m_hWnd, this);
	m_pEXIFDisplay->GetControl<CButtonCtrl*>(CEXIFDisplay::ID_btnShowHideHistogram)->SetButtonPressedHandler(&OnShowHistogram, this);
	m_pEXIFDisplay->SetShowHistogram(CSettingsProvider::This().ShowHistogram());
}

CEXIFDisplayCtl::~CEXIFDisplayCtl() {
	delete m_pEXIFDisplay;
	m_pEXIFDisplay = NULL;
}

bool CEXIFDisplayCtl::IsVisible() { 
	return CurrentImage() != NULL && m_bVisible; 
}

void CEXIFDisplayCtl::SetVisible(bool bVisible) {
	if (m_bVisible != bVisible) {
		m_bVisible = bVisible;
		InvalidateMainDlg();
	}
}

void CEXIFDisplayCtl::SetActive(bool bActive) {
	SetVisible(bActive);
}

void CEXIFDisplayCtl::AfterNewImageLoaded() {
	m_pEXIFDisplay->ClearTexts();
	m_pEXIFDisplay->SetHistogram(NULL);
}

void CEXIFDisplayCtl::OnPrePaintMainDlg(HDC hPaintDC) {
	m_pEXIFDisplay->SetPosition(CPoint(m_pImageProcPanel->PanelRect().left, m_pMainDlg->IsShowFileName() ? 32 : 0));
	FillEXIFDataDisplay();
	if (CurrentImage() != NULL && m_pEXIFDisplay->GetShowHistogram()) {
		m_pEXIFDisplay->SetHistogram(CurrentImage()->GetProcessedHistogram());
	}
}

void CEXIFDisplayCtl::FillEXIFDataDisplay() {
	m_pEXIFDisplay->ClearTexts();

	m_pEXIFDisplay->SetHistogram(NULL);

	CString sFileTitle;
	LPCTSTR sCurrentFileName = m_pMainDlg->CurrentFileName(true);
	const CFileList* pFileList = m_pMainDlg->GetFileList();
	if (CurrentImage()->IsClipboardImage()) {
		sFileTitle = sCurrentFileName;
	} else if (pFileList->Current() != NULL) {
		sFileTitle.Format(_T("[%d/%d]  %s"), pFileList->CurrentIndex() + 1, pFileList->Size(), sCurrentFileName);
	}
	LPCTSTR sComment = NULL;
	m_pEXIFDisplay->AddTitle(sFileTitle);
	m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Image width:")), CurrentImage()->OrigWidth());
	m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Image height:")), CurrentImage()->OrigHeight());
	if (!CurrentImage()->IsClipboardImage()) {
		CEXIFReader* pEXIFReader = CurrentImage()->GetEXIFReader();
		if (pEXIFReader != NULL) {
			sComment = pEXIFReader->GetUserComment();
			if (sComment == NULL || sComment[0] == 0) {
				sComment = pEXIFReader->GetImageDescription();
			}
			if (pEXIFReader->GetAcquisitionTimePresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Acquisition date:")), pEXIFReader->GetAcquisitionTime());
			} else {
				const FILETIME* pFileTime = pFileList->CurrentModificationTime();
				if (pFileTime != NULL) {
					m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Modification date:")), *pFileTime);
				}
			}
			if (pEXIFReader->GetCameraModelPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Camera model:")), pEXIFReader->GetCameraModel());
			}
			if (pEXIFReader->GetExposureTimePresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Exposure time (s):")), pEXIFReader->GetExposureTime());
			}
			if (pEXIFReader->GetExposureBiasPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Exposure bias (EV):")), pEXIFReader->GetExposureBias(), 2);
			}
			if (pEXIFReader->GetFlashFiredPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Flash fired:")), pEXIFReader->GetFlashFired() ? CNLS::GetString(_T("yes")) : CNLS::GetString(_T("no")));
			}
			if (pEXIFReader->GetFocalLengthPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Focal length (mm):")), pEXIFReader->GetFocalLength(), 1);
			}
			if (pEXIFReader->GetFNumberPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("F-Number:")), pEXIFReader->GetFNumber(), 1);
			}
			if (pEXIFReader->GetISOSpeedPresent()) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("ISO Speed:")), (int)pEXIFReader->GetISOSpeed());
			}
		} else {
			const FILETIME* pFileTime = pFileList->CurrentModificationTime();
			if (pFileTime != NULL) {
				m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Modification date:")), *pFileTime);
			}
		}
	}

	if (sComment == NULL || sComment[0] == 0) {
		sComment = CurrentImage()->GetJPEGComment();
	}
	if (CSettingsProvider::This().ShowJPEGComments() && sComment != NULL && sComment[0] != 0) {
		m_pEXIFDisplay->SetComment(sComment);
	}
}

void CEXIFDisplayCtl::OnShowHistogram(void* pContext, int nParameter, CButtonCtrl & sender) {
	CEXIFDisplayCtl* pThis = (CEXIFDisplayCtl*)pContext;
	pThis->m_pEXIFDisplay->SetShowHistogram(!pThis->m_pEXIFDisplay->GetShowHistogram());
	pThis->m_pEXIFDisplay->RequestRepositioning();
	pThis->InvalidateMainDlg();
}
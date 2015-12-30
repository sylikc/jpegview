#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "EXIFDisplayCtl.h"
#include "EXIFDisplay.h"
#include "RawMetadata.h"
#include "SettingsProvider.h"
#include "HelpersGUI.h"
#include "NLS.h"

static int GetFileNameHeight(HDC dc) {
	CSize size;
	HelpersGUI::SelectDefaultFileNameFont(dc);
	::GetTextExtentPoint32(dc, _T("("), 1, &size);
	return size.cy;
}

CEXIFDisplayCtl::CEXIFDisplayCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) : CPanelController(pMainDlg, false) {
	m_bVisible = CSettingsProvider::This().ShowFileInfo();
	m_nFileNameHeight = 0;
	m_pImageProcPanel = pImageProcPanel;
	m_pPanel = m_pEXIFDisplay = new CEXIFDisplay(pMainDlg->m_hWnd, this);
	m_pEXIFDisplay->GetControl<CButtonCtrl*>(CEXIFDisplay::ID_btnShowHideHistogram)->SetButtonPressedHandler(&OnShowHistogram, this);
    CButtonCtrl* pCloseBtn = m_pEXIFDisplay->GetControl<CButtonCtrl*>(CEXIFDisplay::ID_btnClose);
    pCloseBtn->SetButtonPressedHandler(&OnClose, this);
    pCloseBtn->SetShow(false);
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
	if (m_pMainDlg->IsShowFileName() && m_nFileNameHeight == 0) {
		m_nFileNameHeight = GetFileNameHeight(hPaintDC);
	}
	m_pEXIFDisplay->SetPosition(CPoint(m_pImageProcPanel->PanelRect().left, m_pMainDlg->IsShowFileName() ? m_nFileNameHeight + 6 : 0));
	FillEXIFDataDisplay();
	if (CurrentImage() != NULL && m_pEXIFDisplay->GetShowHistogram()) {
		m_pEXIFDisplay->SetHistogram(CurrentImage()->GetProcessedHistogram());
	}
}

void CEXIFDisplayCtl::FillEXIFDataDisplay() {
	m_pEXIFDisplay->ClearTexts();

	m_pEXIFDisplay->SetHistogram(NULL);

	CString sPrefix, sFileTitle;
	LPCTSTR sCurrentFileName = m_pMainDlg->CurrentFileName(true);
	const CFileList* pFileList = m_pMainDlg->GetFileList();
	if (CurrentImage()->IsClipboardImage()) {
		sPrefix = sCurrentFileName;
	} else if (pFileList->Current() != NULL) {
		sPrefix.Format(_T("[%d/%d]"), pFileList->CurrentIndex() + 1, pFileList->Size());
		sFileTitle = sCurrentFileName;
        sFileTitle += Helpers::GetMultiframeIndex(m_pMainDlg->GetCurrentImage());
	}
	LPCTSTR sComment = NULL;
	m_pEXIFDisplay->AddPrefix(sPrefix);
	m_pEXIFDisplay->AddTitle(sFileTitle);
	m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Image width:")), CurrentImage()->OrigWidth());
	m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Image height:")), CurrentImage()->OrigHeight());
	if (!CurrentImage()->IsClipboardImage()) {
		CEXIFReader* pEXIFReader = CurrentImage()->GetEXIFReader();
        CRawMetadata* pRawMetaData = CurrentImage()->GetRawMetadata();
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
        }
        else if (pRawMetaData != NULL) {
            if (pRawMetaData->GetAcquisitionTime().wYear > 1985) {
                m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Acquisition date:")), pRawMetaData->GetAcquisitionTime());
            }
            else {
                const FILETIME* pFileTime = pFileList->CurrentModificationTime();
                if (pFileTime != NULL) {
                    m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Modification date:")), *pFileTime);
                }
            }
            if (pRawMetaData->GetManufacturer()[0] != 0) {
                m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Camera model:")), CString(pRawMetaData->GetManufacturer()) + _T(" ") + pRawMetaData->GetModel());
            }
            if (pRawMetaData->GetExposureTime() > 0.0) {
                double exposureTime = pRawMetaData->GetExposureTime();
                Rational rational = (exposureTime < 1.0) ? Rational(1, Helpers::RoundToInt(1.0 / exposureTime)) : Rational(Helpers::RoundToInt(exposureTime), 1);
                m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Exposure time (s):")), rational);
            }
            if (pRawMetaData->IsFlashFired()) {
                m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Flash fired:")), CNLS::GetString(_T("yes")));
            }
            if (pRawMetaData->GetFocalLength() > 0.0) {
                m_pEXIFDisplay->AddLine(CNLS::GetString(_T("Focal length (mm):")), pRawMetaData->GetFocalLength(), 1);
            }
            if (pRawMetaData->GetAperture() > 0.0) {
                m_pEXIFDisplay->AddLine(CNLS::GetString(_T("F-Number:")), pRawMetaData->GetAperture(), 1);
            }
            if (pRawMetaData->GetIsoSpeed() > 0.0) {
                m_pEXIFDisplay->AddLine(CNLS::GetString(_T("ISO Speed:")), (int)pRawMetaData->GetIsoSpeed());
            }
        }
        else {
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

bool CEXIFDisplayCtl::OnMouseMove(int nX, int nY) {
    bool bHandled = CPanelController::OnMouseMove(nX, nY);
    bool bMouseOver = m_pEXIFDisplay->PanelRect().PtInRect(CPoint(nX, nY));
    m_pEXIFDisplay->GetControl<CButtonCtrl*>(CEXIFDisplay::ID_btnClose)->SetShow(bMouseOver);
    return bHandled;
}

void CEXIFDisplayCtl::OnShowHistogram(void* pContext, int nParameter, CButtonCtrl & sender) {
	CEXIFDisplayCtl* pThis = (CEXIFDisplayCtl*)pContext;
	pThis->m_pEXIFDisplay->SetShowHistogram(!pThis->m_pEXIFDisplay->GetShowHistogram());
	pThis->m_pEXIFDisplay->RequestRepositioning();
	pThis->InvalidateMainDlg();
}

void CEXIFDisplayCtl::OnClose(void* pContext, int nParameter, CButtonCtrl & sender) {
    CEXIFDisplayCtl* pThis = (CEXIFDisplayCtl*)pContext;
    pThis->m_pMainDlg->ExecuteCommand(IDM_SHOW_FILEINFO);
}
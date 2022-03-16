#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "JPEGProvider.h"
#include "PanelMgr.h"
#include "ImageProcPanelCtl.h"
#include "ImageProcessingPanel.h"
#include "NavigationPanel.h"
#include "ProcessParams.h"
#include "TimerEventIDs.h"
#include "FileList.h"
#include "EXIFReader.h"
#include "EXIFDisplayCtl.h"
#include "UnsharpMaskPanelCtl.h"
#include "NavigationPanelCtl.h"
#include "SettingsProvider.h"
#include "HelpersGUI.h"
#include "Helpers.h"
#include "NLS.h"

CImageProcPanelCtl::CImageProcPanelCtl(CMainDlg* pMainDlg, CImageProcessingParams* pParams, bool* pEnableLDC, bool* pEnableContrastCorr) 
: CPanelController(pMainDlg, false) {
	m_bEnabled = CSettingsProvider::This().ShowBottomPanel();
	m_bVisible = false;
	m_nOldMouseY = 0;
	m_pPanel = m_pImageProcPanel = new CImageProcessingPanel(pMainDlg->GetHWND(), this, pParams, pEnableLDC, pEnableContrastCorr);
	m_pImageProcPanel->GetBtnUnsharpMask()->SetButtonPressedHandler(&OnUnsharpMask, this);
	m_pImageProcPanel->GetBtnSaveTo()->SetButtonPressedHandler(&(CMainDlg::OnExecuteCommand), pMainDlg, IDM_SAVE_PARAM_DB);
	m_pImageProcPanel->GetBtnRemoveFrom()->SetButtonPressedHandler(&(CMainDlg::OnExecuteCommand), pMainDlg, IDM_CLEAR_PARAM_DB);
	m_pImageProcPanel->GetTextFilename()->SetTextChangedHandler(&OnRenameFile, this);
}

CImageProcPanelCtl::~CImageProcPanelCtl() {
	delete m_pImageProcPanel;
	m_pImageProcPanel = NULL;
}

bool CImageProcPanelCtl::CanMakeVisible() {
	if (m_pMainDlg->ClientRect().Width() < HelpersGUI::ScaleToScreen(800) || m_pMainDlg->GetPanelMgr()->IsModalPanelShown()) {
		return false;
	}
	return true;
}

void CImageProcPanelCtl::SetVisible(bool bVisible) {
	if (m_bVisible != bVisible) {
		m_bVisible = bVisible;
		if (m_bEnabled) {
			m_pMainDlg->InvalidateRect(PanelRect(), FALSE);
			m_pMainDlg->InvalidateRect(m_pMainDlg->GetNavPanelCtl()->PanelRect(), FALSE);
			if (!bVisible) {
				m_pImageProcPanel->GetTooltipMgr().RemoveActiveTooltip();
			}
			if (bVisible) {
				m_pMainDlg->GetNavPanelCtl()->StartNavPanelAnimation(true, true);
			} else {
				m_pMainDlg->GetNavPanelCtl()->StartNavPanelAnimation(false, true);
			}
		}
	}
}

void CImageProcPanelCtl::AfterNewImageLoaded() {
	ShowHideSaveDBButtons();
	m_pImageProcPanel->GetBtnUnsharpMask()->SetShow(CurrentImage() != NULL);
	if (CurrentImage() != NULL && !CurrentImage()->IsClipboardImage() && !m_pMainDlg->IsInMovieMode()) {
		m_pImageProcPanel->GetTextParamDB()->SetShow(true);
		m_pImageProcPanel->GetTextRename()->SetShow(true);
		LPCTSTR sCurrentFileTitle = m_pMainDlg->GetFileList()->CurrentFileTitle();
		if (sCurrentFileTitle != NULL) {
			m_pImageProcPanel->GetTextFilename()->SetText(sCurrentFileTitle);
		} else {
			m_pImageProcPanel->GetTextFilename()->SetText(_T(""));
		}
		m_pImageProcPanel->GetTextFilename()->SetEditable(!m_pMainDlg->GetFileList()->IsSlideShowList());
		CEXIFReader* pEXIF = CurrentImage()->GetEXIFReader();
		if (pEXIF != NULL && pEXIF->GetAcquisitionTime().wYear > 1600) {
			m_pImageProcPanel->GetTextAcqDate()->SetText(CString(_T("* ")) + Helpers::SystemTimeToString(pEXIF->GetAcquisitionTime()));
		} else {
			m_pImageProcPanel->GetTextAcqDate()->SetText(_T(""));
		}
	} else {
		m_pImageProcPanel->GetTextAcqDate()->SetText(_T(""));
		m_pImageProcPanel->GetTextFilename()->SetText(_T(""));
		m_pImageProcPanel->GetTextFilename()->SetEditable(false);
		m_pImageProcPanel->GetTextParamDB()->SetShow(false);
		m_pImageProcPanel->GetTextRename()->SetShow(false);
	}
}

bool CImageProcPanelCtl::OnTimer(int nTimerId) {
	if (nTimerId == IPPANEL_TIMER_EVENT_ID) {
		CPoint mousePos;
		::GetCursorPos(&mousePos);
		::ScreenToClient(m_pMainDlg->GetHWND(), &mousePos);
		::KillTimer(m_pMainDlg->GetHWND(), IPPANEL_TIMER_EVENT_ID);
		if (mousePos.y > m_pMainDlg->ClientRect().bottom - m_pImageProcPanel->SliderAreaHeight() && mousePos.y < m_pMainDlg->ClientRect().Height() - 1) {
			SetVisible(true);
		}
		return true;
	}
	return false;
}

bool CImageProcPanelCtl::OnMouseMove(int nX, int nY) {
	if (!m_bEnabled) {
		return false;
	}
	if (!CanMakeVisible()) {
		SetVisible(false);
		return false;
	}

	bool bHandled = false;
	if (!m_bVisible) {
		int nIPAreaStart= m_pMainDlg->ClientRect().bottom - m_pImageProcPanel->SliderAreaHeight();
		if (m_pMainDlg->GetNavPanelCtl()->IsActive()) {
			CRect rectNavPanel = m_pMainDlg->GetNavPanelCtl()->PanelRect();
			if (nX > rectNavPanel.left && nX < rectNavPanel.right) {
				nIPAreaStart += m_pImageProcPanel->SliderAreaHeight() / 2;
			}
		}
		if (!m_pMainDlg->IsFullScreenMode()) {
			::KillTimer(m_pMainDlg->GetHWND(), IPPANEL_TIMER_EVENT_ID);
		}
		if (nY > nIPAreaStart) {
			if (!m_pMainDlg->IsFullScreenMode()) {
				::SetTimer(m_pMainDlg->GetHWND(), IPPANEL_TIMER_EVENT_ID, 50, NULL);
			} else if (m_nOldMouseY != 0 && nY > nIPAreaStart) {	
				SetVisible(true);
			}
		}
	} else {
		if (!(bHandled = m_pImageProcPanel->OnMouseMove(nX, nY))) {
			if (nY < m_pMainDlg->GetNavPanelCtl()->PanelRect().top) {
				SetVisible(false);
			}
		}
	}
	m_nOldMouseY = nY;
	return bHandled;
}

void CImageProcPanelCtl::ShowHideSaveDBButtons() {
	bool bFlags = !m_pMainDlg->IsInMovieMode() && !m_pMainDlg->IsKeepParams();
	m_pImageProcPanel->GetBtnSaveTo()->SetShow(CurrentImage() != NULL && !CurrentImage()->IsClipboardImage() && !CurrentImage()->IsProcessedNoParamDB() && bFlags);
	m_pImageProcPanel->GetBtnRemoveFrom()->SetShow(CurrentImage() != NULL && !CurrentImage()->IsProcessedNoParamDB() && bFlags && CurrentImage()->IsInParamDB());
	m_pMainDlg->InvalidateRect(PanelRect(), FALSE);
}

bool CImageProcPanelCtl::EnterRenameCurrentFile() {
	if (!m_bEnabled) {
		return false;
	}
	if (!CanMakeVisible()) {
		return false;
	}
	CTextCtrl* pFileNameCtrl = m_pImageProcPanel->GetTextFilename();
	if (pFileNameCtrl->GetText()[0] == 0 || !pFileNameCtrl->IsShown()) {
		return false;
	}
	SetVisible(true);
	m_pImageProcPanel->UpdateLayout();
	return m_pImageProcPanel->GetTextFilename()->EnterEditMode();
}

CRect CImageProcPanelCtl::GetUnsharpMaskButtonRect() {
	return m_pImageProcPanel->GetBtnUnsharpMask()->GetPosition();
}

bool CImageProcPanelCtl::RenameCurrentFile(LPCTSTR sNewFileTitle) {
	LPCTSTR sCurrentFileName = m_pMainDlg->GetFileList()->Current();
	if (sCurrentFileName == NULL) {
		return false;
	}

	DWORD nAttributes = ::GetFileAttributes(sCurrentFileName);
	if (nAttributes & FILE_ATTRIBUTE_READONLY) {
		::MessageBox(m_pMainDlg->GetHWND(), CNLS::GetString(_T("The file is read-only!")), CNLS::GetString(_T("Can't rename file")), MB_OK | MB_ICONSTOP);
		return false;
	}
	if (_tcschr(sNewFileTitle, _T('\\')) != NULL) {
		::MessageBox(m_pMainDlg->GetHWND(), CNLS::GetString(_T("New name contains backslash character!")), CNLS::GetString(_T("Can't rename file")), MB_OK | MB_ICONSTOP);
		return false;
	}

	CString sNewFileName = sNewFileTitle;
	if (_tcschr(sNewFileTitle, _T('.')) == NULL) {
		// no ending given, take the one from original file
		LPCTSTR sEndingOld = _tcsrchr(sCurrentFileName, _T('.'));
		if (sEndingOld != NULL) {
			sNewFileName += sEndingOld;
		}
	}
	LPCTSTR sPosOld = _tcsrchr(sCurrentFileName, _T('\\'));
	int nNumChrDir = (sPosOld == NULL) ? 0 : (int)(sPosOld - sCurrentFileName);
	sNewFileName = CString(sCurrentFileName, nNumChrDir) + _T('\\') + sNewFileName;

	// Rename the file
	if (!::MoveFileEx(sCurrentFileName, sNewFileName, 0)) {
		CString sError(CNLS::GetString(_T("Renaming file failed!")));
		DWORD lastError = ::GetLastError();
		LPTSTR lpMsgBuf = NULL;
		::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, lastError,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
		sError += _T("\n");
		sError += CNLS::GetString(_T("Reason: "));
		sError += lpMsgBuf;
		LocalFree(lpMsgBuf);
		::MessageBox(m_pMainDlg->GetHWND(), sError, CNLS::GetString(_T("Can't rename file")), MB_OK | MB_ICONSTOP);
		return false;
	}

	// Set new name
	LPCTSTR sNewFinalFileTitle = _tcsrchr(sNewFileName, _T('\\'));
	m_pImageProcPanel->GetTextFilename()->SetText((sNewFinalFileTitle == NULL) ? sNewFileName : sNewFinalFileTitle+1);

	// Tell that file was renamed to JPEG Provider and file list
	CString sCurFileName(sCurrentFileName); // copy, ptr will be replaced
	m_pMainDlg->GetFileList()->FileHasRenamed(sCurFileName, sNewFileName);
	m_pMainDlg->GetJPEGProvider()->FileHasRenamed(sCurFileName, sNewFileName);

	// Needs to update filename
	if (m_pMainDlg->GetEXIFDisplayCtl()->IsActive() || m_pMainDlg->IsShowFileName()) {
		InvalidateMainDlg();
	}
	m_pMainDlg->UpdateWindowTitle();
	return false;
}

void CImageProcPanelCtl::OnUnsharpMask(void* pContext, int nParameter, CButtonCtrl & sender) {
	((CImageProcPanelCtl*)pContext)->m_pMainDlg->GetUnsharpMaskPanelCtl()->SetVisible(true);
}

bool CImageProcPanelCtl::OnRenameFile(void* pContext, CTextCtrl & sender, LPCTSTR sChangedText) {
	return ((CImageProcPanelCtl*)pContext)->RenameCurrentFile(sChangedText);
}
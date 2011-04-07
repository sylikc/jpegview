#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "HelpDisplayCtl.h"
#include "HelpDisplay.h"
#include "NLS.h"
#include "JPEGImage.h"
#include "UserCommand.h"
#include "ProcessParams.h"
#include "EXIFDisplayCtl.h"
#include "NavigationPanelCtl.h"
#include "FileList.h"
#include "SettingsProvider.h"

CHelpDisplayCtl::CHelpDisplayCtl(CMainDlg* pMainDlg, CPaintDC& dc, const CImageProcessingParams* pImageProcParams) {
	m_pMainDlg = pMainDlg;
	m_pImageProcParams = pImageProcParams;
	m_pHelpDisplay = new CHelpDisplay(dc);
	GenerateHelpDisplay();
	CSize helpRectSize = m_pHelpDisplay->GetSize();
	m_helpDisplayRect = (pMainDlg->ClientRect().Width() > pMainDlg->MonitorRect().Width()) ? pMainDlg->MonitorRect() : pMainDlg->ClientRect();
	m_panelRect = CRect(CPoint(m_helpDisplayRect.Width()/2 - helpRectSize.cx/2, m_helpDisplayRect.Height()/2 - helpRectSize.cy/2), helpRectSize);
}

CHelpDisplayCtl::~CHelpDisplayCtl() {
	delete m_pHelpDisplay;
}

void CHelpDisplayCtl::Show() {
	m_pHelpDisplay->Show(CRect(CPoint(0, 0), CSize(m_helpDisplayRect.Width(), m_helpDisplayRect.Height())));
}

void CHelpDisplayCtl::GenerateHelpDisplay() {
	CJPEGImage* pCurrentImage = m_pMainDlg->GetCurrentImage();
	m_pHelpDisplay->AddTitle(CNLS::GetString(_T("JPEGView Help")));
	LPCTSTR sTitle = m_pMainDlg->CurrentFileName(true);
	if (sTitle != NULL && pCurrentImage != NULL) {
		double fMPix = double(pCurrentImage->OrigWidth() * pCurrentImage->OrigHeight())/(1000000);
		TCHAR buff[256];
		_stprintf_s(buff, 256, _T("%s (%d x %d   %.1f MPixel)"), sTitle, pCurrentImage->OrigWidth(), pCurrentImage->OrigHeight(), fMPix);
		m_pHelpDisplay->AddLine(CNLS::GetString(_T("Current image")), buff);
	}
	m_pHelpDisplay->AddLine(_T("Esc"), CNLS::GetString(_T("Close help text display / Close JPEGView")));
	m_pHelpDisplay->AddLine(_T("F1"), CNLS::GetString(_T("Show/hide this help text")));
	m_pHelpDisplay->AddLineInfo(_T("F2"), m_pMainDlg->GetEXIFDisplayCtl()->IsActive(), CNLS::GetString(_T("Show/hide picture information (EXIF data)")));
	m_pHelpDisplay->AddLineInfo(_T("Ctrl+F2"), m_pMainDlg->IsShowFileName(), CNLS::GetString(_T("Show/hide file name")));
	m_pHelpDisplay->AddLineInfo(_T("F3"), m_pMainDlg->IsHQResampling(), CNLS::GetString(_T("Enable/disable high quality resampling")));
	m_pHelpDisplay->AddLineInfo(_T("F4"), m_pMainDlg->IsKeepParams(), CNLS::GetString(_T("Enable/disable keeping of geometry related (zoom/pan/rotation)")));
	m_pHelpDisplay->AddLineInfo(_T(""),  LPCTSTR(NULL), CNLS::GetString(_T("and image processing (brightness/contrast/sharpen) parameters between images")));
	m_pHelpDisplay->AddLineInfo(_T("F5"), m_pMainDlg->IsAutoContrast(), CNLS::GetString(_T("Enable/disable automatic contrast correction (histogram equalization)")));
	m_pHelpDisplay->AddLineInfo(_T("Shift+F5"), m_pMainDlg->IsAutoContrastSection(), CNLS::GetString(_T("Apply auto contrast correction using only visible section of image")));
	m_pHelpDisplay->AddLineInfo(_T("F6"), m_pMainDlg->IsLDC(), CNLS::GetString(_T("Enable/disable automatic density correction (local brightness correction)")));
	TCHAR buffLS[16]; _stprintf_s(buffLS, 16, _T("%.2f"), m_pImageProcParams->LightenShadows);
	m_pHelpDisplay->AddLineInfo(_T("Ctrl/Alt+F6"), buffLS, CNLS::GetString(_T("Increase/decrease lightening of shadows (LDC must be on)")));
	TCHAR buffDH[16]; _stprintf_s(buffDH, 16, _T("%.2f"), m_pImageProcParams->DarkenHighlights);
	m_pHelpDisplay->AddLineInfo(_T("C/A+Shift+F6"), buffDH, CNLS::GetString(_T("Increase/decrease darkening of highlights (LDC must be on)")));
	m_pHelpDisplay->AddLineInfo(_T("Ctrl+L"), m_pMainDlg->IsLandscapeMode(), CNLS::GetString(_T("Enable/disable landscape picture enhancement mode")));
	m_pHelpDisplay->AddLineInfo(_T("F7"), m_pMainDlg->GetFileList()->GetNavigationMode() == Helpers::NM_LoopDirectory, CNLS::GetString(_T("Loop through files in current folder")));
	m_pHelpDisplay->AddLineInfo(_T("F8"), m_pMainDlg->GetFileList()->GetNavigationMode() == Helpers::NM_LoopSubDirectories, CNLS::GetString(_T("Loop through files in current directory and all subfolders")));
	m_pHelpDisplay->AddLineInfo(_T("F9"), m_pMainDlg->GetFileList()->GetNavigationMode() == Helpers::NM_LoopSameDirectoryLevel, CNLS::GetString(_T("Loop through files in current directory and all sibling folders (folders on same level)")));
	m_pHelpDisplay->AddLineInfo(_T("F11"), m_pMainDlg->IsFullScreenMode(), CNLS::GetString(_T("Enable/disable full screen mode")));
	m_pHelpDisplay->AddLineInfo(_T("F12"), m_pMainDlg->IsSpanVirtualDesktop(), CNLS::GetString(_T("Maximize/restore to/from virtual desktop (only for multi-monitor systems)")));
	m_pHelpDisplay->AddLineInfo(_T("Ctrl+F12"), LPCTSTR(NULL), CNLS::GetString(_T("Toggle between screens (only for multi-monitor systems)")));
	m_pHelpDisplay->AddLine(_T("Ctrl+M"), CNLS::GetString(_T("Mark image for toggling. Use Ctrl+Left/Ctrl+Right to toggle between marked and current image")));
	m_pHelpDisplay->AddLineInfo(_T("Ctrl+N"), m_pMainDlg->GetNavPanelCtl()->IsActive(), CNLS::GetString(_T("Show/hide navigation panel")));
	m_pHelpDisplay->AddLine(_T("Ctrl+C/Ctrl+X"), CNLS::GetString(_T("Copy screen to clipboard/ Copy processed full size image to clipboard")));
	m_pHelpDisplay->AddLine(_T("Ctrl+O"), CNLS::GetString(_T("Open new image or slideshow file")));
	m_pHelpDisplay->AddLine(_T("Ctrl+S"), CNLS::GetString(_T("Save processed image to JPEG file (original size)")));
	m_pHelpDisplay->AddLine(_T("Ctrl+Shift+S"), CNLS::GetString(_T("Save processed image to JPEG file (screen size)")));
	m_pHelpDisplay->AddLine(_T("s/d"), CNLS::GetString(_T("Save (s)/ delete (d) image processing parameters in/from parameter DB")));
	m_pHelpDisplay->AddLineInfo(_T("c/m/n"), 
		(m_pMainDlg->GetFileList()->GetSorting() == Helpers::FS_LastModTime) ? _T("m") :
		(m_pMainDlg->GetFileList()->GetSorting() == Helpers::FS_FileName) ? _T("n") : _T("c"), 
		CNLS::GetString(_T("Sort images by creation date, resp. modification date, resp. file name")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("PgUp or Left")), CNLS::GetString(_T("Goto previous image")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("PgDn or Right")), CNLS::GetString(_T("Goto next image")));
	m_pHelpDisplay->AddLine(_T("Home/End"), CNLS::GetString(_T("Goto first/last image of current folder (using sort order as defined)")));
	TCHAR buff[16]; _stprintf_s(buff, 16, _T("%.2f"), m_pImageProcParams->Gamma);
	m_pHelpDisplay->AddLineInfo(_T("Shift +/-"), buff, CNLS::GetString(_T("Increase/decrease brightness")));
	TCHAR buff1[16]; _stprintf_s(buff1, 16, _T("%.2f"), m_pImageProcParams->Contrast);
	m_pHelpDisplay->AddLineInfo(_T("Ctrl +/-"), buff1, CNLS::GetString(_T("Increase/decrease contrast")));
	TCHAR buff2[16]; _stprintf_s(buff2, 16, _T("%.2f"), m_pImageProcParams->Sharpen);
	m_pHelpDisplay->AddLineInfo(_T("Alt +/-"), buff2, CNLS::GetString(_T("Increase/decrease sharpness")));
	TCHAR buff3[16]; _stprintf_s(buff3, 16, _T("%.2f"), m_pImageProcParams->ColorCorrectionFactor);
	m_pHelpDisplay->AddLineInfo(_T("Alt+Ctrl +/-"), buff3, CNLS::GetString(_T("Increase/decrease auto color cast correction amount")));
	TCHAR buff4[16]; _stprintf_s(buff4, 16, _T("%.2f"), m_pImageProcParams->ContrastCorrectionFactor);
	m_pHelpDisplay->AddLineInfo(_T("Ctrl+Shift +/-"), buff4, CNLS::GetString(_T("Increase/decrease auto contrast correction amount")));
	m_pHelpDisplay->AddLine(_T("1 .. 9"), CNLS::GetString(_T("Slide show with timeout of n seconds (ESC to stop)")));
	m_pHelpDisplay->AddLineInfo(_T("Ctrl[+Shift] 1 .. 9"),  LPCTSTR(NULL), CNLS::GetString(_T("Set timeout to n/10 sec, respectively n/100 sec (Ctrl+Shift)")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("up/down")), CNLS::GetString(_T("Rotate image and fit to screen")));
	m_pHelpDisplay->AddLine(_T("Return"), CNLS::GetString(_T("Fit image to screen")));
	m_pHelpDisplay->AddLine(_T("Space"), CNLS::GetString(_T("Zoom 1:1 (100 %)")));
	TCHAR buff5[16]; _stprintf_s(buff5, 16, _T("%.0f %%"), m_pMainDlg->GetZoom()*100);
	m_pHelpDisplay->AddLineInfo(_T("+/-"), buff5, CNLS::GetString(_T("Zoom in/Zoom out (also Ctrl+up/down)")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("Mouse wheel")), CNLS::GetString(_T("Zoom in/out image")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("Left mouse & drag")), CNLS::GetString(_T("Pan image")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("Ctrl + Left mouse")), CNLS::GetString(_T("Crop image")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("Forward mouse button")), CNLS::GetString(_T("Next image")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("Back mouse button")), CNLS::GetString(_T("Previous image")));
	std::list<CUserCommand*>::iterator iter;
	std::list<CUserCommand*> & userCmdList = CSettingsProvider::This().UserCommandList();
	for (iter = userCmdList.begin( ); iter != userCmdList.end( ); iter++ ) {
		if (!(*iter)->HelpText().IsEmpty()) {
			m_pHelpDisplay->AddLine((*iter)->HelpKey(), CNLS::GetString((*iter)->HelpText()));
		}
	}
}
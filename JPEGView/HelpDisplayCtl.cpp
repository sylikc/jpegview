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
#include "KeyMap.h"

CHelpDisplayCtl::CHelpDisplayCtl(CMainDlg* pMainDlg, CDC& dc, const CImageProcessingParams* pImageProcParams) {
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
	m_pHelpDisplay->Show();
}

void CHelpDisplayCtl::GenerateHelpDisplay() {
	CJPEGImage* pCurrentImage = m_pMainDlg->GetCurrentImage();
	LPCTSTR sTitle = m_pMainDlg->CurrentFileName(true);
	if (sTitle != NULL && pCurrentImage != NULL) {
		double fMPix = double(pCurrentImage->OrigWidth() * pCurrentImage->OrigHeight())/(1000000);
		TCHAR buff[256];
		_stprintf_s(buff, 256, _T("%s (%d x %d   %.1f MPixel)"), sTitle, pCurrentImage->OrigWidth(), pCurrentImage->OrigHeight(), fMPix);
		m_pHelpDisplay->AddLine(CNLS::GetString(_T("Current image")), buff);
	}
	m_pHelpDisplay->AddLine(_T("Esc"), CNLS::GetString(_T("Close help text display / Close JPEGView")));
	m_pHelpDisplay->AddLine(_T("F1"), CNLS::GetString(_T("Show/hide this help text")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_SHOW_FILEINFO), m_pMainDlg->GetEXIFDisplayCtl()->IsActive(), CNLS::GetString(_T("Show/hide picture information (EXIF data)")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_SHOW_FILENAME), m_pMainDlg->IsShowFileName(), CNLS::GetString(_T("Show/hide file name")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_TOGGLE_RESAMPLING_QUALITY), m_pMainDlg->IsHQResampling(), CNLS::GetString(_T("Enable/disable high quality resampling")));
	if (m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_KEEP_PARAMETERS), m_pMainDlg->IsKeepParams(), CNLS::GetString(_T("Enable/disable keeping of geometry related (zoom/pan/rotation)")))) {
		m_pHelpDisplay->AddLineInfo(_T(""), LPCTSTR(NULL), CNLS::GetString(_T("and image processing (brightness/contrast/sharpen) parameters between images")));
	}
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_AUTO_CORRECTION), m_pMainDlg->IsAutoContrast(), CNLS::GetString(_T("Enable/disable automatic contrast correction (histogram equalization)")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_AUTO_CORRECTION_SECTION), m_pMainDlg->IsAutoContrastSection(), CNLS::GetString(_T("Apply auto contrast correction using only visible section of image")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_LDC), m_pMainDlg->IsLDC(), CNLS::GetString(_T("Enable/disable automatic density correction (local brightness correction)")));
	TCHAR buffLS[16]; _stprintf_s(buffLS, 16, _T("%.2f"), m_pImageProcParams->LightenShadows);
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_LDC_SHADOWS_INC, IDM_LDC_SHADOWS_DEC), buffLS, CNLS::GetString(_T("Increase/decrease lightening of shadows (LDC must be on)")));
	TCHAR buffDH[16]; _stprintf_s(buffDH, 16, _T("%.2f"), m_pImageProcParams->DarkenHighlights);
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_LDC_HIGHLIGHTS_INC, IDM_LDC_HIGHLIGHTS_DEC), buffDH, CNLS::GetString(_T("Increase/decrease darkening of highlights (LDC must be on)")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_LANDSCAPE_MODE), m_pMainDlg->IsLandscapeMode(), CNLS::GetString(_T("Enable/disable landscape picture enhancement mode")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_LOOP_FOLDER), m_pMainDlg->GetFileList()->GetNavigationMode() == Helpers::NM_LoopDirectory, CNLS::GetString(_T("Loop through files in current folder")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_LOOP_RECURSIVELY), m_pMainDlg->GetFileList()->GetNavigationMode() == Helpers::NM_LoopSubDirectories, CNLS::GetString(_T("Loop through files in current directory and all subfolders")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_LOOP_SIBLINGS), m_pMainDlg->GetFileList()->GetNavigationMode() == Helpers::NM_LoopSameDirectoryLevel, CNLS::GetString(_T("Loop through files in current directory and all sibling folders (folders on same level)")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_FULL_SCREEN_MODE), m_pMainDlg->IsFullScreenMode(), CNLS::GetString(_T("Enable/disable full screen mode")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_SPAN_SCREENS), m_pMainDlg->IsSpanVirtualDesktop(), CNLS::GetString(_T("Maximize/restore to/from virtual desktop (only for multi-monitor systems)")));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_TOGGLE_MONITOR), LPCTSTR(NULL), CNLS::GetString(_T("Toggle between screens (only for multi-monitor systems)")));
	TCHAR buffMI[256]; _stprintf_s(buffMI, 256, CNLS::GetString(_T("Mark image for toggling. Use %s to toggle between marked and current image")), _KeyDesc(IDM_TOGGLE));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_MARK_FOR_TOGGLE), buffMI);
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_SHOW_NAVPANEL), m_pMainDlg->GetNavPanelCtl()->IsActive(), CNLS::GetString(_T("Show/hide navigation panel")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_COPY, IDM_COPY_FULL), CNLS::GetString(_T("Copy screen to clipboard/ Copy processed full size image to clipboard")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_OPEN), CNLS::GetString(_T("Open new image or slideshow file")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_SAVE_ALLOW_NO_PROMPT), CNLS::GetString(_T("Save processed image to JPEG file (original size)")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_SAVE_SCREEN), CNLS::GetString(_T("Save processed image to JPEG file (screen size)")));
	_stprintf_s(buffMI, 256, CNLS::GetString(_T("Save (%s)/ delete (%s) image processing parameters in/from parameter DB")), _KeyDesc(IDM_SAVE_PARAM_DB), _KeyDesc(IDM_CLEAR_PARAM_DB));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_SAVE_PARAM_DB, IDM_CLEAR_PARAM_DB), buffMI);
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_SORT_CREATION_DATE, IDM_SORT_MOD_DATE, IDM_SORT_NAME, IDM_SORT_RANDOM), 
		(m_pMainDlg->GetFileList()->GetSorting() == Helpers::FS_LastModTime) ? _KeyDesc(IDM_SORT_MOD_DATE) :
		(m_pMainDlg->GetFileList()->GetSorting() == Helpers::FS_FileName) ? _KeyDesc(IDM_SORT_NAME) : 
		(m_pMainDlg->GetFileList()->GetSorting() == Helpers::FS_Random) ? _KeyDesc(IDM_SORT_RANDOM) : 
		(m_pMainDlg->GetFileList()->GetSorting() == Helpers::FS_FileSize) ? _KeyDesc(IDM_SORT_SIZE) : _KeyDesc(IDM_SORT_CREATION_DATE), 
		CNLS::GetString(_T("Sort images by creation date, resp. modification date, resp. file name")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_PREV), CNLS::GetString(_T("Goto previous image")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_NEXT), CNLS::GetString(_T("Goto next image")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_FIRST, IDM_LAST), CNLS::GetString(_T("Goto first/last image of current folder (using sort order as defined)")));
	TCHAR buff[16]; _stprintf_s(buff, 16, _T("%.2f"), m_pImageProcParams->Gamma);
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_GAMMA_INC, IDM_GAMMA_DEC), buff, CNLS::GetString(_T("Increase/decrease brightness")));
	TCHAR buff1[16]; _stprintf_s(buff1, 16, _T("%.2f"), m_pImageProcParams->Contrast);
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_CONTRAST_INC, IDM_CONTRAST_DEC), buff1, CNLS::GetString(_T("Increase/decrease contrast")));
	TCHAR buff2[16]; _stprintf_s(buff2, 16, _T("%.2f"), m_pImageProcParams->Sharpen);
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_SHARPEN_INC, IDM_SHARPEN_DEC), buff2, CNLS::GetString(_T("Increase/decrease sharpness")));
	TCHAR buff3[16]; _stprintf_s(buff3, 16, _T("%.2f"), m_pImageProcParams->ColorCorrectionFactor);
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_COLOR_CORRECTION_INC, IDM_COLOR_CORRECTION_DEC), buff3, CNLS::GetString(_T("Increase/decrease auto color cast correction amount")));
	TCHAR buff4[16]; _stprintf_s(buff4, 16, _T("%.2f"), m_pImageProcParams->ContrastCorrectionFactor);
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_CONTRAST_CORRECTION_INC, IDM_CONTRAST_CORRECTION_DEC), buff4, CNLS::GetString(_T("Increase/decrease auto contrast correction amount")));
	m_pHelpDisplay->AddLine(_T("1 .. 9"), CNLS::GetString(_T("Slide show with timeout of n seconds (ESC to stop)")));
	m_pHelpDisplay->AddLineInfo(_T("Ctrl[+Shift] 1 .. 9"),  LPCTSTR(NULL), CNLS::GetString(_T("Set timeout to n/10 sec, respectively n/100 sec (Ctrl+Shift)")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_ROTATE_270, IDM_ROTATE_90), CNLS::GetString(_T("Rotate image and fit to screen")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_FIT_TO_SCREEN), CNLS::GetString(_T("Fit image to screen")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_TOGGLE_FIT_TO_SCREEN_100_PERCENTS), CNLS::GetString(_T("Zoom 1:1 (100 %)")));
	TCHAR buff5[16]; buff5[0] = 0; if (m_pMainDlg->GetZoom() > 0) _stprintf_s(buff5, 16, _T("%.0f %%"), m_pMainDlg->GetZoom() * 100);
	_stprintf_s(buffMI, 256, CNLS::GetString(_T("Zoom in (%s)/Zoom out (%s)")), _KeyDesc(IDM_ZOOM_INC), _KeyDesc(IDM_ZOOM_DEC));
	m_pHelpDisplay->AddLineInfo(_KeyDesc(IDM_ZOOM_INC, IDM_ZOOM_DEC), buff5, buffMI);
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("Mouse wheel")), CNLS::GetString(_T("Zoom in/out image")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("Left mouse & drag")), CNLS::GetString(_T("Pan image")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("Ctrl + Left mouse")), CNLS::GetString(_T("Crop image")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("Forward mouse button")), CNLS::GetString(_T("Next image")));
	m_pHelpDisplay->AddLine(CNLS::GetString(_T("Back mouse button")), CNLS::GetString(_T("Previous image")));
	m_pHelpDisplay->AddLine(_KeyDesc(IDM_MOVE_TO_RECYCLE_BIN_CONFIRM, IDM_MOVE_TO_RECYCLE_BIN), CNLS::GetString(_T("Delete image file")));
	std::list<CUserCommand*>::iterator iter;
	std::list<CUserCommand*> & userCmdList = CSettingsProvider::This().UserCommandList();
	for (iter = userCmdList.begin( ); iter != userCmdList.end( ); iter++ ) {
		if (!(*iter)->HelpText().IsEmpty()) {
			m_pHelpDisplay->AddLine((*iter)->HelpKey(), CNLS::GetString((*iter)->HelpText()));
		}
	}
}

CString CHelpDisplayCtl::_KeyDesc(int nCommandId) {
	CString keyDesc = m_pMainDlg->GetKeyMap()->GetKeyStringForCommand(nCommandId);
	if (keyDesc.IsEmpty())
		return CString(_T("n.a."));
	return keyDesc;
}

CString CHelpDisplayCtl::_KeyDesc(int nCommandId1, int nCommandId2) {
	CString keyDesc1 = m_pMainDlg->GetKeyMap()->GetKeyStringForCommand(nCommandId1);
	CString keyDesc2 = m_pMainDlg->GetKeyMap()->GetKeyStringForCommand(nCommandId2);
	if (keyDesc1.IsEmpty() || keyDesc2.IsEmpty()) {
		if (keyDesc1.IsEmpty() && keyDesc2.IsEmpty()) {
			return CString(_T("n.a."));
		}
		return keyDesc1.IsEmpty() ? keyDesc2 : keyDesc1;
	}
	return keyDesc1 + _T("/") + keyDesc2;
}

CString CHelpDisplayCtl::_KeyDesc(int nCommandId1, int nCommandId2, int nCommandId3, int nCommandId4) {
	CString keyDesc1 = m_pMainDlg->GetKeyMap()->GetKeyStringForCommand(nCommandId1);
	CString keyDesc2 = m_pMainDlg->GetKeyMap()->GetKeyStringForCommand(nCommandId2);
	CString keyDesc3 = m_pMainDlg->GetKeyMap()->GetKeyStringForCommand(nCommandId3);
	CString keyDesc4 = m_pMainDlg->GetKeyMap()->GetKeyStringForCommand(nCommandId4);
	CString result = keyDesc1;
	if (!keyDesc2.IsEmpty()) {
		if (!result.IsEmpty()) result += _T("/");
		result += keyDesc2;
	}
	if (!keyDesc3.IsEmpty()) {
		if (!result.IsEmpty()) result += _T("/");
		result += keyDesc3;
	}
	if (!keyDesc4.IsEmpty()) {
		if (!result.IsEmpty()) result += _T("/");
		result += keyDesc4;
	}
	return result.IsEmpty() ? CString(_T("n.a.")) : result;
}

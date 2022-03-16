#include "StdAfx.h"
#include "TiltCorrectionPanel.h"
#include "NLS.h"

/////////////////////////////////////////////////////////////////////////////////////////////
// CTiltCorrectionPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CTiltCorrectionPanel::CTiltCorrectionPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel) 
: CTransformPanel(hWnd, pNotifyMouseCapture, pImageProcPanel, CNLS::GetString(_T("Perspective Correction")),
				  CNLS::GetString(_T("Auto crop corrected image (avoids black border)"))) {
}

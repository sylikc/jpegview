#include "StdAfx.h"
#include "RotationPanel.h"
#include "ImageProcessingPanel.h"
#include "Helpers.h"
#include "NLS.h"

/////////////////////////////////////////////////////////////////////////////////////////////
// CRotationPanel
/////////////////////////////////////////////////////////////////////////////////////////////

CRotationPanel::CRotationPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel) 
: CTransformPanel(hWnd, pNotifyMouseCapture, pImageProcPanel, CNLS::GetString(_T("Rotate Image")),
				  CNLS::GetString(_T("Auto crop rotated image (avoids black border)"))) {
	AddUserPaintButton(ID_btnAssistMode, CNLS::GetString(_T("Align image to horizontal or vertical line")), &PaintAssistedModeBtn);
}

void CRotationPanel::PaintAssistedModeBtn(void* pContext, const CRect& rect, CDC& dc) {
	CRect r = Helpers::InflateRect(rect, 0.25f);

	int nAw = r.Width() / 3;
	dc.MoveTo(r.left, r.top);
	dc.LineTo(r.right, r.bottom);
	dc.MoveTo(r.left, r.bottom - nAw);
	dc.LineTo(r.left, r.bottom);
	dc.LineTo(r.left + nAw + 1, r.bottom);
	dc.MoveTo(r.left, r.bottom);
	dc.LineTo(r.left + nAw + 2, r.bottom - nAw - 2);
}

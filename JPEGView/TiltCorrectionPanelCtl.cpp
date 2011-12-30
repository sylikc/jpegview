#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "TiltCorrectionPanelCtl.h"
#include "TiltCorrectionPanel.h"
#include "NLS.h"
#include "Helpers.h"
#include <math.h>

static int round(float value) {
	return (value > 0) ? (int)(value + 0.5f) : (int)(value - 0.5f);
}

CTiltCorrectionPanelCtl::CTiltCorrectionPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) 
: CTransformPanelCtl(pMainDlg, pImageProcPanel, new CTiltCorrectionPanel(pMainDlg->m_hWnd, this, pImageProcPanel)) {
	m_pTransformPanel->GetTextHint()->SetText(CNLS::GetString(
		_T("Click and drag the mouse horizontally to correct image tilt.")));
	m_fLeftDeltaX = 0;
	m_fRightDeltaX = 0;
	m_nStartX = 0;
	m_bLeft = true;
	m_fScale = 1.0f;
}

CTrapezoid CTiltCorrectionPanelCtl::GetCurrentTrapezoid(CSize targetSize) {
	CSize fullImageSize = targetSize;
	CSize origImageSize = CurrentImage()->OrigSize();
	float fFactorX = ((float)(fullImageSize.cx - 1))/(origImageSize.cx - 1);
	float fLeftDeltaX = m_fLeftDeltaX * fFactorX;
	float fRightDeltaX = m_fRightDeltaX * fFactorX;
	return CTrapezoid(round(fLeftDeltaX), round(fRightDeltaX + fullImageSize.cx - 1), 
		 0, round(-fLeftDeltaX), round(-fRightDeltaX + fullImageSize.cx - 1), fullImageSize.cy - 1);
}

void* CTiltCorrectionPanelCtl::GetDIBForPreview(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
												 const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {
	CTrapezoid trapezoid = GetCurrentTrapezoid(fullTargetSize);
	return CurrentImage()->GetDIBTrapezoid(fullTargetSize, clippingSize, targetOffset, imageProcParams, 
		SetProcessingFlag(eProcFlags, PFLAG_LDC, false), 
		&trapezoid , m_bShowGrid);
}

void CTiltCorrectionPanelCtl::StartPanel() {
	CTransformPanelCtl::StartPanel();
	m_fLeftDeltaX = 0;
	m_fRightDeltaX = 0;
}

void CTiltCorrectionPanelCtl::StartTransforming(int nX, int nY) {
	if (CurrentImage() != NULL) {
		m_bTransforming = true;
		float fX = (float)nX, fY = (float)nY;
		m_pMainDlg->ScreenToImage(fX, fY);
		float fCenterX = CurrentImage()->OrigWidth() * 0.5f;
		float fCenterY = CurrentImage()->OrigHeight() * 0.5f;
		m_bLeft = fX < fCenterX;
		m_fScale = 0.5f + fabs(0.5f * (fX - fCenterX) / fCenterX);
		if (fY > fCenterY) m_fScale = -m_fScale;
		m_nStartX = nX;
	}
}

void CTiltCorrectionPanelCtl::DoTransforming() {
	if (CurrentImage() != NULL) {
		int nDeltaX = m_nMouseX - m_nStartX;
		m_nStartX = m_nMouseX;
		float fFactorX = ((float)(CurrentImage()->OrigWidth() - 1))/(m_pMainDlg->VirtualImageSize().cx - 1);
		if (m_bLeft) {
			m_fLeftDeltaX += nDeltaX * m_fScale * fFactorX;
		} else {
			m_fRightDeltaX += nDeltaX * m_fScale * fFactorX;
		}
		float fMaxX = CurrentImage()->OrigWidth() * 0.25f;
		m_fLeftDeltaX = max(min(fMaxX, m_fLeftDeltaX), -fMaxX);
		m_fRightDeltaX = max(min(fMaxX, m_fRightDeltaX), -fMaxX);

		InvalidateMainDlg();
	}
}

void CTiltCorrectionPanelCtl::EndTransforming() {
	m_bTransforming = false;
}

void CTiltCorrectionPanelCtl::ApplyTransformation() {
	if (!CurrentImage()->TrapezoidOriginalPixels(GetCurrentTrapezoid(CurrentImage()->OrigSize()), m_bAutoCrop)) {
		::MessageBox(m_pMainDlg->GetHWND(), CNLS::GetString(_T("The operation failed because not enough memory is available!")),
			_T("JPEGView"), MB_ICONSTOP | MB_OK);
	}
}

void CTiltCorrectionPanelCtl::UpdatePanelTitle() {
	// title is static, therefore empty
}

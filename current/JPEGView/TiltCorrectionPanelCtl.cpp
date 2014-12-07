#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "TiltCorrectionPanelCtl.h"
#include "TiltCorrectionPanel.h"
#include "NLS.h"
#include "Helpers.h"
#include <math.h>

static int roundToInt(float value) {
	return (value > 0) ? (int)(value + 0.5f) : (int)(value - 0.5f);
}

CTiltCorrectionPanelCtl::CTiltCorrectionPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) 
: CTransformPanelCtl(pMainDlg, pImageProcPanel, new CTiltCorrectionPanel(pMainDlg->m_hWnd, this, pImageProcPanel)) {
	m_pTransformPanel->GetTextHint()->SetText(
		CString(CNLS::GetString(_T("Click and drag the mouse vertically to correct image tilt."))) + _T("\n") + 
		CNLS::GetString(_T("Drag at the left or right border for asymmetric correction.")));
	m_fLeftDeltaX = m_fLeftDeltaXBackup = 0;
	m_fRightDeltaX = m_fRightDeltaXBackup = 0;
	m_nStartX = 0;
	m_nStartY = 0;
	m_eClickPosition = CP_Left;
	m_fScale = 1.0f;
}

CTrapezoid CTiltCorrectionPanelCtl::GetCurrentTrapezoid(CSize targetSize) {
	CSize fullImageSize = targetSize;
	CSize origImageSize = CurrentImage()->OrigSize();
	float fFactorX = ((float)(fullImageSize.cx - 1))/(origImageSize.cx - 1);
	float fLeftDeltaX = m_fLeftDeltaX * fFactorX;
	float fRightDeltaX = m_fRightDeltaX * fFactorX;
	return CTrapezoid(roundToInt(fLeftDeltaX), roundToInt(fRightDeltaX + fullImageSize.cx - 1),
		0, roundToInt(-fLeftDeltaX), roundToInt(-fRightDeltaX + fullImageSize.cx - 1), fullImageSize.cy - 1);
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
	m_fLeftDeltaX = m_fLeftDeltaXBackup = 0;
	m_fRightDeltaX = m_fRightDeltaXBackup = 0;
}

void CTiltCorrectionPanelCtl::StartTransforming(int nX, int nY) {
	if (CurrentImage() != NULL) {
		m_bTransforming = true;
		float fX = (float)nX, fY = (float)nY;
		m_pMainDlg->ScreenToImage(fX, fY);
		float fCenterX = CurrentImage()->OrigWidth() * 0.5f;
		float fCenterY = CurrentImage()->OrigHeight() * 0.5f;
		m_eClickPosition = (fX < CurrentImage()->OrigWidth() * 0.333f) ? CP_Left : (fX > CurrentImage()->OrigWidth() * 0.666f) ? CP_Right : CP_Middle;
		m_fScale = (m_eClickPosition == CP_Middle) ? 0.5f + fabs(0.5f * (fY - fCenterY) / fCenterY) : 0.5f + fabs(0.5f * (fX - fCenterX) / fCenterX);
		if (fY > fCenterY && m_eClickPosition != CP_Middle) m_fScale = -m_fScale;
		m_nStartX = nX;
		m_nStartY = nY;
		m_fLeftDeltaXBackup = m_fRightDeltaXBackup = 0;
	}
}

void CTiltCorrectionPanelCtl::DoTransforming() {
	if (CurrentImage() != NULL) {
		int nDeltaX = m_nMouseX - m_nStartX;
		int nDeltaY = m_nMouseY - m_nStartY;
		m_nStartX = m_nMouseX;
		m_nStartY = m_nMouseY;
		float fFactorX = ((float)(CurrentImage()->OrigWidth() - 1))/(m_pMainDlg->VirtualImageSize().cx - 1);
		if (m_eClickPosition == CP_Left) {
			m_fLeftDeltaX += nDeltaX * m_fScale * fFactorX;
		} else if (m_eClickPosition == CP_Right) {
			m_fRightDeltaX += nDeltaX * m_fScale * fFactorX;
		} else {
			m_fLeftDeltaX -= nDeltaY * m_fScale * fFactorX;
			m_fRightDeltaX += nDeltaY * m_fScale * fFactorX;
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
	if (!CurrentImage()->TrapezoidOriginalPixels(GetCurrentTrapezoid(CurrentImage()->OrigSize()), m_bAutoCrop, m_bKeepAspectRatio)) {
		::MessageBox(m_pMainDlg->GetHWND(), CNLS::GetString(_T("The operation failed because not enough memory is available!")),
			_T("JPEGView"), MB_ICONSTOP | MB_OK);
	}
}

void CTiltCorrectionPanelCtl::Reset() {
	m_fLeftDeltaX = m_fLeftDeltaXBackup = 0;
	m_fRightDeltaX = m_fRightDeltaXBackup = 0;
	InvalidateMainDlg();
}

void CTiltCorrectionPanelCtl::UpdatePanelTitle() {
	// title is static, therefore empty
}

void CTiltCorrectionPanelCtl::ExchangeTransformationParams() {
	float fTemp = m_fLeftDeltaXBackup;
	m_fLeftDeltaXBackup = m_fLeftDeltaX;
	m_fLeftDeltaX = fTemp;
	fTemp = m_fRightDeltaXBackup;
	m_fRightDeltaXBackup = m_fRightDeltaX;
	m_fRightDeltaX = fTemp;
	InvalidateMainDlg();
}

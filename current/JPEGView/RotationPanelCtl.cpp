#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "RotationPanelCtl.h"
#include "RotationPanel.h"
#include "NLS.h"
#include "Helpers.h"
#include <math.h>

CRotationPanelCtl::CRotationPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) 
: CTransformPanelCtl(pMainDlg, pImageProcPanel, new CRotationPanel(pMainDlg->m_hWnd, this, pImageProcPanel)) {

	m_bRotationModeAssisted = false;
	m_dRotationLQ = m_dRotatonLQStart = 0.0;
	m_rotationStartX = m_rotationStartY = 0;

	((CRotationPanel*)m_pTransformPanel)->GetBtnAssistMode()->SetButtonPressedHandler(&OnRPAssistedMode, this, 0, m_bRotationModeAssisted);
	UpdateAssistedRotationMode();
}


void CRotationPanelCtl::OnPostPaintMainDlg(HDC hPaintDC) {
	if (m_bTransforming && m_bRotationModeAssisted) {
		// Draw the red line in assistet rotation mode
		float fX = m_rotationStartX, fY = m_rotationStartY;
		m_pMainDlg->ImageToScreen(fX, fY);
		HPEN hPen = ::CreatePen(PS_SOLID, 3, RGB(255, 0, 0));
		HGDIOBJ hOldPen = ::SelectObject(hPaintDC, hPen);
		::MoveToEx(hPaintDC, Helpers::RoundToInt(fX), Helpers::RoundToInt(fY), NULL); 
		::LineTo(hPaintDC, m_nMouseX, m_nMouseY);
		::SelectObject(hPaintDC, hOldPen);
		::DeleteObject(hPen);
	}
}

void* CRotationPanelCtl::GetDIBForPreview(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
												 const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {
	return CurrentImage()->GetDIBRotated(fullTargetSize, clippingSize, targetOffset, imageProcParams, 
		SetProcessingFlag(eProcFlags, PFLAG_LDC, false), m_dRotationLQ, m_bShowGrid);
}

void CRotationPanelCtl::TerminatePanel() {
	CTransformPanelCtl::TerminatePanel();
	m_dRotationLQ = 0.0;
}

void CRotationPanelCtl::UpdateAssistedRotationMode() {
	m_pTransformPanel->GetTextHint()->SetText(CNLS::GetString(m_bRotationModeAssisted ? 
		_T("Use the mouse to draw a line that shall be horizontal or vertical.") : 
		_T("Rotate the image by dragging with the mouse.")));
	m_pMainDlg->InvalidateRect(&(m_pTransformPanel->GetTextHint()->GetPosition()), FALSE);
	((CRotationPanel*)m_pTransformPanel)->GetBtnAssistMode()->SetActive(m_bRotationModeAssisted);
	if (m_bShowGrid == m_bRotationModeAssisted) {
		m_bShowGrid = !m_bRotationModeAssisted;
		m_pTransformPanel->GetBtnShowGrid()->SetActive(m_bShowGrid);
		InvalidateMainDlg();
	}
}

void CRotationPanelCtl::StartTransforming(int nX, int nY) {
	if (CurrentImage() != NULL) {
		m_bTransforming = true;
		float fX = (float)nX, fY = (float)nY;
		m_pMainDlg->ScreenToImage(fX, fY);
		m_rotationStartX = fX;
		m_rotationStartY = fY;
		m_dRotatonLQStart = m_dRotationLQ;
	}
}

void CRotationPanelCtl::DoTransforming() {
	const double PI = 3.141592653;
	if (CurrentImage() != NULL) {
		if (!m_bRotationModeAssisted) {
			float fX = (float)m_nMouseX, fY = (float)m_nMouseY;
			m_pMainDlg->ScreenToImage(fX, fY);
			float fCenterX = CurrentImage()->OrigWidth() * 0.5f;
			float fCenterY = CurrentImage()->OrigHeight() * 0.5f;
			double dAngleStart = atan2(m_rotationStartY - fCenterY, m_rotationStartX - fCenterX);
			double dAngleEnd = atan2(fY - fCenterY, fX - fCenterX);
			m_dRotationLQ = fmod(m_dRotatonLQStart + dAngleEnd - dAngleStart, 2 * PI);
			UpdatePanelTitle();
		}
		InvalidateMainDlg();
	}
}

void CRotationPanelCtl::EndTransforming() {
	const float PI = 3.141592653f;
	m_bTransforming = false;
	if (m_bRotationModeAssisted) {
		float fX = m_rotationStartX, fY = m_rotationStartY;
		m_pMainDlg->ImageToScreen(fX, fY);
		float fDX = (m_nMouseX > fX) ? m_nMouseX - fX : fX - m_nMouseX;
		float fDY = (m_nMouseX > fX) ? m_nMouseY - fY : fY - m_nMouseY;
		if (fabs(fDX) > 2 || fabs(fDY) > 2) {
			float fAngle = atan2(fDY, fDX);
			float fInvSignAngle = (fAngle < 0) ? 1.0f : -1.0f;
			if (fabs(fAngle) > PI * 0.25f) {
				m_dRotationLQ -= (fAngle + fInvSignAngle * PI * 0.5f);
			} else {
				m_dRotationLQ -= fAngle;
			}
			UpdatePanelTitle();
		}
		InvalidateMainDlg();
	}
}

void CRotationPanelCtl::ApplyTransformation() {
	if (!CurrentImage()->RotateOriginalPixels(m_dRotationLQ, m_bAutoCrop, m_bKeepAspectRatio)) {
		::MessageBox(m_pMainDlg->GetHWND(), CNLS::GetString(_T("The operation failed because not enough memory is available!")),
			_T("JPEGView"), MB_ICONSTOP | MB_OK);
	}
}

void CRotationPanelCtl::UpdatePanelTitle() {
	const int BUFF_SIZE = 24;
	double dAngleDeg = 360 * m_dRotationLQ / (2 * 3.141592653);
	TCHAR buff[BUFF_SIZE];
	_stprintf_s(buff, BUFF_SIZE, _T("  %.1f °"), dAngleDeg);
	m_pTransformPanel->GetTextTitle()->SetText(CString(CNLS::GetString(_T("Rotate Image"))) + buff);
}

void CRotationPanelCtl::OnRPAssistedMode(void* pContext, int nParameter, CButtonCtrl & sender) {
	CRotationPanelCtl* pThis = (CRotationPanelCtl*)pContext;
	pThis->m_bRotationModeAssisted = !pThis->m_bRotationModeAssisted;
	pThis->UpdateAssistedRotationMode();
}

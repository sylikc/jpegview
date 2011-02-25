#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"
#include "JPEGImage.h"
#include "RotationPanelCtl.h"
#include "RotationPanel.h"
#include "NavigationPanelCtl.h"
#include "NLS.h"
#include "Helpers.h"
#include <math.h>

CRotationPanelCtl::CRotationPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel) : CPanelController(pMainDlg, true) {
	m_bVisible = false;
	m_bRotationModeAssisted = false;
	m_bRotationShowGrid = true;
	m_bRotationAutoCrop = true;
	m_dRotationLQ = m_dRotatonLQStart = 0.0;
	m_rotationStartX = m_rotationStartY = 0;
	m_bRotating = false;
	m_bOldShowNavPanel = false;
	m_nMouseX = m_nMouseY = 0;

	m_pPanel = m_pRotationPanel = new CRotationPanel(pMainDlg->m_hWnd, this, pImageProcPanel);
	m_pRotationPanel->GetBtnShowGrid()->SetButtonPressedHandler(&OnRPShowGridLines, this, 0, m_bRotationShowGrid);
	m_pRotationPanel->GetBtnAutoCrop()->SetButtonPressedHandler(&OnRPAutoCrop, this, 0, m_bRotationAutoCrop);
	m_pRotationPanel->GetBtnAssistMode()->SetButtonPressedHandler(&OnRPAssistedMode, this, 0, m_bRotationModeAssisted);
	m_pRotationPanel->GetBtnCancel()->SetButtonPressedHandler(&OnCancelRotation, this);
	m_pRotationPanel->GetBtnApply()->SetButtonPressedHandler(&OnApplyRotation, this);
	UpdateAssistedRotationMode();
}

CRotationPanelCtl::~CRotationPanelCtl() { 
	delete m_pRotationPanel;
	m_pRotationPanel = NULL;
}

bool CRotationPanelCtl::IsVisible() {
	return m_bVisible;
}

bool CRotationPanelCtl::IsActive() {
	return m_bVisible;
}

void CRotationPanelCtl::SetVisible(bool bVisible) {
	if (bVisible != m_bVisible) {
		if (bVisible) {
			StartRotationPanel();
		} else {
			TerminateRotationPanel();
		}
	}
}

void CRotationPanelCtl::SetActive(bool bActive) {
	SetVisible(bActive);
}

bool CRotationPanelCtl::OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY) {
	if (m_bVisible && m_bRotating && eMouseEvent == MouseEvent_BtnUp) {
		EndRotating();
		return true;
	}
	return CPanelController::OnMouseLButton(eMouseEvent, nX, nY);
}

bool CRotationPanelCtl::OnMouseMove(int nX, int nY) {
	m_nMouseX = nX;
	m_nMouseY = nY;
	if (m_bRotating) {
		DoRotating();
		return true;
	}
	return CPanelController::OnMouseMove(nX, nY);
}

bool CRotationPanelCtl::OnKeyDown(unsigned int nVirtualKey, bool bShift, bool bAlt, bool bCtrl) {
	if (nVirtualKey == VK_ESCAPE) {
		TerminateRotationPanel();
	}
	return true; // all other keys are eaten up in this panel without doing anything
}

void CRotationPanelCtl::OnPostPaintMainDlg(HDC hPaintDC) {
	if (m_bRotating && m_bRotationModeAssisted) {
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

void* CRotationPanelCtl::GetRotatedDIBForPreview(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
												 const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) {
	return CurrentImage()->GetDIBRotated(fullTargetSize, clippingSize, targetOffset, imageProcParams, eProcFlags, m_dRotationLQ, m_bRotationShowGrid);
}

void CRotationPanelCtl::StartRotationPanel() {
	if (CurrentImage() == NULL) {
		return;
	}
	m_bVisible = true;
	m_bOldShowNavPanel = m_pMainDlg->PrepareForModalPanel();
	UpdateRotationPanelTitle();
	InvalidateMainDlg();
}

void CRotationPanelCtl::TerminateRotationPanel() {
	m_bVisible = false;
	m_pMainDlg->GetNavPanelCtl()->SetActive(m_bOldShowNavPanel);
	m_dRotationLQ = 0.0;
	InvalidateMainDlg();
}

void CRotationPanelCtl::UpdateAssistedRotationMode() {
	m_pRotationPanel->GetTextHint()->SetText(CNLS::GetString(m_bRotationModeAssisted ? 
		_T("Use the mouse to draw a line that shall be horizontal or vertical.") : 
		_T("Rotate the image by dragging with the mouse.")));
	m_pMainDlg->InvalidateRect(&(m_pRotationPanel->GetTextHint()->GetPosition()), FALSE);
	m_pRotationPanel->GetBtnAssistMode()->SetActive(m_bRotationModeAssisted);
	if (m_bRotationShowGrid == m_bRotationModeAssisted) {
		m_bRotationShowGrid = !m_bRotationModeAssisted;
		m_pRotationPanel->GetBtnShowGrid()->SetActive(m_bRotationShowGrid);
		InvalidateMainDlg();
	}
}

void CRotationPanelCtl::StartRotating(int nX, int nY) {
	if (CurrentImage() != NULL) {
		m_bRotating = true;
		float fX = (float)nX, fY = (float)nY;
		m_pMainDlg->ScreenToImage(fX, fY);
		m_rotationStartX = fX;
		m_rotationStartY = fY;
		m_dRotatonLQStart = m_dRotationLQ;
	}
}

void CRotationPanelCtl::DoRotating() {
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
			UpdateRotationPanelTitle();
		}
		InvalidateMainDlg();
	}
}

void CRotationPanelCtl::EndRotating() {
	const float PI = 3.141592653f;
	m_bRotating = false;
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
			UpdateRotationPanelTitle();
		}
		InvalidateMainDlg();
	}
}

void CRotationPanelCtl::ApplyRotation() {
	if (CurrentImage() == NULL) {
		return;
	}
	CurrentImage()->RotateOriginalPixels(m_dRotationLQ, m_bRotationAutoCrop);
	InvalidateMainDlg();
}

void CRotationPanelCtl::UpdateRotationPanelTitle() {
	const int BUFF_SIZE = 24;
	double dAngleDeg = 360 * m_dRotationLQ / (2 * 3.141592653);
	TCHAR buff[BUFF_SIZE];
	_stprintf_s(buff, BUFF_SIZE, _T("  %.1f °"), dAngleDeg);
	m_pRotationPanel->GetTextTitle()->SetText(CString(CNLS::GetString(_T("Rotate Image"))) + buff);
}

void CRotationPanelCtl::OnRPShowGridLines(void* pContext, int nParameter, CButtonCtrl & sender) {
	CRotationPanelCtl* pThis = (CRotationPanelCtl*)pContext;
	pThis->m_bRotationShowGrid = !pThis->m_bRotationShowGrid;
	pThis->m_pRotationPanel->GetBtnShowGrid()->SetActive(pThis->m_bRotationShowGrid);
	pThis->InvalidateMainDlg();
}

void CRotationPanelCtl::OnRPAutoCrop(void* pContext, int nParameter, CButtonCtrl & sender) {
	CRotationPanelCtl* pThis = (CRotationPanelCtl*)pContext;
	pThis->m_bRotationAutoCrop = !pThis->m_bRotationAutoCrop;
	pThis->m_pRotationPanel->GetBtnAutoCrop()->SetActive(pThis->m_bRotationAutoCrop);
}

void CRotationPanelCtl::OnRPAssistedMode(void* pContext, int nParameter, CButtonCtrl & sender) {
	CRotationPanelCtl* pThis = (CRotationPanelCtl*)pContext;
	pThis->m_bRotationModeAssisted = !pThis->m_bRotationModeAssisted;
	pThis->UpdateAssistedRotationMode();
}

void CRotationPanelCtl::OnCancelRotation(void* pContext, int nParameter, CButtonCtrl & sender) {
	((CRotationPanelCtl*)pContext)->TerminateRotationPanel();
}

void CRotationPanelCtl::OnApplyRotation(void* pContext, int nParameter, CButtonCtrl & sender) {
	CRotationPanelCtl* pThis = (CRotationPanelCtl*)pContext;
	HCURSOR hOldCursor = ::SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_WAIT)));
	pThis->ApplyRotation();
	::SetCursor(hOldCursor);
	pThis->TerminateRotationPanel();
}
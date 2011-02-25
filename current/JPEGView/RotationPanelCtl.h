#pragma once

#include "PanelController.h"
#include "ProcessParams.h"

class CRotationPanel;

// Implements functionality of the rotation panel
class CRotationPanelCtl : public CPanelController
{
public:
	CRotationPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel);
	virtual ~CRotationPanelCtl();

	virtual float DimFactor() { return 0.5f; }

	virtual bool IsVisible();
	virtual bool IsActive();

	virtual void SetVisible(bool bVisible);
	virtual void SetActive(bool bActive);

	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);
	virtual bool OnKeyDown(unsigned int nVirtualKey, bool bShift, bool bAlt, bool bCtrl);
	virtual void OnPostPaintMainDlg(HDC hPaintDC);

	void* GetRotatedDIBForPreview(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags);

	double GetLQRotationAngle() { return m_dRotationLQ; }

	void StartRotating(int nX, int nY);

private:
	bool m_bVisible;
	CRotationPanel* m_pRotationPanel;
	bool m_bRotationModeAssisted;
	bool m_bRotationShowGrid;
	bool m_bRotationAutoCrop;
	double m_dRotationLQ;
	double m_dRotatonLQStart;
	bool m_bRotating;
	float m_rotationStartX;
	float m_rotationStartY;
	bool m_bOldShowNavPanel;
	int m_nMouseX, m_nMouseY;

	void TerminateRotationPanel();
	void StartRotationPanel();
	void UpdateAssistedRotationMode();
	void DoRotating();
	void EndRotating();
	void ApplyRotation();
	void PaintRotationLine(HDC hPaintDC);
	void UpdateRotationPanelTitle();

	static void OnRPShowGridLines(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnRPAutoCrop(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnRPAssistedMode(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnCancelRotation(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnApplyRotation(void* pContext, int nParameter, CButtonCtrl & sender);

};
#pragma once

#include "TransformPanelCtl.h"
#include "ProcessParams.h"

// Implements functionality of the rotation panel
class CRotationPanelCtl : public CTransformPanelCtl
{
public:
	CRotationPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel);

	virtual void OnPostPaintMainDlg(HDC hPaintDC);

	// Gets a rotated DIB with the current rotation angle(GetLQRotationAngle) using point sampling
	// The returned DIB has size clippingSize.
	virtual void* GetDIBForPreview(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags);

	// Gets current rotation angle (in radians). Rotation is around image center
	double GetLQRotationAngle() { return m_dRotationLQ; }

private:
	bool m_bOriginalShowGrid;
	bool m_bRotationModeAssisted;
	double m_dRotationLQ;
	double m_dRotatonLQStart;
	double m_dRotationBackup;
	float m_rotationStartX;
	float m_rotationStartY;

	virtual void TerminatePanel();
	virtual void StartTransforming(int nX, int nY);
	virtual void DoTransforming();
	virtual void EndTransforming();
	virtual void ApplyTransformation();
	virtual void UpdatePanelTitle();
	virtual void ExchangeTransformationParams();

	void UpdateAssistedRotationMode();
	void PaintRotationLine(HDC hPaintDC);

	static void OnRPAssistedMode(void* pContext, int nParameter, CButtonCtrl & sender);
};
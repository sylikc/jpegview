#pragma once

#include "TransformPanelCtl.h"
#include "ProcessParams.h"

// Implements functionality of the perspective correction panel
class CTiltCorrectionPanelCtl : public CTransformPanelCtl
{
public:
	CTiltCorrectionPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel);

	// Gets a rotated DIB with the current rotation angle(GetLQRotationAngle) using point sampling
	// The returned DIB has size clippingSize.
	virtual void* GetDIBForPreview(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags);

	// Gets the current trapezoid, resized to the given target size of the image
	CTrapezoid GetCurrentTrapezoid(CSize targetSize);
private:
	virtual void StartPanel();
	virtual void StartTransforming(int nX, int nY);
	virtual void DoTransforming();
	virtual void EndTransforming();
	virtual void ApplyTransformation();
	virtual void UpdatePanelTitle();

	enum EClickPosition {
		CP_Left,
		CP_Right,
		CP_Middle
	};

	float m_fLeftDeltaX;
	float m_fRightDeltaX;
	int m_nStartX, m_nStartY;
	EClickPosition m_eClickPosition;
	float m_fScale;
};
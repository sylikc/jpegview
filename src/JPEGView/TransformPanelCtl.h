#pragma once

#include "PanelController.h"
#include "ProcessParams.h"

class CTransformPanel;

// Implements functionality of the rotation and perspective correction panels (these are modal panels)
class CTransformPanelCtl : public CPanelController
{
public:
	CTransformPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel, CTransformPanel* pTransformPanel);
	virtual ~CTransformPanelCtl();

	virtual float DimFactor() { return 0.5f; }

	virtual bool IsVisible();
	virtual bool IsActive();

	virtual void SetVisible(bool bVisible);
	virtual void SetActive(bool bActive);

	virtual void CancelModalPanel() { TerminatePanel(); };

	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);
	virtual bool OnKeyDown(unsigned int nVirtualKey, bool bShift, bool bAlt, bool bCtrl);

	// Gets a DIB having applied the current transformation using point sampling
	// The returned DIB has size clippingSize.
	virtual void* GetDIBForPreview(CSize fullTargetSize, CSize clippingSize, CPoint targetOffset,
		const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags) = 0;

protected:
	bool m_bVisible;
	CTransformPanel* m_pTransformPanel;
	bool m_bShowGrid;
	bool m_bAutoCrop;
	bool m_bKeepAspectRatio;
	bool m_bTransforming;
	bool m_bOldShowNavPanel;
	int m_nMouseX, m_nMouseY;

	virtual void TerminatePanel();
	virtual void StartPanel();
	virtual void StartTransforming(int nX, int nY) = 0;
	virtual void DoTransforming() = 0;
	virtual void EndTransforming() = 0;
	virtual void ApplyTransformation() = 0;
	virtual void UpdatePanelTitle() = 0;
	virtual void ExchangeTransformationParams() = 0;
	virtual void Reset() = 0;

private:
	void Apply();

	static void OnShowGridLines(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnAutoCrop(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnKeepAspectRatio(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnCancel(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnReset(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnApply(void* pContext, int nParameter, CButtonCtrl & sender);

};
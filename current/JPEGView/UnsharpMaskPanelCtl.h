#pragma once

#include "PanelController.h"
#include "ProcessParams.h"

class CUnsharpMaskPanel;

// Implements functionality of the unsharp mask panel
class CUnsharpMaskPanelCtl : public CPanelController
{
public:
	CUnsharpMaskPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel);
	virtual ~CUnsharpMaskPanelCtl();

	virtual float DimFactor() { return 0.5f; }

	virtual bool IsVisible();
	virtual bool IsActive();

	virtual void SetVisible(bool bVisible);
	virtual void SetActive(bool bActive);

	virtual bool OnKeyDown(unsigned int nVirtualKey, bool bShift, bool bAlt, bool bCtrl);

	// Gets preview DIB of unsharp masked image. Returned DIB has size clippingSize
	void* GetUSMDIBForPreview(CSize clippingSize, CPoint offset, const CImageProcessingParams & imageProcParams, EProcessingFlags eProcFlags);

private:
	bool m_bVisible;
	bool m_bOldShowNavPanel;
	CUnsharpMaskParams* m_pUnsharpMaskParams;
	CUnsharpMaskPanel* m_pUnsharpMaskPanel;
	double m_dAlternateUSMAmount;

	void TerminateUnsharpMaskPanel();
	void StartUnsharpMaskPanel();

	static void OnCancelUnsharpMask(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnApplyUnsharpMask(void* pContext, int nParameter, CButtonCtrl & sender);
};
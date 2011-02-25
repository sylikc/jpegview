#pragma once

#include "PanelController.h"

class CImageProcessingPanel;
class CImageProcessingParams;

// Implements functionality of the image processing panel
class CImageProcPanelCtl : public CPanelController
{
public:
	// The given image processing parameters are modified by this panel, the same applied to the given boolean values
	CImageProcPanelCtl(CMainDlg* pMainDlg, CImageProcessingParams* pParams, bool* pEnableLDC, bool* pEnableContrastCorr);
	virtual ~CImageProcPanelCtl();

	virtual float DimFactor() { return 0.5f; }

	virtual bool IsVisible() { return m_bVisible; }
	virtual bool IsActive() { return true; }  // always active, but maybe invisible (when mouse outside hot area)

	virtual void SetVisible(bool bVisible);
	virtual void SetActive(bool bActive) {}  // not possible to set

	virtual void AfterNewImageLoaded();

	virtual bool OnTimer(int nTimerId);
	virtual bool OnMouseMove(int nX, int nY);

	// Called by main dialog e.g. when saving or deleting parameter DB entry
	void ShowHideSaveDBButtons();

	CRect GetUnsharpMaskButtonRect();

private:
	bool m_bVisible;
	int m_nOldMouseY;
	CImageProcessingPanel* m_pImageProcPanel;

	bool RenameCurrentFile(LPCTSTR sNewFileTitle);

	static void OnUnsharpMask(void* pContext, int nParameter, CButtonCtrl & sender);
	static bool OnRenameFile(void* pContext, CTextCtrl & sender, LPCTSTR sChangedText);
};
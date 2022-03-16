#pragma once

#include "PanelController.h"

class CEXIFDisplay;

// Implements functionality of the EXIF display panel
class CEXIFDisplayCtl : public CPanelController
{
public:
	CEXIFDisplayCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel);
	virtual ~CEXIFDisplayCtl();

	virtual float DimFactor() { return 0.5f; }

	virtual bool IsVisible();
	virtual bool IsActive() { return m_bVisible; }

	virtual void SetVisible(bool bVisible);
	virtual void SetActive(bool bActive);

	virtual void AfterNewImageLoaded();

	virtual bool OnMouseMove(int nX, int nY);
	virtual void OnPrePaintMainDlg(HDC hPaintDC);

private:
	bool m_bVisible;
	CEXIFDisplay* m_pEXIFDisplay;
	CPanel* m_pImageProcPanel;
	int m_nFileNameHeight;

	void FillEXIFDataDisplay();
	static void OnShowHistogram(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnClose(void* pContext, int nParameter, CButtonCtrl & sender);
};
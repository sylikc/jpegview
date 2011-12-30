#pragma once

#include "Panel.h"

// Navigation panel containing buttons and sliders
class CNavigationPanel : public CPanel {
public:
	// IDs of the controls on this panel
	enum {
		ID_btnHome,
		ID_btnPrev,
		ID_gap1,
		ID_btnNext,
		ID_btnEnd,
		ID_gap2,
		ID_btnZoomMode,
		ID_btnFitToScreen,
		ID_btnWindowMode,
		ID_gap3,
		ID_btnRotateCW,
		ID_btnRotateCCW,
		ID_btnRotateFree,
		ID_btnPerspectiveCorrection,
		ID_gap4,
		ID_btnKeepParams,
		ID_btnLandscapeMode,
		ID_gap5,
		ID_btnShowInfo
	};
public:
	// The panel is on the given window above the image processing panel
	CNavigationPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel, bool* pFullScreenMode,
		DecisionMethod* isCurrentImageFitToScreen, void* pDecisionMethodParam);

	CButtonCtrl* GetBtnHome() { return GetControl<CButtonCtrl*>(ID_btnHome); }
	CButtonCtrl* GetBtnPrev() { return GetControl<CButtonCtrl*>(ID_btnPrev); }
	CButtonCtrl* GetBtnNext() { return GetControl<CButtonCtrl*>(ID_btnNext); }
	CButtonCtrl* GetBtnEnd() { return GetControl<CButtonCtrl*>(ID_btnEnd); }
	CButtonCtrl* GetBtnZoomMode() { return GetControl<CButtonCtrl*>(ID_btnZoomMode); }
	CButtonCtrl* GetBtnFitToScreen() { return GetControl<CButtonCtrl*>(ID_btnFitToScreen); }
	CButtonCtrl* GetBtnWindowMode() { return GetControl<CButtonCtrl*>(ID_btnWindowMode); }
	CButtonCtrl* GetBtnRotateCW() { return GetControl<CButtonCtrl*>(ID_btnRotateCW); }
	CButtonCtrl* GetBtnRotateCCW() { return GetControl<CButtonCtrl*>(ID_btnRotateCCW); }
	CButtonCtrl* GetBtnRotateFree() { return GetControl<CButtonCtrl*>(ID_btnRotateFree); }
	CButtonCtrl* GetBtnPerspectiveCorrection() { return GetControl<CButtonCtrl*>(ID_btnPerspectiveCorrection); }
	CButtonCtrl* GetBtnKeepParams() { return GetControl<CButtonCtrl*>(ID_btnKeepParams); }
	CButtonCtrl* GetBtnLandscapeMode() { return GetControl<CButtonCtrl*>(ID_btnLandscapeMode); }
	CButtonCtrl* GetBtnShowInfo() { return GetControl<CButtonCtrl*>(ID_btnShowInfo); }

	virtual CRect PanelRect();
	virtual void RequestRepositioning();

	bool AdjustMaximalWidth(int nMaxWidth); // return if the width has been adjusted

protected:
	virtual void RepositionAll();

private:
	// Painting handlers for the buttons
	static void PaintHomeBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintPrevBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintNextBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintEndBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintZoomModeBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintZoomToFitBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintZoomTo1to1Btn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintZoomFitToggleBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintWindowModeBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintRotateCWBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintRotateCCWBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintFreeRotBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintPerspectiveBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintInfoBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintKeepParamsBtn(void* pContext, const CRect& rect, CDC& dc);
	static void PaintLandscapeModeBtn(void* pContext, const CRect& rect, CDC& dc);

	static LPCTSTR WindowModeTooltip(void* pContext);
	static LPCTSTR ZoomFitToggleTooltip(void* pContext);

	void AddGap(int nID, int nGap);
	void SetScaledWidth(float fScale);

	DecisionMethod* m_isCurrentImageFitToScreen;
	void* m_pDecisionMethodParam;
	CPanel* m_pImageProcPanel;
	CRect m_clientRect;
	int m_nWidth, m_nHeight;
	int m_nBorder;
	int m_nGap;
	int m_nOptimalWidth;
	float m_fAdditionalScale;
};

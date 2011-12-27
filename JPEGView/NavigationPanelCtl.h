#pragma once

#include "PanelController.h"

class CNavigationPanel;

// Implements functionality of the navigation panel
class CNavigationPanelCtl : public CPanelController
{
public:
	// Sets the pFullScreenMode when corresponding button is pressed
	CNavigationPanelCtl(CMainDlg* pMainDlg, CPanel* pImageProcPanel, bool* pFullScreenMode);
	virtual ~CNavigationPanelCtl();

	// Current blending factor with background, 1 -> fully visible, 0 -> invisible
	float CurrentBlendingFactor() { return m_fCurrentBlendingFactorNavPanel; }

	virtual bool BlendPanel() { return !m_bMouseInNavPanel; }
	virtual float DimFactor() { return 0.5f; }

	void AdjustMaximalWidth(int nMaxWidth);

	virtual bool IsVisible();
	virtual bool IsActive() { return m_bEnabled; }

	virtual void SetVisible(bool bVisible) {} // not possible
	virtual void SetActive(bool bActive);

	virtual bool OnMouseLButton(EMouseEvent eMouseEvent, int nX, int nY);
	virtual bool OnMouseMove(int nX, int nY);
	virtual bool OnTimer(int nTimerId);

	// Starts blending in or out (bFadeOut parameter) the navigation panel
	void StartNavPanelAnimation(bool bFadeOut, bool bFast);
	// Immediately terminates any ongoing animation of the navigation panel
	void EndNavPanelAnimation();
	// Hides the navigation panel temporarily (invisible) - the panel remains active however
	void HideNavPanelTemporary();
	// Shows the navigation panel when it is blended out (CurrentBlendingFactor zero)
	void ShowNavPanelTemporary();
	// Gets managed panel containing the buttons
	CNavigationPanel* GetNavPanel() { return m_pNavPanel; }

private:
	bool m_bEnabled;
	CNavigationPanel* m_pNavPanel;
	bool m_bMouseInNavPanel;
	int m_nMouseX, m_nMouseY;

	// navigation panel animation
	bool m_bInNavPanelAnimation;
	bool m_bFadeOut;
	float m_fCurrentBlendingFactorNavPanel;
	int m_nBlendInNavPanelCountdown;
	CDC* m_pMemDCAnimation;
	HBITMAP m_hOffScreenBitmapAnimation;

	bool CheckMouseInNavPanel(int nX, int nY);
	void DoNavPanelAnimation();
	void StartNavPanelTimer(int nMilliseconds);
	void MoveMouseCursorToButton(CButtonCtrl & sender, const CRect & oldRect);

	static void OnGotoImage(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnToggleZoomFit(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnToggleWindowMode(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnRotateFree(void* pContext, int nParameter, CButtonCtrl & sender);
	static void OnRotate(void* pContext, int nParameter, CButtonCtrl & sender);
};
#pragma once

class CMainDlg;
class CHelpDisplay;
class CImageProcessingParams;

// Implements functionality of the help display panel
// This is not a CPanelController because the help display is not a CPanel (not painted to offscreen bitmap)
class CHelpDisplayCtl {
public:
	CHelpDisplayCtl(CMainDlg* pMainDlg, CPaintDC& dc, const CImageProcessingParams* pImageProcParams);
	virtual ~CHelpDisplayCtl();

	CRect PanelRect() { return m_panelRect; }

	// Displays the help screen on the device context passed to the constructor
	void Show();

private:
	const CImageProcessingParams* m_pImageProcParams;
	CHelpDisplay* m_pHelpDisplay;
	CMainDlg* m_pMainDlg;
	CRect m_panelRect;
	CRect m_helpDisplayRect;

	void GenerateHelpDisplay();
	CString _KeyDesc(int nCommandId);
	CString _KeyDesc(int nCommandId1, int nCommandId2);
	CString _KeyDesc(int nCommandId1, int nCommandId2, int nCommandId3, int nCommandId4);
};
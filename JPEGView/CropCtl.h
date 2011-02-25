#pragma once

class CMainDlg;

// Implements functionality of the rotation panel
class CCropCtl {
public:
	CCropCtl(CMainDlg* pMainDlg);

	bool IsCropping() { return m_bCropping; }
	bool IsDoCropping() { return m_bDoCropping; }
	double GetCropRatio() { return m_dCropRatio; }
	void SetCropRatio(double dRatio) {  m_dCropRatio = dRatio; }
	CRect GetImageCropRect();

	void OnPaint(CPaintDC& dc);

	void StartCropping(int nX, int nY);
	void DoCropping(int nX, int nY);
	void EndCropping();
	void CancelCropping() { m_bCropping = false; m_bDoCropping = false; }
	void ResetStartCropOnNextClick() { m_bDontStartCropOnNextClick = false; }

	void CropLossless();

private:
	CMainDlg* m_pMainDlg;
	CPoint m_cropStart;
	CPoint m_cropEnd;
	bool m_bCropping;
	bool m_bDoCropping;
	bool m_bDontStartCropOnNextClick;
	bool m_bBlockPaintCropRect;
	CPoint m_cropMouse;
	double m_dCropRatio;

	void ShowCroppingRect(int nX, int nY, HDC hPaintDC, bool bShow);
	void PaintCropRect(HDC hPaintDC);
	void DeleteSceenCropRect();
	CRect GetScreenCropRect();
};
#pragma once

class CMainDlg;

// Implements cropping functionality. Binds to main dialog.
class CCropCtl {
public:
	CCropCtl(CMainDlg* pMainDlg);

	bool IsCropping() { return m_bCropping; } // during cropping
	bool IsDoCropping() { return m_bDoCropping; } // during cropping and cropping rectangle visible
	
	// Get and set aspect ratio of cropping rectangle, -1 for free cropping
	double GetCropRectAR() { return m_dCropRectAspectRatio; }
	void SetCropRectAR(double dRatio) {  m_dCropRectAspectRatio = dRatio; }

	// Cropping rectangle in image coordinates (in original size image)
	CRect GetImageCropRect();

	void OnPaint(CPaintDC& dc); // paints cropping rectangle when in cropping
	bool OnTimer(int nTimerId);

	// nX, nY is the current mouse coordinate
	void StartCropping(int nX, int nY);
	void DoCropping(int nX, int nY);
	void EndCropping();

	void CancelCropping() { m_bCropping = false; m_bDoCropping = false; }
	void ResetStartCropOnNextClick() { m_bDontStartCropOnNextClick = false; }

	// perform lossless cropping, using the current cropping rectangle
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
	double m_dCropRectAspectRatio;

	void ShowCroppingRect(int nX, int nY, HDC hPaintDC, bool bShow);
	void PaintCropRect(HDC hPaintDC);
	void DeleteSceenCropRect();
	CRect GetScreenCropRect();
};
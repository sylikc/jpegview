#pragma once

class CMainDlg;

// Implements cropping functionality. Binds to main dialog.
class CCropCtl {
public:
	enum Handle {
		HH_None,
		HH_Border,
		HH_TopLeft,
		HH_TopRight,
		HH_BottomLeft,
		HH_BottomRight,
		HH_Left,
		HH_Right,
		HH_Top,
		HH_Bottom
	};
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
	void AbortCropping();

	void CancelCropping() { m_bCropping = false; m_bDoCropping = false; }
	void ResetStartCropOnNextClick() { m_bDontStartCropOnNextClick = false; }

	// perform lossless cropping, using the current cropping rectangle
	void CropLossless();

	// Gets the handle hit at the given mouse position
	Handle HitHandle(int nX, int nY);

	// Shows the crop context menu and executes the chosen menu command (if a menu item was chosen)
	// Returns the executed menu command ID, zero if none
	int ShowCropContextMenu();

private:
	CMainDlg* m_pMainDlg;
	CPoint m_cropStart;
	CPoint m_cropEnd;
	bool m_bCropping;
	bool m_bDoCropping;
	bool m_bDoTracking;
	bool m_bTrackingMode;
	bool m_bDontStartCropOnNextClick;
	CPoint m_cropMouse;
	CPoint m_startTrackMousePos;
	double m_dCropRectAspectRatio;
	int m_nHandleSize;
	Handle m_eHitHandle;

	void TrackCroppingRect(int nX, int nY, Handle eHandle);
	void UpdateCroppingRect(int nX, int nY, HDC hPaintDC, bool bShow);
	void PaintCropRect(HDC hPaintDC);
	void InvalidateSceenCropRect();
	CRect GetScreenCropRect();
	CPoint ScreenToImage(int nX, int nY);
	CPoint PreserveAspectRatio(CPoint cropStart, CPoint cropEnd, bool bCalculateEnd);
	void NormalizeCroppingRect();
	void SetMouseCursor(int nX, int nY);
};
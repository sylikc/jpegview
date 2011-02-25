#pragma once

#include "Panel.h"

class CImageProcessingParams;

// Manages a set of sliders and other controls to perform image processing tasks
// This panel is placed on the bottom of the window
class CImageProcessingPanel : public CPanel {
public:
	// IDs of the controls on this panel
	enum {
		ID_slContrast,
		ID_slBrightness,
		ID_slSaturation,
		ID_slCR,
		ID_slMG,
		ID_slYB,
		ID_slLightenShadows,
		ID_slDarkenHighlights,
		ID_slDeepShadows,
		ID_slColorCorr,
		ID_slContrastCorr,
		ID_slSharpen,
		ID_btnUnsharpMask,
		ID_txtParamDB,
		ID_btnSaveTo,
		ID_btnRemoveFrom,
		ID_txtRename,
		ID_txtFilename,
		ID_txtAcqDate
	};
public:
	// The sliders are placed on the given window (at bottom)
	CImageProcessingPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CImageProcessingParams* pParams, bool* pEnableLDC, bool* pEnableContrastCorr);

	CButtonCtrl* GetBtnUnsharpMask() { return GetControl<CButtonCtrl*>(ID_btnUnsharpMask); }
	CButtonCtrl* GetBtnSaveTo() { return GetControl<CButtonCtrl*>(ID_btnSaveTo); }
	CButtonCtrl* GetBtnRemoveFrom() { return GetControl<CButtonCtrl*>(ID_btnRemoveFrom); }
	CTextCtrl* GetTextParamDB() { return GetControl<CTextCtrl*>(ID_txtParamDB); }
	CTextCtrl* GetTextRename() { return GetControl<CTextCtrl*>(ID_txtRename); }
	CTextCtrl* GetTextFilename() { return GetControl<CTextCtrl*>(ID_txtFilename); }
	CTextCtrl* GetTextAcqDate() { return GetControl<CTextCtrl*>(ID_txtAcqDate); }

	// height of slider area
	int SliderAreaHeight() const { return m_nTotalAreaHeight; }
	virtual CRect PanelRect();

	// Request recalculation of the layout
	virtual void RequestRepositioning() { m_clientRect = CRect(0, 0, 0, 0); }

protected:
	virtual void RepositionAll();

private:
	void AddSlider(int nID, LPCTSTR sName, double* pdValue, bool* pbEnable, 
		 double dMin, double dMax, double dDefaultValue, bool bAllowPreviewAndReset, bool bLogarithmic, bool bInvert);

	void DetermineSliderWidth();

	int m_nSliderWidth;
	int m_nSliderHeight;
	int m_nNoLabelWidth;
	int m_nSliderGap;
	int m_nTotalAreaHeight;
	int m_nYStart;
	CRect m_clientRect;
	CRect m_sliderAreaRect;
	int m_nTotalSliders;
};
#pragma once

#include "TransformPanel.h"

// Panel for perspective (tilt) correction
class CTiltCorrectionPanel : public CTransformPanel {
public:
	// The panel is on the given window above the image processing panel
	CTiltCorrectionPanel(HWND hWnd, INotifiyMouseCapture* pNotifyMouseCapture, CPanel* pImageProcPanel);
};
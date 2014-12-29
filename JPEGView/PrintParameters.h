#pragma once

// Parameters entered by user for printing and stored between calls to the printing dialog
class CPrintParameters {
public:
	enum HorizontalAlignment {
		HA_Left,
		HA_Center,
		HA_Right
	};

	enum VerticalAlignment {
		VA_Top,
		VA_Center,
		VA_Bottom
	};

	bool FitToPaper;
	bool ScaleByPrinterDriver;
	double PrintWidth; // in 1/10 mm. Print height is given by aspect ratio of image
	double MarginLeft, MarginTop, MarginRight, MarginBottom; // in 1/10 mm
	HorizontalAlignment AlignmentX;
	VerticalAlignment AlignmentY;

	// Uses default parameters
	CPrintParameters() {
		FitToPaper = true;
		ScaleByPrinterDriver = true;
		PrintWidth = 150 * 10;
		MarginLeft = 10 * 10;
		MarginTop = 10 * 10;
		MarginRight = 10 * 10;
		MarginBottom = 10 * 10;
		AlignmentX = HA_Center;
		AlignmentY = VA_Top;
	}
};
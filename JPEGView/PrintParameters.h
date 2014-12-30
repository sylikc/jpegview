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
	CPrintParameters(double defaultMarginCm, double defaultPrintWidthCm) {
		FitToPaper = defaultPrintWidthCm < 0.0;
		ScaleByPrinterDriver = true;
		PrintWidth = abs(defaultPrintWidthCm) * 100;
		MarginLeft = defaultMarginCm * 100;
		MarginTop = defaultMarginCm * 100;
		MarginRight = defaultMarginCm * 100;
		MarginBottom = defaultMarginCm * 100;
		AlignmentX = HA_Center;
		AlignmentY = VA_Top;
	}
};
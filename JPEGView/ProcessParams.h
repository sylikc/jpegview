#pragma once

#include "Helpers.h"

// Processing flags for image processing
enum EProcessingFlags {
	PFLAG_None = 0,
	PFLAG_AutoContrast = 1,
	PFLAG_AutoContrastSection = 2,
	PFLAG_LDC = 4,
	PFLAG_HighQualityResampling = 8,
	PFLAG_KeepParams = 16,
	PFLAG_LandscapeMode = 32,
	PFLAG_NoProcessingAfterLoad = 64
};

static inline EProcessingFlags SetProcessingFlag(EProcessingFlags eFlags, EProcessingFlags eFlagToSet, bool bValue) {
	return bValue ? (EProcessingFlags)(eFlags | eFlagToSet) : (EProcessingFlags)(eFlags & ~eFlagToSet);
};

static inline bool GetProcessingFlag(EProcessingFlags eFlags, EProcessingFlags eFlagToGet) {
	return (eFlags & eFlagToGet) != 0;
}

// Parameters for image processing, geometry independent
class CImageProcessingParams {
public:
	CImageProcessingParams(double dContrast, double dGamma, double dSaturation, double dSharpen, double dColorCorrectionFactor,
		double dContrastCorrectionFactor, double dLightenShadows, double dDarkenHighlights, double dLightenShadowSteepness,
		double dCyanRed, double dMagentaGreen, double dYellowBlue) {
		Contrast = dContrast;
		Gamma = dGamma;
		Saturation = dSaturation;
		Sharpen = dSharpen;
		ColorCorrectionFactor = dColorCorrectionFactor;
		ContrastCorrectionFactor = dContrastCorrectionFactor;
		LightenShadows = dLightenShadows;
		DarkenHighlights = dDarkenHighlights;
		LightenShadowSteepness = dLightenShadowSteepness;
		CyanRed = dCyanRed;
		MagentaGreen = dMagentaGreen;
		YellowBlue = dYellowBlue;
	}

	CImageProcessingParams() {
		Contrast = -1;
		Gamma = -1;
		Sharpen = -1;
		Saturation = -1;
		ColorCorrectionFactor = -1;
		ContrastCorrectionFactor = -1;
		LightenShadows = -1;
		DarkenHighlights = -1;
		LightenShadowSteepness = -1;
		CyanRed = -1;
		MagentaGreen = -1;
		YellowBlue = -1;
	}

	double Contrast;
	double Gamma;
	double Saturation;
	double Sharpen;
	double ColorCorrectionFactor;
	double ContrastCorrectionFactor;
	double LightenShadows;
	double DarkenHighlights;
	double LightenShadowSteepness;
	double CyanRed;
	double MagentaGreen;
	double YellowBlue;
};

// Parameters used in unsharp masking
class CUnsharpMaskParams {
public:
	CUnsharpMaskParams() {
		Radius = Amount = Threshold = 0.0;
	}

	CUnsharpMaskParams(double dRadius, double dAmount, double dThreshold) {
		Radius = dRadius;
		Amount = dAmount;
		Threshold = dThreshold;
	}

	CUnsharpMaskParams(const CUnsharpMaskParams& other) {
		Radius = other.Radius;
		Amount = other.Amount;
		Threshold = other.Threshold;
	}

	double Radius;
	double Amount;
	double Threshold;
};

// Parameters used to process an image, including geometry
class CProcessParams {
public:
	// TargetWidth,TargetHeight is the dimension of the target output screen, the image is fit into this rectangle
	// nRotation must be 0, 90, 270 degrees
	// dZoom is the zoom factor compared to intial image size (1.0 means no zoom)
	// offsets are relative to center of image and refer to original image size (not zoomed)
	CProcessParams(int nTargetWidth, int nTargetHeight,
        CSize monitorSize,
        int nRotation, double dZoom, 
		Helpers::EAutoZoomMode eAutoZoomMode, CPoint offsets,
		const CImageProcessingParams& imageProcParams,
		EProcessingFlags eProcFlags) : ImageProcParams(imageProcParams) {
		TargetWidth = nTargetWidth;
		TargetHeight = nTargetHeight;
        MonitorSize = monitorSize;
		Rotation = nRotation;
		Zoom = dZoom;
		AutoZoomMode = eAutoZoomMode;
		Offsets = offsets;
		ProcFlags = eProcFlags;
	}

	int TargetWidth;
	int TargetHeight;
    CSize MonitorSize;
	int Rotation;
	double Zoom;
	CPoint Offsets;
	CImageProcessingParams ImageProcParams;
	EProcessingFlags ProcFlags;
	Helpers::EAutoZoomMode AutoZoomMode;
};
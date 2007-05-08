#pragma once

// Processing flags for image processing
enum EProcessingFlags {
	PFLAG_None = 0,
	PFLAG_AutoContrast = 1,
	PFLAG_AutoContrastSection = 2,
	PFLAG_LDC = 4,
	PFLAG_HighQualityResampling = 8,
	PFLAG_KeepParams = 16
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
	CImageProcessingParams(double dContrast, double dGamma, double dSharpen, double dColorCorrectionFactor,
		double dContrastCorrectionFactor, double dLightenShadows, double dDarkenHighlights, double dLightenShadowSteepness,
		double dCyanRed, double dMagentaGreen, double dYellowBlue) {
		Contrast = dContrast;
		Gamma = dGamma;
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

// Parameters used to process an image, including geometry
class CProcessParams {
public:
	// TargetWidth,TargetHeight is the dimension of the target output screen, the image is fit into this rectangle
	// nRotation must be 0, 90, 270 degrees
	// dZoom is the zoom factor compared to intial image size (1.0 means no zoom)
	// offsets are relative to center of image and refer to original image size (not zoomed)
	CProcessParams(int nTargetWidth, int nTargetHeight, int nRotation, double dZoom, CPoint offsets,
		const CImageProcessingParams& imageProcParams,
		EProcessingFlags eProcFlags) : ImageProcParams(imageProcParams) {
		TargetWidth = nTargetWidth;
		TargetHeight = nTargetHeight;
		Rotation = nRotation;
		Zoom = dZoom;
		Offsets = offsets;
		ProcFlags = eProcFlags;
	}

	int TargetWidth;
	int TargetHeight;
	int Rotation;
	double Zoom;
	CPoint Offsets;
	CImageProcessingParams ImageProcParams;
	EProcessingFlags ProcFlags;
};
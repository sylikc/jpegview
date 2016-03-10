#pragma once

#include "Helpers.h"

// Processing flags for image processing
enum EProcessingFlags {
	PFLAG_None = 0,
	PFLAG_AutoContrast = 1,
	PFLAG_AutoContrastSection = 2,
	PFLAG_LDC = 4, // Local density correction
	PFLAG_HighQualityResampling = 8,
	PFLAG_KeepParams = 16, // Keep parameters between images
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

	double Contrast; // in [-0.5..0.5], 0 means no contrast enhancement
	double Gamma; // in [0.5..2], 1 means no gamma (brightness) correction
	double Saturation; // in [0..2], 1 means saturation unchanged
	double Sharpen; // in [0..0.5], 0 means no sharpening
	double ColorCorrectionFactor; // in [-0.5..0.5], 0 means no color correction
	double ContrastCorrectionFactor; // in [0..1], 0 means no contrast correction
	double LightenShadows; // in [0..1], 0 means no correction
	double DarkenHighlights; // in [0..1], 0 means no correction
	double LightenShadowSteepness; // in [0..1], 0 means no correction
	double CyanRed; // in [-1..1], 0 means no color correction
	double MagentaGreen; // in [-1..1], 0 means no color correction
	double YellowBlue; // in [-1..1], 0 means no color correction
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

	double Radius; // in pixels
	double Amount; // in [0..10]
	double Threshold; // in [0..20]
};

// Processing flags for free rotation
enum ERotationFlags {
	RFLAG_None = 0,
	RFLAG_AutoCrop = 1,
	RFLAG_KeepAspectRatio = 2
};

static inline ERotationFlags SetRotationFlag(ERotationFlags eFlags, ERotationFlags eFlagToSet, bool bValue) {
	return bValue ? (ERotationFlags)(eFlags | eFlagToSet) : (ERotationFlags)(eFlags & ~eFlagToSet);
};

static inline bool GetRotationFlag(ERotationFlags eFlags, ERotationFlags eFlagToGet) {
	return (eFlags & eFlagToGet) != 0;
}

// Parameters used for image rotation
class CRotationParams {
public:
	explicit CRotationParams(int rotation) {
		Rotation = rotation;
		FreeRotation = 0;
		Flags = RFLAG_None;
	}

	CRotationParams(int rotation, double freeRotation, ERotationFlags flags) {
		Rotation = rotation;
		FreeRotation = freeRotation;
		Flags = flags;
	}

	CRotationParams(const CRotationParams& other) {
		Rotation = other.Rotation;
		FreeRotation = other.FreeRotation;
		Flags = other.Flags;
	}

	CRotationParams(const CRotationParams& other, int rotation) {
		Rotation = rotation;
		FreeRotation = other.FreeRotation;
		Flags = other.Flags;
	}

	int Rotation; // Main rotation in degrees, must be in [0, 90, 180, 270]
	double FreeRotation; // Free rotation in degrees, added to Rotation, must be in [-359...360]
	ERotationFlags Flags;
};

// Parameters used to process an image, including geometry
class CProcessParams {
public:
	// TargetWidth,TargetHeight is the dimension of the target output window, the image is fit into this rectangle
	// nRotation must be 0, 90, 270 degrees
	// dZoom is the zoom factor compared to intial image size (1.0 means no zoom)
	// offsets are relative to center of image and refer to original image size (not zoomed)
	CProcessParams(int nTargetWidth, int nTargetHeight,
		CSize monitorSize,
		const CRotationParams& rotationParams,
		double dZoom,
		Helpers::EAutoZoomMode eAutoZoomMode, CPoint offsets,
		const CImageProcessingParams& imageProcParams,
		EProcessingFlags eProcFlags) : ImageProcParams(imageProcParams), RotationParams(rotationParams) {
		TargetWidth = nTargetWidth;
		TargetHeight = nTargetHeight;
		MonitorSize = monitorSize;
		Zoom = dZoom;
		AutoZoomMode = eAutoZoomMode;
		Offsets = offsets;
		ProcFlags = eProcFlags;
	}

	int TargetWidth;
	int TargetHeight;
	CSize MonitorSize;
	CRotationParams RotationParams;
	double Zoom;
	CPoint Offsets;
	CImageProcessingParams ImageProcParams;
	EProcessingFlags ProcFlags;
	Helpers::EAutoZoomMode AutoZoomMode;
};
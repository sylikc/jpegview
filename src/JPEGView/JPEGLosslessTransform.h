#pragma once

// Class that performs lossless JPEG transformations using the TJPEG library
class CJPEGLosslessTransform
{

public:

	// Results of the lossless transformation
	enum EResult {
		Success,
		ReadFileFailed,
		WriteFileFailed,
		TransformationFailed
	};

	// Lossless transformations
	enum ETransformation {
		Rotate90,
		Rotate180,
		Rotate270,
		MirrorH,
		MirrorV
	};

	// Performs a lossless JPEG transformation, transforming the input file and writing the result to the output file.
	// Input and output file can be identical, then the input file is overriden by the resulting output file.
	static EResult PerformTransformation(LPCTSTR sInputFile, LPCTSTR sOutputFile, ETransformation transformation, bool bAllowTrim);

	// Performs a lossless JPEG crop, using the input file and writing the result to the output file.
	// Input and output file can be identical, then the input file is overriden by the resulting output file.
	static EResult PerformCrop(LPCTSTR sInputFile, LPCTSTR sOutputFile, const CRect& cropRect);
};
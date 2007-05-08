#pragma once

// Maximal length of filter kernels. The kernels may be shorter but never longer.
#define MAX_FILTER_LEN 16

struct FilterKernel {
	int16 Kernel[MAX_FILTER_LEN];
	int FilterLen;
	int FilterOffset; // the offset is stored as a number >= 0 and must be subracted! from the filter position to get the start of the filter
};

// for SSE, we need 8 repeations of each kernel element (128 bit)
struct XMMKernelElement {
	int16 valueRepeated[8];
};

struct XMMFilterKernel {
	int FilterLen;
	int FilterOffset;
	int nPad1, nPad2; // padd to 16 bytes before kernel starts
	XMMKernelElement Kernel[1]; // this is a placehoder for a kernel of FilterLen elements
};

struct ResizeFilterKernels {
	FilterKernel * Kernels; // size is NumKernels
	FilterKernel** Indices; // size equals target size, each entry point to a kernel in the Kernels array
	int NumKernels; // this is NUM_KERNELS_RESIZE + border handling kernels as needed
};

struct XMMResizeFilterKernels {
	XMMFilterKernel * Kernels;
	XMMFilterKernel** Indices; // Length equals target size
	int NumKernels; // this is NUM_KERNELS_RESIZE + border handling kernels as needed
	uint8* UnalignedMemory; // do not use directly
};

// Class for a (resize) filter. The filters are one dimensional FIR filters.
class CResizeFilter
{
public:
	enum {
		// 1.0 in our fixed point format
		FP_ONE = 16383
	};

	// dSharpen must be in [0..0.5]. It is ignored for upsampling kernels.
	CResizeFilter(int nSourceSize, int nTargetSize, double dSharpen);
	~CResizeFilter(void);

	// Calulates a set of kernels for resizing from the source size to the target size.
	// The returned filter must not be deleted by the caller and is only valid during the
	// lifetime of the CResizeFilter object that generated it.
	ResizeFilterKernels CalculateFilterKernels(EFilterType eFilter);

	// As above, returns the structure suitable for XMM processing with aligned memory.
	// Lifetime: see above
	XMMResizeFilterKernels CalculateXMMFilterKernels(EFilterType eFilter);

private:
	int m_nSourceSize, m_nTargetSize;
	double m_dSharpen;
	double m_dMultX;
	int m_nFilterLen;
	int m_nFilterOffset;
	int16 m_Filter[MAX_FILTER_LEN];
	ResizeFilterKernels m_kernels;
	XMMResizeFilterKernels m_kernelsXMM;
	bool m_bCalculated;
	bool m_bXMMCalculated;

	void CalculateFilterParams(EFilterType eFilter);
	double EvaluateKernel(double dX, EFilterType eFilter);
	double EvaluateCubicFilterKernel(double dFrac, int nKernelElement);
	int16* GetFilter(uint16 nFrac, EFilterType eFilter);
};

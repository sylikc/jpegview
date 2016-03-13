#pragma once

// Maximal length of filter kernels. The kernels may be shorter but never longer.
#define MAX_FILTER_LEN 16

enum FilterSIMDType {
	FilterSIMDType_None, // filter is not for SIMD processing
	FilterSIMDType_SSE, // filter is for SSE (and MMX) 128 bit SIMD
	FilterSIMDType_AVX // filter is for AVX 256 bit SIMD
};

struct FilterKernel {
	int16 Kernel[MAX_FILTER_LEN]; // elements are fixed point numbers, typically with 2.14 format
	int FilterLen; // actual filter length
	int FilterOffset; // the offset is stored as a number >= 0 and must be subracted! from the calculated position to get the start position for applying the filter
};

// Indexed block of filter kernels, assigning a filter kernel to indices [1..t], allowing sharing the same
// kernel for multiple indices. Filter for index t is: block.Indices[t]
struct FilterKernelBlock {
	FilterKernel * Kernels; // size is NumKernels
	FilterKernel** Indices; // size equals size of target, each entry points to a kernel in the Kernels array
	int NumKernels; // this is NUM_KERNELS_RESIZE + border handling kernels as needed
};

// Filter kernel and filter kernel block for SSE (SIMD).
// For SSE, we need 8 repetitions of each kernel element (128 bit in total, SSE register size)
struct XMMKernelElement {
	int16 valueRepeated[8];
};

struct XMMFilterKernel {
	int FilterLen;
	int FilterOffset;
	int nPad1, nPad2; // padd to 16 bytes before kernel starts
	XMMKernelElement Kernel[1]; // this is a placehoder for a kernel of FilterLen elements
};

struct XMMFilterKernelBlock {
	XMMFilterKernel * Kernels;
	XMMFilterKernel** Indices; // Length equals target size
	int NumKernels; // this is NUM_KERNELS_RESIZE + border handling kernels as needed
	uint8* UnalignedMemory; // do not use directly
};

// Filter kernel and filter kernel block for AVX (SIMD).
// For AVX, we need 16 repetitions of each kernel element (256 bit in total, AVX register size)
struct AVXKernelElement {
	int16 valueRepeated[16];
};

struct AVXFilterKernel {
	int FilterLen;
	int FilterOffset;
	int pad[6]; // padd to 32 bytes before kernel starts
	AVXKernelElement Kernel[1]; // this is a placehoder for a kernel of FilterLen elements
};

struct AVXFilterKernelBlock {
	AVXFilterKernel * Kernels;
	AVXFilterKernel** Indices; // Length equals target size
	int NumKernels; // this is NUM_KERNELS_RESIZE + border handling kernels as needed
	uint8* UnalignedMemory; // do not use directly
};


// Class for resize filters. These filters are one dimensional FIR filters. Because these filters are separable,
// resizing a 2D image can be done by applying a CResizeFilter to all x-rows, then another CResizeFilter to the
// y-columns of the resulting image.
// Resize filters are represented as filter blocks. In a filter block, for each index t in the target 1D array, 
// a filter kernel is present. The element t in the 1D target array is calculated by applying the filter with
// index t to position (s - filter[t].FilterOffset) in the source image, where s is the integer position in the
// source array corresponding to position t in the target array.
class CResizeFilter
{
public:
	enum {
		// 1.0 in our fixed point format (thus using 2.14 FP format)
		FP_ONE = 16383
	};

	// Creates a filter for resizing from nSourceSize to nTargetSize 
	// dSharpen must be in [0..0.5] - it is ignored for upsampling kernels.
	CResizeFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, FilterSIMDType filterSIMDType);
	~CResizeFilter();

	// Gets a block of kernels for resizing from the source size to the target size.
	// The returned filter must not be deleted by the caller and is only valid during the
	// lifetime of the CResizeFilter object that generated it.
	const FilterKernelBlock& GetFilterKernels() const { return m_kernels; }

	// As above, returns the structure suitable for XMM processing with aligned memory.
	// CResizeFilter must have been created with XMM support (FilterSIMDType_SSE)
	const XMMFilterKernelBlock& GetXMMFilterKernels() const { assert(m_filterSIMDType == FilterSIMDType_SSE); return m_kernelsXMM; }

	// As above, returns the structure suitable for AVX2 processing with aligned memory.
	// CResizeFilter must have been created with AVX2 support (FilterSIMDType_AVX)
	const AVXFilterKernelBlock& GetAVXFilterKernels() const { assert(m_filterSIMDType == FilterSIMDType_AVX); return m_kernelsAVX; }

	// Get bicubic filter kernels for fractional positions. These kernels have length 4 and must be applied with offset -1 to current integer position.
	// E.g. when requesting 33 kernels, the kernel for fractional position 0.5 is starting at pKernels[4 * 16]
	static void GetBicubicFilterKernels(int nNumKernels, int16* pKernels);

private:
	friend class CResizeFilterCache;

	int m_nSourceSize, m_nTargetSize;
	double m_dSharpen;
	EFilterType m_eFilter;
	double m_dMultX;
	int m_nFilterLen;
	int m_nFilterOffset;
	int16 m_Filter[MAX_FILTER_LEN];
	FilterKernelBlock m_kernels;
	XMMFilterKernelBlock m_kernelsXMM;
	AVXFilterKernelBlock m_kernelsAVX;
	FilterSIMDType m_filterSIMDType;
	int m_nRefCnt;

	void CalculateFilterKernels();
	void CalculateXMMFilterKernels();
	void CalculateAVXFilterKernels();

	// Checks if this filter matches the given parameters
	bool ParametersMatch(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, FilterSIMDType filterSIMDType);

	void CalculateFilterParams(EFilterType eFilter);
	int16* GetFilter(uint16 nFrac, EFilterType eFilter);
};

// Caches the last used resize filters (LRU cache)
class CResizeFilterCache
{
public:
	// Singleton instance
	static CResizeFilterCache& This();

	// Gets filter
	const CResizeFilter& GetFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, FilterSIMDType filterSIMDType);
	// Release filter
	void ReleaseFilter(const CResizeFilter& filter);

private:
	static CResizeFilterCache* sm_instance;

	CRITICAL_SECTION m_csList; // access to list must be thread safe
	std::list<CResizeFilter*> m_filterList;

	CResizeFilterCache();
	~CResizeFilterCache();
	static void Delete() { delete sm_instance; }
};

// Helper class for accessing filters from filter cache, automatically releasing the filter when object goes out of scope
class CAutoFilter {
public:
	CAutoFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter) 
		: m_filter(CResizeFilterCache::This().GetFilter(nSourceSize, nTargetSize, dSharpen, eFilter, FilterSIMDType_None)) {}
	
	const FilterKernelBlock& Kernels() { return m_filter.GetFilterKernels(); }
	
	~CAutoFilter() { CResizeFilterCache::This().ReleaseFilter(m_filter); }
private:
	const CResizeFilter& m_filter;
};

// Helper class for accessing filters from filter cache, automatically releasing the filter when object goes out of scope
class CAutoXMMFilter {
public:
	CAutoXMMFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter) 
		: m_filter(CResizeFilterCache::This().GetFilter(nSourceSize, nTargetSize, dSharpen, eFilter, FilterSIMDType_SSE)) {}
	
	const XMMFilterKernelBlock& Kernels() { return m_filter.GetXMMFilterKernels(); }
	
	~CAutoXMMFilter() { CResizeFilterCache::This().ReleaseFilter(m_filter); }
private:
	const CResizeFilter& m_filter;
};

// Helper class for accessing filters from filter cache, automatically releasing the filter when object goes out of scope
class CAutoAVXFilter {
public:
	CAutoAVXFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter)
		: m_filter(CResizeFilterCache::This().GetFilter(nSourceSize, nTargetSize, dSharpen, eFilter, FilterSIMDType_AVX)) {}

	const AVXFilterKernelBlock& Kernels() { return m_filter.GetAVXFilterKernels(); }

	~CAutoAVXFilter() { CResizeFilterCache::This().ReleaseFilter(m_filter); }
private:
	const CResizeFilter& m_filter;
};

// Gauss filter (low pass filter). This filter is not a resize filter.
class CGaussFilter {
public:
	// Calculate 1D Gauss filter block of specified size, with border handling.
	// Radius is in pixels.
	CGaussFilter(int nSourceSize, double dRadius);
	~CGaussFilter(void);

	// Gets a set of kernels for applying a 1D Gauss filter to an image of size give in constructor.
	// Gauss is a separable filter, thus an 1D filter can be applied to X, then another 1D filter to Y.
	// The returned filter must not be deleted by the caller
	const FilterKernelBlock& GetFilterKernels() const { return m_kernels; }

	enum {
		// 1.0 in our fixed point format, thus using 2.14 FP format
		FP_ONE = 16383
	};
private:
	int m_nSourceSize;
	double m_dRadius;
	FilterKernelBlock m_kernels;

	void CalculateKernels();
	static FilterKernel CalculateKernel(double dRadius);
};
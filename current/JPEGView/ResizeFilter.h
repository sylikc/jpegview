#pragma once

// Maximal length of filter kernels. The kernels may be shorter but never longer.
#define MAX_FILTER_LEN 16

struct FilterKernel {
	int16 Kernel[MAX_FILTER_LEN];
	int FilterLen; // actual filter length
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

struct FilterKernelBlock {
	FilterKernel * Kernels; // size is NumKernels
	FilterKernel** Indices; // size equals size of target image, each entry points to a kernel in the Kernels array
	int NumKernels; // this is NUM_KERNELS_RESIZE + border handling kernels as needed
};

struct XMMFilterKernelBlock {
	XMMFilterKernel * Kernels;
	XMMFilterKernel** Indices; // Length equals target size
	int NumKernels; // this is NUM_KERNELS_RESIZE + border handling kernels as needed
	uint8* UnalignedMemory; // do not use directly
};

// Class for a resize filter. The filters are one dimensional FIR filters.
class CResizeFilter
{
public:
	enum {
		// 1.0 in our fixed point format
		FP_ONE = 16383
	};

	// dSharpen must be in [0..0.5]. It is ignored for upsampling kernels.
	CResizeFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, bool bXMM);
	~CResizeFilter();

	// Gets a set of kernels for resizing from the source size to the target size.
	// The returned filter must not be deleted by the caller and is only valid during the
	// lifetime of the CResizeFilter object that generated it.
	const FilterKernelBlock& GetFilterKernels() const { return m_kernels; }

	// As above, returns the structure suitable for XMM processing with aligned memory.
	// Object must have been created with XMM support
	const XMMFilterKernelBlock& GetXMMFilterKernels() const { assert(m_bXMMCalculated); return m_kernelsXMM; }

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
	bool m_bCalculated;
	bool m_bXMMCalculated;
	int m_nRefCnt;

	void CalculateFilterKernels();
	void CalculateXMMFilterKernels();

	// Checks if this filter matches the given parameters
	bool ParametersMatch(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, bool bXMM);

	void CalculateFilterParams(EFilterType eFilter);
	double EvaluateKernelIntegrated(double dX, EFilterType eFilter);
	double EvaluateCubicFilterKernel(double dFrac, int nKernelElement);
	int16* GetFilter(uint16 nFrac, EFilterType eFilter);
};

// Caches the last used resize filters (LRU cache)
class CResizeFilterCache
{
public:
	// Singleton instance
	static CResizeFilterCache& This();

	// Gets filter
	const CResizeFilter& GetFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter, bool bXMM);
	// Release filter
	void ReleaseFilter(const CResizeFilter& filter);

private:
	static CResizeFilterCache* sm_instance;

	CRITICAL_SECTION m_csList; // access to list must be MT save
	std::list<CResizeFilter*> m_filterList;

	CResizeFilterCache();
	~CResizeFilterCache();
	static void Delete() { delete sm_instance; }
};

// Helper class for accessing filters from filter cache, automatically releasing the filter when object goes out of scope
class CAutoFilter {
public:
	CAutoFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter) 
		: m_filter(CResizeFilterCache::This().GetFilter(nSourceSize, nTargetSize, dSharpen, eFilter, false)) {}
	
	const FilterKernelBlock& Kernels() { return m_filter.GetFilterKernels(); }
	
	~CAutoFilter() { CResizeFilterCache::This().ReleaseFilter(m_filter); }
private:
	const CResizeFilter& m_filter;
};

// Helper class for accessing filters from filter cache, automatically releasing the filter when object goes out of scope
class CAutoXMMFilter {
public:
	CAutoXMMFilter(int nSourceSize, int nTargetSize, double dSharpen, EFilterType eFilter) 
		: m_filter(CResizeFilterCache::This().GetFilter(nSourceSize, nTargetSize, dSharpen, eFilter, true)) {}
	
	const XMMFilterKernelBlock& Kernels() { return m_filter.GetXMMFilterKernels(); }
	
	~CAutoXMMFilter() { CResizeFilterCache::This().ReleaseFilter(m_filter); }
private:
	const CResizeFilter& m_filter;
};

// Gauss filter (bluring image)
class CGaussFilter {
public:
	CGaussFilter(int nSourceSize, double dRadius);
	~CGaussFilter(void);

	// Gets a set of kernels for applying a Gauss filter to an image of given size.
	// The returned filter must not be deleted by the caller
	const FilterKernelBlock& GetFilterKernels() const { return m_kernels; }

	enum {
		// 1.0 in our fixed point format
		FP_ONE = 16383
	};
private:
	int m_nSourceSize;
	double m_dRadius;
	FilterKernelBlock m_kernels;

	void CalculateKernels();
	static FilterKernel CalculateKernel(double dRadius);
};
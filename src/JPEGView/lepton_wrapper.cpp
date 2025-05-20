#include "stdafx.h"
#include "lepton_wrapper.h"
#include "SettingsProvider.h"

bool lepton_wrapper::LeptonSupport()
{
#ifdef _WIN64
	return (::GetFileAttributes(GetLibPath()) != INVALID_FILE_ATTRIBUTES);
#else
	// We don't have an x86 variant neither of "lepton_jpeg.dll" nor "lepton_jpeg_avx2.dll".
	return false;
#endif
}

const lepton_wrapper::lib_lepton& lepton_wrapper::lib_lepton::get()
{
	static lib_lepton lepton(GetLibPath());
	return lepton;
}

CString lepton_wrapper::GetLibPath()
{
	return CString(CSettingsProvider::This().GetEXEPath()) + CSettingsProvider::This().LeptonLibName();
}

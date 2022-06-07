#include "stdafx.h"
#include "LepLoader.h"
#include "SettingsProvider.h"

bool LepLoader::LeptonToolPresent()
{
	return (::GetFileAttributes(GetToolPath()) != INVALID_FILE_ATTRIBUTES);
}

CString LepLoader::GetToolPath()
{
	return CString(CSettingsProvider::This().GetEXEPath()) + CSettingsProvider::This().LeptonToolName();
}

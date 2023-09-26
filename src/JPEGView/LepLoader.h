#pragma once

class LepLoader
{
public:
	static bool LeptonToolPresent();

	// Gets the path where the global INI file and the EXE is located
	static CString GetToolPath();
};


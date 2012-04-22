#pragma once

// Support for multiple-monitor systems
class CMultiMonitorSupport
{
public:
	// returns true if the system is a multi monitor system
	static bool IsMultiMonitorSystem();

	// virtual desktop (spanning all monitors)
	static CRect GetVirtualDesktop();

	// Display rectangle of the monitor, use -1 for largest monitor, 0 for primary monitor or 
	// 1 to n for the secondary or other monitors
	static CRect GetMonitorRect(int nIndex);

	// Gets the working rectangle (monitor rectangle excluding taskbar) of the monitor the given window is displayed on
	static CRect GetWorkingRect(HWND hWnd);

	// Gets the default window rectangle for windowed mode
	static CRect GetDefaultWindowRect();

    // Sets teh default window rectangle for windowed mode
    static void SetDefaultWindowRect(CRect rect);

	// Gets the default client rectangle for windowed mode
	static CRect GetDefaultClientRectInWindowMode(bool bAutoFitWndToImage);

private:
	CMultiMonitorSupport(void);
};

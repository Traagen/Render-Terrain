/*
Window.cpp

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Class for creating a window in a Win32 application.
*/
#include "Window.h"

using namespace window;

Window::Window(LPCWSTR appName, int height, int width, WNDPROC WndProc, bool isFullscreen) {
	m_nameApp = appName;
	m_hWindow = height;
	m_wWindow = width;
	m_isFullscreen = isFullscreen;

	// Get the instance of this application.
	m_Instance = GetModuleHandle(NULL);

	// Setup the windows class with default settings.
	WNDCLASSEX wc;
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_Instance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = m_nameApp;
	wc.cbSize = sizeof(WNDCLASSEX);

	// Register the window class.
	if (RegisterClassEx(&wc) == 0) {
		throw Window_Exception("Failed to RegisterClassEx on window init.");
	}

	// Setup the screen settings depending on whether it is running in full screen or in windowed mode.
	DEVMODE dmScreenSettings;
	int posX, posY, h, w;

	// Determine the resolution of the clients desktop screen.
	h = GetSystemMetrics(SM_CYSCREEN);
	w = GetSystemMetrics(SM_CXSCREEN);

	if (m_isFullscreen) {
		// If full screen set, the screen to maximum size of the users desktop and 32bit.
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsHeight = (unsigned long)h;
		dmScreenSettings.dmPelsWidth = (unsigned long)w;
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		// Change the display settings to full screen.
		if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
			throw Window_Exception("ChangeDisplaySettings for fullscreen on window init failed.");
		}
		// Set the position of the window to the top left corner.
		posX = posY = 0;
		m_hWindow = h;
		m_wWindow = w;
	}
	else {
		// Place the window in the middle of the screen.
		posX = (w - m_wWindow) / 2;
		posY = (h - m_hWindow) / 2;
		// set the height and width to what we want the window to be.
		h = m_hWindow;
		w = m_wWindow;
	}

	// Create the window with the screen settings and get the handle to it.
	m_Window = CreateWindowEx(WS_EX_APPWINDOW, m_nameApp, m_nameApp, WS_OVERLAPPEDWINDOW, posX, posY, w, h, NULL, NULL, m_Instance, NULL);
	if (!m_Window) {
		throw Window_Exception("Failed to CreateWindowEx on window init.");
	}

	// Bring the window up on the screen and set it as main focus.
	ShowWindow(m_Window, SW_SHOW); // returns 0 if window was previously hidden, non-zero if it wasn't. We don't care so ignoring return value.
	SetForegroundWindow(m_Window); // returns zero if window already in foreground, non-zero if it wasn't. We don't care so ignoring return value.

								 // Hide the mouse cursor.
	ShowCursor(false); // returns the current display count. We don't really care, so we're ignoring it, but it should be zero.
}

Window::~Window() {
	// Show the mouse cursor.
	ShowCursor(true);

	// Fix the display settings if leaving full screen mode.
	if (m_isFullscreen) {
		ChangeDisplaySettings(NULL, 0);
	}

	// Remove the window.
	DestroyWindow(m_Window);
	m_Window = NULL;

	// Remove the application instance.
	UnregisterClass(m_nameApp, m_Instance);
	m_Instance = NULL;
}

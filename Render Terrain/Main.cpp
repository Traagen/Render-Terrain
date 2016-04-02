#include <Windows.h>
#include <assert.h>
#include "Graphics.h"

using namespace std;
using namespace graphics;

enum InitWinFailStates { OK = 1, RCX = -1, CDS = -2, CWX = -3};

static const int		WINDOW_HEIGHT = 1080;	// dimensions for the window we're making.
static const int		WINDOW_WIDTH = 1920;
static const bool		FULL_SCREEN = false;
static const LPCWSTR	appName = L"Directx 12 Terrain Renderer";
static HINSTANCE		thisInstance;
static HWND				window;

static void KeyUp(UINT key) {
	switch (key) {
		case VK_ESCAPE:
			PostQuitMessage(0);
			return;
	}
}
static LRESULT CALLBACK WndProc(HWND win , UINT msg, WPARAM wp, LPARAM lp) {
	switch (msg) {
		case WM_DESTROY:
		case WM_CLOSE:
			PostQuitMessage(0);
			return 0;
		case WM_KEYUP:
			KeyUp((UINT)wp);
			break;
		default:
			return DefWindowProc(win, msg, wp, lp);
	}

	return 0;
}
InitWinFailStates InitWin() {
	// Get the instance of this application.
	thisInstance = GetModuleHandle(NULL);

	// Setup the windows class with default settings.
	WNDCLASSEX wc;
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = thisInstance;
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hIconSm = wc.hIcon;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = appName;
	wc.cbSize = sizeof(WNDCLASSEX);

	// Register the window class.
	if (RegisterClassEx(&wc) == 0) return RCX; // if registering the window class fails, kick out.

	// Setup the screen settings depending on whether it is running in full screen or in windowed mode.
	DEVMODE dmScreenSettings;
	int posX, posY, height, width;

	// Determine the resolution of the clients desktop screen.
	height = GetSystemMetrics(SM_CYSCREEN);
	width = GetSystemMetrics(SM_CXSCREEN);
	
	if (FULL_SCREEN) {
		// If full screen set, the screen to maximum size of the users desktop and 32bit.
		memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsHeight = (unsigned long)height;
		dmScreenSettings.dmPelsWidth = (unsigned long)width;
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

		// Change the display settings to full screen.
		if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) return CDS; // if changing the display settings fails, kick out.

		// Set the position of the window to the top left corner.
		posX = posY = 0;
	}
	else {
		// Place the window in the middle of the screen.
		posX = (width - WINDOW_WIDTH) / 2;
		posY = (height - WINDOW_HEIGHT) / 2;
		// set the height and width to what we want the window to be.
		height = WINDOW_HEIGHT;
		width = WINDOW_WIDTH;
	}

	// Create the window with the screen settings and get the handle to it.
	window = CreateWindowEx(WS_EX_APPWINDOW, appName, appName, WS_POPUP, posX, posY, width, height, NULL, NULL, thisInstance, NULL);
	if (!window) return CWX;

	// Bring the window up on the screen and set it as main focus.
	ShowWindow(window, SW_SHOW); // returns 0 if window was previously hidden, non-zero if it wasn't. We don't care so ignoring return value.
	SetForegroundWindow(window); // returns zero if window already in foreground, non-zero if it wasn't. We don't care so ignoring return value.

	// Hide the mouse cursor.
	ShowCursor(false); // returns the current display count. We don't really care, so we're ignoring it, but it should be zero.

	return OK;
}
void KillWin() {
	// Show the mouse cursor.
	ShowCursor(true);

	// Fix the display settings if leaving full screen mode.
	if (FULL_SCREEN) {
		ChangeDisplaySettings(NULL, 0);
	}

	// Remove the window.
	DestroyWindow(window);
	window = NULL;

	// Remove the application instance.
	UnregisterClass(appName, thisInstance);
	thisInstance = NULL;

	return;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, PSTR cmdLine, int cmdShow) {
	InitWinFailStates fs;
	if ((fs = InitWin()) != OK) { // this should trip if InitWin fails, then we can check fs against the possible fail state returns of InitWin.
		switch (fs) {
			case RCX: OutputDebugString(L"Error registering classex."); break;
			case CDS: OutputDebugString(L"Error changing display settings."); break;
			case CWX: OutputDebugString(L"Error creating window."); break;
		}

		return 0;
	}

	try {
		Graphics GFX(WINDOW_HEIGHT, WINDOW_WIDTH, window, FULL_SCREEN);
	
		MSG msg;
		ZeroMemory(&msg, sizeof(MSG));

		while (1) {
			if (PeekMessage(&msg, window, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			} 
			if (msg.message == WM_QUIT) { 
				KillWin();
				return 1;
			}

			GFX.Render();
		}

		return 2;
	} catch (GFX_Exception& e) {
		OutputDebugStringA(e.what());
		KillWin();
		return 3;
	}
}
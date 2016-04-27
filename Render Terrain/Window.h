#pragma once

#include <Windows.h>
#include <stdexcept>

namespace window {
	class Window_Exception : public std::runtime_error {
	public:
		Window_Exception(const char *msg) : std::runtime_error(msg) {}
	};

	class Window
	{
	public:
		Window(LPCWSTR appName, int height, int width, WNDPROC WndProc, bool isFullscreen);
		~Window();

		HWND GetWindow() { return mWindow; }

	private:
		HINSTANCE mInstance;
		HWND mWindow;
		bool mFullscreen;
		int mHeight;
		int mWidth;
		LPCWSTR mAppName;
	};
}
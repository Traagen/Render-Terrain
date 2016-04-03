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
		Window(LPCWSTR, int, int, WNDPROC, bool);
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
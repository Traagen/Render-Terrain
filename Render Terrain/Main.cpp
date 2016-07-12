/*
Main.cpp

Author:			Chris Serson
Last Edited:	July 6, 2016

Description:	Render Terrain - Win32/DirectX 12 application.
				Loads and displays a heightmap as a terrain.
				Press T to toggle between 2D or 3D view.
*/
#include "Window.h"
#include "Scene.h"
#include <windowsx.h> // included for mouse input stuff

using namespace std;
using namespace graphics;
using namespace window;

static const LPCWSTR	appName = L"Directx 12 Terrain Renderer";
static const int		WINDOW_HEIGHT = 1080;	// dimensions for the window we're making.
static const int		WINDOW_WIDTH = 1920;
//static const int		WINDOW_HEIGHT = 2160;	// dimensions for the window we're making.
//static const int		WINDOW_WIDTH = 3840;

static const bool		FULL_SCREEN = false;
static Scene*			pScene = nullptr;
static int				lastMouseX = -1;
static int				lastMouseY = -1;

static void KeyUp(UINT key) {
	switch (key) {
		case VK_ESCAPE:
			PostQuitMessage(0);
			return;
	}
}

static void KeyDown(UINT key) {
	switch (key) {
		case _W:
		case _S:
		case _A:
		case _D:
		case _Q:
		case _Z:
		case _1:
		case _2:
		case _3:
			pScene->HandleKeyboardInput(key);
			break;
	}
}

static void HandleMouseMove(LPARAM lp) {
	int x = GET_X_LPARAM(lp);
	int y = GET_Y_LPARAM(lp);

	if (lastMouseX == -1) { // then we haven't actually moved the mouse yet, we're just starting. 
		// do nothing
	} else { // calculate how far we've moved from the last time we updated and pass that info to the scene.
		// clamp the values as well so that when we're in windowed mode, we don't cause massive jumps when the
		// mouse moves out of the window and then back in at a completely different position.
		int moveX = lastMouseX - x;
		moveX = moveX > 20 ? 20 : moveX;
		moveX = moveX < -20 ? -20 : moveX;
		int moveY = lastMouseY - y;
		moveY = moveY > 20 ? 20 : moveY;
		moveY = moveY < -20 ? -20 : moveY;
		pScene->HandleMouseInput(moveX, moveY);
	}
	
	// update our last mouse coordinates;
	lastMouseX = x;
	lastMouseY = y;
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
		case WM_KEYDOWN:
			KeyDown((UINT)wp);
			break;
		case WM_MOUSEMOVE:
			HandleMouseMove(lp);
			break;
		default:
			return DefWindowProc(win, msg, wp, lp);
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, PSTR cmdLine, int cmdShow) {
	try {
		Window WIN(appName, WINDOW_HEIGHT, WINDOW_WIDTH, WndProc, FULL_SCREEN);
		Graphics GFX(WIN.Height(), WIN.Width(), WIN.GetWindow(), FULL_SCREEN);
		Scene S(WIN.Height(), WIN.Width(), &GFX);
		pScene = &S; // create a pointer to the scene for access outside of main.

		MSG msg;
		ZeroMemory(&msg, sizeof(MSG));

		while (1) {
			if (PeekMessage(&msg, WIN.GetWindow(), 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			} 
			if (msg.message == WM_QUIT) { 
				pScene = nullptr;
				return 1;
			}

			S.Draw();
		}

		pScene = nullptr;
		return 2;
	} catch (GFX_Exception& e) {
		OutputDebugStringA(e.what());
		pScene = nullptr;
		return 3;
	} catch (Window_Exception& e) {
		OutputDebugStringA(e.what());
		pScene = nullptr;
		return 4;
	}
}
/*
Scene.cpp

Author:			Chris Serson
Last Edited:	June 21, 2016

Description:	Class for creating, managing, and rendering a scene.
*/
#include "Scene.h"

Scene::Scene(int height, int width, Graphics* GFX) : T(GFX) {
	mpGFX = GFX;

	// create a viewport and scissor rectangle.
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = (float)width;
	mViewport.Height = (float)height;
	mViewport.MinDepth = 0;
	mViewport.MaxDepth = 1;

	mScissorRect.left = 0;
	mScissorRect.top = 0;
	mScissorRect.right = width;
	mScissorRect.bottom = height;

	// after creating and initializing the heightmap for the terrain, we need to close the command list
	// and tell the Graphics object to execute the command list to actually finish the subresource init.
	CloseCommandLists();
	mpGFX->Render();
}


Scene::~Scene() {
	mpGFX = 0;
}

// Close all command lists. Currently there is only the one.
void Scene::CloseCommandLists() {
	// close the command list.
	if (FAILED(mpGFX->GetCommandList()->Close())) {
		throw GFX_Exception("CommandList Close failed.");
	}
}

void Scene::SetViewport() {
	ID3D12GraphicsCommandList* cmdList = mpGFX->GetCommandList();
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);
}

void Scene::Draw() {
	mpGFX->ResetPipeline();

	// set a clear color.
	const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	mpGFX->SetBackBufferRender(mpGFX->GetCommandList(), clearColor);

	SetViewport();
	T.Draw(mpGFX->GetCommandList());
	mpGFX->SetBackBufferPresent(mpGFX->GetCommandList());
	CloseCommandLists();
	mpGFX->Render();
}
#include "Scene.h"

Scene::Scene(int height, int width, Graphics* GFX) : T(GFX) {
	mpCmdList = GFX->GetCommandList();
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
	mpCmdList = 0; // we don't want to release the command list here as it is owned by the Graphics object. Just set the pointer to null so we know it's free.
	mpGFX = 0;
}

// Close all command lists. Currently there is only the one.
void Scene::CloseCommandLists() {
	// close the command list.
	if (FAILED(mpCmdList->Close())) {
		throw GFX_Exception("CommandList Close failed.");
	}
}

void Scene::SetViewport() {
	mpCmdList->RSSetViewports(1, &mViewport);
	mpCmdList->RSSetScissorRects(1, &mScissorRect);
}

void Scene::Draw() {
	mpGFX->ResetPipeline();
	mpGFX->SetBackBufferRender(mpCmdList);
	SetViewport();
	T.Draw(mpCmdList);
	mpGFX->SetBackBufferPresent(mpCmdList);
	CloseCommandLists();
	mpGFX->Render();
}
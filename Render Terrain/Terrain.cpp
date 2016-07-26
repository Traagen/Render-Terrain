/*
Terrain.cpp

Author:			Chris Serson
Last Edited:	July 26, 2016

Description:	Class for loading a heightmap and rendering as a terrain.
*/
#include "lodepng.h"
#include "Terrain.h"

Terrain::Terrain() {
	mpHeightmap = nullptr;
	mpVertexBuffer = nullptr;
	mpIndexBuffer = nullptr;
	maImage = nullptr;
	maVertices = nullptr;
	maIndices = nullptr;

	LoadHeightMap("heightmap6.png");
	CreateMesh3D();
}

Terrain::~Terrain() {
	// The order resources are released appears to matter. I haven't tested all possible orders, but at least releasing the heap
	// and resources after the pso and rootsig was causing my GPU to hang on shutdown. Using the current order resolved that issue.
	if (mpHeightmap) {
		mpHeightmap->Release();
		mpHeightmap = nullptr;
	}

	if (mpIndexBuffer) {
		mpIndexBuffer->Release();
		mpIndexBuffer = nullptr;
	}

	if (mpVertexBuffer) {
		mpVertexBuffer->Release();
		mpVertexBuffer = nullptr;
	}

	if (maImage) delete[] maImage;

	DeleteVertexAndIndexArrays();
}

void Terrain::Draw(ID3D12GraphicsCommandList* cmdList, bool Draw3D) {
	if (Draw3D) {
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST); // describe how to read the vertex buffer.
		cmdList->IASetVertexBuffers(0, 1, &mVBV);
		cmdList->IASetIndexBuffer(&mIBV);

		cmdList->DrawIndexedInstanced(mIndexCount, 1, 0, 0, 0);
	} else {
		// draw in 2D
		cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // describe how to read the vertex buffer.
		cmdList->DrawInstanced(3, 1, 0, 0);
	}
}

// Once the GPU has completed uploading buffers to GPU memory, we need to free system memory.
void Terrain::DeleteVertexAndIndexArrays() {
	if (maVertices) {
		delete[] maVertices;
		maVertices = nullptr;
	}

	if (maIndices) {
		delete[] maIndices;
		maIndices = nullptr;
	}
}

// generate vertex and index buffers for 3D mesh of terrain
void Terrain::CreateMesh3D() {
	// Create a vertex buffer
	mHeightScale = (float)mWidth / 4.0f;
	int tessFactor = 4;
	int scalePatchX = mWidth / tessFactor;
	int scalePatchY = mDepth / tessFactor;
	mVertexCount = scalePatchX * scalePatchY;

	// create a vertex array 1/4 the size of the height map in each dimension,
	// to be stretched over the height map
	int arrSize = (int)(scalePatchX * scalePatchY);
	maVertices = new Vertex[arrSize];
	for (int y = 0; y < scalePatchY; ++y) {
		for (int x = 0; x < scalePatchX; ++x) {
			maVertices[y * scalePatchX + x].position = XMFLOAT3((float)x * tessFactor, (float)y * tessFactor, maImage[y * mWidth * tessFactor + x * tessFactor] * mHeightScale);
			maVertices[y * scalePatchX + x].tex = XMFLOAT2((float)x / scalePatchX, (float)y / scalePatchY);
		}
	}

	// create an index buffer
	// our grid is scalePatchX * scalePatchY in size.
	// the vertices are oriented like so:
	//  0,  1,  2,  3,  4,
	//  5,  6,  7,  8,  9,
	// 10, 11, 12, 13, 14
	arrSize = (scalePatchX - 1) * (scalePatchY - 1) * 4;
	maIndices = new UINT[arrSize];
	int i = 0;
	for (int y = 0; y < scalePatchY - 1; ++y) {
		for (int x = 0; x < scalePatchX - 1; ++x) {
			UINT vert0 = x + y * scalePatchX;
			UINT vert1 = x + 1 + y * scalePatchX;
			UINT vert2 = x + (y + 1) * scalePatchX;
			UINT vert3 = x + 1 + (y + 1) * scalePatchX;
			maIndices[i++] = vert0;
			maIndices[i++] = vert1;
			maIndices[i++] = vert2;
			maIndices[i++] = vert3;
			
			// now that we have the indices for our patch, we need to calculate the bounding box.
			// z bounds is a bit harder as we need to find the max and min y values in the heightmap for the patch range.
			// store it in the first vertex
			XMFLOAT2 bz = CalcZBounds(maVertices[vert0], maVertices[vert3]);
			maVertices[vert0].boundsZ = bz;
		}
	}

	mIndexCount = arrSize;
}

// calculate the minimum and maximum z values for vertices between the provided bounds.
XMFLOAT2 Terrain::CalcZBounds(Vertex bottomLeft, Vertex topRight) {
	float max = -100000;
	float min = 100000;
	int bottomLeftX = bottomLeft.position.x == 0 ? (int)bottomLeft.position.x: (int)bottomLeft.position.x - 1;
	int bottomLeftY = bottomLeft.position.y == 0 ? (int)bottomLeft.position.y : (int)bottomLeft.position.y - 1;
	int topRightX = topRight.position.x >= mWidth ? (int)topRight.position.x : (int)topRight.position.x + 1;
	int topRightY = topRight.position.y >= mWidth ? (int)topRight.position.y : (int)topRight.position.y + 1;

	for (int y = bottomLeftY; y <= topRightY; ++y) {
		for (int x = bottomLeftX; x <= topRightX; ++x) {
			float z = maImage[x + y * mWidth] * mHeightScale;

			if (z > max) max = z;
			if (z < min) min = z;
		}
	}

	return XMFLOAT2(min, max);
}

// load the specified file containing the heightmap data.
void Terrain::LoadHeightMap(const char* fnHeightMap) {
	// load the black and white heightmap png file. Data is RGBA unsigned char.
	unsigned char* tmpHeightMap;
	unsigned error = lodepng_decode32_file(&tmpHeightMap, &mWidth, &mDepth, fnHeightMap);
	if (error) {
		throw GFX_Exception("Error loading terrain heightmap texture.");
	}

	// Convert the height values to a float.
	maImage = new float[mWidth * mDepth]; // one slot for the height and 3 for the normal at the point.
	// in this first loop, just copy the height value. We're going to scale it here as well.
	for (unsigned int i = 0; i < mWidth * mDepth; ++i) {
		// convert values to float between 0 and 1.
		// store height value as a floating point value between 0 and 1.
		maImage[i] = (float)tmpHeightMap[i * 4] / 255.0f;
	}
	// we don't need the original data anymore.
	delete[] tmpHeightMap; 
}
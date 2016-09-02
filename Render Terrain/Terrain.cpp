/*
Terrain.cpp

Author:			Chris Serson
Last Edited:	September 1, 2016

Description:	Class for loading a heightmap and rendering as a terrain.
*/
#include "lodepng.h"
#include "Terrain.h"

Terrain::Terrain() {
	mpHeightmap = nullptr;
	mpDisplacementMap = nullptr;
	mpDetailMap = nullptr;
	mpVertexBuffer = nullptr;
	mpIndexBuffer = nullptr;
	maImage = nullptr;
	maDispImage = nullptr;
	maVertices = nullptr;
	maIndices = nullptr;

	LoadHeightMap("heightmap6.png");
	LoadDisplacementMap("displacementmap.png", "displacementmapnormals.png");
	LoadDetailMap("rocknormalmap.png");

	CreateMesh3D();
}

Terrain::~Terrain() {
	// The order resources are released appears to matter. I haven't tested all possible orders, but at least releasing the heap
	// and resources after the pso and rootsig was causing my GPU to hang on shutdown. Using the current order resolved that issue.
	if (mpHeightmap) {
		mpHeightmap->Release();
		mpHeightmap = nullptr;
	}

	if (mpDisplacementMap) {
		mpDisplacementMap->Release();
		mpDisplacementMap = nullptr;
	}

	if (mpDetailMap) {
		mpDetailMap->Release();
		mpDetailMap = nullptr;
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
	if (maDispImage) delete[] maDispImage;
	if (maDetailImage) delete[] maDetailImage;

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
	int tessFactor = 8;
	int scalePatchX = mWidth / tessFactor;
	int scalePatchY = mDepth / tessFactor;
	int numVertsInTerrain = scalePatchX * scalePatchY;
	// number of vertices needed for terrain + base vertices for skirt.
	mVertexCount = numVertsInTerrain + scalePatchX * 4;

	// create a vertex array 1/4 the size of the height map in each dimension,
	// to be stretched over the height map
	int arrSize = (int)(mVertexCount);
	maVertices = new Vertex[arrSize];
	for (int y = 0; y < scalePatchY; ++y) {
		for (int x = 0; x < scalePatchX; ++x) {
			maVertices[y * scalePatchX + x].position = XMFLOAT3((float)x * tessFactor, (float)y * tessFactor, maImage[(y * mWidth + x) * tessFactor] * mHeightScale);
			maVertices[y * scalePatchX + x].skirt = 5;
		}
	}

	mBaseHeight = (int)(CalcZBounds(maVertices[0], maVertices[numVertsInTerrain - 1]).x - 10);

	// create base vertices for side 1 of skirt. y = 0.
	int iVertex = numVertsInTerrain;
	for (int x = 0; x < scalePatchX; ++x) {
		maVertices[iVertex].position = XMFLOAT3(x * tessFactor, 0.0f, mBaseHeight);
		maVertices[iVertex++].skirt = 1;
	}

	// create base vertices for side 2 of skirt. y = mDepth - tessFactor.
	for (int x = 0; x < scalePatchX; ++x) {
		maVertices[iVertex].position = XMFLOAT3(x * tessFactor, mDepth - tessFactor, mBaseHeight);
		maVertices[iVertex++].skirt = 2;
	}

	// create base vertices for side 3 of skirt. x = 0.
	for (int y = 0; y < scalePatchY; ++y) {
		maVertices[iVertex].position = XMFLOAT3(0.0f, y * tessFactor, mBaseHeight);
		maVertices[iVertex++].skirt = 3;
	}

	// create base vertices for side 4 of skirt. x = mWidth - tessFactor.
	for (int y = 0; y < scalePatchY; ++y) {
		maVertices[iVertex].position = XMFLOAT3(mWidth - tessFactor, y * tessFactor, mBaseHeight);
		maVertices[iVertex++].skirt = 4;
	}

	// create an index buffer
	// our grid is scalePatchX * scalePatchY in size.
	// the vertices are oriented like so:
	//  0,  1,  2,  3,  4,
	//  5,  6,  7,  8,  9,
	// 10, 11, 12, 13, 14
	// need to add indices for 4 edge skirts (2 x-aligned, 2 y-aligned, 4 indices per patch).
	// + 4 indices for patch representing bottom plane.
	arrSize = (scalePatchX - 1) * (scalePatchY - 1) * 4 + 2 * 4 * (scalePatchX - 1) + 2 * 4 * (scalePatchY - 1) + 4;
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
			maVertices[vert0].aabbmin = XMFLOAT3(maVertices[vert0].position.x, maVertices[vert0].position.y, bz.x);
			maVertices[vert0].aabbmax = XMFLOAT3(maVertices[vert3].position.x, maVertices[vert3].position.y, bz.y);
		}
	}

	// so as not to interfere with the terrain wrt bounds for frustum culling, we need the 0th control point of each skirt patch to be a base vertex as defined above.
	// add indices for side 1 of skirt. y = 0.
	iVertex = numVertsInTerrain;
	for (int x = 0; x < scalePatchX - 1; ++x) {
		maIndices[i++] = iVertex;		// control point 0
		maIndices[i++] = iVertex + 1;	// control point 1
		maIndices[i++] = x;				// control point 2
		maIndices[i++] = x + 1;			// control point 3
		XMFLOAT2 bz = CalcZBounds(maVertices[x], maVertices[x + 1]);
		maVertices[iVertex].aabbmin = XMFLOAT3(x * tessFactor, 0.0f, mBaseHeight);
		maVertices[iVertex++].aabbmax = XMFLOAT3((x + 1) * tessFactor, 0.0f, bz.y);
	}
	// add indices for side 2 of skirt. y = mDepth - tessFactor.
	++iVertex;
	for (int x = 0; x < scalePatchX - 1; ++x) {
		maIndices[i++] = iVertex + 1;
		maIndices[i++] = iVertex;
		int offset = scalePatchX * (scalePatchY - 1);
		maIndices[i++] = x + offset + 1;
		maIndices[i++] = x + offset;
		XMFLOAT2 bz = CalcZBounds(maVertices[x + offset], maVertices[x + offset + 1]);
		maVertices[++iVertex].aabbmin = XMFLOAT3(x * tessFactor, mDepth - tessFactor, mBaseHeight);
		maVertices[iVertex].aabbmax = XMFLOAT3((x + 1) * tessFactor, mDepth - tessFactor, bz.y);
	}
	// add indices for side 3 of skirt. x = 0.
	++iVertex;
	for (int y = 0; y < scalePatchY - 1; ++y) {
		maIndices[i++] = iVertex + 1;
		maIndices[i++] = iVertex;
		maIndices[i++] = (y + 1) * scalePatchX;
		maIndices[i++] = y * scalePatchX;
		XMFLOAT2 bz = CalcZBounds(maVertices[y * scalePatchX], maVertices[(y + 1) * scalePatchX]);
		maVertices[++iVertex].aabbmin = XMFLOAT3(0.0f, y * tessFactor, mBaseHeight);
		maVertices[iVertex].aabbmax = XMFLOAT3(0.0f, (y + 1) * tessFactor, bz.y);
	}
	// add indices for side 4 of skirt. x = mWidth - tessFactor.
	++iVertex;
	for (int y = 0; y < scalePatchY - 1; ++y) {
		maIndices[i++] = iVertex;
		maIndices[i++] = iVertex + 1;
		maIndices[i++] = y * scalePatchX + scalePatchX - 1;
		maIndices[i++] = (y + 1) * scalePatchX + scalePatchX - 1;
		XMFLOAT2 bz = CalcZBounds(maVertices[y * scalePatchX + scalePatchX - 1], maVertices[(y + 1) * scalePatchX + scalePatchX - 1]);
		maVertices[iVertex].aabbmin = XMFLOAT3(mWidth - tessFactor, y * tessFactor, mBaseHeight);
		maVertices[iVertex++].aabbmax = XMFLOAT3(mWidth - tessFactor, (y + 1) * tessFactor, bz.y);
	}
	// add indices for bottom plane.
	maIndices[i++] = numVertsInTerrain + scalePatchX - 1;
	maIndices[i++] = numVertsInTerrain;
	maIndices[i++] = numVertsInTerrain + scalePatchX + scalePatchX - 1;
	maIndices[i++] = numVertsInTerrain + scalePatchX;
	maVertices[numVertsInTerrain + scalePatchX - 1].aabbmin = XMFLOAT3(0.0f, 0.0f, mBaseHeight);
	maVertices[numVertsInTerrain + scalePatchX - 1].aabbmax = XMFLOAT3(mWidth, mDepth, mBaseHeight);
	maVertices[numVertsInTerrain + scalePatchX - 1].skirt = 0;
	
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

void Terrain::LoadDisplacementMap(const char* fnMap, const char* fnNMap) {
	// load the black and white heightmap png file. Data is RGBA unsigned char.
	unsigned char* tmpMap;
	unsigned char* tmpNMap;

	unsigned error = lodepng_decode32_file(&tmpMap, &mDispWidth, &mDispDepth, fnMap);
	if (error) {
		throw GFX_Exception("Error loading terrain displacement map texture.");
	}
	error = lodepng_decode32_file(&tmpNMap, &mDispWidth, &mDispDepth, fnNMap);
	if (error) {
		throw GFX_Exception("Error loading terrain displacement normal map texture.");
	}
	
	// Convert the height values to a float.
	maDispImage = new float[mDispWidth * mDispDepth * 4]; 

	// combine the normal map with the displacement height map so that xyz is normal, w is height
	for (unsigned int i = 0; i < mDispWidth * mDispDepth * 4; i += 4) {
		// convert values to float between 0 and 1.
		maDispImage[i] = (float)tmpNMap[i] / 255.0f;
		maDispImage[i + 1] = (float)tmpNMap[i + 1] / 255.0f;
		maDispImage[i + 2] = (float)tmpNMap[i + 2] / 255.0f;
		maDispImage[i + 3] = (float)tmpMap[i] / 255.0f;
	}
	// we don't need the original data anymore.
	delete[] tmpMap;
	delete[] tmpNMap;
}

void Terrain::LoadDetailMap(const char* fnMap) {
	// load the black and white heightmap png file. Data is RGBA unsigned char.
	unsigned char* tmpMap;
	unsigned error = lodepng_decode32_file(&tmpMap, &mDetailWidth, &mDetailHeight, fnMap);
	if (error) {
		throw GFX_Exception("Error loading terrain detail map texture.");
	}

	// Convert the height values to a float.
	maDetailImage = new float[mDetailWidth * mDetailHeight * 4]; // one slot for the height and 3 for the normal at the point.
										  // in this first loop, just copy the height value. We're going to scale it here as well.
	for (unsigned int i = 0; i < mDetailWidth * mDetailHeight * 4; ++i) {
		// convert values to float between 0 and 1.
		// store height value as a floating point value between 0 and 1.
		maDetailImage[i] = (float)tmpMap[i] / 255.0f;
	}
	// we don't need the original data anymore.
	delete[] tmpMap;
}
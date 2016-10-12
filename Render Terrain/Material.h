/*
Material.h

Author:			Chris Serson
Last Edited:	October 12, 2016

Description:	Classes for creating and managing Direct3D 12 materials.

Usage:			- Proper shutdown is handled by the destructor.
				- Currently just a TerrainMaterial that takes 4 normal maps
					and 4 diffuse maps, as well as 4 colors.

Future Work:	- Add a more generic Material class.
				- Add more material properties, ie specularity.
				- Add methods to access material properties.
*/
#pragma once
#include "ResourceManager.h"

class TerrainMaterial {
public:
	TerrainMaterial(ResourceManager* rm, const char* fnNormals1, const char* fnNormals2, const char* fnNormals3, 
		const char* fnNormals4, const char* fnDiff1, const char* fnDiff2, const char* fnDiff3, const char* fnDiff4,
		XMFLOAT4 colors[4]);
	~TerrainMaterial();

	// Attach our SRV to the provided root descriptor table slot.
	void Attach(ID3D12GraphicsCommandList* cmdList,	unsigned int srvDescTableIndex);
	// return the array of colors.
	XMFLOAT4* GetColors() { return m_listColors; }
private:
	ResourceManager*			m_pResMgr;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hdlTextureSRV_CPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_hdlTextureSRV_GPU;
	XMFLOAT4					m_listColors[4];
};


#pragma once

#include <string>
#include "MeshDataEx.h"

class AssimpImporterEx {
public:
	static bool LoadFBX_PNTT_AndMaterials(
		const std::wstring& path,
		MeshData_PNTT& out,
		bool filpUV = false,
		bool leftHanded = true);
};
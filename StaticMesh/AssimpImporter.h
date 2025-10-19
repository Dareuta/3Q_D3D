#pragma once

#include <string>
#include "MeshData.h"

// Assimp 로더. 파밍만 하는게 특징, 이것도 D3D 모름

class AssimpImporter {
public:
    static bool LoadFBX_PosIndex(
        const std::wstring& path,
        MeshData_PosIndex& out,
        bool flipUV = false,
        bool leftHanded = true);
};
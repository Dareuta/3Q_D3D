#pragma once
#include <vector>
#include <cstdint>

// CPU 중간 구조, D3D의존 없는게 특징

struct VertexCPU_PosOnly {
	float x, y, z;
};

struct SubMeshCPU {
	uint32_t baseVertex = 0;
	uint32_t indexStart = 0;
	uint32_t indexCount = 0;
	uint32_t materialIndex = 0;
};

struct MeshData_PosIndex {
	std::vector<VertexCPU_PosOnly> vertices;
	std::vector<uint32_t>          indices;
	std::vector<SubMeshCPU>        submeshes;
};
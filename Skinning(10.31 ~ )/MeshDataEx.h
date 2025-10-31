#pragma once

#include <vector>
#include <cstdint>
#include <string>

struct VertexCPU_PNTT {
	float px, py, pz;
	float nx, ny, nz;
	float u, v;
	float tx, ty, tz, tw;
};

struct SubMeshCPU {
	uint32_t baseVertex = 0, indexStart = 0, indexCount = 0, materialIndex = 0;
};

struct MaterialCPU {
	std::wstring diffuse, normal, specular, emissive, opacity;
};

struct MeshData_PNTT {
	std::vector<VertexCPU_PNTT> vertices;
	std::vector<uint32_t> indices;
	std::vector<SubMeshCPU> submeshes;
	std::vector<MaterialCPU> materials;
};
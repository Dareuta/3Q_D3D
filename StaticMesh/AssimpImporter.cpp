#include "AssimpImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

//1. 정점 좌표(pos만) 를 out.vertices에 쌓고,
//2. 삼각형 인덱스(버텍스 연결정보) 를 out.indices에 쌓고,
//3. 각 메시별 드로우 범위를 out.submeshes에 기록( baseVertex / indexStart / indexCount / materialIndex )

// 개선 / 주의사항
// 노드 변환(트랜스폼) 적용을 안함 << 회전 이동 스케일 오프셋을 반영하지 못함
// 유니코드 깨질 수 있음
// 2^32 넘어가면 터짐
// 속성(normal/uv/tangent/bitangent/색상/스킨(본/웨이트)/머티리얼-텍스처)는 관리하지 않음

static unsigned MakeFlags(bool flipUV, bool leftHanded) {
	unsigned f = aiProcess_Triangulate
		| aiProcess_JoinIdenticalVertices
		| aiProcess_ImproveCacheLocality
		| aiProcess_SortByPType;

	if (leftHanded) f |= aiProcess_ConvertToLeftHanded;
	if (flipUV) f |= aiProcess_FlipUVs;

	return f;
}

bool AssimpImporter::LoadFBX_PosIndex(
	const std::wstring& path, 
	MeshData_PosIndex& out, 
	bool flipUV, bool leftHanded)
{
	std::string pathA(path.begin(), path.end()); // 범위 생성자, 비ASCII 문자는 깨진다
	Assimp::Importer imp;
	const aiScene* sc = imp.ReadFile(pathA.c_str(), MakeFlags(flipUV, leftHanded));

	if (!sc || !sc->mRootNode) return false;

	size_t totalV = 0, totalI = 0;
	for (unsigned mi = 0; mi < sc->mNumMeshes; ++mi) {
		const aiMesh* m = sc->mMeshes[mi];
		totalV += m->mNumVertices;
		totalI += m->mNumFaces * 3;
	}

	out.vertices.clear(); 	out.indices.clear();	out.submeshes.clear();

	out.vertices.reserve(totalV);
	out.indices.reserve(totalI);
	out.submeshes.reserve(sc->mNumMeshes);

	uint32_t baseV = 0, baseI = 0;

	for (unsigned mi = 0; mi < sc->mNumMeshes; ++mi) {
		const aiMesh* m = sc->mMeshes[mi];
		SubMeshCPU sm{};
		sm.baseVertex = baseV;
		sm.indexStart = baseI;
		sm.materialIndex = m->mMaterialIndex;

		// Vertices (pos값만)
		for (unsigned v = 0; v < m->mNumVertices; ++v) {
			const aiVector3D& p = m->mVertices[v];
			out.vertices.push_back({ p.x, p.y, p.z });

		}

		// indices
		for (unsigned f = 0; f < m->mNumFaces; ++f) {
			const aiFace& face = m->mFaces[f];
			out.indices.push_back(baseV + face.mIndices[0]);
			out.indices.push_back(baseV + face.mIndices[1]);
			out.indices.push_back(baseV + face.mIndices[2]);
		}

		sm.indexCount = m->mNumFaces * 3;
		baseV += m->mNumVertices;
		baseI += sm.indexCount;
		out.submeshes.push_back(sm);
	}

	return true;
}

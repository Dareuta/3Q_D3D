// AssimpImporterEx.cpp
#include "AssimpImporterEx.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>

using std::filesystem::path;

static unsigned MakeFlags(bool flipUV, bool leftHanded) {
    unsigned f = aiProcess_Triangulate
        | aiProcess_JoinIdenticalVertices
        | aiProcess_ImproveCacheLocality
        | aiProcess_SortByPType
        | aiProcess_CalcTangentSpace   // <- 탠젠트/비탠젠트
        | aiProcess_GenNormals;        // 노멀 없으면 생성
    if (leftHanded) f |= aiProcess_ConvertToLeftHanded;
    if (flipUV)     f |= aiProcess_FlipUVs;
    return f;
}

static std::wstring Widen(const aiString& s) {
    std::string a = s.C_Str();
    return std::wstring(a.begin(), a.end()); // (수업 과제 가정: ASCII 파일명)
}

// 파일명만 추출해서 보관 (과제 지침)
static std::wstring FileOnly(const std::wstring& p) {
    return path(p).filename().wstring();
}

bool AssimpImporterEx::LoadFBX_PNTT_AndMaterials(
    const std::wstring& pathW, MeshData_PNTT& out, bool flipUV, bool leftHanded)
{
    std::string pathA(pathW.begin(), pathW.end());
    Assimp::Importer imp;
    const aiScene* sc = imp.ReadFile(pathA.c_str(), MakeFlags(flipUV, leftHanded));
    if (!sc || !sc->mRootNode) return false;

    // 1) Materials (파일명만)
    out.materials.clear();
    out.materials.resize(sc->mNumMaterials);
    auto grabTex = [&](aiMaterial* m, aiTextureType t)->std::wstring {
        aiString p; if (AI_SUCCESS == m->GetTexture(t, 0, &p)) return FileOnly(Widen(p));
        return L"";
        };
    for (unsigned i = 0;i < sc->mNumMaterials;++i) {
        auto* m = sc->mMaterials[i];
        MaterialCPU mc;
        mc.diffuse = grabTex(m, aiTextureType_DIFFUSE);
        mc.normal = grabTex(m, aiTextureType_NORMALS);
        if (mc.normal.empty()) mc.normal = grabTex(m, aiTextureType_HEIGHT); // 일부 툴
        mc.specular = grabTex(m, aiTextureType_SPECULAR);
        mc.emissive = grabTex(m, aiTextureType_EMISSIVE);
        mc.opacity = grabTex(m, aiTextureType_OPACITY);
        out.materials[i] = mc;
    }

    // 2) Vertices / Indices / Submeshes
    size_t totalV = 0, totalI = 0;
    for (unsigned mi = 0; mi < sc->mNumMeshes; ++mi) {
        totalV += sc->mMeshes[mi]->mNumVertices;
        totalI += sc->mMeshes[mi]->mNumFaces * 3;
    }
    out.vertices.clear(); out.indices.clear(); out.submeshes.clear();
    out.vertices.reserve(totalV); out.indices.reserve(totalI);
    out.submeshes.reserve(sc->mNumMeshes);

    uint32_t baseV = 0, baseI = 0;
    for (unsigned mi = 0; mi < sc->mNumMeshes; ++mi) {
        const aiMesh* m = sc->mMeshes[mi];
        SubMeshCPU sm{}; sm.baseVertex = baseV; sm.indexStart = baseI; sm.materialIndex = m->mMaterialIndex;

        for (unsigned v = 0; v < m->mNumVertices; ++v) {
            auto& p = m->mVertices[v];
            aiVector3D n = m->HasNormals() ? m->mNormals[v] : aiVector3D(0, 1, 0);
            aiVector3D t(1, 0, 0); float w = 1.0f;
            if (m->HasTangentsAndBitangents()) { t = m->mTangents[v]; w = 1.0f; }
            aiVector3D uv = m->HasTextureCoords(0) ? m->mTextureCoords[0][v] : aiVector3D(0, 0, 0);
            out.vertices.push_back({ p.x,p.y,p.z, n.x,n.y,n.z, uv.x,uv.y, t.x,t.y,t.z, w });
        }
        for (unsigned f = 0; f < m->mNumFaces; ++f) {
            auto& face = m->mFaces[f];
            out.indices.push_back(baseV + face.mIndices[0]);
            out.indices.push_back(baseV + face.mIndices[1]);
            out.indices.push_back(baseV + face.mIndices[2]);
        }
        sm.indexCount = m->mNumFaces * 3;
        baseV += m->mNumVertices; baseI += sm.indexCount;
        out.submeshes.push_back(sm);
    }
    return true;
}

#pragma once
// SkinnedSkeletal.h (�ű�)
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <directxtk/SimpleMath.h>
#include <d3d11.h>

#include "SkinnedMesh.h"
#include "Material.h"

using namespace DirectX::SimpleMath;

struct SK_Node {
    std::string name;
    int parent = -1;
    std::vector<int> children;
    Matrix bindLocal = Matrix::Identity;
    Matrix poseLocal = Matrix::Identity;
    Matrix poseGlobal = Matrix::Identity;
    std::vector<int> partIndices;
};

struct SK_KeyT { double t; Vector3 v; };
struct SK_KeyR { double t; Quaternion q; };
struct SK_KeyS { double t; Vector3 v; };

struct SK_Channel {
    std::string target;
    std::vector<SK_KeyT> T;
    std::vector<SK_KeyR> R;
    std::vector<SK_KeyS> S;
};

struct SK_Clip {
    std::string name;
    double duration = 0.0;
    double tps = 25.0;
    std::vector<SK_Channel> channels;
    std::unordered_map<std::string, int> map; // target -> channel index
};

struct SK_Bone {
    std::string name;
    int node = -1;         // �� ���� ���ε�� ��� �ε���
    Matrix offset = Matrix::Identity; // aiBone::mOffsetMatrix (inverse bind)
};

struct SK_Part {
    SkinnedMesh mesh;
    std::vector<MaterialGPU> materials;
    int ownerNode = -1;     // ��Ʈ�� �ٴ� ���
};

class SkinnedSkeletal {
public:
    static std::unique_ptr<SkinnedSkeletal> LoadFromFBX(
        ID3D11Device* dev,
        const std::wstring& fbxPath,
        const std::wstring& texDir);

    void EvaluatePose(double tSec); // Rigid�� ����
    void DrawOpaqueOnly(
        ID3D11DeviceContext* ctx,
        const Matrix& worldModel, const Matrix& view, const Matrix& proj,
        ID3D11Buffer* cb0, ID3D11Buffer* useCB, ID3D11Buffer* boneCB,
        const Vector4& vLightDir, const Vector4& vLightColor,
        const Vector3& eyePos,
        const Vector3& kA, float ks, float shininess, const Vector3& Ia,
        bool disableNormal, bool disableSpecular, bool disableEmissive);
    void DrawAlphaCutOnly(ID3D11DeviceContext* ctx,
        const Matrix& worldModel, const Matrix& view, const Matrix& proj,
        ID3D11Buffer* cb0, ID3D11Buffer* useCB, ID3D11Buffer* boneCB,
        const Vector4& vLightDir, const Vector4& vLightColor,
        const Vector3& eyePos,
        const Vector3& kA, float ks, float shininess, const Vector3& Ia,
        bool disableNormal, bool disableSpecular, bool disableEmissive);
    void DrawTransparentOnly(ID3D11DeviceContext* ctx,
        const Matrix& worldModel, const Matrix& view, const Matrix& proj,
        ID3D11Buffer* cb0, ID3D11Buffer* useCB, ID3D11Buffer* boneCB,
        const Vector4& vLightDir, const Vector4& vLightColor,
        const Vector3& eyePos,
        const Vector3& kA, float ks, float shininess, const Vector3& Ia,
        bool disableNormal, bool disableSpecular, bool disableEmissive);

    // �� �ȷ�Ʈ ��� �� boneCB�� ���ε�
    void UpdateBonePalette(ID3D11DeviceContext* ctx, ID3D11Buffer* boneCB, const Matrix& worldModel);

    // ����
    double DurationSec() const {
        return (mClip.tps > 0.0) ? (mClip.duration / mClip.tps)
            : (mClip.duration / 25.0);
    }
private:
    SkinnedSkeletal() = default;
    static int UB_T(double t, const std::vector<SK_KeyT>& v);
    static int UB_R(double t, const std::vector<SK_KeyR>& v);
    static int UB_S(double t, const std::vector<SK_KeyS>& v);
    Matrix SampleLocalOf(int nodeIdx, double tTick) const;

private:
    std::vector<SK_Node> mNodes;
    std::vector<SK_Part> mParts;
    std::vector<SK_Bone> mBones;       

    SK_Clip mClip;
    int mRoot = 0;
    std::unordered_map<std::string, int> mNameToNode;

    // ĳ��
    std::vector<Matrix> mBonePalette;   // offset * poseGlobal(node)
};

// RigidSkeletal.h
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <directxtk/SimpleMath.h>
#include <d3d11.h>

#include "StaticMesh.h"
#include "Material.h"

using namespace DirectX::SimpleMath;

struct RS_Node
{
    std::string name;
    int parent = -1;
    std::vector<int> children;

    Matrix bindLocal = Matrix::Identity;     // FBX ����� ���� ���ε�
    Matrix poseLocal = Matrix::Identity;     // �ִϸ��̼����� ���� ���� ����
    Matrix poseGlobal = Matrix::Identity;    // �θ� ������ �۷ι� ����

    // �� ��忡 ���� '�κ� �޽�' �ε���(���� ���� �� ����, ���� 0~1��)
    std::vector<int> partIndices;
};

struct RS_KeyT { double t; Vector3 v; };
struct RS_KeyR { double t; Quaternion q; };
struct RS_KeyS { double t; Vector3 v; };

struct RS_Channel
{
    std::string target;          // ��� �̸�
    std::vector<RS_KeyT> T;
    std::vector<RS_KeyR> R;
    std::vector<RS_KeyS> S;
};

struct RS_Clip
{
    std::string name;
    double duration = 0.0;       // ticks
    double ticksPerSec = 25.0;   // �⺻ 25
    std::vector<RS_Channel> channels;

    // targetName -> index
    std::unordered_map<std::string, int> map;
};

struct RS_Part
{
    // �� ��Ʈ�� ���� StaticMesh (�����ϰ� ����; BoxHuman�� ��Ʈ �� ����)
    StaticMesh mesh;
    std::vector<MaterialGPU> materials; // �ش� ��Ʈ�� ���̴� ��Ƽ�����
    int ownerNode = -1;                 // �� ��Ʈ�� ��� �ε���
};

class RigidSkeletal
{
public:
    // FBX���� ����/�ִϸ��̼�/��Ʈ ���� �� GPU �������
    static std::unique_ptr<RigidSkeletal> LoadFromFBX(
        ID3D11Device* dev,
        const std::wstring& fbxPath,
        const std::wstring& texDir);

    // �ð� ������Ʈ(tSec = ��). ù �ִϸ��̼�(���� Walk)�� ���
    void EvaluatePose(double tSec);

    // Opaque / Cutout / Transparent ����(���� ���������ο� �״�� ����)
    void DrawOpaqueOnly(
        ID3D11DeviceContext* ctx,
        const DirectX::SimpleMath::Matrix& worldModel,
        const DirectX::SimpleMath::Matrix& view,
        const DirectX::SimpleMath::Matrix& proj,
        ID3D11Buffer* cb0, ID3D11Buffer* useCB,
        const DirectX::SimpleMath::Vector4& vLightDir,
        const DirectX::SimpleMath::Vector4& vLightColor,
        const DirectX::SimpleMath::Vector3& eyePos,
        const DirectX::SimpleMath::Vector3& kA, float ks, float shininess,
        const DirectX::SimpleMath::Vector3& Ia,
        bool disableNormal, bool disableSpecular, bool disableEmissive);

    void DrawAlphaCutOnly(
        ID3D11DeviceContext* ctx,
        const DirectX::SimpleMath::Matrix& worldModel,
        const DirectX::SimpleMath::Matrix& view,
        const DirectX::SimpleMath::Matrix& proj,
        ID3D11Buffer* cb0, ID3D11Buffer* useCB, float alphaCut,
        const DirectX::SimpleMath::Vector4& vLightDir,
        const DirectX::SimpleMath::Vector4& vLightColor,
        const DirectX::SimpleMath::Vector3& eyePos,
        const DirectX::SimpleMath::Vector3& kA, float ks, float shininess,
        const DirectX::SimpleMath::Vector3& Ia,
        bool disableNormal, bool disableSpecular, bool disableEmissive);

    void DrawTransparentOnly(
        ID3D11DeviceContext* ctx,
        const DirectX::SimpleMath::Matrix& worldModel,
        const DirectX::SimpleMath::Matrix& view,
        const DirectX::SimpleMath::Matrix& proj,
        ID3D11Buffer* cb0, ID3D11Buffer* useCB,
        const DirectX::SimpleMath::Vector4& vLightDir,
        const DirectX::SimpleMath::Vector4& vLightColor,
        const DirectX::SimpleMath::Vector3& eyePos,
        const DirectX::SimpleMath::Vector3& kA, float ks, float shininess,
        const DirectX::SimpleMath::Vector3& Ia,
        bool disableNormal, bool disableSpecular, bool disableEmissive);

public:
    // --- IMGUI/Ÿ�ֿ̹� ���� Getter ---
    double GetClipDurationTicks() const noexcept { return mClip.duration; }
    double GetTicksPerSecond()   const noexcept { return (mClip.ticksPerSec > 0.0) ? mClip.ticksPerSec : 25.0; }
    double GetClipDurationSec()  const noexcept { return GetClipDurationTicks() / GetTicksPerSecond(); }
    const std::string& GetClipName() const noexcept { return mClip.name; }


private:
    RigidSkeletal() = default;

    // ���� ��ƿ(����)
    static int UpperBoundT(double t, const std::vector<RS_KeyT>& v);
    static int UpperBoundR(double t, const std::vector<RS_KeyR>& v);
    static int UpperBoundS(double t, const std::vector<RS_KeyS>& v);

    Matrix SampleLocalOf(int nodeIdx, double tTick) const;

private:
    std::vector<RS_Node> mNodes;
    std::vector<RS_Part> mParts;

    RS_Clip mClip;     // ù ��° Ŭ�� ���(��: Walk)
    int mRoot = 0;

    // ĳ��: �̸�->���
    std::unordered_map<std::string, int> mNameToNode;
};

// Material.h
#pragma once
#include <string>
#include <d3d11.h>
#include "MeshDataEx.h"

struct MaterialGPU {
    bool Build(ID3D11Device* dev, const MaterialCPU& cpu, const std::wstring& searchDir);
    void Bind(ID3D11DeviceContext* ctx) const;
    static void Unbind(ID3D11DeviceContext* ctx);

    bool hasDiffuse = false, hasNormal = false, hasSpecular = false, hasEmissive = false, hasOpacity = false;

    ID3D11ShaderResourceView* srvDiffuse = nullptr;
    ID3D11ShaderResourceView* srvNormal = nullptr;
    ID3D11ShaderResourceView* srvSpecular = nullptr;
    ID3D11ShaderResourceView* srvEmissive = nullptr;
    ID3D11ShaderResourceView* srvOpacity = nullptr;

    ~MaterialGPU();
};

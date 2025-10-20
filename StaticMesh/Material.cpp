// Material.cpp
#include "Material.h"
#include "../D3DCore/Helper.h"
#include <filesystem>

using std::filesystem::path;

static std::wstring Resolve(const std::wstring& dir, const std::wstring& file) {
    if (file.empty()) return L"";
    path p = path(dir) / file; return p.wstring();
}

bool MaterialGPU::Build(ID3D11Device* dev, const MaterialCPU& cpu, const std::wstring& dir)
{
    auto load = [&](const std::wstring& f, ID3D11ShaderResourceView** out)->bool {
        if (f.empty()) return false;
        if (SUCCEEDED(CreateTextureFromFile(dev, Resolve(dir, f).c_str(), out))) return true;
        return false;
        };
    hasDiffuse = load(cpu.diffuse, &srvDiffuse);
    hasNormal = load(cpu.normal, &srvNormal);
    hasSpecular = load(cpu.specular, &srvSpecular);
    hasEmissive = load(cpu.emissive, &srvEmissive);
    hasOpacity = load(cpu.opacity, &srvOpacity);
    return true;
}

void MaterialGPU::Bind(ID3D11DeviceContext* ctx) const
{
    ID3D11ShaderResourceView* srvs[5] = { srvDiffuse, srvNormal, srvSpecular, srvEmissive, srvOpacity };
    ctx->PSSetShaderResources(0, 5, srvs);
}

void MaterialGPU::Unbind(ID3D11DeviceContext* ctx)
{
    ID3D11ShaderResourceView* nulls[5] = { nullptr,nullptr,nullptr,nullptr,nullptr };
    ctx->PSSetShaderResources(0, 5, nulls);
}

MaterialGPU::~MaterialGPU()
{
    SAFE_RELEASE(srvDiffuse); SAFE_RELEASE(srvNormal);
    SAFE_RELEASE(srvSpecular); SAFE_RELEASE(srvEmissive);
    SAFE_RELEASE(srvOpacity);
}

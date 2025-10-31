// SkinnedMesh.cpp (�ű�)
#include "SkinnedMesh.h"

bool SkinnedMesh::Build(ID3D11Device* dev,
    const std::vector<VertexCPU_PNTT_BW>& vtx,
    const std::vector<uint32_t>& idx,
    const std::vector<SubMeshCPU>& submeshes)
{
    D3D11_BUFFER_DESC vb{}; vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb.ByteWidth = (UINT)(vtx.size() * sizeof(VertexCPU_PNTT_BW));
    vb.Usage = D3D11_USAGE_IMMUTABLE;
    D3D11_SUBRESOURCE_DATA vsd{ vtx.data(),0,0 };
    if (FAILED(dev->CreateBuffer(&vb, &vsd, mVB.GetAddressOf()))) return false;

    D3D11_BUFFER_DESC ib{}; ib.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ib.ByteWidth = (UINT)(idx.size() * sizeof(uint32_t));
    ib.Usage = D3D11_USAGE_IMMUTABLE;
    D3D11_SUBRESOURCE_DATA isd{ idx.data(),0,0 };
    if (FAILED(dev->CreateBuffer(&ib, &isd, mIB.GetAddressOf()))) return false;

    mRanges = submeshes;
    return true;
}

void SkinnedMesh::DrawSubmesh(ID3D11DeviceContext* ctx, size_t i) const
{
    UINT offset = 0; ID3D11Buffer* vb = mVB.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &mStride, &offset);
    ctx->IASetIndexBuffer(mIB.Get(), DXGI_FORMAT_R32_UINT, 0);
    const auto& r = mRanges[i];
    ctx->DrawIndexed(r.indexCount, r.indexStart, 0);
}

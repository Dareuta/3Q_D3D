#include "StaticMeshMinimal.h"

// “파일→CPU 데이터(Assimp)” 다음 단계,
// “CPU→GPU 업로드 + 서브메시 드로우 범위 세팅”을 하는 역할 

bool StaticMeshMinimal::Build(ID3D11Device* dev, const MeshData_PosIndex& src)
{
	D3D11_BUFFER_DESC vb{};
	vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb.ByteWidth = (UINT)(src.vertices.size() * sizeof(float) * 3);
	vb.Usage = D3D11_USAGE_IMMUTABLE;

	std::vector<float> packed;
	packed.reserve(src.vertices.size() * 3);
	for (auto& v : src.vertices) {
		packed.push_back(v.x);
		packed.push_back(v.y);
		packed.push_back(v.z);
	}

	D3D11_SUBRESOURCE_DATA vsd{ packed.data(), 0,0 };
	if (FAILED(dev->CreateBuffer(&vb, &vsd, mVB.GetAddressOf()))) return false;

	D3D11_BUFFER_DESC ib{};
	ib.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ib.ByteWidth = (UINT)(src.indices.size() * sizeof(uint32_t));
	ib.Usage = D3D11_USAGE_IMMUTABLE;

	D3D11_SUBRESOURCE_DATA isd{ src.indices.data(),0,0 };
	if (FAILED(dev->CreateBuffer(&ib, &isd, mIB.GetAddressOf()))) return false;

	mRanges.clear();
	mRanges.reserve(src.submeshes.size());
	for (auto& sm : src.submeshes)
		mRanges.push_back({ sm.indexStart, sm.indexCount });

	mIndexCount = (UINT)src.indices.size();
	return true;
}

void StaticMeshMinimal::Draw(ID3D11DeviceContext* ctx) const
{
	UINT offset = 0;
	ID3D11Buffer* vb = mVB.Get();
	ctx->IASetVertexBuffers(0, 1, &vb, &mStride, &offset);
	ctx->IASetIndexBuffer(mIB.Get(), DXGI_FORMAT_R32_UINT, 0);

	for (auto& r : mRanges)
		ctx->DrawIndexed(r.indexCount, r.indexStart, 0);
}

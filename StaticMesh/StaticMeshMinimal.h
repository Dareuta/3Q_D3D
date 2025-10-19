#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "MeshData.h"

// GPU 래퍼, D3D는 이곳만 알고있음

class StaticMeshMinimal {
public:
	bool Build(ID3D11Device* dev, const MeshData_PosIndex& src);
	void Draw(ID3D11DeviceContext* ctx) const; 

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> mVB, mIB;
	UINT mStride = sizeof(float) * 3;
	UINT mIndexCount = 0;

	struct DrawRange { UINT indexStart, indexCount; };
	std::vector<DrawRange> mRanges;

};
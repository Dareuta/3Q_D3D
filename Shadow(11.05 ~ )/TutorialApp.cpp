//================================================================================================
// TutorialApp.cpp
//================================================================================================

#include "TutorialApp.h"
#include "../D3DCore/Helper.h"
#include "AssimpImporterEx.h"

#include <d3dcompiler.h>
#include <Directxtk/DDSTextureLoader.h>  // CreateDDSTextureFromFile

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

//================================================================================================

// GPU가 기대하고 있는 메모리 레이아웃과 1대1로 대응해야한다
struct ConstantBuffer // 상수버퍼
{
	Matrix mWorld;
	Matrix mView;
	Matrix mProjection;
	Matrix mWorldInvTranspose;

	Vector4 vLightDir;
	Vector4 vLightColor;
};

struct BlinnPhongCB
{
	Vector4 EyePosW;   // (ex,ey,ez,1)
	Vector4 kA;        // (ka.r,ka.g,ka.b,0)
	Vector4 kSAlpha;   // (ks, alpha, 0, 0)
	Vector4 I_ambient; // (Ia.r,Ia.g,Ia.b,0)
};


struct UseCB
{
	UINT  useDiffuse, useNormal, useSpecular, useEmissive;
	UINT  useOpacity;
	float alphaCut;
	float pad[2];
};

bool TutorialApp::OnInitialize()
{
	if (!InitD3D())
		return false;

#ifdef _DEBUG
	if (!InitImGUI())
		return false;
#endif

	if (!InitScene())
		return false;

	return true;
}

void TutorialApp::OnUninitialize()
{
	UninitScene();

#ifdef _DEBUG
	UninitImGUI();
#endif

	UninitD3D();
}

// 공용 애니메이션 컨트롤 UI
static void AnimUI(const char* label,
	bool& play, bool& loop, float& speed, double& t,
	double durationSec,
	const std::function<void(double)>& evalPose)
{
	if (ImGui::TreeNode(label)) {
		ImGui::Checkbox("Play", &play); ImGui::SameLine();
		ImGui::Checkbox("Loop", &loop);

		ImGui::DragFloat("Speed (x)", &speed, 0.01f, -4.0f, 4.0f, "%.2f");

		const float maxT = (float)((durationSec > 0.0) ? durationSec : 1.0);
		float tUI = (float)t;
		if (ImGui::SliderFloat("Time (sec)", &tUI, 0.0f, maxT, "%.3f")) {
			t = (double)tUI;
			if (evalPose) evalPose(t);
		}

		if (ImGui::Button("Rewind")) { t = 0.0; if (evalPose) evalPose(t); }
		ImGui::SameLine();
		if (ImGui::Button("Go End")) { t = durationSec; if (evalPose) evalPose(t); }

		ImGui::TreePop();
	}
}

bool TutorialApp::CreateShadowResources(ID3D11Device* dev)
{
	// 1) 텍스처 + DSV + SRV (R32 typeless)
	D3D11_TEXTURE2D_DESC td{};
	td.Width = mShadowW; td.Height = mShadowH;
	td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R32_TYPELESS;
	td.SampleDesc.Count = 1;
	td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	if (FAILED(dev->CreateTexture2D(&td, nullptr, mShadowTex.GetAddressOf()))) return false;

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
	dsvd.Format = DXGI_FORMAT_D32_FLOAT;
	dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	if (FAILED(dev->CreateDepthStencilView(mShadowTex.Get(), &dsvd, mShadowDSV.GetAddressOf()))) return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
	srvd.Format = DXGI_FORMAT_R32_FLOAT;
	srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvd.Texture2D.MipLevels = 1;
	if (FAILED(dev->CreateShaderResourceView(mShadowTex.Get(), &srvd, mShadowSRV.GetAddressOf()))) return false;

	// 2) 비교 샘플러 (s1)
	D3D11_SAMPLER_DESC sd{};
	sd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
	sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; // 일반적 설정
	//sd.ComparisonFunc = D3D11_COMPARISON_LESS;
	sd.MinLOD = 0; sd.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(dev->CreateSamplerState(&sd, mSamShadowCmp.GetAddressOf()))) return false;

	// 3) 깊이 바이어스용 RS
	D3D11_RASTERIZER_DESC rs{};
	rs.FillMode = D3D11_FILL_SOLID;
	rs.CullMode = D3D11_CULL_BACK;
	rs.DepthClipEnable = TRUE;
	rs.DepthBias = (INT)mShadowDepthBias;     // 1000 정도 스타트
	rs.SlopeScaledDepthBias = mShadowSlopeBias;
	rs.DepthBiasClamp = 0.0f;
	if (FAILED(dev->CreateRasterizerState(&rs, mRS_ShadowBias.GetAddressOf()))) return false;

	// 4) 뷰포트
	mShadowVP = { 0,0,(float)mShadowW,(float)mShadowH, 0.0f,1.0f };

	// 5) ShadowCB (b6)
	D3D11_BUFFER_DESC cbd{};
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.ByteWidth = sizeof(DirectX::XMFLOAT4X4) + sizeof(DirectX::XMFLOAT4); // LightViewProj + ShadowParams
	return SUCCEEDED(dev->CreateBuffer(&cbd, nullptr, mCB_Shadow.GetAddressOf()));
}

bool TutorialApp::CreateDepthOnlyShaders(ID3D11Device* dev)
{
	using Microsoft::WRL::ComPtr;
	ComPtr<ID3DBlob> vsb, vsb2, psb;

	HR_T(CompileShaderFromFile(L"Shader/DepthOnly_VS.hlsl", "main", "vs_5_0", vsb.GetAddressOf()));
	HR_T(CompileShaderFromFile(L"Shader/DepthOnly_SkinnedVS.hlsl", "main", "vs_5_0", vsb2.GetAddressOf()));
	HR_T(CompileShaderFromFile(L"Shader/DepthOnly_PS.hlsl", "main", "ps_5_0", psb.GetAddressOf()));

	HR_T(dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, mVS_Depth.GetAddressOf()));
	HR_T(dev->CreateVertexShader(vsb2->GetBufferPointer(), vsb2->GetBufferSize(), nullptr, mVS_DepthSkinned.GetAddressOf()));
	HR_T(dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, mPS_Depth.GetAddressOf()));

	// IL for VertexCPU_PNTT
	D3D11_INPUT_ELEMENT_DESC ilPNTT[] = {
		{"POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	HR_T(dev->CreateInputLayout(ilPNTT, _countof(ilPNTT),
		vsb->GetBufferPointer(), vsb->GetBufferSize(), mIL_PNTT.GetAddressOf()));

	// IL for VertexCPU_PNTT_BW
	D3D11_INPUT_ELEMENT_DESC ilBW[] = {
		{"POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 52, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	HR_T(dev->CreateInputLayout(ilBW, _countof(ilBW),
		vsb2->GetBufferPointer(), vsb2->GetBufferSize(), mIL_PNTT_BW.GetAddressOf()));

	return true;
}

void TutorialApp::BuildLightCameraAndUpload(ID3D11DeviceContext* ctx,
	const DirectX::SimpleMath::Vector3& camPos,
	const DirectX::SimpleMath::Vector3& camForward,
	const DirectX::SimpleMath::Vector3& lightDir_unit,
	float focusDist, float lightDist)
{
	// === 라이트 카메라 구성 ===
	using namespace DirectX;
	const Vector3 eyePos = m_Camera.m_World.Translation();
	const Vector3 camFwd = m_Camera.GetForward();

	// 1) lookAt 결정
	Vector3 lookAt;
	if (mShUI.followCamera) {
		lookAt = eyePos + camFwd * mShUI.focusDist;
	}
	else {
		lookAt = XMLoadFloat3(&mShUI.manualTarget);
	}

	// 2) 라이트 방향 (기존 yaw/pitch → vLightDir)
	Vector3 Ldir(vLightDir.x, vLightDir.y, vLightDir.z);
	Ldir.Normalize();

	// 3) lightPos 결정 (directional을 '계산용 위치'로)
	Vector3 lightPos;
	if (mShUI.useManualPos) {
		lightPos = XMLoadFloat3(&mShUI.manualPos);
	}
	else {
		lightPos = lookAt - Ldir * mShUI.lightDist;
	}

	// 4) View 행렬
	const Matrix lightView = Matrix::CreateLookAt(lightPos, lookAt, Vector3::UnitY);

	// 5) Proj 행렬
	float aspectSh = float(mShadowW) / float(mShadowH);

	// “카메라 화면의 focusDist 평면 직사각형”을 라이트 프러스텀이 덮게(자동 커버)
	if (mShUI.autoCover) {
		// 카메라 파라미터에서 반높이/반너비
		const float camFovY = XMConvertToRadians(m_FovDegree);
		const float halfH = tanf(camFovY * 0.5f) * mShUI.focusDist;
		const float halfW = halfH * (m_ClientWidth / float(m_ClientHeight));
		const float r = sqrtf(halfW * halfW + halfH * halfH) * mShUI.coverMargin;

		// lightDist 기준으로 FOV / 클립 자동 산출
		const float d = mShUI.lightDist;
		const float nz = max(d - r, 0.01f);
		const float fz = d + r;
		mShadowNear = nz;
		mShadowFar = fz;
		mShadowFovY = 2.0f * atanf(r / d); // 라이트 FOVY 자동화
	}

	// 최종 투영
	const Matrix lightProj = Matrix::CreatePerspectiveFieldOfView(mShadowFovY, aspectSh, mShadowNear, mShadowFar);
	const Matrix lightVP = lightView * lightProj;

	// (디버그용 보관)
	mLightView = lightView;
	mLightProj = lightProj;

	// 4) CB(b6) 업로드 (Shared.hlsli: LightViewProj, ShadowParams)
	struct ShadowCB_ {
		DirectX::XMFLOAT4X4 LVP;
		DirectX::XMFLOAT4   Params; // x=bias(미사용), y=1/w, z=1/h, w=0
	} scb{};

	auto LVP = (mLightView * mLightProj).Transpose();
	XMStoreFloat4x4(&scb.LVP, LVP);
	scb.Params = { 0.0f, 1.0f / mShadowW, 1.0f / mShadowH, 0.0f };

	ctx->UpdateSubresource(mCB_Shadow.Get(), 0, nullptr, &scb, 0, 0);
	ID3D11Buffer* b6 = mCB_Shadow.Get();
	ctx->VSSetConstantBuffers(6, 1, &b6); // b6: ShadowCB
	ctx->PSSetConstantBuffers(6, 1, &b6);
}

void TutorialApp::RenderShadowPass(ID3D11DeviceContext* ctx,
	const DirectX::SimpleMath::Vector3& camPos,
	const DirectX::SimpleMath::Vector3& camForward,
	const DirectX::SimpleMath::Vector3& lightDir_unit,
	float focusDist, float lightDist)
{
	// 0) LightViewProj 계산 + CB(b6) 업로드
	BuildLightCameraAndUpload(ctx, camPos, camForward, lightDir_unit, focusDist, lightDist);

	// 1) 타깃 설정 (컬러 RTV 없음, DSV만)
	ctx->OMSetRenderTargets(0, nullptr, mShadowDSV.Get());
	ctx->ClearDepthStencilView(mShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
	ctx->RSSetViewports(1, &mShadowVP);
	ctx->RSSetState(mRS_ShadowBias.Get());

	// 2) 공용 상태
	float blend[4] = { 0,0,0,0 };
	ctx->OMSetBlendState(nullptr, blend, 0xFFFFFFFF);
	ID3D11SamplerState* cmp = mSamShadowCmp.Get();
	ctx->PSSetSamplers(1, 1, &cmp); // s1

	// 3) 장면 오브젝트 렌더 (Rigid / Skinned)
	//    worldModel / cb0 / useCB / boneCB 등은 네가 이미 쓰던 것을 그대로 전달
	// 예시:
	// rigid->DrawDepthOnly(ctx, world, mLightView, mLightProj, cb0, useCB, mVS_Depth.Get(), mPS_Depth.Get(), mIL_PNTT.Get(), mShadowAlphaCut);
	// skel->DrawDepthOnly (ctx, world, mLightView, mLightProj, cb0, useCB, boneCB, mVS_DepthSkinned.Get(), mPS_Depth.Get(), mIL_PNTT_BW.Get(), mShadowAlphaCut);

	// 4) 상태 원복은 메인 패스 들어가기 전에 네 기존 코드 흐름대로
}





//================================================================================================

void TutorialApp::OnUpdate()
{
	static float tHold = 0.0f;
	if (!mDbg.freezeTime) tHold = GameTimer::m_Instance->TotalTime();
	float t = tHold;

	XMMATRIX mSpin = XMMatrixRotationY(t * spinSpeed);

	// 데모용 큐브 world (그대로 유지)
	XMMATRIX mScaleA = XMMatrixScaling(cubeScale.x, cubeScale.y, cubeScale.z);
	XMMATRIX mTranslateA = XMMatrixTranslation(cubeTransformA.x, cubeTransformA.y, cubeTransformA.z);
	m_World = mScaleA * mSpin * mTranslateA;

	// TutorialApp.cpp::OnUpdate()

	const double dt = (double)GameTimer::m_Instance->DeltaTime();

	// --- BoxHuman ---
	if (mBoxRig) {
		if (!mDbg.freezeTime && mBoxAC.play) mBoxAC.t += dt * mBoxAC.speed;
		const double durSec = mBoxRig->GetClipDurationSec();
		if (durSec > 0.0) {
			if (mBoxAC.loop) {
				mBoxAC.t = fmod(mBoxAC.t, durSec); if (mBoxAC.t < 0.0) mBoxAC.t += durSec;
			}
			else {
				if (mBoxAC.t >= durSec) { mBoxAC.t = durSec; mBoxAC.play = false; } // 끝에서 정지
				if (mBoxAC.t < 0.0) { mBoxAC.t = 0.0;   mBoxAC.play = false; } // 앞에서 정지
			}
		}
		mBoxRig->EvaluatePose(mBoxAC.t, mBoxAC.loop);  // ← loop 전달
	}

	// --- Skinned ---
	if (mSkinRig) {
		if (!mDbg.freezeTime && mSkinAC.play) mSkinAC.t += dt * mSkinAC.speed;
		const double durSec = mSkinRig->DurationSec();
		if (durSec > 0.0) {
			if (mSkinAC.loop) {
				mSkinAC.t = fmod(mSkinAC.t, durSec); if (mSkinAC.t < 0.0) mSkinAC.t += durSec;
			}
			else {
				if (mSkinAC.t >= durSec) { mSkinAC.t = durSec; mSkinAC.play = false; }
				if (mSkinAC.t < 0.0) { mSkinAC.t = 0.0;   mSkinAC.play = false; }
			}
		}
		mSkinRig->EvaluatePose(mSkinAC.t, mSkinAC.loop);
	}


}

//================================================================================================
//////////////////////////////////////////////////////////////////////////////////////////////////
//================================================================================================

void TutorialApp::OnRender()
{
	// ===== 기본 프로젝션/클리어 =====
	if (m_FovDegree < 10.0f)      m_FovDegree = 10.0f;
	else if (m_FovDegree > 120.0f) m_FovDegree = 120.0f;
	if (m_Near < 0.0001f) m_Near = 0.0001f;
	float minFar = m_Near + 0.001f;
	if (m_Far < minFar) m_Far = minFar;

	float aspect = m_ClientWidth / (float)m_ClientHeight;

	// RS 선택: wire > cullNone > backCull
	if (mDbg.wireframe && m_pWireRS) {
		m_pDeviceContext->RSSetState(m_pWireRS);
	}
	else if (mDbg.cullNone && m_pDbgRS) {
		m_pDeviceContext->RSSetState(m_pDbgRS);
	}
	else {
		m_pDeviceContext->RSSetState(m_pCullBackRS);
	}

	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	m_Projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FovDegree), aspect, m_Near, m_Far);

	// ===== 파이프라인 셋업(메쉬 셰이더) =====
	m_pDeviceContext->IASetInputLayout(m_pMeshIL);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->VSSetShader(m_pMeshVS, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pMeshPS, nullptr, 0);
	if (m_pSamplerLinear) m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

	// ===== 공통 CB0(뷰/프로젝션/라이트) + b1(카메라/재질) =====
	Matrix view; m_Camera.GetViewMatrix(view);
	Matrix viewNoTrans = view; viewNoTrans._41 = viewNoTrans._42 = viewNoTrans._43 = 0.0f;

	ConstantBuffer cb{};
	cb.mView = XMMatrixTranspose(view);
	cb.mProjection = XMMatrixTranspose(m_Projection);
	cb.mWorld = XMMatrixTranspose(Matrix::Identity);
	cb.mWorldInvTranspose = XMMatrixInverse(nullptr, Matrix::Identity);

	XMMATRIX R = XMMatrixRotationRollPitchYaw(m_LightPitch, m_LightYaw, 0.0f);
	XMVECTOR base = XMVector3Normalize(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
	XMVECTOR L = XMVector3Normalize(XMVector3TransformNormal(base, R));
	Vector3 dir = { XMVectorGetX(L), XMVectorGetY(L), XMVectorGetZ(L) };
	cb.vLightDir = Vector4(dir.x, dir.y, dir.z, 0.0f);
	cb.vLightColor = Vector4(m_LightColor.x * m_LightIntensity, m_LightColor.y * m_LightIntensity, m_LightColor.z * m_LightIntensity, 1.0f);

	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &cb, 0, 0);
	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

	BlinnPhongCB bp{};
	const Vector3 eye = m_Camera.m_World.Translation();
	bp.EyePosW = Vector4(eye.x, eye.y, eye.z, 1.0f);
	bp.kA = Vector4(m_Ka.x, m_Ka.y, m_Ka.z, 0.0f);
	bp.kSAlpha = Vector4(m_Ks, m_Shininess, 0.0f, 0.0f);
	bp.I_ambient = Vector4(m_Ia.x, m_Ia.y, m_Ia.z, 0.0f);
	m_pDeviceContext->UpdateSubresource(m_pBlinnCB, 0, nullptr, &bp, 0, 0);
	m_pDeviceContext->PSSetConstantBuffers(1, 1, &m_pBlinnCB);


	// 공통 값
	auto* ctx = m_pDeviceContext;
	vLightDir = cb.vLightDir;
	vLightColor = cb.vLightColor;

	// Shadow PASS 들어가기 직전
	ID3D11RasterizerState* rsBeforeShadow = nullptr;
	ctx->RSGetState(&rsBeforeShadow); // AddRef됨

// ============================================================================
// SHADOW (DepthOnly) PASS  —  카메라 정면+거리 지점을 향하는 원근 라이트 카메라
// ============================================================================

	{
		// 0) 라이트 카메라 구성 (요구사항: 카메라 앞쪽 focusDist 지점을 라이트가 응시)
		const Vector3 eyePos = m_Camera.m_World.Translation();
		const Vector3 camFwd = m_Camera.GetForward(); // Camera.cpp: return -m_World.Forward();
		Vector3 L = Vector3(vLightDir.x, vLightDir.y, vLightDir.z); // 위에서 만든 라이트 방향
		L.Normalize();

		const float focusDist = 50.0f;   // 장면에 맞게 가감
		const float lightDist = 100.0f;  // 장면에 맞게 가감

		const Vector3 lookAt = eyePos + camFwd * focusDist;
		const Vector3 lightPos = lookAt - L * lightDist;

		const Matrix lightView = XMMatrixLookAtLH(lightPos, lookAt, Vector3::UnitY);
		

		const float  aspectSh = float(mShadowW) / float(mShadowH);
		const Matrix lightProj = XMMatrixPerspectiveFovLH(mShadowFovY, aspectSh, mShadowNear, mShadowFar);
		const Matrix lightVP = lightView * lightProj;

		// [라이트 카메라 계산 직후에 추가]
		mLightView = lightView;          // 멤버에 저장 (다음 단계/디버그용)
		mLightProj = lightProj;
				
		// 1) ShadowCB(b6) 업로드  (Shared.hlsli: LightViewProj, ShadowParams)
		struct ShadowCB_ {
			Matrix LVP;      // transpose 해서 보냄
			Vector4 Params;  // x: (옵션) depthBias, y: 1/w, z: 1/h, w: 0
		} scb;

		scb.LVP = XMMatrixTranspose(lightVP);
		scb.Params = Vector4(0.0f, 1.0f / mShadowW, 1.0f / mShadowH, 0.0f);

		// (멤버 이름이 다르면 맞춰서 변경)
		ctx->UpdateSubresource(mCB_Shadow.Get(), 0, nullptr, &scb, 0, 0);
		{ ID3D11Buffer* b6 = mCB_Shadow.Get(); ctx->VSSetConstantBuffers(6, 1, &b6); ctx->PSSetConstantBuffers(6, 1, &b6); }

		// 2) 렌더 타깃: DSV만 설정 (컬러 RTV 없음), 뷰포트는 그림자용
		//    혹시 t5에 SRV가 물려 있을 수 있으니 미리 해제(습관)
		{ ID3D11ShaderResourceView* nullSRV[1] = { nullptr }; ctx->PSSetShaderResources(5, 1, nullSRV); }

		ctx->OMSetRenderTargets(0, nullptr, mShadowDSV.Get());
		ctx->ClearDepthStencilView(mShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
		ctx->RSSetViewports(1, &mShadowVP);

		// (선택) 깊이 바이어스 RS 적용
		if (mRS_ShadowBias) ctx->RSSetState(mRS_ShadowBias.Get());		


		// 3) 깊이 전용 셰이더 바인딩 + CB0(View/Proj)만 라이트 카메라로 일시 교체
		ConstantBuffer cbDepth = cb;
		cbDepth.mView = XMMatrixTranspose(lightView);
		cbDepth.mProjection = XMMatrixTranspose(lightProj);
		ctx->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &cbDepth, 0, 0);
		ctx->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		ctx->IASetInputLayout(mIL_PNTT.Get()); // Static PNTT
		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ctx->VSSetShader(mVS_Depth.Get(), nullptr, 0);
		ctx->PSSetShader(mPS_Depth.Get(), nullptr, 0);

		// 4) 정적 메시(StaticMesh) 그리기 — Opaque + AlphaCut 나눠서
		auto DrawDepth_StaticOpaque = [&](StaticMesh& mesh, const std::vector<MaterialGPU>& mtls, const Matrix& world)
			{
				ConstantBuffer local = cbDepth;
				local.mWorld = XMMatrixTranspose(world);
				local.mWorldInvTranspose = world.Invert();
				ctx->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &local, 0, 0);

				for (size_t i = 0; i < mesh.Ranges().size(); ++i) {
					const auto& r = mesh.Ranges()[i];
					const auto& mat = mtls[r.materialIndex];
					if (mat.hasOpacity) continue; // 불투명만

					// PS의 clip() 비활성
					UseCB use{}; use.useOpacity = 0u; use.alphaCut = -1.0f;
					ctx->UpdateSubresource(m_pUseCB, 0, nullptr, &use, 0, 0);
					ctx->PSSetConstantBuffers(2, 1, &m_pUseCB);

					mat.Bind(ctx);
					mesh.DrawSubmesh(ctx, (UINT)i);
					MaterialGPU::Unbind(ctx);
				}
			};

		auto DrawDepth_StaticCut = [&](StaticMesh& mesh, const std::vector<MaterialGPU>& mtls, const Matrix& world)
			{
				ConstantBuffer local = cbDepth;
				local.mWorld = XMMatrixTranspose(world);
				local.mWorldInvTranspose = world.Invert();
				ctx->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &local, 0, 0);

				for (size_t i = 0; i < mesh.Ranges().size(); ++i) {
					const auto& r = mesh.Ranges()[i];
					const auto& mat = mtls[r.materialIndex];
					if (!mat.hasOpacity) continue; // 컷아웃만

					// PS의 clip() 활성 (alphaCut 기준)
					UseCB use{}; use.useOpacity = 1u; use.alphaCut = mDbg.alphaCut;
					ctx->UpdateSubresource(m_pUseCB, 0, nullptr, &use, 0, 0);
					ctx->PSSetConstantBuffers(2, 1, &m_pUseCB);

					mat.Bind(ctx);
					mesh.DrawSubmesh(ctx, (UINT)i);
					MaterialGPU::Unbind(ctx);
				}
			};

		if (mSkinRig && mSkinX.enabled)
		{
			ctx->IASetInputLayout(mIL_PNTT_BW.Get());
			ctx->VSSetShader(mVS_DepthSkinned.Get(), nullptr, 0);
			ctx->PSSetShader(mPS_Depth.Get(), nullptr, 0);

			mSkinRig->DrawDepthOnly(
				ctx,
				ComposeSRT(mSkinX),
				lightView, lightProj,
				m_pConstantBuffer,  // b0 (지금 light VP로 교체됨)
				m_pUseCB,           // b2
				m_pBoneCB,          // b4
				mVS_DepthSkinned.Get(),
				mPS_Depth.Get(),
				mIL_PNTT_BW.Get(),
				mShadowAlphaCut
			);
		}

		// 정적 메시들 투입 (네가 쓰는 것만 남겨도 됨)
		if (mTreeX.enabled) { auto W = ComposeSRT(mTreeX);  DrawDepth_StaticOpaque(gTree, gTreeMtls, W); DrawDepth_StaticCut(gTree, gTreeMtls, W); }
		if (mCharX.enabled) { auto W = ComposeSRT(mCharX);  DrawDepth_StaticOpaque(gChar, gCharMtls, W); DrawDepth_StaticCut(gChar, gCharMtls, W); }
		if (mZeldaX.enabled) { auto W = ComposeSRT(mZeldaX); DrawDepth_StaticOpaque(gZelda, gZeldaMtls, W); DrawDepth_StaticCut(gZelda, gZeldaMtls, W); }

		// TODO(다음 단계): 스키닝 메시(mBoxRig, mSkinRig)도 DepthOnly로 렌더 (전용 VS 사용)

		// 5) 메인 렌더 타깃/뷰포트 복구
		{
			ID3D11RenderTargetView* rtv = m_pRenderTargetView;
			ctx->OMSetRenderTargets(1, &rtv, m_pDepthStencilView);

			D3D11_VIEWPORT vp{};
			vp.TopLeftX = 0; vp.TopLeftY = 0;
			vp.Width = (float)m_ClientWidth; vp.Height = (float)m_ClientHeight;
			vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
			ctx->RSSetViewports(1, &vp);
		}
	}

	ctx->RSSetState(rsBeforeShadow);
	SAFE_RELEASE(rsBeforeShadow);

	// ==================== SHADOW PASS END ====================


	// ===== 람다: OPAQUE ONLY (트렁크 등 불투명만) =====
	auto DrawOpaqueOnly = [&](StaticMesh& mesh,
		const std::vector<MaterialGPU>& mtls,
		const Matrix& world)
		{
			ConstantBuffer local = cb;
			local.mWorld = XMMatrixTranspose(world);
			local.mWorldInvTranspose = world.Invert();
			m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &local, 0, 0);

			for (size_t i = 0; i < mesh.Ranges().size(); ++i)
			{
				const auto& r = mesh.Ranges()[i];
				const auto& mat = mtls[r.materialIndex];

				// 불투명만 (opacity 텍스처가 있는 잎은 제외)
				if (mat.hasOpacity) continue;

				mat.Bind(m_pDeviceContext);

				UseCB use{};
				use.useDiffuse = mat.hasDiffuse ? 1u : 0u;
				use.useNormal = (mat.hasNormal && !mDbg.disableNormal) ? 1u : 0u;
				use.useSpecular = (!mDbg.disableSpecular) ? (mat.hasSpecular ? 1u : 2u) : 0u;

				use.useEmissive = (mat.hasEmissive && !mDbg.disableEmissive) ? 1u : 0u;
				use.useOpacity = /* OpaqueOnly */ 0u /* TransparentOnly 에선 1u */;
				if (mDbg.forceAlphaClip) {
					use.alphaCut = mDbg.alphaCut;  // 클립 활성
				}
				else {
					use.alphaCut = -1.0f;          // 클립 비활성
				}

				m_pDeviceContext->UpdateSubresource(m_pUseCB, 0, nullptr, &use, 0, 0);
				m_pDeviceContext->PSSetConstantBuffers(2, 1, &m_pUseCB);

				mesh.DrawSubmesh(m_pDeviceContext, i);
				MaterialGPU::Unbind(m_pDeviceContext);
			}
		};

	auto DrawAlphaCutOnly = [&](StaticMesh& mesh,
		const std::vector<MaterialGPU>& mtls,
		const Matrix& world)
		{
			ConstantBuffer local = cb;
			local.mWorld = XMMatrixTranspose(world);
			local.mWorldInvTranspose = world.Invert();
			m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &local, 0, 0);

			for (size_t i = 0; i < mesh.Ranges().size(); ++i)
			{
				const auto& r = mesh.Ranges()[i];
				const auto& mat = mtls[r.materialIndex];

				// opacity 맵 있는 서브메시만 컷아웃으로
				if (!mat.hasOpacity) continue;

				mat.Bind(m_pDeviceContext);

				UseCB use{};
				use.useDiffuse = mat.hasDiffuse ? 1u : 0u;
				use.useNormal = (mat.hasNormal && !mDbg.disableNormal) ? 1u : 0u;
				use.useSpecular = (!mDbg.disableSpecular) ? (mat.hasSpecular ? 1u : 2u) : 0u;
				use.useEmissive = (mat.hasEmissive && !mDbg.disableEmissive) ? 1u : 0u;

				use.useOpacity = 1u;              // opacity 텍스처 사용
				use.alphaCut = mDbg.alphaCut;   // 컷 기준 활성화

				m_pDeviceContext->UpdateSubresource(m_pUseCB, 0, nullptr, &use, 0, 0);
				m_pDeviceContext->PSSetConstantBuffers(2, 1, &m_pUseCB);

				mesh.DrawSubmesh(m_pDeviceContext, i);
				MaterialGPU::Unbind(m_pDeviceContext);
			}
		};


	// ===== 람다: TRANSPARENT ONLY (잎 등 반투명만) =====
	auto DrawTransparentOnly = [&](StaticMesh& mesh,
		const std::vector<MaterialGPU>& mtls,
		const Matrix& world)
		{
			if (mDbg.forceAlphaClip) return;

			ConstantBuffer local = cb;
			local.mWorld = XMMatrixTranspose(world);
			local.mWorldInvTranspose = world.Invert();
			m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &local, 0, 0);

			for (size_t i = 0; i < mesh.Ranges().size(); ++i)
			{
				const auto& r = mesh.Ranges()[i];
				const auto& mat = mtls[r.materialIndex];

				if (!mat.hasOpacity) continue;

				mat.Bind(m_pDeviceContext);

				UseCB use{};
				use.useDiffuse = mat.hasDiffuse ? 1u : 0u;
				use.useNormal = (mat.hasNormal && !mDbg.disableNormal) ? 1u : 0u;
				use.useSpecular = (!mDbg.disableSpecular) ? (mat.hasSpecular ? 1u : 2u) : 0u;

				use.useEmissive = (mat.hasEmissive && !mDbg.disableEmissive) ? 1u : 0u;
				use.useOpacity = 1u; // 투명

				// 강제 컷아웃 테스트 모드 지원
				use.alphaCut = mDbg.forceAlphaClip ? mDbg.alphaCut : -1.0f;


				m_pDeviceContext->UpdateSubresource(m_pUseCB, 0, nullptr, &use, 0, 0);
				m_pDeviceContext->PSSetConstantBuffers(2, 1, &m_pUseCB);

				mesh.DrawSubmesh(m_pDeviceContext, i);
				MaterialGPU::Unbind(m_pDeviceContext);
			}
		};

	auto BindStatic = [&]() {
		ctx->IASetInputLayout(m_pMeshIL);
		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ctx->VSSetShader(m_pMeshVS, nullptr, 0);
		ctx->PSSetShader(m_pMeshPS, nullptr, 0);
		};

	auto BindSkinned = [&]() {
		ctx->IASetInputLayout(m_pSkinnedIL);
		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ctx->VSSetShader(m_pSkinnedVS, nullptr, 0);
		ctx->PSSetShader(m_pMeshPS, nullptr, 0);
		};

	// ===== A) SKYBOX FIRST =====
	if (mDbg.showSky) {
		// RS/OM 상태 백업
		ID3D11RasterizerState* oldRS = nullptr;
		m_pDeviceContext->RSGetState(&oldRS);
		ID3D11DepthStencilState* oldDSS = nullptr; UINT oldStencilRef = 0;
		m_pDeviceContext->OMGetDepthStencilState(&oldDSS, &oldStencilRef);

		m_pDeviceContext->RSSetState(m_pSkyRS);
		m_pDeviceContext->OMSetDepthStencilState(m_pSkyDSS, 0);

		m_pDeviceContext->IASetInputLayout(m_pSkyIL);
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetShader(m_pSkyVS, nullptr, 0);
		m_pDeviceContext->PSSetShader(m_pSkyPS, nullptr, 0);

		ConstantBuffer skyCB{};
		skyCB.mWorld = XMMatrixTranspose(Matrix::Identity);
		skyCB.mView = XMMatrixTranspose(viewNoTrans);
		skyCB.mProjection = XMMatrixTranspose(m_Projection);
		skyCB.mWorldInvTranspose = Matrix::Identity;
		skyCB.vLightDir = Vector4(0, 0, 0, 0);
		skyCB.vLightColor = Vector4(0, 0, 0, 0);
		m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &skyCB, 0, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		m_pDeviceContext->PSSetShaderResources(0, 1, &m_pSkySRV);
		m_pDeviceContext->PSSetSamplers(0, 1, &m_pSkySampler);

		UINT stride = sizeof(DirectX::XMFLOAT3);
		UINT offset = 0;
		ID3D11Buffer* vbs[] = { m_pSkyVB };
		m_pDeviceContext->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
		m_pDeviceContext->IASetIndexBuffer(m_pSkyIB, DXGI_FORMAT_R16_UINT, 0);
		m_pDeviceContext->DrawIndexed(36, 0, 0);

		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		m_pDeviceContext->PSSetShaderResources(0, 1, nullSRV);
		m_pDeviceContext->RSSetState(oldRS);
		m_pDeviceContext->OMSetDepthStencilState(oldDSS, oldStencilRef);
		SAFE_RELEASE(oldRS);
		SAFE_RELEASE(oldDSS);
	}
	// 스카이박스 후 메쉬 파이프라인 복구
	m_pDeviceContext->IASetInputLayout(m_pMeshIL);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->VSSetShader(m_pMeshVS, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pMeshPS, nullptr, 0);
	if (m_pSamplerLinear) m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinear);


	// (1) 그림자 맵 SRV: PS t5
	ID3D11ShaderResourceView* shSRV = mShadowSRV.Get();
	ctx->PSSetShaderResources(5, 1, &shSRV);

	// (2) 비교 샘플러: PS s1
	ID3D11SamplerState* shCmp = mSamShadowCmp.Get();
	ctx->PSSetSamplers(1, 1, &shCmp);

	// (3) 라이트 VP/파라미터 CB: b6  (안전하게 매 프레임 재바인드)
	ID3D11Buffer* b6 = mCB_Shadow.Get();
	ctx->VSSetConstantBuffers(6, 1, &b6); // VS에서 쓸 일 있으면 유지
	ctx->PSSetConstantBuffers(6, 1, &b6);


	// ===== (SHADOW) 본 패스용 바인딩: PS b6 / t5 / s1 =====
	{
		struct ShadowCB_ { Matrix LVP; Vector4 Params; } scb;
		scb.LVP = XMMatrixTranspose(mLightView * mLightProj);     // Shadow PASS에서 만든 멤버 사용
		scb.Params = Vector4(mShadowCmpBias, 1.0f / mShadowW, 1.0f / mShadowH, 0.0f);

		m_pDeviceContext->UpdateSubresource(mCB_Shadow.Get(), 0, nullptr, &scb, 0, 0);

		ID3D11Buffer* b6 = mCB_Shadow.Get();
		m_pDeviceContext->PSSetConstantBuffers(6, 1, &b6);           // b6

		ID3D11ShaderResourceView* s = mShadowSRV.Get();
		m_pDeviceContext->PSSetShaderResources(5, 1, &s);            // t5

		ID3D11SamplerState* cmp = mSamShadowCmp.Get();
		m_pDeviceContext->PSSetSamplers(1, 1, &cmp);                 // s1
	}



	// ===== B) OPAQUE =====
	{
		float bf[4] = { 0,0,0,0 };
		m_pDeviceContext->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

		if (mDbg.depthWriteOff && m_pDSS_Disabled) {
			m_pDeviceContext->OMSetDepthStencilState(m_pDSS_Disabled, 0);
		}
		else {
			m_pDeviceContext->OMSetDepthStencilState(m_pDSS_Opaque, 0);
		}

		if (mDbg.showOpaque) {
			BindStatic();
			if (mTreeX.enabled)  DrawOpaqueOnly(gTree, gTreeMtls, ComposeSRT(mTreeX));
			if (mCharX.enabled)  DrawOpaqueOnly(gChar, gCharMtls, ComposeSRT(mCharX));
			if (mZeldaX.enabled) DrawOpaqueOnly(gZelda, gZeldaMtls, ComposeSRT(mZeldaX));
			if (mBoxRig && mBoxX.enabled) {
				mBoxRig->DrawOpaqueOnly(
					m_pDeviceContext,
					ComposeSRT(mBoxX),
					/* view / proj */        view, m_Projection,
					/* cb/use */             m_pConstantBuffer, m_pUseCB,
					/* light */              cb.vLightDir, cb.vLightColor,
					/* eye */                eye,
					/* material */           m_Ka, m_Ks, m_Shininess, m_Ia,
					/* disable toggles */    mDbg.disableNormal, mDbg.disableSpecular, mDbg.disableEmissive
				);
			}
			if (mSkinRig && mSkinX.enabled) {
				BindSkinned();
				mSkinRig->DrawOpaqueOnly(
					ctx,
					ComposeSRT(mSkinX),
					view, m_Projection,
					m_pConstantBuffer, m_pUseCB, m_pBoneCB,
					vLightDir, vLightColor,
					eye, m_Ka, m_Ks, m_Shininess, m_Ia,
					mDbg.disableNormal, mDbg.disableSpecular, mDbg.disableEmissive
				);

				BindStatic();
			}
		}
	}




	// ===== (투명 패스 전) RS 백업 & 필요시 override =====
	ID3D11RasterizerState* oldRS = nullptr;
	m_pDeviceContext->RSGetState(&oldRS); // AddRef됨

	// wireframe가 켜져있으면 wire RS(이미 CullNone) 우선, 아니면 CullNone(SOLID)
	ID3D11RasterizerState* passRS = nullptr;
	if (mDbg.wireframe && m_pWireRS) {
		passRS = m_pWireRS;
	}
	else if (mDbg.cullNone && m_pDbgRS) { // m_pDbgRS = SOLID + CullNone
		passRS = m_pDbgRS;
	}
	if (passRS) m_pDeviceContext->RSSetState(passRS);

	// ===== B2) CUTOUT (alpha-test) =====
	if (mDbg.forceAlphaClip) {
		float bf[4] = { 0,0,0,0 };
		m_pDeviceContext->OMSetBlendState(nullptr, bf, 0xFFFFFFFF); // 블렌딩 OFF
		m_pDeviceContext->OMSetDepthStencilState(m_pDSS_Opaque, 0); // 깊이쓰기 ON

		if (mDbg.cullNone && m_pDbgRS) m_pDeviceContext->RSSetState(m_pDbgRS);

		if (mDbg.showTransparent) {
			BindStatic();
			if (mTreeX.enabled)  DrawAlphaCutOnly(gTree, gTreeMtls, ComposeSRT(mTreeX));
			if (mCharX.enabled)  DrawAlphaCutOnly(gChar, gCharMtls, ComposeSRT(mCharX));
			if (mZeldaX.enabled) DrawAlphaCutOnly(gZelda, gZeldaMtls, ComposeSRT(mZeldaX));

			if (mBoxRig && mBoxX.enabled) {
				mBoxRig->DrawAlphaCutOnly(
					m_pDeviceContext,
					ComposeSRT(mBoxX),
					/* view/proj */       view, m_Projection,
					/* cb/use */          m_pConstantBuffer, m_pUseCB,
					/* alphaCut */        mDbg.alphaCut,
					/* light */           cb.vLightDir, cb.vLightColor,
					/* eye/material */    eye, m_Ka, m_Ks, m_Shininess, m_Ia,
					/* toggles */         mDbg.disableNormal, mDbg.disableSpecular, mDbg.disableEmissive
				);
			}

			if (mSkinRig && mSkinX.enabled) {
				BindSkinned();
				mSkinRig->DrawAlphaCutOnly(
					ctx,
					ComposeSRT(mSkinX),
					view, m_Projection,
					m_pConstantBuffer, m_pUseCB, m_pBoneCB,
					vLightDir, vLightColor,
					eye, m_Ka, m_Ks, m_Shininess, m_Ia,
					mDbg.disableNormal, mDbg.disableSpecular, mDbg.disableEmissive
				);
				BindStatic();
				
			}
		}
	}

	// ===== C) TRANSPARENT =====
	{
		ID3D11BlendState* oldBS = nullptr; float oldBF[4]; UINT oldSM = 0xFFFFFFFF;
		m_pDeviceContext->OMGetBlendState(&oldBS, oldBF, &oldSM);
		ID3D11DepthStencilState* oldDSS = nullptr; UINT oldSR = 0;
		m_pDeviceContext->OMGetDepthStencilState(&oldDSS, &oldSR);

		float bf[4] = { 0,0,0,0 };
		m_pDeviceContext->OMSetBlendState(m_pBS_Alpha, bf, 0xFFFFFFFF);

		if (mDbg.depthWriteOff && m_pDSS_Disabled) {
			m_pDeviceContext->OMSetDepthStencilState(m_pDSS_Disabled, 0);
		}
		else {
			m_pDeviceContext->OMSetDepthStencilState(m_pDSS_Trans, 0);
		}

		if (mDbg.showTransparent) {
			BindStatic();
			if (mTreeX.enabled)  DrawTransparentOnly(gTree, gTreeMtls, ComposeSRT(mTreeX));
			if (mCharX.enabled)  DrawTransparentOnly(gChar, gCharMtls, ComposeSRT(mCharX));
			if (mZeldaX.enabled) DrawTransparentOnly(gZelda, gZeldaMtls, ComposeSRT(mZeldaX));

			if (mBoxRig && mBoxX.enabled) {
				mBoxRig->DrawTransparentOnly(
					m_pDeviceContext,
					ComposeSRT(mBoxX),
					/* view/proj */       view, m_Projection,
					/* cb/use */          m_pConstantBuffer, m_pUseCB,
					/* light */           cb.vLightDir, cb.vLightColor,
					/* eye/material */    eye, m_Ka, m_Ks, m_Shininess, m_Ia,
					/* toggles */         mDbg.disableNormal, mDbg.disableSpecular, mDbg.disableEmissive
				);
			}

			if (mSkinRig && mSkinX.enabled) {
				BindSkinned();
				mSkinRig->DrawTransparentOnly(
					ctx,
					ComposeSRT(mSkinX),
					view, m_Projection,
					m_pConstantBuffer, m_pUseCB, m_pBoneCB,
					vLightDir, vLightColor,
					eye, m_Ka, m_Ks, m_Shininess, m_Ia,
					mDbg.disableNormal, mDbg.disableSpecular, mDbg.disableEmissive
				);
				BindStatic();

			}

			m_pDeviceContext->OMSetBlendState(oldBS, oldBF, oldSM);
			m_pDeviceContext->OMSetDepthStencilState(oldDSS, oldSR);
			SAFE_RELEASE(oldBS); SAFE_RELEASE(oldDSS);
		}

		// ===== 투명 패스 끝나고 RS 복원 =====
		if (oldRS) {
			m_pDeviceContext->RSSetState(oldRS);
			oldRS->Release();
		}

		// ===== D) DEBUG: Directional light arrow in world (origin base, unlit) =====
		if (mDbg.showLightArrow) {
			// 1) -lightDir로 향하게 (광선 진행방향)
			Vector3 D = -dir;   // dir은 이미 Vector3임
			D.Normalize();      // 멤버 Normalize()는 in-place, 반환값 없음

			Matrix worldArrow =
				Matrix::CreateScale(m_ArrowScale) *
				Matrix::CreateWorld(m_ArrowPos, D, Vector3::UnitY);

			// 3) CB0 업데이트 (World만 교체)
			ConstantBuffer local = cb;
			local.mWorld = XMMatrixTranspose(worldArrow);
			local.mWorldInvTranspose = worldArrow.Invert();
			m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &local, 0, 0);
			m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);

			// 4) 상태 백업
			ID3D11RasterizerState* oldRS = nullptr; m_pDeviceContext->RSGetState(&oldRS);
			ID3D11DepthStencilState* oldDSS = nullptr; UINT oldRef = 0; m_pDeviceContext->OMGetDepthStencilState(&oldDSS, &oldRef);
			ID3D11BlendState* oldBS = nullptr; float oldBF[4]; UINT oldSM = 0xFFFFFFFF; m_pDeviceContext->OMGetBlendState(&oldBS, oldBF, &oldSM);
			ID3D11InputLayout* oldIL = nullptr; m_pDeviceContext->IAGetInputLayout(&oldIL);
			ID3D11VertexShader* oldVS = nullptr; m_pDeviceContext->VSGetShader(&oldVS, nullptr, 0);
			ID3D11PixelShader* oldPS = nullptr; m_pDeviceContext->PSGetShader(&oldPS, nullptr, 0);

			// 5) 파이프라인 바인드 (깊이 ON, 블렌드 OFF, CullNone)
			float bf[4] = { 0,0,0,0 };
			m_pDeviceContext->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);
			m_pDeviceContext->OMSetDepthStencilState(m_pDSS_Opaque, 0);
			if (m_pDbgRS) m_pDeviceContext->RSSetState(m_pDbgRS);

			UINT stride = sizeof(DirectX::XMFLOAT3) + sizeof(DirectX::XMFLOAT4);
			UINT offset = 0;
			m_pDeviceContext->IASetInputLayout(m_pDbgIL);
			m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pArrowVB, &stride, &offset);
			m_pDeviceContext->IASetIndexBuffer(m_pArrowIB, DXGI_FORMAT_R16_UINT, 0);
			m_pDeviceContext->VSSetShader(m_pDbgVS, nullptr, 0);
			m_pDeviceContext->PSSetShader(m_pDbgPS, nullptr, 0);

			// 6) Draw
			const UINT indexCount = 6   // back
				+ 24 // sides
				+ 6  // head base
				+ 12;// head sides

			// 항상 밝은 노랑(원하면 바꿔)
			const DirectX::XMFLOAT4 kBright = { 1.0f, 0.95f, 0.2f, 1.0f };
			m_pDeviceContext->UpdateSubresource(m_pDbgCB, 0, nullptr, &kBright, 0, 0);
			m_pDeviceContext->PSSetConstantBuffers(3, 1, &m_pDbgCB);

			// 혹시 남아있는 SRV 상태가 이상하게 영향 주는 카드가 있어서, 그냥 싹 언바인드
			ID3D11ShaderResourceView* nullSRV[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
			m_pDeviceContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, nullSRV);

			m_pDeviceContext->DrawIndexed(indexCount, 0, 0);

			// 7) 상태 복원
			m_pDeviceContext->VSSetShader(oldVS, nullptr, 0);
			m_pDeviceContext->PSSetShader(oldPS, nullptr, 0);
			m_pDeviceContext->IASetInputLayout(oldIL);
			m_pDeviceContext->OMSetBlendState(oldBS, oldBF, oldSM);
			m_pDeviceContext->OMSetDepthStencilState(oldDSS, oldRef);
			m_pDeviceContext->RSSetState(oldRS);
			SAFE_RELEASE(oldVS); SAFE_RELEASE(oldPS); SAFE_RELEASE(oldIL);
			SAFE_RELEASE(oldBS); SAFE_RELEASE(oldDSS); SAFE_RELEASE(oldRS);
		}

		// ===== Debug Grid (opaque, receives shadow) =====
		{
			// 상태 설정 (불투명)
			float bf[4] = { 0,0,0,0 };
			m_pDeviceContext->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);
			m_pDeviceContext->OMSetDepthStencilState(m_pDSS_Opaque, 0);
			m_pDeviceContext->RSSetState(m_pCullBackRS); // 위쪽을 보게 CCW로 만들었음

			// CB0: World(=Identity), 나머지는 현재 view/proj 그대로
			ConstantBuffer local = {};
			local.mWorld = XMMatrixTranspose(Matrix::Identity);
			local.mWorldInvTranspose = Matrix::Identity;
			local.mView = XMMatrixTranspose(view);
			local.mProjection = XMMatrixTranspose(m_Projection);
			local.vLightDir = cb.vLightDir;
			local.vLightColor = cb.vLightColor;
			m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &local, 0, 0);
			m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
			m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

			// b1(ambient, kA 등) 이미 바인딩되어 있음 (위에서 m_pBlinnCB)

			UINT stride = sizeof(DirectX::XMFLOAT3);
			UINT offset = 0;
			m_pDeviceContext->IASetInputLayout(mGridIL.Get());
			m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			m_pDeviceContext->IASetVertexBuffers(0, 1, mGridVB.GetAddressOf(), &stride, &offset);
			m_pDeviceContext->IASetIndexBuffer(mGridIB.Get(), DXGI_FORMAT_R16_UINT, 0);
			m_pDeviceContext->VSSetShader(mGridVS.Get(), nullptr, 0);
			m_pDeviceContext->PSSetShader(mGridPS.Get(), nullptr, 0);

			// 섀도우 사용: 이미 위에서 PS b6/t5/s1 바인딩되어 있어야 함
			m_pDeviceContext->DrawIndexed(mGridIndexCount, 0, 0);

			// 필요시 메쉬 셰이더로 복구 (아래에 다시 BindStatic 있으니 생략 가능)
		}




#ifdef _DEBUG
		UpdateImGUI();
#endif
		m_pSwapChain->Present(1, 0);
	}
}

//================================================================================================
//////////////////////////////////////////////////////////////////////////////////////////////////
//================================================================================================

bool TutorialApp::InitScene()
{
	CreateShadowResources(m_pDevice);
	CreateDepthOnlyShaders(m_pDevice);

	// 1) PNTT 셰이더/IL =================================================================
	{
		ID3D10Blob* vsb = nullptr;
		HR_T(CompileShaderFromFile(L"Shader/VertexShader.hlsl", "main", "vs_5_0", &vsb));
		HR_T(m_pDevice->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &m_pMeshVS));

		D3D11_INPUT_ELEMENT_DESC il[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		HR_T(m_pDevice->CreateInputLayout(il, _countof(il), vsb->GetBufferPointer(), vsb->GetBufferSize(), &m_pMeshIL));
		SAFE_RELEASE(vsb);

		ID3D10Blob* psb = nullptr;
		HR_T(CompileShaderFromFile(L"Shader/PixelShader.hlsl", "main", "ps_5_0", &psb));
		HR_T(m_pDevice->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &m_pMeshPS));
		SAFE_RELEASE(psb);
	}

	// === DebugColor shaders / IL ===
	{
		ID3D10Blob* vsb = nullptr;
		HR_T(CompileShaderFromFile(L"../Resource/DebugColor_VS.hlsl", "main", "vs_5_0", &vsb));
		HR_T(m_pDevice->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &m_pDbgVS));

		D3D11_INPUT_ELEMENT_DESC il[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		HR_T(m_pDevice->CreateInputLayout(il, _countof(il), vsb->GetBufferPointer(), vsb->GetBufferSize(), &m_pDbgIL));
		SAFE_RELEASE(vsb);

		ID3D10Blob* psb = nullptr;
		HR_T(CompileShaderFromFile(L"../Resource/DebugColor_PS.hlsl", "main", "ps_5_0", &psb));
		HR_T(m_pDevice->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &m_pDbgPS));
		SAFE_RELEASE(psb);
	}

	// ===== Skinned VS(+IL) =====
	{
		D3D_SHADER_MACRO defs[] = { {"SKINNED","1"}, {nullptr,nullptr} };

		ID3DBlob* vsbSkin = nullptr;
		HR_T(D3DCompileFromFile(
			L"Shader/VertexShaderSkinning.hlsl",
			defs,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			"main", "vs_5_0", 0, 0,
			&vsbSkin, nullptr));

		HR_T(m_pDevice->CreateVertexShader(
			vsbSkin->GetBufferPointer(),
			vsbSkin->GetBufferSize(),
			nullptr,
			&m_pSkinnedVS));

		// 스키닝용 InputLayout (PNTT + BI/BW)
		D3D11_INPUT_ELEMENT_DESC ILD_SKIN[] = {
			{"POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,         0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,         0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,            0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT,      0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,           0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,      0, 52, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};
		HR_T(m_pDevice->CreateInputLayout(
			ILD_SKIN, _countof(ILD_SKIN),
			vsbSkin->GetBufferPointer(),
			vsbSkin->GetBufferSize(),
			&m_pSkinnedIL));

		SAFE_RELEASE(vsbSkin);
	}

	// ===== Bone Palette CB (VS b4) =====
	{
		D3D11_BUFFER_DESC cbd{};
		cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbd.Usage = D3D11_USAGE_DEFAULT;
		cbd.ByteWidth = sizeof(DirectX::XMFLOAT4X4) * 256; // 256 bones 가정
		HR_T(m_pDevice->CreateBuffer(&cbd, nullptr, &m_pBoneCB));
	}

	// === Debug Arrow mesh (월드축 +Z 기준) ===
	{
		struct V { DirectX::XMFLOAT3 p; DirectX::XMFLOAT4 c; };
		// 굵기/길이(원하면 숫자만 바꿔)
		const float halfT = 6.0f;     // 샤프트 반두께 → 전체 두께 12
		const float shaftLen = 120.0f;   // 샤프트 길이
		const float headLen = 30.0f;    // 화살촉 길이
		const float headHalf = 10.0f;    // 화살촉 베이스 반폭
		const DirectX::XMFLOAT4 YELLOW = { 1.0f, 0.9f, 0.1f, 1.0f };

		// 인덱스 매핑용
		enum {
			s0, s1, s2, s3, s4, s5, s6, s7, // shaft 8
			h0, h1, h2, h3,                 // head base 4
			tip,                            // tip 1
			COUNT
		};
		const float zEps = 0.05f; // 0.01~0.2 사이

		V verts[COUNT] = {
			// shaft back(z=0)
			{{-halfT,-halfT, 0}, YELLOW}, {{+halfT,-halfT, 0}, YELLOW},
			{{+halfT,+halfT, 0}, YELLOW}, {{-halfT,+halfT, 0}, YELLOW},
			// shaft front(z=shaftLen)
			{{-halfT,-halfT, shaftLen}, YELLOW}, {{+halfT,-halfT, shaftLen}, YELLOW},
			{{+halfT,+halfT, shaftLen}, YELLOW}, {{-halfT,+halfT, shaftLen}, YELLOW},


			{{-headHalf,-headHalf, shaftLen}, YELLOW},
			{{+headHalf,-headHalf, shaftLen}, YELLOW},
			{{+headHalf,+headHalf, shaftLen}, YELLOW},
			{{-headHalf,+headHalf, shaftLen}, YELLOW},

			// tip
			{{0,0, shaftLen + headLen}, YELLOW},
		};


		uint16_t idx[] = {
			s0,s2,s1,  s0,s3,s2,
			s0,s1,s5,  s0,s5,s4,
			s1,s2,s6,  s1,s6,s5,
			s3,s7,s6,  s3,s6,s2,
			s0,s4,s7,  s0,s7,s3,

			// head base cap
			h2,h1,h0,
			h3,h2,h0,
			// head sides
			h0,h1,tip,
			h1,h2,tip,
			h2,h3,tip,
			h3,h0,tip,
		};

		// VB
		D3D11_BUFFER_DESC vbd{};
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.Usage = D3D11_USAGE_IMMUTABLE;
		vbd.ByteWidth = sizeof(verts);
		D3D11_SUBRESOURCE_DATA vinit{ verts };
		HR_T(m_pDevice->CreateBuffer(&vbd, &vinit, &m_pArrowVB));

		// IB
		D3D11_BUFFER_DESC ibd{};
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibd.Usage = D3D11_USAGE_IMMUTABLE;
		ibd.ByteWidth = sizeof(idx);
		D3D11_SUBRESOURCE_DATA iinit{ idx };
		HR_T(m_pDevice->CreateBuffer(&ibd, &iinit, &m_pArrowIB));
	}

	// ===== 초기 트랜스폼 스냅샷 =====
	mTreeX.pos = { -100, -150, 100 };  mTreeX.initPos = mTreeX.pos;
	mTreeX.scl = { 100,100,100 };
	mCharX.pos = { 100, -150, 100 };  mCharX.initPos = mCharX.pos;
	mZeldaX.pos = { 0, -150, 250 };  mZeldaX.initPos = mZeldaX.pos;
	mBoxX.pos = { -200, -300, 400 };
	mSkinX.pos = { 200, -150, 400 };

	mTreeX.enabled = mCharX.enabled = mZeldaX.enabled = mBoxX.enabled = false; // 초기 설정 숨기기

	mTreeX.initScl = mTreeX.scl; mCharX.initScl = mCharX.scl; mZeldaX.initScl = mZeldaX.scl;
	mTreeX.initRotD = mTreeX.rotD; mCharX.initRotD = mCharX.rotD; mZeldaX.initRotD = mZeldaX.rotD;

	mBoxX.initScl = mBoxX.scl;	mBoxX.initRotD = mBoxX.rotD; mBoxX.initPos = mBoxX.pos;
	mSkinX.initScl = mSkinX.scl; mSkinX.initRotD = mSkinX.rotD; mSkinX.initPos = mSkinX.pos;
	
	// PS b3: dbgColor
	{
		D3D11_BUFFER_DESC bd{};
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = 16; // float4 = 16
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pDbgCB));
	}

	// 2) 상수버퍼(CB0, b1, b2) + 샘플러 ==================================================
	{
		// CB0: World/View/Proj/WorldInvTranspose + Light
		if (!m_pConstantBuffer) {
			D3D11_BUFFER_DESC bd{};
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bd.ByteWidth = sizeof(ConstantBuffer);
			HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pConstantBuffer));
		}

		// BlinnPhong(b1): Eye, kA, kSAlpha, I_ambient
		if (!m_pBlinnCB) {
			D3D11_BUFFER_DESC bd{};
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bd.ByteWidth = sizeof(BlinnPhongCB);
			HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pBlinnCB));
		}

		// UseCB(b2): 텍스처 사용 플래그 + alphaCut
		if (!m_pUseCB) {
			D3D11_BUFFER_DESC bd{};
			bd.Usage = D3D11_USAGE_DEFAULT;
			bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bd.ByteWidth = sizeof(UseCB);
			HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pUseCB));
		}

		// PS 샘플러(Linear Wrap)
		if (!m_pSamplerLinear) {
			D3D11_SAMPLER_DESC sd{};
			sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			sd.MaxLOD = D3D11_FLOAT32_MAX;
			HR_T(m_pDevice->CreateSamplerState(&sd, &m_pSamplerLinear));
		}
	}

	// 3) FBX 로딩 + GPU 업로드 ============================================================
	{
		// 확장 임포터 + GPU 빌더
		auto BuildAll = [&](const std::wstring& fbx, const std::wstring& texDir,
			StaticMesh& mesh, std::vector<MaterialGPU>& mtls)
			{
				MeshData_PNTT cpu;
				// flipUV = true 권장, 좌표계는 LH
				if (!AssimpImporterEx::LoadFBX_PNTT_AndMaterials(fbx, cpu, /*flipUV*/true, /*leftHanded*/true))
					throw std::runtime_error("FBX load failed");

				if (!mesh.Build(m_pDevice, cpu))
					throw std::runtime_error("Mesh build failed");

				mtls.resize(cpu.materials.size());
				for (size_t i = 0; i < cpu.materials.size(); ++i) {
					mtls[i].Build(m_pDevice, cpu.materials[i], texDir);
				}
			};

		BuildAll(L"../Resource/Tree/Tree.fbx", L"../Resource/Tree/", gTree, gTreeMtls);
		BuildAll(L"../Resource/Character/Character.fbx", L"../Resource/Character/", gChar, gCharMtls);
		BuildAll(L"../Resource/Zelda/zeldaPosed001.fbx", L"../Resource/Zelda/", gZelda, gZeldaMtls);

		BuildAll(L"../Resource/BoxHuman/BoxHuman.fbx", L"../Resource/BoxHuman/", gBoxHuman, gBoxMtls);
		mBoxRig = RigidSkeletal::LoadFromFBX(
			m_pDevice,
			L"../Resource/BoxHuman/BoxHuman.fbx",
			L"../Resource/BoxHuman/"
		);

		// ===== SkinningTest.fbx 로드 =====
		{
			mSkinRig = SkinnedSkeletal::LoadFromFBX(
				m_pDevice,
				L"../Resource/Skinning/SkinningTest.fbx",   // 너의 리소스 경로에 맞춰 조정
				L"../Resource/Skinning/"                    // 텍스처 루트
			);
		}

		if (mSkinRig && m_pBoneCB) {
			mSkinRig->WarmupBoneCB(m_pDeviceContext, m_pBoneCB);
		}
	}
	// === Rasterizer states ===
	{
		// BACK CULL (기본)
		D3D11_RASTERIZER_DESC rsBack{};
		rsBack.FillMode = D3D11_FILL_SOLID;
		rsBack.CullMode = D3D11_CULL_BACK;
		rsBack.FrontCounterClockwise = FALSE;
		rsBack.DepthClipEnable = TRUE;
		HR_T(m_pDevice->CreateRasterizerState(&rsBack, &m_pCullBackRS));

		// SOLID + Cull None (양면) 
		D3D11_RASTERIZER_DESC rsNone{};
		rsNone.FillMode = D3D11_FILL_SOLID;
		rsNone.CullMode = D3D11_CULL_NONE;
		rsNone.FrontCounterClockwise = FALSE;
		rsNone.DepthClipEnable = TRUE;
		HR_T(m_pDevice->CreateRasterizerState(&rsNone, &m_pDbgRS));

		// 와이어프레임 + Cull None
		D3D11_RASTERIZER_DESC rw{};
		rw.FillMode = D3D11_FILL_WIREFRAME;
		rw.CullMode = D3D11_CULL_NONE;
		rw.FrontCounterClockwise = FALSE;
		rw.DepthClipEnable = TRUE;
		HR_T(m_pDevice->CreateRasterizerState(&rw, &m_pWireRS));

		// 깊이 끔(디버깅)
		D3D11_DEPTH_STENCIL_DESC dsOff{};
		dsOff.DepthEnable = FALSE;
		dsOff.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsOff.StencilEnable = FALSE;
		HR_T(m_pDevice->CreateDepthStencilState(&dsOff, &m_pDSS_Disabled));
	}

	//======================  SKYBOX: Shaders / IL  ======================
	{
		ID3D10Blob* vsb = nullptr;
		HR_T(CompileShaderFromFile(L"../Resource/Sky_VS.hlsl", "main", "vs_5_0", &vsb));
		HR_T(m_pDevice->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &m_pSkyVS));

		// Sky VS는 position-only( float3 POSITION ) 기준
		D3D11_INPUT_ELEMENT_DESC il[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		HR_T(m_pDevice->CreateInputLayout(il, _countof(il), vsb->GetBufferPointer(), vsb->GetBufferSize(), &m_pSkyIL));
		SAFE_RELEASE(vsb);

		ID3D10Blob* psb = nullptr;
		HR_T(CompileShaderFromFile(L"../Resource/Sky_PS.hlsl", "main", "ps_5_0", &psb));
		HR_T(m_pDevice->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &m_pSkyPS));
		SAFE_RELEASE(psb);
	}

	//======================  SKYBOX: Geometry (unit cube)  ======================
	{
		struct SkyV { DirectX::XMFLOAT3 pos; };

		const SkyV v[] = {
			{{-1,-1,-1}}, {{-1,+1,-1}}, {{+1,+1,-1}}, {{+1,-1,-1}}, // back (z-)
			{{-1,-1,+1}}, {{-1,+1,+1}}, {{+1,+1,+1}}, {{+1,-1,+1}}, // front (z+)
		};
		const uint16_t idx[] = {
			// 각 면 CCW (밖을 향함). Cull FRONT라 내부면이 렌더됨.
			0,1,2, 0,2,3, // back
			4,6,5, 4,7,6, // front
			4,5,1, 4,1,0, // left
			3,2,6, 3,6,7, // right
			1,5,6, 1,6,2, // top
			4,0,3, 4,3,7  // bottom
		};

		// VB
		D3D11_BUFFER_DESC vbDesc{}; vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbDesc.ByteWidth = UINT(sizeof(v));
		vbDesc.Usage = D3D11_USAGE_DEFAULT;
		D3D11_SUBRESOURCE_DATA vinit{}; vinit.pSysMem = v;
		HR_T(m_pDevice->CreateBuffer(&vbDesc, &vinit, &m_pSkyVB));

		// IB
		D3D11_BUFFER_DESC ibDesc{}; ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibDesc.ByteWidth = UINT(sizeof(idx));
		ibDesc.Usage = D3D11_USAGE_DEFAULT;
		D3D11_SUBRESOURCE_DATA iinit{}; iinit.pSysMem = idx;
		HR_T(m_pDevice->CreateBuffer(&ibDesc, &iinit, &m_pSkyIB));
	}

	//======================  SKYBOX: Texture / Sampler  ======================
	{
		HR_T(CreateDDSTextureFromFile(m_pDevice,
			L"../Resource/Cubemap.dds", nullptr, &m_pSkySRV));

		D3D11_SAMPLER_DESC sd{}; // clamp가 세렝게티
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		HR_T(m_pDevice->CreateSamplerState(&sd, &m_pSkySampler));
	}

	//======================  SKYBOX: Depth/Raster states  ======================
	{
		// depth write 끄고, LEQUAL(sky는 write=ZERO가 핵심)
		D3D11_DEPTH_STENCIL_DESC sd{};
		sd.DepthEnable = TRUE;
		sd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		sd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		sd.StencilEnable = FALSE;
		HR_T(m_pDevice->CreateDepthStencilState(&sd, &m_pSkyDSS));

		// 내부 면 렌더링: FRONT 컬링
		D3D11_RASTERIZER_DESC rs{};
		rs.FillMode = D3D11_FILL_SOLID;
		rs.CullMode = D3D11_CULL_FRONT;
		rs.FrontCounterClockwise = FALSE;
		HR_T(m_pDevice->CreateRasterizerState(&rs, &m_pSkyRS));

	}

	//======================  ALPHA BLEND / DEPTH STATES  ======================
	{
		// Opaque: 깊이쓰기 ON
		D3D11_DEPTH_STENCIL_DESC dsO{};
		dsO.DepthEnable = TRUE;
		dsO.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsO.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		HR_T(m_pDevice->CreateDepthStencilState(&dsO, &m_pDSS_Opaque));

		// Transparent: 깊이쓰기 OFF
		D3D11_DEPTH_STENCIL_DESC dsT{};
		dsT.DepthEnable = TRUE;
		dsT.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // 이게 그 중요한 그거임
		dsT.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		HR_T(m_pDevice->CreateDepthStencilState(&dsT, &m_pDSS_Trans));

		// Straight Alpha: Src*SrcA + Dst*(1-SrcA)
		D3D11_BLEND_DESC bd{};
		auto& rt = bd.RenderTarget[0];
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		HR_T(m_pDevice->CreateBlendState(&bd, &m_pBS_Alpha));
	}

	// ==== 2-1) VB/IB: XZ 평면 두 삼각형 (CCW, 위를 바라보게) ====
	{
		struct V { DirectX::XMFLOAT3 pos; };
		float S = mGridHalfSize;
		float Y = mGridY;
		V v[4] = {
			{ {-S, Y, -S} },
			{ { S, Y, -S} },
			{ { S, Y,  S} },
			{ {-S, Y,  S} },
		};
		// winding flip (윗면이 보이도록 뒤집기)
		const uint16_t idx[] = { 0, 2, 1,  0, 3, 2 };

		mGridIndexCount = 6;

		D3D11_BUFFER_DESC vb = { sizeof(v), D3D11_USAGE_IMMUTABLE, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
		D3D11_SUBRESOURCE_DATA vsd = { v };
		HR_T(m_pDevice->CreateBuffer(&vb, &vsd, &mGridVB));

		D3D11_BUFFER_DESC ib = { sizeof(idx), D3D11_USAGE_IMMUTABLE, D3D11_BIND_INDEX_BUFFER, 0, 0, 0 };
		D3D11_SUBRESOURCE_DATA isd = { idx };
		HR_T(m_pDevice->CreateBuffer(&ib, &isd, &mGridIB));
	}

	// ==== 2-2) 셰이더 & IL ====
	{
		Microsoft::WRL::ComPtr<ID3DBlob> vsb, psb;
		HR_T(CompileShaderFromFile(L"Shader/DbgGrid.hlsl", "VS_Main", "vs_5_0", &vsb));
		HR_T(CompileShaderFromFile(L"Shader/DbgGrid.hlsl", "PS_Main", "ps_5_0", &psb));
		HR_T(m_pDevice->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &mGridVS));
		HR_T(m_pDevice->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &mGridPS));

		// 입력: POSITION(float3)만
		D3D11_INPUT_ELEMENT_DESC il[] = {
			{ "POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		HR_T(m_pDevice->CreateInputLayout(il, 1, vsb->GetBufferPointer(), vsb->GetBufferSize(), &mGridIL));
	}


	return true;
}

//================================================================================================
//////////////////////////////////////////////////////////////////////////////////////////////////
//================================================================================================

bool TutorialApp::InitD3D()
{
	HRESULT hr = 0;

	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;
	swapDesc.Windowed = true;
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	HR_T(D3D11CreateDeviceAndSwapChain(
		NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
		creationFlags, NULL, NULL, D3D11_SDK_VERSION,
		&swapDesc,
		&m_pSwapChain,
		&m_pDevice, NULL,
		&m_pDeviceContext));

	ID3D11Texture2D* pBackBufferTexture = nullptr;

	HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));

	HR_T(m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_pRenderTargetView));
	SAFE_RELEASE(pBackBufferTexture);

	//==============================================================================================

	D3D11_TEXTURE2D_DESC dsDesc = {};
	dsDesc.Width = m_ClientWidth;
	dsDesc.Height = m_ClientHeight;
	dsDesc.MipLevels = 1;
	dsDesc.ArraySize = 1;
	dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 흔한 조합
	dsDesc.SampleDesc.Count = 1;  // 스왑체인과 동일하게
	dsDesc.SampleDesc.Quality = 0;
	dsDesc.Usage = D3D11_USAGE_DEFAULT;
	dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	HR_T(m_pDevice->CreateTexture2D(&dsDesc, nullptr, &m_pDepthStencil));
	HR_T(m_pDevice->CreateDepthStencilView(m_pDepthStencil, nullptr, &m_pDepthStencilView));

	D3D11_DEPTH_STENCIL_DESC dss = {};
	dss.DepthEnable = TRUE;
	dss.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dss.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; // 스카이박스 쓸거면 LEQUAL이 편함. 기본은 LESS
	dss.StencilEnable = FALSE;
	HR_T(m_pDevice->CreateDepthStencilState(&dss, &m_pDepthStencilState));
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStencilState, 0);

	//==============================================================================================

	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)m_ClientWidth;
	viewport.Height = (float)m_ClientHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	m_pDeviceContext->RSSetViewports(1, &viewport);

	return true;
}

//================================================================================================

void TutorialApp::UninitScene()
{
	// FBX 전용 파이프라인 자원
	SAFE_RELEASE(m_pMeshIL);
	SAFE_RELEASE(m_pMeshVS);
	SAFE_RELEASE(m_pMeshPS);
	SAFE_RELEASE(m_pConstantBuffer);

	SAFE_RELEASE(m_pUseCB);
	SAFE_RELEASE(m_pNoCullRS);
	SAFE_RELEASE(m_pSamplerLinear);
	SAFE_RELEASE(m_pBlinnCB);

	SAFE_RELEASE(m_pSkyVS);
	SAFE_RELEASE(m_pSkyPS);
	SAFE_RELEASE(m_pSkyIL);
	SAFE_RELEASE(m_pSkyVB);
	SAFE_RELEASE(m_pSkyIB);
	SAFE_RELEASE(m_pSkySRV);
	SAFE_RELEASE(m_pSkySampler);
	SAFE_RELEASE(m_pSkyDSS);
	SAFE_RELEASE(m_pSkyRS);

	SAFE_RELEASE(m_pDbgRS);
	SAFE_RELEASE(m_pArrowIB);
	SAFE_RELEASE(m_pArrowVB);
	SAFE_RELEASE(m_pDbgIL);
	SAFE_RELEASE(m_pDbgVS);
	SAFE_RELEASE(m_pDbgPS);
	SAFE_RELEASE(m_pDbgCB);

	SAFE_RELEASE(m_pWireRS);
	SAFE_RELEASE(m_pCullBackRS);
	SAFE_RELEASE(m_pDSS_Disabled);

	SAFE_RELEASE(m_pBS_Alpha);
	SAFE_RELEASE(m_pDSS_Opaque);
	SAFE_RELEASE(m_pDSS_Trans);
	//머가 이리 많음
	SAFE_RELEASE(m_pSkinnedIL);
	SAFE_RELEASE(m_pSkinnedVS);
	SAFE_RELEASE(m_pBoneCB);
}

void TutorialApp::UninitD3D()
{
	SAFE_RELEASE(m_pDepthStencilState);
	SAFE_RELEASE(m_pDepthStencilView);
	SAFE_RELEASE(m_pDepthStencil);
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDevice);
}

//================================================================================================

bool TutorialApp::InitImGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	//폰트 등록
	ImGuiIO& io = ImGui::GetIO();
	const ImWchar* kr = io.Fonts->GetGlyphRangesKorean();
	io.Fonts->Clear();
	io.Fonts->AddFontFromFileTTF("../Resource/fonts/Regular.ttf", 16.0f, nullptr, kr);

	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(this->m_pDevice, this->m_pDeviceContext);


	return true;
}

void TutorialApp::UninitImGUI()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

//================================================================================================

void TutorialApp::UpdateImGUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(u8"임꾸이(IMGUI)"))
	{
		// 상단 상태
		ImGui::Text("FPS: %.1f (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
		ImGui::Separator();

		// 스냅샷 1회 저장
		static bool s_inited = false;
		static double s_initAnimT = 0.0;
		static float  s_initAnimSpeed = 1.0f;
		static Vector3 s_initCubePos{}, s_initCubeScale{};
		static float   s_initSpin = 0.0f, s_initFov = 60.0f, s_initNear = 0.1f, s_initFar = 1.0f;
		static Vector3 s_initLightColor{};
		static float   s_initLightYaw = 0.0f, s_initLightPitch = 0.0f, s_initLightIntensity = 1.0f;
		static Vector3 s_initKa{}, s_initIa{};
		static float   s_initKs = 0.5f, s_initShin = 32.0f;
		static Vector3 s_initArrowPos{}, s_initArrowScale{};
		if (!s_inited) {
			s_inited = true;
			s_initAnimT = mAnimT;
			s_initAnimSpeed = mAnimSpeed;
			s_initCubePos = cubeTransformA;   s_initCubeScale = cubeScale;   s_initSpin = spinSpeed;
			s_initFov = m_FovDegree;          s_initNear = m_Near;           s_initFar = m_Far;
			s_initLightColor = m_LightColor;  s_initLightYaw = m_LightYaw;   s_initLightPitch = m_LightPitch; s_initLightIntensity = m_LightIntensity;
			s_initKa = m_Ka; s_initIa = m_Ia; s_initKs = m_Ks; s_initShin = m_Shininess;
			s_initArrowPos = m_ArrowPos;      s_initArrowScale = m_ArrowScale;
		}

		// === Camera ===
		if (ImGui::CollapsingHeader(u8"Camera"))
		{
			ImGui::SliderFloat("FOV (deg)", &m_FovDegree, 10.0f, 120.0f, "%.1f");
			ImGui::DragFloat("Near", &m_Near, 0.001f, 0.0001f, 10.0f, "%.5f");
			ImGui::DragFloat("Far", &m_Far, 0.1f, 0.01f, 20000.0f);
			ImGui::Text(u8"카메라 속도 변경: F1 ~ F3");
			if (ImGui::Button(u8"카메라 초기화")) {
				m_FovDegree = s_initFov; m_Near = s_initNear; m_Far = s_initFar;
			}
		}

		// === Lighting ===
		if (ImGui::CollapsingHeader(u8"Lighting"))
		{
			ImGui::SliderAngle("Yaw", &m_LightYaw, -180.0f, 180.0f);
			ImGui::SliderAngle("Pitch", &m_LightPitch, -89.0f, 89.0f);
			ImGui::ColorEdit3("Color", (float*)&m_LightColor);
			ImGui::SliderFloat("Intensity", &m_LightIntensity, 0.0f, 5.0f);
			if (ImGui::Button(u8"조명 초기화")) {
				m_LightColor = s_initLightColor;
				m_LightYaw = s_initLightYaw;
				m_LightPitch = s_initLightPitch;
				m_LightIntensity = s_initLightIntensity;
			}
		}

		if (ImGui::Begin("Shadow / Light Camera"), ImGuiCond_FirstUseEver) {
			ImGui::Checkbox("Show ShadowMap", &mShUI.showSRV);
			if (mShUI.showSRV) {
				// DX11 백엔드는 SRV를 그대로 ImTextureID로 받는다.
				ImTextureID id = (ImTextureID)mShadowSRV.Get();
				if (id) ImGui::Image(id, ImVec2(256, 256), ImVec2(0, 0), ImVec2(1, 1));
				else    ImGui::TextUnformatted("Shadow SRV is null");
			}

			ImGui::SeparatorText("Mode");
			ImGui::Checkbox("Follow Camera Focus", &mShUI.followCamera);
			ImGui::Checkbox("Use Manual Light Position", &mShUI.useManualPos);

			if (mShUI.useManualPos) {
				ImGui::DragFloat3("Manual Light Pos", &mShUI.manualPos.x, 0.1f);
			}
			if (!mShUI.followCamera) {
				ImGui::DragFloat3("Manual LookAt", &mShUI.manualTarget.x, 0.1f);
			}

			ImGui::SeparatorText("Coverage");
			ImGui::Checkbox("Auto Cover Camera View", &mShUI.autoCover);
			ImGui::DragFloat("FocusDist", &mShUI.focusDist, 0.1f, 0.1f, 5000.0f);
			ImGui::DragFloat("LightDist", &mShUI.lightDist, 0.1f, 0.1f, 10000.0f);
			ImGui::DragFloat("Margin", &mShUI.coverMargin, 0.01f, 1.0f, 2.0f);

			if (!mShUI.autoCover) {
				// 수동 FOV/클립 (기존 멤버 사용)
				float deg = DirectX::XMConvertToDegrees(mShadowFovY);
				if (ImGui::DragFloat("FovY(deg)", &deg, 0.1f, 5.0f, 170.0f)) {
					mShadowFovY = DirectX::XMConvertToRadians(deg);
				}
				ImGui::DragFloat("NearZ", &mShadowNear, 0.01f, 0.01f, mShadowFar - 0.01f);
				ImGui::DragFloat("FarZ", &mShadowFar, 0.1f, mShadowNear + 0.01f, 100000.0f);
			}

			ImGui::SeparatorText("Bias");
			ImGui::DragFloat("CmpBias", &mShadowCmpBias, 0.0001f, 0.0f, 0.02f, "%.5f");
		}
		ImGui::End();

		// === Material (Blinn-Phong) ===
		if (ImGui::CollapsingHeader(u8"Material"))
		{
			ImGui::ColorEdit3("I_a (ambient light)", (float*)&m_Ia);
			ImGui::ColorEdit3("k_a (ambient refl.)", (float*)&m_Ka);
			ImGui::SliderFloat("k_s (specular)", &m_Ks, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("shininess", &m_Shininess, 2.0f, 256.0f, "%.0f");
			if (ImGui::Button(u8"재질 초기화")) {
				m_Ka = s_initKa; m_Ia = s_initIa; m_Ks = s_initKs; m_Shininess = s_initShin;
			}
		}

		// === Models ===
		if (ImGui::CollapsingHeader(u8"Models"))
		{
			auto ModelUI = [&](const char* name, XformUI& xf) {
				if (ImGui::TreeNode(name)) {
					ImGui::Checkbox("Enabled", &xf.enabled);
					ImGui::DragFloat3("Position", (float*)&xf.pos, 0.1f, -10000.0f, 10000.0f);
					ImGui::DragFloat3("Rotation (deg XYZ)", (float*)&xf.rotD, 0.5f, -720.0f, 720.0f);
					ImGui::DragFloat3("Scale", (float*)&xf.scl, 0.01f, 0.0001f, 1000.0f);
					if (ImGui::Button(u8"모델 초기화")) {
						xf.pos = xf.initPos; xf.rotD = xf.initRotD; xf.scl = xf.initScl; xf.enabled = true;
					}
					ImGui::TreePop();
				}
				};

			ModelUI("Tree", mTreeX);
			ModelUI("Character", mCharX);
			ModelUI("Zelda", mZeldaX);

			// Models 
			if (ImGui::TreeNode(u8"Debug Arrow (라이트 방향)")) {
				ImGui::Checkbox("Enabled", &mDbg.showLightArrow);
				ImGui::DragFloat3("Position", (float*)&m_ArrowPos, 0.1f, -10000.0f, 10000.0f);
				ImGui::DragFloat3("Scale", (float*)&m_ArrowScale, 0.01f, 0.0001f, 1000.0f);
				if (ImGui::Button(u8"화살표 초기화")) {
					m_ArrowPos = s_initArrowPos;
					m_ArrowScale = s_initArrowScale;
					mDbg.showLightArrow = true;
				}
				ImGui::TreePop();
			}

			if (ImGui::Button(u8"모든 모델 초기화")) {
				for (XformUI* p : { &mTreeX, &mCharX, &mZeldaX }) {
					p->pos = p->initPos; p->rotD = p->initRotD; p->scl = p->initScl; p->enabled = true;
				}
				m_ArrowPos = s_initArrowPos;
				m_ArrowScale = s_initArrowScale;
				mDbg.showLightArrow = true;
			}
		}

		if (ImGui::CollapsingHeader(u8"BoxHuman (RigidSkeletal)"))
		{
			// Transform
			ImGui::Checkbox("Enabled##Box", &mBoxX.enabled);
			ImGui::DragFloat3("Position##Box", (float*)&mBoxX.pos, 0.1f, -10000.0f, 10000.0f);
			ImGui::DragFloat3("Rotation (deg XYZ)##Box", (float*)&mBoxX.rotD, 0.5f, -720.0f, 720.0f);
			ImGui::DragFloat3("Scale##Box", (float*)&mBoxX.scl, 0.01f, 0.0001f, 1000.0f);
			if (ImGui::Button(u8"트랜스폼 초기화")) {
				mBoxX.pos = mBoxX.initPos; mBoxX.rotD = mBoxX.initRotD; mBoxX.scl = mBoxX.initScl; mBoxX.enabled = true;
			}

			ImGui::SeparatorText("Animation");
			if (mBoxRig)
			{
				const double tps = mBoxRig->GetTicksPerSecond();
				const double durS = mBoxRig->GetClipDurationSec();
				ImGui::Text("Ticks/sec: %.3f", tps);
				ImGui::Text("Duration : %.3f sec", durS);

				AnimUI("Controls",
					mBoxAC.play, mBoxAC.loop, mBoxAC.speed, mBoxAC.t,
					durS,
					[&](double tNow) { mBoxRig->EvaluatePose(tNow); });

				if (ImGui::Button(u8"애니메이션 초기화")) {
					mBoxAC.play = true; mBoxAC.loop = true; mBoxAC.speed = 1.0f; mBoxAC.t = 0.0;
					mBoxRig->EvaluatePose(mBoxAC.t);
				}
			}
			else {
				ImGui::TextDisabled("BoxHuman not loaded.");
			}
		}

		if (ImGui::CollapsingHeader(u8"SkinningTest (SkinnedSkeletal)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Enabled", &mSkinX.enabled);
			ImGui::DragFloat3("Position", (float*)&mSkinX.pos, 0.1f, -10000.0f, 10000.0f);
			ImGui::DragFloat3("Rotation (deg XYZ)", (float*)&mSkinX.rotD, 0.5f, -720.0f, 720.0f);
			ImGui::DragFloat3("Scale", (float*)&mSkinX.scl, 0.01f, 0.0001f, 1000.0f);
			if (ImGui::Button(u8"트랜스폼 초기화##skin")) {
				mSkinX.pos = mSkinX.initPos; mSkinX.rotD = mSkinX.initRotD; mSkinX.scl = mSkinX.initScl; mSkinX.enabled = true;
			}

			ImGui::SeparatorText("Animation");
			if (mSkinRig)
			{
				const double durS = mSkinRig->DurationSec();
				ImGui::Text("Duration : %.3f sec", durS);

				AnimUI("Controls##skin",
					mSkinAC.play, mSkinAC.loop, mSkinAC.speed, mSkinAC.t,
					durS,
					[&](double tNow) { mSkinRig->EvaluatePose(tNow); });

				if (ImGui::Button(u8"애니메이션 초기화##skin")) {
					mSkinAC.play = true; mSkinAC.loop = true; mSkinAC.speed = 1.0f; mSkinAC.t = 0.0;
					mSkinRig->EvaluatePose(mSkinAC.t);
				}
			}
			else {
				ImGui::TextDisabled("Skinned rig not loaded.");
			}
		}


		// === Toggles / Render Debug ===
		if (ImGui::CollapsingHeader(u8"Toggles & Debug"))
		{
			ImGui::Checkbox("Show Skybox", &mDbg.showSky);
			ImGui::Checkbox("Show Opaque", &mDbg.showOpaque);
			ImGui::Checkbox("Show Transparent", &mDbg.showTransparent);

			ImGui::Separator();

			ImGui::Checkbox("Wireframe", &mDbg.wireframe); ImGui::SameLine();
			ImGui::Checkbox("Cull None", &mDbg.cullNone);
			ImGui::Checkbox("Depth Write/Test OFF (mesh)", &mDbg.depthWriteOff);
			ImGui::Checkbox("Freeze Time", &mDbg.freezeTime); // 이거 작동 안함

			ImGui::Separator();

			ImGui::Checkbox("Disable Normal", &mDbg.disableNormal);
			ImGui::Checkbox("Disable Specular", &mDbg.disableSpecular);
			ImGui::Checkbox("Disable Emissive", &mDbg.disableEmissive);
			ImGui::Checkbox("Force AlphaClip", &mDbg.forceAlphaClip);
			ImGui::DragFloat("alphaCut", &mDbg.alphaCut, 0.01f, 0.0f, 1.0f);

			if (ImGui::Button(u8"디버그 토글 초기화")) {
				mDbg = DebugToggles(); // 리셋
			}
		}
	}

	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

//================================================================================================

#ifdef _DEBUG
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
#endif

LRESULT CALLBACK TutorialApp::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#ifdef _DEBUG
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;
#endif
	return __super::WndProc(hWnd, message, wParam, lParam);
}

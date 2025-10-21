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

using namespace DirectX::SimpleMath; // 남발하면 오염됨 아무튼 많이 쓰지 마셈

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


struct UseCB {
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

//================================================================================================

void TutorialApp::OnUpdate()
{
	float t = GameTimer::m_Instance->TotalTime();

	XMMATRIX mSpin = XMMatrixRotationY(t * spinSpeed);

	XMMATRIX mScaleA = XMMatrixScaling(cubeScale.x, cubeScale.y, cubeScale.z);
	XMMATRIX mScaleB = XMMatrixScaling(cubeScale.x * 0.6f, cubeScale.y * 0.6f, cubeScale.z * 0.6f);
	XMMATRIX mScaleC = XMMatrixScaling(cubeScale.x * 0.3f, cubeScale.y * 0.3f, cubeScale.z * 0.3f);

	XMMATRIX mTranslateA = XMMatrixTranslation(cubeTransformA.x, cubeTransformA.y, cubeTransformA.z);
	XMMATRIX mTranslateB = XMMatrixTranslation(cubeTransformB.x, cubeTransformB.y, cubeTransformB.z);
	XMMATRIX mTranslateC = XMMatrixTranslation(cubeTransformC.x, cubeTransformC.y, cubeTransformC.z);

	//오브젝트
	XMMATRIX tmpA = mSpin * mTranslateA;
	m_World = mScaleA * tmpA;
}

//================================================================================================
void TutorialApp::OnRender()
{
	m_pDeviceContext->RSSetState(m_pNoCullRS);
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	/*
	* float aspect = (float)m_ClientWidth / (float)m_ClientHeight; 
	m_Projection = Matrix::CreatePerspectiveFieldOfView( 
		DirectX::XMConvertToRadians(m_FovDegree), aspect, m_Near, m_Far);
		해당 코드 사용했을때, 마우스 반전되고, 앞 뒤 방향이 뒤틀렸음
		이유는 뭘까
		
		CreatePerspectiveFieldOfView << 오른손 좌표계임
		그런데, 내 파이프라인 구성은 왼손 좌표계인거임
		그래서 오류난거임 허허
	*/

	if (m_FovDegree < 10.0f)      m_FovDegree = 10.0f;
	else if (m_FovDegree > 120.0f) m_FovDegree = 120.0f;

	// 최소값 보장
	if (m_Near < 0.0001f) m_Near = 0.0001f;

	// Near보다 약간 더 크게
	float minFar = m_Near + 0.001f;
	if (m_Far < minFar) m_Far = minFar;

	float aspect = m_ClientWidth / (float)m_ClientHeight;
	m_Projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FovDegree), aspect, m_Near, m_Far);


	// ===== 1) 파이프라인 셋업 =====
	m_pDeviceContext->IASetInputLayout(m_pMeshIL);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->VSSetShader(m_pMeshVS, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pMeshPS, nullptr, 0);
	if (m_pSamplerLinear) m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

	// ===== 2) 공통 CB0(뷰/프로젝션/라이트) + b1(카메라/재질) =====
	Matrix view;
	ConstantBuffer cb{};
	m_Camera.GetViewMatrix(view);
	cb.mView = XMMatrixTranspose(view);
	cb.mProjection = XMMatrixTranspose(m_Projection);

	XMMATRIX R = XMMatrixRotationRollPitchYaw(m_LightPitch, m_LightYaw, 0.0f);
	XMVECTOR base = XMVector3Normalize(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
	XMVECTOR L = XMVector3Normalize(XMVector3TransformNormal(base, R));
	Vector3 dir = { XMVectorGetX(L), XMVectorGetY(L), XMVectorGetZ(L) };

	auto world = m_World;
	cb.mWorld = XMMatrixTranspose(Matrix::Identity);        // 개별 드로우에서 바꿔치기

	cb.mWorldInvTranspose = XMMatrixInverse(nullptr, Matrix::Identity);
	cb.vLightDir = Vector4(dir.x, dir.y, dir.z, 0.0f);
	cb.vLightColor = Vector4(m_LightColor.x * m_LightIntensity, m_LightColor.y * m_LightIntensity, m_LightColor.z * m_LightIntensity, 1.0f);

	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &cb, 0, 0);
	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);

	// b1: 카메라/재질 파라미터 (헤더 멤버: m_Ka, m_Ks, m_Shininess, m_Ia)
	BlinnPhongCB bp{};
	const Vector3 eye = m_Camera.m_World.Translation();
	bp.EyePosW = Vector4(eye.x, eye.y, eye.z, 1.0f);
	bp.kA = Vector4(m_Ka.x, m_Ka.y, m_Ka.z, 0.0f);
	bp.kSAlpha = Vector4(m_Ks, m_Shininess, 0.0f, 0.0f);
	bp.I_ambient = Vector4(m_Ia.x, m_Ia.y, m_Ia.z, 0.0f);

	m_pDeviceContext->UpdateSubresource(m_pBlinnCB, 0, nullptr, &bp, 0, 0);
	m_pDeviceContext->PSSetConstantBuffers(1, 1, &m_pBlinnCB);

	// ===== 3) 드로우 람다 (서브메시별 머티리얼 바인딩 + b2 USE 플래그) =====
	auto DrawModel = [&](StaticMesh& mesh,
		const std::vector<MaterialGPU>& mtls,
		const Matrix& world)
		{
			// CB0: 월드만 바꿔서 다시 업로드
			ConstantBuffer local = cb;
			local.mWorld = XMMatrixTranspose(world);
			local.mWorldInvTranspose = XMMatrixTranspose(world.Invert());
			m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &local, 0, 0);

			// 서브메시 루프
			for (size_t i = 0; i < mesh.Ranges().size(); ++i)
			{
				const auto& r = mesh.Ranges()[i];
				const auto& mat = mtls[r.materialIndex];

				// t0..t4 : Diffuse, Normal, Specular, Emissive, Opacity
				mat.Bind(m_pDeviceContext);


				UseCB use{};
				use.useDiffuse = mat.hasDiffuse ? 1u : 0u;
				use.useNormal = mat.hasNormal ? 1u : 0u;
				use.useSpecular = mat.hasSpecular ? 1u : 0u;
				use.useEmissive = mat.hasEmissive ? 1u : 0u;
				use.useOpacity = mat.hasOpacity ? 1u : 0u;
				use.alphaCut = mat.hasOpacity ? 0.5f : -1.0f; // ★ 핵심

				m_pDeviceContext->UpdateSubresource(m_pUseCB, 0, nullptr, &use, 0, 0);
				m_pDeviceContext->PSSetConstantBuffers(2, 1, &m_pUseCB);

				// 실제 드로우
				mesh.DrawSubmesh(m_pDeviceContext, i);

				// SRV 언바인드(디버그 런타임 경고 방지 습관)
				MaterialGPU::Unbind(m_pDeviceContext);
			}
		};

	// Draw
	DrawModel(gTree, gTreeMtls, Matrix::CreateTranslation(0, 0, -10));
	DrawModel(gChar, gCharMtls, Matrix::CreateTranslation(0, 0, -20));
	DrawModel(gZelda, gZeldaMtls, Matrix::CreateTranslation(10, 0, -20));

	// ====== SKYBOX PASS (draw last) ======
	{
		// RS/OM 상태 백업
		ID3D11RasterizerState* oldRS = nullptr;
		m_pDeviceContext->RSGetState(&oldRS);
		ID3D11DepthStencilState* oldDSS = nullptr; UINT oldStencilRef = 0;
		m_pDeviceContext->OMGetDepthStencilState(&oldDSS, &oldStencilRef);

		// 파이프라인 셋업		
		m_pDeviceContext->RSSetState(m_pSkyRS);
		//m_pDeviceContext->RSSetState(nullptr);
		m_pDeviceContext->OMSetDepthStencilState(m_pSkyDSS, 0);

		m_pDeviceContext->IASetInputLayout(m_pSkyIL);
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_pDeviceContext->VSSetShader(m_pSkyVS, nullptr, 0);
		m_pDeviceContext->PSSetShader(m_pSkyPS, nullptr, 0);

		// CB0 업데이트: view에서 translation 제거
		Matrix view; m_Camera.GetViewMatrix(view);
		Matrix viewRot = view;
		viewRot._41 = viewRot._42 = viewRot._43 = 0.0f; // SimpleMath Matrix는 행렬 멤버 직접 접근 가능

		ConstantBuffer skyCB = {};
		skyCB.mWorld = XMMatrixTranspose(Matrix::Identity);
		skyCB.mView = XMMatrixTranspose(viewRot);
		skyCB.mProjection = XMMatrixTranspose(m_Projection);
		skyCB.mWorldInvTranspose = XMMatrixTranspose(Matrix::Identity); // 안 씀
		skyCB.vLightDir = Vector4(0, 0, 0, 0);
		skyCB.vLightColor = Vector4(0, 0, 0, 0);
		m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &skyCB, 0, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);

		// 텍스처 바인딩 (Sky_PS.hlsl: TextureCube t0, Sampler s0 가정)
		m_pDeviceContext->PSSetShaderResources(0, 1, &m_pSkySRV);
		m_pDeviceContext->PSSetSamplers(0, 1, &m_pSkySampler);

		// VB/IB 바인딩 후 드로우
		UINT stride = sizeof(DirectX::XMFLOAT3);
		UINT offset = 0;
		ID3D11Buffer* vbs[] = { m_pSkyVB };
		m_pDeviceContext->IASetVertexBuffers(0, 1, vbs, &stride, &offset);
		m_pDeviceContext->IASetIndexBuffer(m_pSkyIB, DXGI_FORMAT_R16_UINT, 0);
		m_pDeviceContext->DrawIndexed(36, 0, 0);

		// SRV 언바인드 (디버그 경고 예방)
		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		m_pDeviceContext->PSSetShaderResources(0, 1, nullSRV);

		// 상태 복원
		m_pDeviceContext->RSSetState(oldRS);
		m_pDeviceContext->OMSetDepthStencilState(oldDSS, oldStencilRef);
		SAFE_RELEASE(oldRS);
		SAFE_RELEASE(oldDSS);
	}


#ifdef _DEBUG
	UpdateImGUI();
#endif

	m_pSwapChain->Present(1, 0);
}

//================================================================================================

bool TutorialApp::InitScene()
{
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
			{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};

		HR_T(m_pDevice->CreateInputLayout(il, _countof(il), vsb->GetBufferPointer(), vsb->GetBufferSize(), &m_pMeshIL));
		SAFE_RELEASE(vsb);

		ID3D10Blob* psb = nullptr;
		HR_T(CompileShaderFromFile(L"Shader/PixelShader.hlsl", "main", "ps_5_0", &psb));
		HR_T(m_pDevice->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &m_pMeshPS));
		SAFE_RELEASE(psb);
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

		// 과제 명세: tree( diffuse, opacity ), character( diffuse, normal, specular, emissive ), zelda( diffuse, opacity )
		// 경로는 네 프로젝트 구조에 맞게 조정해줘.
		BuildAll(L"../Resource/Tree/Tree.fbx", L"../Resource/Tree/", gTree, gTreeMtls);
		BuildAll(L"../Resource/Character/Character.fbx", L"../Resource/Character/", gChar, gCharMtls);
		BuildAll(L"../Resource/Zelda/zeldaPosed001.fbx", L"../Resource/Zelda/", gZelda, gZeldaMtls);
	}

	// 4) 초기 프로젝션/월드 값(있다면 유지) ==============================================
	// - m_Projection, m_World 등은 네 기존 코드 흐름(윈도우 리사이즈/카메라 세팅)에서 갱신되므로 여기선 건드리지 않아도 OK.

	D3D11_RASTERIZER_DESC rd{};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;         // ★ 컬링 끔
	rd.FrontCounterClockwise = FALSE;      // 일단 기본
	HR_T(m_pDevice->CreateRasterizerState(&rd, &m_pNoCullRS));

	return true;
}

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
		// 단위 큐브(센터 원점). 내부에서 보도록 FRONT 컬링 예정
		const SkyV v[] = {
			{{-1,-1,-1}}, {{-1,+1,-1}}, {{+1,+1,-1}}, {{+1,-1,-1}}, // back (z-)
			{{-1,-1,+1}}, {{-1,+1,+1}}, {{+1,+1,+1}}, {{+1,-1,+1}}, // front (z+)
		};
		const uint16_t idx[] = {
			// 각 면 CCW (밖을 향함). 우리는 Cull FRONT라 내부면이 렌더됨.
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
		// Hanako.dds 경로는 네 프로젝트 구조에 맞게 조정
		HR_T(CreateDDSTextureFromFile(m_pDevice,
			L"../Resource/Hanako.dds", nullptr, &m_pSkySRV));

		D3D11_SAMPLER_DESC sd{}; // clamp가 세렝게티
		sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sd.MaxLOD = D3D11_FLOAT32_MAX;
		HR_T(m_pDevice->CreateSamplerState(&sd, &m_pSkySampler));
	}

	//======================  SKYBOX: Depth/Raster states  ======================
	{
		// depth write 끄고, LEQUAL (네가 기본 dss를 LEQUAL로 써도, sky는 write=ZERO가 핵심)
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
	return true;
}

//================================================================================================
// Uninit

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
// 임꾸이

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

void TutorialApp::UpdateImGUI()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(u8"임꾸이(IMGUI)"))  // 창 제목만 깔끔히
	{
		// 0) 상단 바 — 프레임/리셋
		ImGui::Text("FPS: %.1f (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
		ImGui::Separator();

		// 섹션별 기본값 저장 (최초 한 번)
		static bool s_inited = false;
		static Vector3 s_initCubePos{}, s_initCubeScale{};
		static float   s_initSpin = 0.0f, s_initFov = 60.0f, s_initNear = 0.001f, s_initFar = 1.0f;
		static Vector3 s_initLightColor{};
		static float   s_initLightYaw = 0.0f, s_initLightPitch = 0.0f, s_initLightIntensity = 1.0f;
		static Vector3 s_initKa{}, s_initIa{};
		static float   s_initKs = 0.5f, s_initShin = 32.0f;
		if (!s_inited) {
			s_inited = true;
			s_initCubePos = cubeTransformA;
			s_initCubeScale = cubeScale;
			s_initSpin = spinSpeed;
			s_initFov = m_FovDegree;
			s_initNear = m_Near;
			s_initFar = m_Far;
			s_initLightColor = m_LightColor;
			s_initLightYaw = m_LightYaw;
			s_initLightPitch = m_LightPitch;
			s_initLightIntensity = m_LightIntensity;
			s_initKa = m_Ka; s_initIa = m_Ia;
			s_initKs = m_Ks; s_initShin = m_Shininess;
		}

		// 2) Transform
		if (ImGui::CollapsingHeader("Transform"))
		{
			ImGui::DragFloat3("Position", (float*)&cubeTransformA, 0.05f, -100.0f, 100.0f);
			ImGui::DragFloat3("Scale", (float*)&cubeScale, 0.01f, 0.001f, 100.0f);
			ImGui::SliderFloat("Spin Speed", &spinSpeed, 0.0f, 10.0f, "%.2f");

			if (ImGui::Button(u8"변환 초기화")) {
				cubeTransformA = s_initCubePos;
				cubeScale = s_initCubeScale;
				spinSpeed = s_initSpin;
			}
		}

		// 3) Camera
		if (ImGui::CollapsingHeader("Camera"))
		{
			ImGui::SliderFloat("FOV (deg)", &m_FovDegree, 10.0f, 120.0f, "%.1f");
			ImGui::DragFloat("Near", &m_Near, 0.001f, 0.0001f, 10.0f, "%.5f");
			ImGui::DragFloat("Far", &m_Far, 0.1f, 0.01f, 20000.0f);

			if (ImGui::Button(u8"카메라 초기화")) {
				m_FovDegree = s_initFov; m_Near = s_initNear; m_Far = s_initFar;
			}
		}

		// 4) Lighting
		if (ImGui::CollapsingHeader("Lighting"))
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

		// 5) Material (Blinn-Phong)
		if (ImGui::CollapsingHeader("Material"))
		{
			ImGui::ColorEdit3("I_a (ambient light)", (float*)&m_Ia);
			ImGui::ColorEdit3("k_a (ambient refl.)", (float*)&m_Ka);
			ImGui::SliderFloat("k_s (specular)", &m_Ks, 0.0f, 2.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::SliderFloat("shininess", &m_Shininess, 2.0f, 256.0f, "%.0f");


			if (ImGui::Button(u8"재질 초기화")) {
				m_Ka = s_initKa; m_Ia = s_initIa; m_Ks = s_initKs; m_Shininess = s_initShin;
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

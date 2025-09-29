//================================================================================================
// TutorialApp.cpp
//================================================================================================

#include "TutorialApp.h"
#include "../D3DCore/Helper.h"
#include <d3dcompiler.h>
#include <Directxtk/DDSTextureLoader.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

// 정점 선언.
struct Vertex
{
	Vector3 position;
	Vector3 normal;
	Vector2 tex;
};



struct ConstantBuffer // 상수버퍼
{
	Matrix mWorld;
	Matrix mView;
	Matrix mProjection;
	Matrix mWorldInvTranspose;

	Vector4 vLightDir[2]; // 한번에 2개의 광원을 처리하게 설계해서 2개짜리임
	Vector4 vLightColor[2];
	Vector4 vOutputColor;
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
{	//OM
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);

	//Clear
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	//================================================================================================

	ID3D11RasterizerState* prevRS = nullptr;
	m_pDeviceContext->RSGetState(&prevRS);

	// 상태: DepthWrite=ZERO + LEQUAL, Cull=FRONT
	m_pDeviceContext->OMSetDepthStencilState(m_pSkyDSS, 0);
	m_pDeviceContext->RSSetState(m_pSkyRS);

	// IA: 스카이용 IL (VB/IB는 기존 큐브 재사용)
	m_pDeviceContext->IASetInputLayout(m_pSkyIL);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &m_VertextBufferStride, &m_VertextBufferOffset);
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// VS/PS
	m_pDeviceContext->VSSetShader(m_pSkyVS, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pSkyPS, nullptr, 0);

	// CB: View의 이동 성분 제거
	Matrix camNoTrans;
	m_Camera.GetViewMatrix(camNoTrans);
	camNoTrans._41 = camNoTrans._42 = camNoTrans._43 = 0.0f;

	ConstantBuffer skyCB{};
	skyCB.mWorld = XMMatrixTranspose(XMMatrixIdentity());
	skyCB.mView = XMMatrixTranspose(camNoTrans);
	skyCB.mProjection = XMMatrixTranspose(m_Projection);
	skyCB.mWorldInvTranspose = XMMatrixIdentity(); // 안 씀

	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &skyCB, 0, 0);
	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);

	// 텍스처/샘플러
	m_pDeviceContext->PSSetShaderResources(0, 1, &m_pSkySRV);
	m_pDeviceContext->PSSetSamplers(0, 1, &m_pSkySampler);

	// Draw
	m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);

	// SRV 언바인드(다음 패스에서 t0 충돌 방지)
	ID3D11ShaderResourceView* nullSRV = nullptr;
	m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

	// 상태 복구
	m_pDeviceContext->OMSetDepthStencilState(m_pDepthStencilState, 0);
	m_pDeviceContext->RSSetState(prevRS);
	SAFE_RELEASE(prevRS);

	//================================================================================================
	//IA
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &m_VertextBufferStride, &m_VertextBufferOffset);
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	//VS
	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	//PS
	m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
	m_pDeviceContext->PSSetShaderResources(0, 1, &m_pTextureRV);
	m_pDeviceContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

	//================================================================================================

	if (m_FovDegree < 10.0f)      m_FovDegree = 10.0f;
	else if (m_FovDegree > 120.0f) m_FovDegree = 120.0f;

	// 최소값 보장
	if (m_Near < 0.0001f) m_Near = 0.0001f;

	// Near보다 약간 더 크게
	float minFar = m_Near + 0.001f;
	if (m_Far < minFar) m_Far = minFar;

	float aspect = m_ClientWidth / (float)m_ClientHeight;
	m_Projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FovDegree), aspect, m_Near, m_Far);
	//================================================================================================
	XMMATRIX R = XMMatrixRotationRollPitchYaw(m_LightPitch, m_LightYaw, 0.0f);
	XMVECTOR base = XMVector3Normalize(XMVectorSet(1.0f, 1.0f, -1.0f, 0.0f));
	XMVECTOR L = XMVector3Normalize(XMVector3TransformNormal(base, R));
	Vector3 dir = { XMVectorGetX(L), XMVectorGetY(L), XMVectorGetZ(L) };

	Matrix cam;
	ConstantBuffer cb = {}; // MVP 상수버퍼임

	m_Camera.GetViewMatrix(cam);
	cb.mView = XMMatrixTranspose(cam);
	cb.mProjection = XMMatrixTranspose(m_Projection);

	cb.vLightDir[0] = Vector4(dir.x, dir.y, dir.z, 0.0f);
	cb.vLightColor[0] = Vector4(m_LightColor.x * m_LightIntensity, m_LightColor.y * m_LightIntensity, m_LightColor.z * m_LightIntensity, 1.0f);
	cb.vLightDir[1] = Vector4(0, 0, 0, 0);
	cb.vLightColor[1] = Vector4(0, 0, 0, 0);

	{
		auto world = m_World;
		cb.mWorld = XMMatrixTranspose(world);

		//cb.mWorldInvTranspose = XMMatrixTranspose(XMMatrixTranspose(XMMatrixInverse(nullptr, world))); // 역행렬 + 전치		
		cb.mWorldInvTranspose = XMMatrixInverse(nullptr, world);

		m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &cb, 0, 0);
		m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);
	}

	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

#ifdef _DEBUG
	UpdateImGUI();
#endif

	m_pSwapChain->Present(0, 0);
}

//================================================================================================

bool TutorialApp::InitScene()
{
	HRESULT hr = 0;

	//스카이 박스 뚝딱뚝딱
	//================================================================================================

	HR_T(CreateDDSTextureFromFile(m_pDevice, L"../Resource/Hanako.dds", nullptr, &m_pSkySRV));

	D3D11_SAMPLER_DESC skySamp{}; // 샘플러
	skySamp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	skySamp.AddressU = skySamp.AddressV = skySamp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	HR_T(m_pDevice->CreateSamplerState(&skySamp, &m_pSkySampler));

	D3D11_DEPTH_STENCIL_DESC sd{};
	sd.DepthEnable = TRUE;
	sd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	sd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	HR_T(m_pDevice->CreateDepthStencilState(&sd, &m_pSkyDSS));

	// 여기가 그거임, 뒤집기
	D3D11_RASTERIZER_DESC rs{};
	rs.FillMode = D3D11_FILL_SOLID;
	rs.CullMode = D3D11_CULL_FRONT;
	rs.FrontCounterClockwise = FALSE;
	//FALSE → 시계방향(CW) 이 Front
	//TRUE → 반시계(CCW) 가 Front

	HR_T(m_pDevice->CreateRasterizerState(&rs, &m_pSkyRS));

	//================================================================================================

	//vs
	ID3D10Blob* vsb = nullptr;
	HR_T(CompileShaderFromFile(L"../Resource/Sky_VS.hlsl", "main", "vs_4_0", &vsb));
	HR_T(m_pDevice->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &m_pSkyVS));

	D3D11_INPUT_ELEMENT_DESC skyLayout[] = {
		{"POSITION", 0 , DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	HR_T(m_pDevice->CreateInputLayout(skyLayout, ARRAYSIZE(skyLayout), vsb->GetBufferPointer(), vsb->GetBufferSize(), &m_pSkyIL));
	SAFE_RELEASE(vsb);

	//ps
	ID3D10Blob* psb = nullptr;
	HR_T(CompileShaderFromFile(L"../Resource/Sky_PS.hlsl", "main", "ps_4_0", &psb));
	HR_T(m_pDevice->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &m_pSkyPS));
	SAFE_RELEASE(psb);

	//================================================================================================	

	Vertex vertices[] =
	{
		// Top (+Y)
		{ Vector3(-1.0f,  1.0f, -1.0f), Vector3(0, 1, 0), Vector2(1.0f, 0.0f) },
		{ Vector3(1.0f,  1.0f, -1.0f), Vector3(0, 1, 0), Vector2(0.0f, 0.0f) },
		{ Vector3(1.0f,  1.0f,  1.0f), Vector3(0, 1, 0), Vector2(0.0f, 1.0f) },
		{ Vector3(-1.0f,  1.0f,  1.0f), Vector3(0, 1, 0), Vector2(1.0f, 1.0f) },

		// Bottom (-Y)
		{ Vector3(-1.0f, -1.0f, -1.0f), Vector3(0,-1, 0), Vector2(0.0f, 0.0f) },
		{ Vector3(1.0f, -1.0f, -1.0f), Vector3(0,-1, 0), Vector2(1.0f, 0.0f) },
		{ Vector3(1.0f, -1.0f,  1.0f), Vector3(0,-1, 0), Vector2(1.0f, 1.0f) },
		{ Vector3(-1.0f, -1.0f,  1.0f), Vector3(0,-1, 0), Vector2(0.0f, 1.0f) },

		// Left (-X)
		{ Vector3(-1.0f, -1.0f,  1.0f), Vector3(-1,0,0), Vector2(0.0f, 1.0f) },
		{ Vector3(-1.0f, -1.0f, -1.0f), Vector3(-1,0,0), Vector2(1.0f, 1.0f) },
		{ Vector3(-1.0f,  1.0f, -1.0f), Vector3(-1,0,0), Vector2(1.0f, 0.0f) },
		{ Vector3(-1.0f,  1.0f,  1.0f), Vector3(-1,0,0), Vector2(0.0f, 0.0f) },

		// Right (+X)
		{ Vector3(1.0f, -1.0f,  1.0f), Vector3(1,0,0), Vector2(1.0f, 1.0f) },
		{ Vector3(1.0f, -1.0f, -1.0f), Vector3(1,0,0), Vector2(0.0f, 1.0f) },
		{ Vector3(1.0f,  1.0f, -1.0f), Vector3(1,0,0), Vector2(0.0f, 0.0f) },
		{ Vector3(1.0f,  1.0f,  1.0f), Vector3(1,0,0), Vector2(1.0f, 0.0f) },

		// Back (-Z)
		{ Vector3(-1.0f, -1.0f, -1.0f), Vector3(0,0,-1), Vector2(0.0f, 1.0f) },
		{ Vector3(1.0f, -1.0f, -1.0f), Vector3(0,0,-1), Vector2(1.0f, 1.0f) },
		{ Vector3(1.0f,  1.0f, -1.0f), Vector3(0,0,-1), Vector2(1.0f, 0.0f) },
		{ Vector3(-1.0f,  1.0f, -1.0f), Vector3(0,0,-1), Vector2(0.0f, 0.0f) },

		// Front (+Z)
		{ Vector3(-1.0f, -1.0f,  1.0f), Vector3(0,0, 1), Vector2(1.0f, 1.0f) },
		{ Vector3(1.0f, -1.0f,  1.0f), Vector3(0,0, 1), Vector2(0.0f, 1.0f) },
		{ Vector3(1.0f,  1.0f,  1.0f), Vector3(0,0, 1), Vector2(0.0f, 0.0f) },
		{ Vector3(-1.0f,  1.0f,  1.0f), Vector3(0,0, 1), Vector2(1.0f, 0.0f) },
	};

	for (auto& v : vertices) {
		v.normal.Normalize();
	}

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = sizeof(Vertex) * ARRAYSIZE(vertices);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.CPUAccessFlags = 0; // cpu 접근 금지

	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));

	m_VertextBufferStride = sizeof(Vertex);
	m_VertextBufferOffset = 0;

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"../Resource/BasicVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));

	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(),
		&m_pInputLayout));

	HR_T(m_pDevice->CreateVertexShader(
		vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(),
		NULL, &m_pVertexShader));

	SAFE_RELEASE(vertexShaderBuffer);


	WORD indices[] =
	{
		3,1,0, 2,1,3,
		6,4,5, 7,4,6,
		11,9,8, 10,9,11,
		14,12,13, 15,12,14,
		19,17,16, 18,17,19,
		22,20,21, 23,20,22
	};

	m_nIndices = ARRAYSIZE(indices);

	vbDesc = {};
	vbDesc.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
	vbDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.CPUAccessFlags = 0; // cpu접근 금지

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &ibData, &m_pIndexBuffer));

	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"../Resource/BasicPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);

	HR_T(CompileShaderFromFile(L"../Resource/SolidPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShaderSolid));
	SAFE_RELEASE(pixelShaderBuffer);

	//================================================================================================

	// 6. Render() 에서 파이프라인에 바인딩할 상수 버퍼 생성	
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	vbDesc.ByteWidth = sizeof(ConstantBuffer);
	vbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	vbDesc.CPUAccessFlags = 0;
	HR_T(m_pDevice->CreateBuffer(&vbDesc, nullptr, &m_pConstantBuffer));


	//================================================================================================

	// 텍스쳐
	HR_T(CreateDDSTextureFromFile(
		m_pDevice,
		L"../Resource/koyuki.dds",
		nullptr,
		&m_pTextureRV
	));

	D3D11_SAMPLER_DESC samp = {};
	samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samp.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samp.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samp.ComparisonFunc = D3D11_COMPARISON_NEVER;
	samp.MinLOD = 0;
	samp.MaxLOD = D3D11_FLOAT32_MAX;
	HR_T(m_pDevice->CreateSamplerState(&samp, &m_pSamplerLinear));

	//================================================================================================

	// 초기값설정 (MVP)
	m_World = XMMatrixIdentity();

	float aspect = m_ClientWidth / (FLOAT)m_ClientHeight;
	m_Projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FovDegree), aspect, m_Near, m_Far);

	return true;
}

void TutorialApp::UninitScene()
{
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	SAFE_RELEASE(m_pPixelShaderSolid);
	SAFE_RELEASE(m_pConstantBuffer);
	SAFE_RELEASE(m_pTextureRV);
	SAFE_RELEASE(m_pSamplerLinear);
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
		creationFlags,
		NULL, NULL,
		D3D11_SDK_VERSION,
		&swapDesc,
		&m_pSwapChain,
		&m_pDevice,
		NULL,
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

void TutorialApp::UninitD3D()
{
	SAFE_RELEASE(m_pDepthStencilState);
	SAFE_RELEASE(m_pDepthStencilView);
	SAFE_RELEASE(m_pDepthStencil);
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDevice);

	//푸짐한 스카이박스 한상
	SAFE_RELEASE(m_pSkySRV);
	SAFE_RELEASE(m_pSkySampler);

	SAFE_RELEASE(m_pSkyDSS);
	SAFE_RELEASE(m_pSkyRS);

	SAFE_RELEASE(m_pSkyVS);
	SAFE_RELEASE(m_pSkyPS);
	SAFE_RELEASE(m_pSkyIL);
}

//================================================================================================

bool TutorialApp::InitImGUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();

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

	ImGui::Begin("Majesty Han's IMGUI"); // 임꾸이

	ImGui::Text("[BackGround Color]");
	ImGui::ColorEdit3("RGB", color);

	ImGui::Separator();

	ImGui::Text("[Transform A]");
	ImGui::DragFloat3("-10.0f ~ 10.0f##1", (float*)&cubeTransformA, 0.5f, -10.0f, 10.0f);

	ImGui::Separator();

	ImGui::Text("[Directional Light]");
	ImGui::SliderAngle("[Yaw]", &m_LightYaw, -180.0f, 180.0f); // 좌우
	ImGui::SliderAngle("[Pitch]", &m_LightPitch, -89.0f, 89.0f); // 90 되면 곤란함 - 위아래
	ImGui::ColorEdit3("[Light Color]", (float*)&m_LightColor);
	ImGui::SliderFloat("[Intensity]", &m_LightIntensity, 0.0f, 5.0f);

	ImGui::Separator();

	ImGui::Text("[Scale]");
	ImGui::DragFloat3("-10.0f ~ 10.0f##4", (float*)&cubeScale, 0.5f, -10.0f, 10.0f);
	ImGui::Text("[SpinSpeed]");
	ImGui::SliderFloat("0.0f ~ 100.0f", &spinSpeed, 0.0f, 100.0f);
	ImGui::Text("[Camera Control]");
	ImGui::Text("[Mouse Right Button + WASD]");

	ImGui::Separator();

	ImGui::Text("[Fov]");
	ImGui::SliderFloat("10.0f ~ 120.0f", &m_FovDegree, 10.0f, 120.0f);
	ImGui::Text("[Near]");
	ImGui::DragFloat("default : 0.001f", &m_Near, 0.01f, 0.001f, 100.0f);
	ImGui::Text("[Far]");
	ImGui::DragFloat("default : 1.0f", &m_Far, 0.01f, 1.0f, 5000.0f);

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

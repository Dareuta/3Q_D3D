//================================================================================================
// TutorialApp.cpp
//================================================================================================

#include "TutorialApp.h"
#include "../D3DCore/Helper.h"
#include <d3dcompiler.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

// 정점 선언.
struct Vertex
{
	Vector3 position;
	Vector3 normal;
	Vertex() = default;
	Vertex(Vector3 p, Vector3 n) : position(p), normal(n) {}
};


struct ConstantBuffer // 상수버퍼
{
	Matrix mWorld;
	Matrix mView;
	Matrix mProjection;

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

	//광원
	XMMATRIX tmpB = mSpin * mTranslateB * tmpA;
	m_World_A = mScaleB * tmpB;

	//광원 2
	XMMATRIX tmpC = mSpin * mTranslateC * tmpB;
	m_World_B = mScaleC * tmpC;
}

//================================================================================================

void TutorialApp::OnRender()
{
	//OM
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);
	//Clear
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

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

	Matrix cam;
	m_Camera.GetViewMatrix(cam);

	ConstantBuffer cb{}; // MVP 상수버퍼임

	cb.mView = XMMatrixTranspose(cam);
	cb.mProjection = XMMatrixTranspose(m_Projection);

	cb.vLightDir[0] = Vector4(-0.3f, -1.0f, -0.2f, 0.0f); // 정규화 권장
	cb.vLightDir[1] = Vector4(0.8f, 0.4f, 0.1f, 0.0f);
	cb.vLightColor[0] = Vector4(1.0f, 1.0f, 1.0f, 1.0f);    // 흰색광
	cb.vLightColor[1] = Vector4(1.0f, 0.6f, 0.3f, 1.0f);    // 주황빛

	cb.vOutputColor = Vector4(0, 0, 0, 0);

	{
		cb.mWorld = XMMatrixTranspose(m_World);
		m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &cb, 0, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pConstantBuffer);
		m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);
	}

	{
		cb.mWorld = XMMatrixTranspose(m_World_A);
		m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &cb, 0, 0);
		m_pDeviceContext->PSSetShader(m_pPixelShaderSolid, nullptr, 0);
		m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);
	}

	{
		cb.mWorld = XMMatrixTranspose(m_World_B);
		m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &cb, 0, 0);
		m_pDeviceContext->PSSetShader(m_pPixelShaderSolid, nullptr, 0);
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

	const float hTop = 1.4f;   // 위 꼭짓점 높이
	const float hBot = 1.4f;   // 아래 꼭짓점 깊이
	const float r = 1.0f;   // 가운데 링 반경

	const float PHI = (1.0f + sqrtf(5.0f)) * 0.5f; // 황금비

	Vertex vertices[] = {
		{ { -1,  PHI,  0 },  Vector3(-1,  PHI,  0) },
		{ {  1,  PHI,  0 },  Vector3(1,  PHI,  0) },
		{ { -1, -PHI,  0 },  Vector3(-1, -PHI,  0) },
		{ {  1, -PHI,  0 },  Vector3(1, -PHI,  0) },
		{ {  0, -1,  PHI },  Vector3(0, -1,  PHI) },
		{ {  0,  1,  PHI },  Vector3(0,  1,  PHI) },
		{ {  0, -1, -PHI },  Vector3(0, -1, -PHI) },
		{ {  0,  1, -PHI },  Vector3(0,  1, -PHI) },
		{ {  PHI,  0, -1 },  Vector3(PHI,  0, -1) },
		{ {  PHI,  0,  1 },  Vector3(PHI,  0,  1) },
		{ { -PHI,  0, -1 },  Vector3(-PHI,  0, -1) },
		{ { -PHI,  0,  1 },  Vector3(-PHI,  0,  1) },
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
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"06_BasicVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));

	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(),
		&m_pInputLayout));

	HR_T(m_pDevice->CreateVertexShader(
		vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(),
		NULL, &m_pVertexShader));

	SAFE_RELEASE(vertexShaderBuffer);

	WORD indices[] = {
		0,11,5,   0,5,1,    0,1,7,    0,7,10,   0,10,11,
		1,5,9,    5,11,4,   11,10,2,  10,7,6,   7,1,8,
		3,9,4,    3,4,2,    3,2,6,    3,6,8,    3,8,9,
		4,9,5,    2,4,11,   6,2,10,   8,6,7,    9,8,1
	};

	m_nIndices = ARRAYSIZE(indices);

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.CPUAccessFlags = 0; // cpu접근 금지

	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));

	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"06_BasicPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);

	HR_T(CompileShaderFromFile(L"06_SolidPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShaderSolid));
	SAFE_RELEASE(pixelShaderBuffer);



	// 6. Render() 에서 파이프라인에 바인딩할 상수 버퍼 생성	
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(ConstantBuffer);
	ibDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ibDesc.CPUAccessFlags = 0;
	HR_T(m_pDevice->CreateBuffer(&ibDesc, nullptr, &m_pConstantBuffer));

	// 초기값설정 (MVP)
	m_World = XMMatrixIdentity();
	//XMVECTOR Eye = XMVectorSet(0.0f, 4.0f, -10.0f, 0.0f);
	//XMVECTOR At = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	//XMVECTOR Up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	//m_View = XMMatrixLookAtLH(Eye, At, Up); // 카메라로 대체함

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

	ImGui::Text("[Transform A]");
	ImGui::DragFloat3("-10.0f ~ 10.0f##1", (float*)&cubeTransformA, 0.5f, -10.0f, 10.0f);

	ImGui::Text("[Transform B]");
	ImGui::DragFloat3("-10.0f ~ 10.0f##2", (float*)&cubeTransformB, 0.5f, -10.0f, 10.0f);

	ImGui::Text("[Transform C]");
	ImGui::DragFloat3("-10.0f ~ 10.0f##3", (float*)&cubeTransformC, 0.5f, -10.0f, 10.0f);

	ImGui::Text("[Scale]");
	ImGui::DragFloat3("-10.0f ~ 10.0f##4", (float*)&cubeScale, 0.5f, -10.0f, 10.0f);
	ImGui::Text("[SpinSpeed]");
	ImGui::SliderFloat("0.0f ~ 100.0f", &spinSpeed, 0.0f, 100.0f);
	ImGui::Text("[Camera Control]");
	ImGui::Text("[Mouse Right Button + WASD]");

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

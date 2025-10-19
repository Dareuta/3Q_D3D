//================================================================================================
// TutorialApp.cpp
//================================================================================================

#include "TutorialApp.h"
#include "../D3DCore/Helper.h"
#include <d3dcompiler.h>
#include <Directxtk/DDSTextureLoader.h>
#include <DirectXTK/WICTextureLoader.h> 

#include "AssimpImporter.h"
#include "StaticMeshMinimal.h"

#pragma comment (lib, "d3d11.lib")
#pragma comment(lib,"d3dcompiler.lib")

std::unique_ptr<StaticMeshMinimal> gTestMesh;

// 정점 선언.
struct Vertex
{
	Vector3 position;
	Vector3 normal;
	Vector2 uv;
	Vector4 tan; // +U 방향
	//Vector3 bit;  // +V 방향
};

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
	// 바인딩 & 클리어
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, m_pDepthStencilView);
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color);
	m_pDeviceContext->ClearDepthStencilView(
		m_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 셰이더/IL/토폴로지
	m_pDeviceContext->IASetInputLayout(m_pMeshIL);
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pDeviceContext->VSSetShader(m_pMeshVS, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pMeshPS, nullptr, 0);

	// 상수버퍼 업데이트 (World/View/Proj 만 사용)
	Matrix view;
	m_Camera.GetViewMatrix(view);

	ConstantBuffer cb{};
	cb.mWorld = XMMatrixTranspose(m_World);
	cb.mView = XMMatrixTranspose(view);
	cb.mProjection = XMMatrixTranspose(m_Projection);
	cb.mWorldInvTranspose = XMMatrixIdentity(); // 안 씀
	cb.vLightDir = Vector4(0, 0, 0, 0);   // 안 씀
	cb.vLightColor = Vector4(0, 0, 0, 0);   // 안 씀

	m_pDeviceContext->UpdateSubresource(m_pConstantBuffer, 0, nullptr, &cb, 0, 0);
	m_pDeviceContext->VSSetConstantBuffers(0, 1, &m_pConstantBuffer);
	// (PS에서 상수 안 쓰면 PSSetConstantBuffers는 생략)

	// FBX(static mesh) 드로우
	if (gTestMesh) {
		gTestMesh->Draw(m_pDeviceContext);
	}

#ifdef _DEBUG
	UpdateImGUI();
#endif
	m_pSwapChain->Present(1, 0);
}


//================================================================================================
//////////////////////////////////////////////////////////////////////////////////////////////////
//================================================================================================

bool TutorialApp::InitScene()
{
	// 1) FBX 로드 → CPU 메쉬
	MeshData_PosIndex cpu;
	// (pos/idx만 빼오도록 만든 기존 함수 그대로 사용)
	AssimpImporter::LoadFBX_PosIndex(
		L"../Resource/Zelda/zeldaPosed001.fbx",
		cpu,
		/*calcTangent*/ true,
		/*flipUV*/     true);

	// 2) GPU 메쉬 생성 (VB/IB)
	gTestMesh = std::make_unique<StaticMeshMinimal>();
	gTestMesh->Build(m_pDevice, cpu);

	// 3) POSITION-only 셰이더/IL
	{
		ID3D10Blob* vsb = nullptr;
		HR_T(CompileShaderFromFile(L"Shader/PosOnly_VS.hlsl", "main", "vs_4_0", &vsb));
		HR_T(m_pDevice->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &m_pMeshVS));

		D3D11_INPUT_ELEMENT_DESC il[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		HR_T(m_pDevice->CreateInputLayout(il, _countof(il), vsb->GetBufferPointer(), vsb->GetBufferSize(), &m_pMeshIL));
		SAFE_RELEASE(vsb);
	}
	{
		ID3D10Blob* psb = nullptr;
		HR_T(CompileShaderFromFile(L"Shader/PosOnly_PS.hlsl", "main", "ps_4_0", &psb));
		HR_T(m_pDevice->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &m_pMeshPS));
		SAFE_RELEASE(psb);
	}

	// 4) 상수버퍼 (World/View/Proj만 사용)
	{
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(ConstantBuffer);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pConstantBuffer));
	}

	// 5) 기본 트랜스폼/프로젝션
	m_World = XMMatrixIdentity();
	float aspect = m_ClientWidth / (float)m_ClientHeight;
	m_Projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FovDegree), aspect, m_Near, m_Far);

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

	// (StaticMeshMinimal가 내부에서 Release 하는 구조면) 이 한 줄이면 GPU 버퍼 정리됨
	gTestMesh.reset();

	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pInputLayout);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pPixelShader);
	//SAFE_RELEASE(m_pPixelShaderSolid);	
	//SAFE_RELEASE(m_pTextureRV);
	SAFE_RELEASE(m_pSamplerLinear);
	SAFE_RELEASE(m_pBlinnCB);

	SAFE_RELEASE(m_pDiffuseSRV);
	SAFE_RELEASE(m_pNormalSRV);
	SAFE_RELEASE(m_pSpecularSRV);
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

		// 6) Textures preview (SRV가 있으면 썸네일)
		if (ImGui::CollapsingHeader("Textures"))
		{
			const ImVec2 thumb(96, 96);
			if (m_pDiffuseSRV) { ImGui::Image((ImTextureID)m_pDiffuseSRV, thumb);  ImGui::SameLine(); ImGui::Text("Albedo [t0]"); }
			if (m_pNormalSRV) { ImGui::Image((ImTextureID)m_pNormalSRV, thumb);  ImGui::SameLine(); ImGui::Text("Normal [t1]"); }
			if (m_pSpecularSRV) { ImGui::Image((ImTextureID)m_pSpecularSRV, thumb);  ImGui::SameLine(); ImGui::Text("Specular [t2]"); }
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

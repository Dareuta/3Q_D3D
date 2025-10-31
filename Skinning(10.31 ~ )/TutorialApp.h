//================================================================================================
// TutorialApp.h 
//================================================================================================
#pragma once
#include <windows.h>
#include "../D3DCore/GameApp.h"
#include <d3d11.h>

#include "StaticMesh.h"
#include "Material.h"

#include "RigidSkeletal.h"  
#include "SkinnedSkeletal.h"

#include <directxtk/SimpleMath.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>



class TutorialApp : public GameApp
{
public:
	using Vector2 = DirectX::SimpleMath::Vector2;
	using Vector3 = DirectX::SimpleMath::Vector3;
	using Vector4 = DirectX::SimpleMath::Vector4;
	using Matrix = DirectX::SimpleMath::Matrix;

	ID3D11Device* GetDevice()  const noexcept { return m_pDevice; }
	ID3D11DeviceContext* GetContext() const noexcept { return m_pDeviceContext; }
	const Matrix& GetProjection() const noexcept { return m_Projection; }

protected:
	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate() override;
	void OnRender() override;
	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) override;

private:
	// ===== 내부 초기화 유틸 =====
	bool InitD3D();
	void UninitD3D();
	bool InitImGUI();
	void UninitImGUI();
	void UpdateImGUI();
	bool InitScene();
	void UninitScene();

	// ===== D3D 핵심 객체 =====
	ID3D11Device* m_pDevice = nullptr;
	ID3D11DeviceContext* m_pDeviceContext = nullptr;
	IDXGISwapChain* m_pSwapChain = nullptr;
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;

	// Depth/Stencil
	ID3D11Texture2D* m_pDepthStencil = nullptr;
	ID3D11DepthStencilView* m_pDepthStencilView = nullptr;
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;

	// 공용 리소스
	ID3D11SamplerState* m_pSamplerLinear = nullptr;
	ID3D11Buffer* m_pConstantBuffer = nullptr;   // VS/PS 공용 CB
	ID3D11Buffer* m_pBlinnCB = nullptr;   // Blinn-Phong 전용 CB
	Matrix                  m_Projection;                    // 투영행렬
	DirectX::XMMATRIX       m_World;                         // 임시 월드(필요 시)

	// ===== 렌더 상태 =====
	ID3D11RasterizerState* m_pNoCullRS = nullptr;
	ID3D11BlendState* m_pBS_Alpha = nullptr; // Straight alpha
	ID3D11DepthStencilState* m_pDSS_Opaque = nullptr; // Depth write ON
	ID3D11DepthStencilState* m_pDSS_Trans = nullptr; // Depth write OFF

	// 디버그/테스트용 상태
	ID3D11RasterizerState* m_pWireRS = nullptr; // Wireframe + CullNone
	ID3D11RasterizerState* m_pCullBackRS = nullptr; // 기본 Back
	ID3D11DepthStencilState* m_pDSS_Disabled = nullptr; // DepthEnable = FALSE

	// ===== 스카이박스 =====
	ID3D11VertexShader* m_pSkyVS = nullptr;
	ID3D11PixelShader* m_pSkyPS = nullptr;
	ID3D11InputLayout* m_pSkyIL = nullptr;
	ID3D11Buffer* m_pSkyVB = nullptr;
	ID3D11Buffer* m_pSkyIB = nullptr;
	ID3D11ShaderResourceView* m_pSkySRV = nullptr; // Hanako.dds
	ID3D11SamplerState* m_pSkySampler = nullptr;
	ID3D11DepthStencilState* m_pSkyDSS = nullptr; // Depth write OFF, LEQUAL
	ID3D11RasterizerState* m_pSkyRS = nullptr; // Cull FRONT

	// ===== 메쉬 파이프라인 =====
	ID3D11VertexShader* m_pMeshVS = nullptr;
	ID3D11PixelShader* m_pMeshPS = nullptr;
	ID3D11InputLayout* m_pMeshIL = nullptr;
	ID3D11Buffer* m_pUseCB = nullptr;

	// FBX/머티리얼
	StaticMesh              gTree, gChar, gZelda;
	std::vector<MaterialGPU> gTreeMtls, gCharMtls, gZeldaMtls;

	StaticMesh gBoxHuman;
	std::vector<MaterialGPU> gBoxMtls;

	// 클래스 멤버(기존 렌더링/셰이더 멤버들 근처에 추가)
	ID3D11VertexShader* m_pSkinnedVS = nullptr;
	ID3D11InputLayout* m_pSkinnedIL = nullptr;
	ID3D11Buffer* m_pBoneCB = nullptr;   // VS b4: bone palette

	std::unique_ptr<SkinnedSkeletal> mSkinRig;   // SkinningTest.fbx

	// TutorialApp.h
	struct AnimCtrl {
		bool   play = true;
		bool   loop = true;
		float  speed = 1.0;
		double  t = 0.0;
	};

	AnimCtrl mBoxAC;   // BoxHuman용
	AnimCtrl mSkinAC;  // Skinned 모델용



	// ===== 디버그 화살표 =====
	ID3D11VertexShader* m_pDbgVS = nullptr;
	ID3D11PixelShader* m_pDbgPS = nullptr;
	ID3D11InputLayout* m_pDbgIL = nullptr;
	ID3D11Buffer* m_pArrowVB = nullptr;
	ID3D11Buffer* m_pArrowIB = nullptr;
	ID3D11RasterizerState* m_pDbgRS = nullptr;   // CullNone
	ID3D11Buffer* m_pDbgCB = nullptr;   // PS b3

	// ===== UI/디버그 데이터 =====
	struct XformUI {
		DirectX::SimpleMath::Vector3 pos{ 0,0,0 };
		DirectX::SimpleMath::Vector3 rotD{ 0,0,0 }; // degrees
		DirectX::SimpleMath::Vector3 scl{ 1,1,1 };
		DirectX::SimpleMath::Vector3 initPos{ 0,0,0 }, initRotD{ 0,0,0 }, initScl{ 1,1,1 };
		bool enabled = true;
	};

	struct DebugToggles {
		bool showSky = true;
		bool showOpaque = true;
		bool showTransparent = true;
		bool showLightArrow = true;

		bool wireframe = false;
		bool cullNone = true;
		bool depthWriteOff = false;
		bool freezeTime = false;

		bool disableNormal = false;
		bool disableSpecular = false;
		bool disableEmissive = false;
		bool forceAlphaClip = true;
		float alphaCut = 0.4f;
	};

	// 변환 유틸
	static Matrix ComposeSRT(const XformUI& xf) {
		using namespace DirectX;
		using namespace DirectX::SimpleMath;
		Matrix S = Matrix::CreateScale(xf.scl);
		Matrix R = Matrix::CreateFromYawPitchRoll(
			XMConvertToRadians(xf.rotD.y),
			XMConvertToRadians(xf.rotD.x),
			XMConvertToRadians(xf.rotD.z)
		);
		Matrix T = Matrix::CreateTranslation(xf.pos);
		return S * R * T;
	}

	// ===== 씬/조명/카메라 파라미터 =====
	float  color[4] = { 0.10f, 0.11f, 0.13f, 1.0f };
	float  spinSpeed = 0.0f;

	float  m_FovDegree = 60.0f;   // degrees
	float  m_Near = 0.1f;
	float  m_Far = 5000.0f;

	float  m_LightYaw = DirectX::XMConvertToRadians(45.0f);
	float  m_LightPitch = DirectX::XMConvertToRadians(-20.0f);
	Vector3 m_LightColor{ 1,1,1 };
	float   m_LightIntensity = 1.0f;

	Vector3 cubeScale{ 5.0f, 5.0f, 5.0f };
	Vector3 cubeTransformA{ 0.0f, 0.0f, -20.0f };
	Vector3 cubeTransformB{ 5.0f, 0.0f,  0.0f };
	Vector3 cubeTransformC{ 3.0f, 0.0f,  0.0f };

	Vector3 m_Ia{ 0.1f, 0.1f, 0.1f };
	Vector3 m_Ka{ 1.0f, 1.0f, 1.0f };
	float   m_Ks = 0.9f;
	float   m_Shininess = 64.0f;

	XformUI     mTreeX, mCharX, mZeldaX;
	XformUI     mBoxX;
	XformUI mSkinX;                               
	DebugToggles mDbg;

	// 디버그 화살표 위치/스케일
	Vector3 m_ArrowPos{ 0.0f, -100.0f, 100.0f };
	Vector3 m_ArrowScale{ 1.0f,  1.0f,   1.0f };

	std::unique_ptr<RigidSkeletal> mBoxRig; // BoxHuman의 Rigid 스켈레톤
	double mAnimT = 0.0;                    // 애니메이션 시간(초)
	double mAnimSpeed = 1.0;                // 재생 속도 배율
	bool  mBox_Play = true;   // 재생/일시정지
	bool  mBox_Loop = true;   // 루프
	float mBox_Speed = 1.0f;  // 재생 속도 배수(음수면 역재생)
};

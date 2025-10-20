//================================================================================================
// TutorialApp.h
//================================================================================================

#pragma once
#include <windows.h>
#include "../D3DCore/GameApp.h"
#include <d3d11.h> // ID3D11Device, …Context, …Buffer 등을 포함

#include "StaticMesh.h"
#include "Material.h"

#include <directxtk/SimpleMath.h> // 경량 수학 타입(Vector2/3/4, Matrix 등)

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

class TutorialApp : public GameApp
{
public:
	/*
	*	D3D 필수 4종.
	*	Device: 리소스 생성용 팩토리.
	*	Immediate Context: 드로우 호출/상태 세팅.
	*	SwapChain: 백버퍼 교환.
	*	RTV: 백버퍼를 렌더 타깃으로 쓰기 위한 뷰.
	*/
	//================================================================================================
	// 렌더링 파이프라인을 구성하는 필수 객체의 인터페이스 (  뎊스 스텐실 뷰도 있지만 아직 사용하지 않는다.)
	ID3D11Device* m_pDevice = nullptr;						// 디바이스
	ID3D11DeviceContext* m_pDeviceContext = nullptr;		// 즉시 디바이스 컨텍스트
	IDXGISwapChain* m_pSwapChain = nullptr;					// 스왑체인
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;	// 렌더링 타겟뷰		
	ID3D11Buffer* m_pConstantBuffer = nullptr;			// 상수 버퍼.	
	DirectX::SimpleMath::Matrix m_Projection;			// 단위장치좌표계( Normalized Device Coordinate) 공간으로 변환을 위한 행렬.
	ID3D11Texture2D* m_pDepthStencil = nullptr;
	ID3D11DepthStencilView* m_pDepthStencilView = nullptr;
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;
	ID3D11SamplerState* m_pSamplerLinear = nullptr;		
	ID3D11Buffer* m_pBlinnCB = nullptr;

	//================================================================================================
				
	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate() override;
	void OnRender() override;
	bool InitD3D();
	void UninitD3D();
	bool InitImGUI();
	void UninitImGUI();
	void UpdateImGUI();
	bool InitScene();		// 쉐이더,버텍스,인덱스
	void UninitScene();

	virtual LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	//================================================================================================

private:
	float color[3] = { 0.1f, 0.11f, 0.13f };
	float spinSpeed = 0.0f;

	float m_FovDegree = 60.0f;    // degrees
	float m_Near = 0.1f;
	float m_Far = 100.0f;

	float m_LightYaw = XMConvertToRadians(45.0f);         // -π ~ +π (Y축 기준으로 회전) << 좌우로 회전
	float m_LightPitch = XMConvertToRadians(-20.0f);       // -π/2 ~ +π/2 (하늘/땅 제한용) ( X축 기준으로 회전) << 위 아래로 끄덕
								       // Roll이라고 있는데 이건 까딱임
	Vector3 m_LightColor = { 1,1,1 };  // RGB (색깔) 
	float m_LightIntensity = 1.0f;     // 0~N (빛의 세기)


	Vector3 cubeScale = { 5.0f, 5.0f, 5.0f };
	Vector3 cubeTransformA = { 0.0f, 0.0f, -20.0f };
	Vector3 cubeTransformB = { 5.0f, 0.0f, 0.0f };
	Vector3 cubeTransformC = { 3.0f, 0.0f, 0.0f };

	Vector3 m_Ia = { 0.1f, 0.1f, 0.1f };
	Vector3 m_Ka = { 1.0f, 1.0f, 1.0f };
	float   m_Ks = 0.9f;
	float   m_Shininess = 64.0f;	

	ID3D11VertexShader* m_pMeshVS{}; 
	ID3D11PixelShader* m_pMeshPS{}; 
	ID3D11InputLayout* m_pMeshIL{};

	ID3D11Buffer* m_pUseCB = nullptr;
	StaticMesh gTree, gChar, gZelda;
	std::vector<MaterialGPU> gTreeMtls, gCharMtls, gZeldaMtls;
	ID3D11RasterizerState* m_pNoCullRS = nullptr;
};


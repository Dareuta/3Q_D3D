//================================================================================================
// TutorialApp.h
//================================================================================================

#pragma once
#include <windows.h>
#include "../D3DCore/GameApp.h"
#include "../D3DCore/DebugArrow.h"
#include <d3d11.h> // ID3D11Device, …Context, …Buffer 등을 포함

#include <directxtk/SimpleMath.h> // 경량 수학 타입(Vector2/3/4, Matrix 등)
using namespace DirectX::SimpleMath; // 남발하면 오염됨 아무튼 많이 쓰지 마셈

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
	//================================================================================================


	//================================================================================================	
	// 렌더링 파이프라인에 적용하는  객체와 정보
	ID3D11VertexShader* m_pVertexShader = nullptr;	// 정점 셰이더.
	ID3D11PixelShader* m_pPixelShader = nullptr;	// 픽셀 셰이더.	

	//ID3D11PixelShader* m_pPixelShaderSolid = nullptr;	// 픽셀 셰이더 라이트 표시용.	

	ID3D11InputLayout* m_pInputLayout = nullptr;	// 입력 레이아웃.
	ID3D11Buffer* m_pVertexBuffer = nullptr;		// 버텍스 버퍼.
	UINT m_VertextBufferStride = 0;					// 버텍스 하나의 크기.
	UINT m_VertextBufferOffset = 0;					// 버텍스 버퍼의 오프셋.
	ID3D11Buffer* m_pIndexBuffer = nullptr;			// 버텍스 버퍼.
	int m_nIndices = 0;								// 인덱스 개수.
	//================================================================================================
	// 추가한 뭐시기
	ID3D11Buffer* m_pConstantBuffer = nullptr;			// 상수 버퍼.
	XMMATRIX m_World;				// 월드좌표계 공간으로 변환을 위한 행렬.
	XMMATRIX m_World_A;				// 월드좌표계 공간으로 변환을 위한 행렬.
	XMMATRIX m_World_B;				// 월드좌표계 공간으로 변환을 위한 행렬.

	XMMATRIX m_View;				// 뷰좌표계 공간으로 변환을 위한 행렬.
	XMMATRIX m_Projection;			// 단위장치좌표계( Normalized Device Coordinate) 공간으로 변환을 위한 행렬.

	ID3D11Texture2D* m_pDepthStencil = nullptr;
	ID3D11DepthStencilView* m_pDepthStencilView = nullptr;
	ID3D11DepthStencilState* m_pDepthStencilState = nullptr;

	//================================================================================================
	// 추가한 뭐시기2
	ID3D11ShaderResourceView* m_pTextureRV = nullptr;	// 텍스처 리소스 뷰.	
	ID3D11SamplerState* m_pSamplerLinear = nullptr;

	//================================================================================================
	// 스카이박스에 사용하는거 따로 한세트 추가함

	ID3D11ShaderResourceView* m_pSkySRV = nullptr;  // 스카이박스
	ID3D11SamplerState* m_pSkySampler = nullptr;  // 스카이용

	ID3D11DepthStencilState* m_pSkyDSS = nullptr;  
	ID3D11RasterizerState* m_pSkyRS = nullptr; // 이거, 스카이 박스는 감기는 방향 반대로 해줘야해서 필요함 - 원래는 기본값 사용했던거임

	ID3D11VertexShader* m_pSkyVS = nullptr;
	ID3D11PixelShader* m_pSkyPS = nullptr;
	ID3D11InputLayout* m_pSkyIL = nullptr;

	//================================================================================================
	// 블링퐁퐁
	ID3D11Buffer* m_pBlinnCB = nullptr;


	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate() override;
	void OnRender() override;

	bool InitD3D();
	void UninitD3D();

	bool InitImGUI();
	void UninitImGUI();
	void UpdateImGUI();
	virtual LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	bool InitScene();		// 쉐이더,버텍스,인덱스
	void UninitScene();
	//================================================================================================

private:
	float color[3] = { 0.1f, 0.11f, 0.13f };
	float spinSpeed = 0.0f;

	float m_FovDegree = 60.0f;    // degrees
	float m_Near = 0.01f;
	float m_Far = 400.0f;

	float m_LightYaw = 0.0f;           // -π ~ +π (Y축 기준으로 회전) << 좌우로 회전
	float m_LightPitch = -0.5f;        // -π/2 ~ +π/2 (하늘/땅 제한용) ( X축 기준으로 회전) << 위 아래로 끄덕
								       // Roll이라고 있는데 이건 까딱임
	Vector3 m_LightColor = { 1,1,1 };  // RGB (색깔) 
	float m_LightIntensity = 1.0f;     // 0~N (빛의 세기)

	Vector3 cubeScale = { 0.7f, 0.7f, 0.7f };
	Vector3 cubeTransformA = { 0.0f, 0.0f, -20.0f };
	Vector3 cubeTransformB = { 5.0f, 0.0f, 0.0f };
	Vector3 cubeTransformC = { 3.0f, 0.0f, 0.0f };

	Vector3 m_Ka = { 0.1f, 0.1f, 0.1f };
	float   m_Ks = 0.5f;
	float   m_Shininess = 64.0f;
	Vector3 m_Ia = { 0.1f, 0.1f, 0.1f };

	DebugArrow gArrow;
};


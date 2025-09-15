#pragma once
#include <windows.h>
#include "../D3DCore/GameApp.h"
#include <d3d11.h> // ID3D11Device, …Context, …Buffer 등을 포함

#include <directxtk/SimpleMath.h> // 경량 수학 타입(Vector2/3/4, Matrix 등)
using namespace DirectX::SimpleMath; // 남발하면 오염됨 아무튼 많이 쓰지 마셈


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
	ID3D11InputLayout* m_pInputLayout = nullptr;	// 입력 레이아웃.
	ID3D11Buffer* m_pVertexBuffer = nullptr;		// 버텍스 버퍼.
	UINT m_VertextBufferStride = 0;					// 버텍스 하나의 크기.
	UINT m_VertextBufferOffset = 0;					// 버텍스 버퍼의 오프셋.
	ID3D11Buffer* m_pIndexBuffer = nullptr;			// 버텍스 버퍼.
	int m_nIndices = 0;								// 인덱스 개수.
	//================================================================================================


	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate() override;
	void OnRender() override;

	bool InitD3D();
	void UninitD3D();

	bool InitScene();		// 쉐이더,버텍스,인덱스
	void UninitScene();
};


#include "TutorialApp.h"
#include "../D3DCore/Helper.h" //HR_T, SAFE_RELEASE, CompileShaderFromFile 같은게 들어있다고 함
#include <d3dcompiler.h>

#pragma comment (lib, "d3d11.lib") // comment(lib, "...") << 링커에게 특정 라이브러리를 링크하라고 명령
#pragma comment(lib,"d3dcompiler.lib")

//================================================================================================
//////////////////////////////////////////////////////////////////////////////////////////////////
//================================================================================================

// 정점 선언.
struct Vertex
{
	Vector3 position;		// 정점 위치 정보.
	Vector4 color{ 1,1,1,1 };			// 정점 색상 정보.

	Vertex(float x, float y, float z) : position(x, y, z) {}
	Vertex(Vector3 position) : position(position) {}

	Vertex(Vector3 position, Vector4 color) : position(position), color(color) {}
};

//================================================================================================
//////////////////////////////////////////////////////////////////////////////////////////////////
//================================================================================================

bool TutorialApp::OnInitialize()
{
	if (!InitD3D())
		return false;

	if (!InitScene())
		return false;

	return true;
}

void TutorialApp::OnUninitialize()
{
	UninitScene();
	UninitD3D();
}

//================================================================================================
//////////////////////////////////////////////////////////////////////////////////////////////////
//================================================================================================

void TutorialApp::OnUpdate()
{
	// 나중에 여기다 뭔가 뚜시뚜시 하면 될듯
}

void TutorialApp::OnRender()
{
	float color[4] = { 0.0f, 0.5f, 0.5f, 1.0f }; // 임의의 색상

	//m_pDeviceContext : GPU 명령을 발행하는 인터페이스.
	
	//그릴대상 설정	
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL); // 랜더타겟 1개, DSV사용 안함
	// OM = Output_Merger(OM) 단계, 여기서 사용할 렌더타겟을 설정하고, DepthStencil View를 바인딩함
	// OM = 픽셀 셰이더가 출력한 색을 실제 버퍼(화면, 텍스처) 에 합치는 단계.

	/* OMSetRenderTargets의 원형
	*	void OMSetRenderTargets(
		UINT NumViews,                             // 렌더 타겟 뷰 개수
		ID3D11RenderTargetView *const *ppRTVs,     // 렌더 타겟 뷰 배열 << 이거, C언어 시절 관습이라 길이랑 배열을 따로받는 거임
		ID3D11DepthStencilView *pDSV               // 깊이-스텐실 뷰 (Z버퍼)
		);
	*/

	// 아 렌더타겟이라는건, 버퍼인데,
	// 여러개 만들어서, 각 버퍼에 필요한 정보들을 담아서 쓸 수 있음
	// 예를 들면, RTV[0] -> 색상, 1-> 법선, 2-> 월드좌표 이런식으로
	// 랜더링의 중간 과정을 기억해둘 필요가 있거나, 그럴때 만들어둬야함
	// 으음... 그러니까, 필요할때마다 계산하면 비효율적이니까
	// 최종 결과물을 만드는 중간 과정들을 버퍼에 담아서 보존하는거임
	// 그러면, 재계산 할필요 없이 다시 가져와서 쓸 수 있으니까
	// 깊이, 스텐실은 DSV라는 곳에 담는다고 함(몰루)
	// 일반적으로 최대 8개까지 가능함.
	// 여러 렌더 타겟 = 한 번의 드로우에서 필요한 여러 ‘중간 데이터’를 보존해서, 
	// 다음 패스(라이팅/후처리)가 바로 쓰도록 하려는 것.


	// 화면 칠하기.
	m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView, color); // 해당 색상으로 칠하는거임, 초기화느낌? Clear잖슴
	// ClearRenderTargetView의 원형
	//	void ID3D11DeviceContext::ClearRenderTargetView(
	//		ID3D11RenderTargetView * pRenderTargetView,
	//		const FLOAT ColorRGBA[4]
	//	);


	//================================================================================================
	// Draw계열 함수를 호출하기전에 렌더링 파이프라인에 필수 스테이지 설정을 해야한다.	
	m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // 정점을 이어서 그릴 방식 설정.
	
	// Triangle List가 제일 안전하다고 함(권장)

	/*	D3D11_PRIMITIVE_TOPOLOGY_POINTLIST → 각 정점을 점으로 그림
		D3D11_PRIMITIVE_TOPOLOGY_LINELIST → 정점 2개 = 한 선분
		D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP → 정점들을 차례대로 이어서 선분
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST → 정점 3개 = 독립 삼각형(지금 설정이 이거임)
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP → 첫 3개 = 삼각형, 이후 1개씩 추가할 때마다 새로운 삼각형*/

	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &m_VertextBufferStride, &m_VertextBufferOffset);
	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

	m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0);

	// Present the information rendered to the back buffer to the front buffer (the screen)
	m_pSwapChain->Present(0, 0);
}

//================================================================================================
//////////////////////////////////////////////////////////////////////////////////////////////////
//================================================================================================

bool TutorialApp::InitD3D()
{
	HRESULT hr = 0;

	// 스왑체인 속성 설정 구조체 생성.
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1;
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;	// 스왑체인 출력할 창 핸들 값.
	swapDesc.Windowed = true;		// 창 모드 여부 설정.
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	// 백버퍼(텍스처)의 가로/세로 크기 설정.
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;
	// 화면 주사율 설정.
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	// 샘플링 관련 설정.
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	// 1. 장치 생성.   2.스왑체인 생성. 3.장치 컨텍스트 생성.
	HR_T(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, creationFlags, NULL, NULL,
		D3D11_SDK_VERSION, &swapDesc, &m_pSwapChain, &m_pDevice, NULL, &m_pDeviceContext));

	// 4. 렌더타겟뷰 생성.  (백버퍼를 이용하는 렌더타겟뷰)	
	ID3D11Texture2D* pBackBufferTexture = nullptr;
	HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));
	HR_T(m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_pRenderTargetView));  // 텍스처는 내부 참조 증가
	SAFE_RELEASE(pBackBufferTexture);	//외부 참조 카운트를 감소시킨다.
	// 렌더 타겟을 최종 출력 파이프라인에 바인딩합니다.
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);

	// 뷰포트 설정.	
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (float)m_ClientWidth;
	viewport.Height = (float)m_ClientHeight;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// 뷰포트 설정.
	m_pDeviceContext->RSSetViewports(1, &viewport);
	return true;
}

void TutorialApp::UninitD3D()
{
	SAFE_RELEASE(m_pRenderTargetView);
	SAFE_RELEASE(m_pDeviceContext);
	SAFE_RELEASE(m_pSwapChain);
	SAFE_RELEASE(m_pDevice);
}
//================================================================================================
//////////////////////////////////////////////////////////////////////////////////////////////////
//================================================================================================

// 1. Render() 에서 파이프라인에 바인딩할 버텍스 버퍼및 버퍼 정보 준비
// 2. Render() 에서 파이프라인에 바인딩할 InputLayout 생성 	
// 3. Render() 에서 파이프라인에 바인딩할  버텍스 셰이더 생성
// 4. Render() 에서 파이프라인에 바인딩할 인덱스 버퍼 생성
// 5. Render() 에서 파이프라인에 바인딩할 픽셀 셰이더 생성
bool TutorialApp::InitScene()
{
	HRESULT hr = 0; // 결과값.
	// 1. Render() 에서 파이프라인에 바인딩할 버텍스 버퍼및 버퍼 정보 준비
	// Normalized Device Coordinate
	//   0-----1
	//   |    /|
	//   |  /  |                중앙이 (0,0)  왼쪽이 (-1,0) 오른쪽이 (1,0) , 위쪽이 (0,1) 아래쪽이 (0,-1)
	//   |/    |
	//	 2-----3
	Vertex vertices[] =
	{
		Vertex(Vector3(-0.5f,  0.5f, 0.5f), Vector4(1.0f, 0.0f, 0.0f, 1.0f)),  // 
		Vertex(Vector3(0.5f,  0.5f, 0.5f), Vector4(0.0f, 1.0f, 0.0f, 1.0f)),
		Vertex(Vector3(-0.5f, -0.5f, 0.5f), Vector4(0.0f, 0.0f, 1.0f, 1.0f)),
		Vertex(Vector3(0.5f, -0.5f, 0.5f), Vector4(0.0f, 0.0f, 1.0f, 1.0f))
	};

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = sizeof(Vertex) * ARRAYSIZE(vertices);
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA vbData = {};
	vbData.pSysMem = vertices;	// 배열 데이터 할당.
	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));
	m_VertextBufferStride = sizeof(Vertex);		// 버텍스 버퍼 정보
	m_VertextBufferOffset = 0;

	// 2. Render() 에서 파이프라인에 바인딩할 InputLayout 생성 	
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
	{   // SemanticName , SemanticIndex , Format , InputSlot , AlignedByteOffset , InputSlotClass , InstanceDataStepRate	
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	ID3D10Blob* vertexShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"BasicVertexShader.hlsl", "main", "vs_4_0", &vertexShaderBuffer));
	HR_T(m_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout),
		vertexShaderBuffer->GetBufferPointer(), vertexShaderBuffer->GetBufferSize(), &m_pInputLayout));

	// 3. Render() 에서 파이프라인에 바인딩할  버텍스 셰이더 생성
	HR_T(m_pDevice->CreateVertexShader(vertexShaderBuffer->GetBufferPointer(),
		vertexShaderBuffer->GetBufferSize(), NULL, &m_pVertexShader));
	SAFE_RELEASE(vertexShaderBuffer);	// 버퍼 해제.

	// 4. Render() 에서 파이프라인에 바인딩할 인덱스 버퍼 생성
	WORD indices[] =
	{
		0, 1, 2,
		2, 1, 3
	};
	m_nIndices = ARRAYSIZE(indices);	// 인덱스 개수 저장.
	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(WORD) * ARRAYSIZE(indices);
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA ibData = {};
	ibData.pSysMem = indices;
	HR_T(m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pIndexBuffer));

	// 5. Render() 에서 파이프라인에 바인딩할 픽셀 셰이더 생성
	ID3D10Blob* pixelShaderBuffer = nullptr;
	HR_T(CompileShaderFromFile(L"BasicPixelShader.hlsl", "main", "ps_4_0", &pixelShaderBuffer));
	HR_T(m_pDevice->CreatePixelShader(pixelShaderBuffer->GetBufferPointer(),
		pixelShaderBuffer->GetBufferSize(), NULL, &m_pPixelShader));
	SAFE_RELEASE(pixelShaderBuffer);	// 픽셀 셰이더 버퍼 더이상 필요없음.
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

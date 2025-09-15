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
		D3D11_PRIMITIVE_TOPOLOGY_LINELIST → 정점 2개 = 한 선분ㅁ
		D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP → 정점들을 차례대로 이어서 선분
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST → 정점 3개 = 독립 삼각형(지금 설정이 이거임)
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP → 첫 3개 = 삼각형, 이후 1개씩 추가할 때마다 새로운 삼각형*/

	m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &m_VertextBufferStride, &m_VertextBufferOffset);
	// 어떤 버퍼의 정점을 가져와서 사용할지 설정하는것
	// GPT : 애니메이션이나 스트리밍 최적화 때문에 Position만 업데이트하고, 나머지는 그대로 쓰는 식.
	// 으로도 사용한다고 함, 잘 몰루겠음, 아무튼 0, 1이 거의 대부분임

	// 보통 귀찮으니 1개만 사용하지만, 경우에 따라(최적화를 위해) 여러 버퍼를 가져와서 사용하기도 함

	//IASetVertexBuffers의 원형
	//	void IASetVertexBuffers(
	//		UINT StartSlot,             // 몇 번째 슬롯부터 바인딩할지
	//		UINT NumBuffers,            // 몇 개의 버퍼를 바인딩할지
	//		ID3D11Buffer* const* ppVBs, // 버퍼 배열 (정점 버퍼)
	//		const UINT* pStrides,       // 각 버퍼에서 정점 하나의 크기(byte)
	//		const UINT* pOffsets        // 각 버퍼에서 시작 위치(byte)
	//	);

	m_pDeviceContext->IASetInputLayout(m_pInputLayout);
	// InputLayout은, 문서화랑 같은 역할임, 바이트 단위의 정보를 어떻게 해석할지 제시함
	// 정점 버퍼 데이터 → 셰이더 입력 시멘틱 매핑을 지정하는 단계.

	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	// 인덱스 버퍼
	// 정점 버퍼의 중복을 줄여주는 역할임
	// 즉, 정점 버퍼에 담긴 데이터의 조합을 적어둔거임
	// 포맷은 보통 R16_UINT (6만5천 정점 이하 모델)이나 R32_UINT (대형 메시용)를 쓴다.

	//IASetIndexBuffer의 원형
	//	void IASetIndexBuffer(
	//		ID3D11Buffer * pIndexBuffer, // 인덱스 버퍼
	//		DXGI_FORMAT Format,         // 인덱스 자료형 (16비트 or 32비트)
	//		UINT Offset                 // 버퍼 시작 오프셋 (보통 0)
	//	);

	m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
	// ID3D11Device::CreateVertexShader()로 만들어둔 버텍스 셰이더 객체를 넣음
	// nullptr + 0 >> HLSL Class linkage라는 기능인데, 보통 안 쓰니까 비워둔다.

	//VSSetShader의 원형
	//	void VSSetShader(
	//		ID3D11VertexShader * pVertexShader, // 바인딩할 VS 객체
	//		ID3D11ClassInstance* const* ppClassInstances, // 셰이더 클래스 인스턴스(거의 안씀)
	//		UINT NumClassInstances             // 위 인스턴스 개수
	//	);


	m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
	// 픽셀 쉐이더
	//	m_pPixelShader → ID3D11Device::CreatePixelShader() 로 만들어둔 픽셀 셰이더 객체
	//	nullptr, 0 → 셰이더 클래스 인스턴스 기능은 안 쓰니까 비움

	//PSSetShader의 원형
	//	void PSSetShader(
	//		ID3D11PixelShader* pPixelShader,        // 바인딩할 픽셀 셰이더 객체
	//		ID3D11ClassInstance* const* ppClassInstances, // 셰이더 클래스 인스턴스 (거의 안씀)
	//		UINT NumClassInstances                  // 인스턴스 개수
	//	);

	m_pDeviceContext->DrawIndexed(m_nIndices, 0, 0); // 그리기 명령

	//DrawIndexed의 원형
	//	void DrawIndexed(
	//		UINT IndexCount,			// 그릴 인덱스 개수
	//		UINT StartIndexLocation,	// 인덱스 버퍼에서 시작할 위치
	//		INT BaseVertexLocation		// 정점 버퍼에서 시작할 위치(오프셋)
	//	);


	// Present the information rendered to the back buffer to the front buffer (the screen)
	m_pSwapChain->Present(0, 0);
	// 버퍼 뒤집기, 앞에 숫자가 교체 주기 프레임 수 0(바로교체) 1(모니터 리프레시 주기에 맞춰서 교체) 2(2프레이마다)

}

//================================================================================================
//////////////////////////////////////////////////////////////////////////////////////////////////
//================================================================================================

bool TutorialApp::InitD3D()
{
	HRESULT hr = 0;

	// 스왑체인 속성 설정 구조체 생성.
	DXGI_SWAP_CHAIN_DESC swapDesc = {};
	swapDesc.BufferCount = 1; // 백버퍼의 갯수, 2개이상을 추천한다고 함
	swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapDesc.OutputWindow = m_hWnd;	// 스왑체인 출력할 창 핸들 값.
	swapDesc.Windowed = true;		// 창 모드 여부 설정.
	swapDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Red, Green, Blue, Alpha 각각 8비트 = 총 32비트(4바이트)
	// 백버퍼(텍스처)의 가로/세로 크기 설정.
	swapDesc.BufferDesc.Width = m_ClientWidth;
	swapDesc.BufferDesc.Height = m_ClientHeight;
	// 화면 주사율 설정.
	swapDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapDesc.BufferDesc.RefreshRate.Denominator = 1;
	// 샘플링 관련 설정.(MSAA?)
	swapDesc.SampleDesc.Count = 1;
	swapDesc.SampleDesc.Quality = 0;

	UINT creationFlags = 0;
#ifdef _DEBUG
	creationFlags |= D3D11_CREATE_DEVICE_DEBUG; // 디버그 모드 설정
#endif
	// 1. 장치 생성.   2.스왑체인 생성. 3.장치 컨텍스트 생성.
	HR_T(D3D11CreateDeviceAndSwapChain(
		NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, // 기본 GPU 사용한다는 옵션
		creationFlags, // 설정한 옵션(디버그 레이어 켜기)
		NULL, NULL, // 널값으로 넘겨주면, 드라이버가 알아서 "가능한 최상 레벨(D3D_FEATURE_LEVEL_11_0 등)"을 선택한다.
		D3D11_SDK_VERSION, 
		&swapDesc, // 스왑체인 설정
		&m_pSwapChain, // 결과로 얻는 스왑체인 객체->(out)
		&m_pDevice, // 리소스 생성, 셰이더 컴파일, 뷰 생성할 객치(디바이스 객체)->(out)
		NULL, // 실제 선택된 레벨은 확인 안함
		&m_pDeviceContext)); // 결과로 얻는 디바이스 컨텍스트(렌더링 명령, 상태 세팅 등)->(out)

	// 결과적으로 m_pSwapChain, m_pDevice, m_pDeviceContext를 얻어내는 거임
	// 체인(화면버퍼 관리), 디바이스(리소스 만들기), 컨텍스트(렌더링)

	/*
	HRESULT D3D11CreateDeviceAndSwapChain(
		IDXGIAdapter*               pAdapter,        // 어떤 GPU를 쓸지
		D3D_DRIVER_TYPE             DriverType,      // 하드웨어 / WARP / 레퍼런스 등
		HMODULE                     Software,        // 소프트웨어 드라이버 모듈 (거의 NULL)
		UINT                        Flags,           // creationFlags
		const D3D_FEATURE_LEVEL*    pFeatureLevels,  // 지원할 피처레벨 배열
		UINT                        FeatureLevels,   // 배열 크기
		UINT                        SDKVersion,      // D3D11_SDK_VERSION 매크로 고정
		const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,  // 스왑체인 설정 구조체
		IDXGISwapChain**            ppSwapChain,     // 스왑체인 결과
		ID3D11Device**              ppDevice,        // 디바이스 결과
		D3D_FEATURE_LEVEL*          pFeatureLevel,   // 실제 선택된 피처레벨 반환
		ID3D11DeviceContext**       ppImmediateContext // 디바이스 컨텍스트 결과
		);
	*/


	// 4. 렌더타겟뷰 생성.  (백버퍼를 이용하는 렌더타겟뷰)	
	ID3D11Texture2D* pBackBufferTexture = nullptr;

	// 체인버퍼는 2D임(화면에 나오는거니까)

	HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBufferTexture));

	// 백버퍼를 하나 꺼내서 가져오는거임, 꺼내서 기록하고 다시 집어넣는 듯?

	//GetBuffer의 원형
	//	HRESULT IDXGISwapChain::GetBuffer(
	//		UINT Buffer,        // 버퍼 인덱스 (보통 0 = 첫 번째 백버퍼)
	//		REFIID riid,        // 요청할 인터페이스 타입 (ex: __uuidof(ID3D11Texture2D))
	//		void** ppSurface    // 그 인터페이스 포인터가 들어갈 주소
	//	);

	HR_T(m_pDevice->CreateRenderTargetView(pBackBufferTexture, NULL, &m_pRenderTargetView));  // 텍스처는 내부 참조 증가

	// GetBuffer로 얻어온 벡버퍼 텍스처를 그냥 사용할 수 없기 때문에, 한번 담궜다 빼는거임
	// NULL << 텍스처 전체를 렌더 타겟으로 설정하는거임
	// 렌더 타겟 뷰 객치(RTV 객체)를 만들면, 원본 리소스를 참고하게 되는데,
	// 이때문에 원본을 release해도 RTV가 소유권을 주장해서 백버퍼가 살아있게됨
	// 이걸 해결하기위해, 원본이 된 리소스를 릴리즈해버림 - 소유권을 넘기는 행위

	//CreateRenderTargetView의 원형
	//	HRESULT ID3D11Device::CreateRenderTargetView(
	//		ID3D11Resource * pResource,                   // RTV를 만들 리소스(보통 텍스처)
	//		const D3D11_RENDER_TARGET_VIEW_DESC * pDesc,  // RTV 속성(포맷/슬라이스 지정). NULL이면 기본
	//		ID3D11RenderTargetView * *ppRTView            // 생성된 RTV 반환
	//	);

	SAFE_RELEASE(pBackBufferTexture);	//외부 참조 카운트를 감소시킨다.
	
	// 렌더 타겟을 최종 출력 파이프라인에 바인딩합니다.
	m_pDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, NULL);
	// 이거, OnRender 가장 위에도 있는거임 윗줄 참고

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
	
	// 카메라 좌표계를 화면과 대응시켜주는 역할임

	//RSSetViewports의 원형
	//	void RSSetViewports(
	//		UINT NumViewports,           // 뷰포트 개수 (최대 D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)
	//		const D3D11_VIEWPORT* pViewports // 뷰포트 배열
	//	);


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

	D3D11_BUFFER_DESC vbDesc = {}; // 버퍼 만들기	
	vbDesc.ByteWidth = sizeof(Vertex) * ARRAYSIZE(vertices);
	// 버텍스 하나의 크기 * 버텍스 총 숫자 = 총 크기
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // 이게 뭐하는 버퍼임?에 대한 기록 << 버텍스 버퍼임
	vbDesc.Usage = D3D11_USAGE_DEFAULT; // 이 버퍼를 CPU/GPU가 어떤 방식으로 쓸 거냐 << 초기 설정인듯(GPU전용으로 설정)

	D3D11_SUBRESOURCE_DATA vbData = {}; // 전부 0으로 초기화 하고
	vbData.pSysMem = vertices;	// 배열 데이터 할당. 
	//즉, 초기화 어떻게 해줄지 정하는거임, 지금은 만들어둔 버텍스(하드코딩) 박아넣음

	HR_T(m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pVertexBuffer));
	//	CreateBuffer의 원형
	//		HRESULT ID3D11Device::CreateBuffer(
	//			const D3D11_BUFFER_DESC *pDesc,            // 버퍼 설명
	//			const D3D11_SUBRESOURCE_DATA *pInitialData,// 초기 데이터 (옵션)
	//			ID3D11Buffer **ppBuffer                    // 결과 버퍼 (출력)
	//		);

	m_VertextBufferStride = sizeof(Vertex);		// 버텍스 버퍼 정보
	m_VertextBufferOffset = 0;

	// 2. Render() 에서 파이프라인에 바인딩할 InputLayout 생성 	
	D3D11_INPUT_ELEMENT_DESC layout[] = // 입력 레이아웃.
	{   // SemanticName , SemanticIndex , Format , InputSlot , AlignedByteOffset , InputSlotClass , InstanceDataStepRate	
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	}; // D3D11_APPEND_ALIGNED_ELEMENT : 앞 필드가 끝난 다음에 자동으로 정렬(alignment)에 맞춰 이어진다는 뜻.

	// 정점 하나에 담기는 데이터를 정의하는거임
	// 현재 위치와 색만 넣고있음
	// 나중에 법선같은거 들어감, 지금은 필요없어서 안넣은듯?

	//D3D11_INPUT_ELEMENT_DESC 구저체의 원형
	//typedef struct D3D11_INPUT_ELEMENT_DESC {
	//	LPCSTR                     SemanticName;      // 의미 태그 문자열 (예: "POSITION", "COLOR", "TEXCOORD")
	//	UINT                       SemanticIndex;     // 같은 의미 태그가 여러 개일 때 번호 (예: TEXCOORD0, TEXCOORD1)
	//	DXGI_FORMAT                Format;            // 데이터 형식 (예: DXGI_FORMAT_R32G32B32_FLOAT)
	//	UINT                       InputSlot;         // 이 데이터가 들어 있는 정점 버퍼 슬롯 번호
	//	UINT                       AlignedByteOffset; // 정점 구조체 안에서 시작 위치 (바이트 단위)
	//	D3D11_INPUT_CLASSIFICATION InputSlotClass;    // 정점당 데이터인지, 인스턴스당 데이터인지
	//	UINT                       InstanceDataStepRate; // 인스턴스당 데이터일 경우, 몇 개 인스턴스마다 갱신할지
	//} D3D11_INPUT_ELEMENT_DESC;

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

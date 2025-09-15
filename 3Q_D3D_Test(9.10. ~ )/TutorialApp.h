#pragma once
#include <windows.h>
#include "../D3DCore/GameApp.h"
#include <d3d11.h> // ID3D11Device, ��Context, ��Buffer ���� ����

#include <directxtk/SimpleMath.h> // �淮 ���� Ÿ��(Vector2/3/4, Matrix ��)
using namespace DirectX::SimpleMath; // �����ϸ� ������ �ƹ�ư ���� ���� ����


class TutorialApp : public GameApp
{
public:
	/*
	*	D3D �ʼ� 4��.
	*	Device: ���ҽ� ������ ���丮.
	*	Immediate Context: ��ο� ȣ��/���� ����.
	*	SwapChain: ����� ��ȯ.
	*	RTV: ����۸� ���� Ÿ������ ���� ���� ��.
	*/
	//================================================================================================
	// ������ ������������ �����ϴ� �ʼ� ��ü�� �������̽� (  �X�� ���ٽ� �䵵 ������ ���� ������� �ʴ´�.)
	ID3D11Device* m_pDevice = nullptr;						// ����̽�
	ID3D11DeviceContext* m_pDeviceContext = nullptr;		// ��� ����̽� ���ؽ�Ʈ
	IDXGISwapChain* m_pSwapChain = nullptr;					// ����ü��
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;	// ������ Ÿ�ٺ�
	//================================================================================================
	

	//================================================================================================	
	// ������ ���������ο� �����ϴ�  ��ü�� ����
	ID3D11VertexShader* m_pVertexShader = nullptr;	// ���� ���̴�.
	ID3D11PixelShader* m_pPixelShader = nullptr;	// �ȼ� ���̴�.	
	ID3D11InputLayout* m_pInputLayout = nullptr;	// �Է� ���̾ƿ�.
	ID3D11Buffer* m_pVertexBuffer = nullptr;		// ���ؽ� ����.
	UINT m_VertextBufferStride = 0;					// ���ؽ� �ϳ��� ũ��.
	UINT m_VertextBufferOffset = 0;					// ���ؽ� ������ ������.
	ID3D11Buffer* m_pIndexBuffer = nullptr;			// ���ؽ� ����.
	int m_nIndices = 0;								// �ε��� ����.
	//================================================================================================


	bool OnInitialize() override;
	void OnUninitialize() override;
	void OnUpdate() override;
	void OnRender() override;

	bool InitD3D();
	void UninitD3D();

	bool InitScene();		// ���̴�,���ؽ�,�ε���
	void UninitScene();
};


#include "RenderAPI.h"
#include "PlatformBase.h"

// Direct3D 11 implementation of RenderAPI.

#if SUPPORT_D3D11

#include <assert.h>
#include <d3d11.h>
#include <d3d11_1.h>
//#include <dxgi1_2.h>
#include "Unity/IUnityGraphicsD3D11.h"
#include <gst/gst.h>
#include <wrl.h>
using namespace Microsoft::WRL;

class RenderAPI_D3D11 : public RenderAPI
{
public:
	RenderAPI_D3D11();
	virtual ~RenderAPI_D3D11() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

	virtual bool GetUsesReverseZ() { return true; /*return (int)m_Device->GetFeatureLevel() >= (int)D3D_FEATURE_LEVEL_10_0;*/ }

	virtual void DrawSimpleTriangles(const float worldMatrix[16], int triangleCount, const void* verticesFloat3Byte4);

	virtual void* BeginModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int* outRowPitch);
	virtual void EndModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr);

	virtual void* BeginModifyVertexBuffer(void* bufferHandle, size_t* outBufferSize);
	virtual void EndModifyVertexBuffer(void* bufferHandle);

	virtual void Render(void* texture[2]);

private:
	void CreateResources();
	void ReleaseResources();

private:
	/*ID3D11Device* m_Device;
	ID3D11Buffer* m_VB; // vertex buffer
	ID3D11Buffer* m_CB; // constant buffer
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;
	ID3D11InputLayout* m_InputLayout;
	ID3D11RasterizerState* m_RasterState;
	ID3D11BlendState* m_BlendState;
	ID3D11DepthStencilState* m_DepthState;*/

	ComPtr<IDXGIFactory2> factory_;
	ComPtr<IDXGISwapChain> swapchain_;
	ComPtr<ID3D11Device> device_;
	//ComPtr<ID3D11DeviceContext> context_;
	D3D11_TEXTURE2D_DESC backbuf_desc_ = { };
	ComPtr<ID3D11Texture2D> backbuf_;
	ComPtr<ID3D11RasterizerState> rs_;
	ComPtr<ID3D11RenderTargetView> rtv_;
	ComPtr<ID3D11PixelShader> ps_;
	ComPtr<ID3D11VertexShader> vs_;
	ComPtr<ID3D11InputLayout> layout_;
	ComPtr<ID3D11SamplerState> sampler_;
	ComPtr<ID3D11Buffer> vertex_buf_;
	ComPtr<ID3D11Buffer> index_buf_;
	D3D11_VIEWPORT viewport_[2] = { };
};


RenderAPI* CreateRenderAPI_D3D11()
{
	return new RenderAPI_D3D11();
}


// Simple compiled shader bytecode.
//
// Shader source that was used:
/*#if 0
cbuffer MyCB : register(b0)
{
	float4x4 worldMatrix;
}
void VS(float3 pos : POSITION, float4 Level : Level, out float4 oLevel : Level, out float4 opos : SV_Position)
{
	opos = mul(worldMatrix, float4(pos, 1));
	oLevel = Level;
}
float4 PS(float4 Level : Level) : SV_TARGET
{
	return Level;
}
#endif // #if 0
*/
//
// Which then was compiled with:
// fxc /Tvs_4_0_level_9_3 /EVS source.hlsl /Fh outVS.h /Qstrip_reflect /Qstrip_debug /Qstrip_priv
// fxc /Tps_4_0_level_9_3 /EPS source.hlsl /Fh outPS.h /Qstrip_reflect /Qstrip_debug /Qstrip_priv
// and results pasted & formatted to take less lines here
/*
const BYTE kVertexShaderCode[] =
{
	68,88,66,67,86,189,21,50,166,106,171,1,10,62,115,48,224,137,163,129,1,0,0,0,168,2,0,0,4,0,0,0,48,0,0,0,0,1,0,0,4,2,0,0,84,2,0,0,
	65,111,110,57,200,0,0,0,200,0,0,0,0,2,254,255,148,0,0,0,52,0,0,0,1,0,36,0,0,0,48,0,0,0,48,0,0,0,36,0,1,0,48,0,0,0,0,0,
	4,0,1,0,0,0,0,0,0,0,0,0,1,2,254,255,31,0,0,2,5,0,0,128,0,0,15,144,31,0,0,2,5,0,1,128,1,0,15,144,5,0,0,3,0,0,15,128,
	0,0,85,144,2,0,228,160,4,0,0,4,0,0,15,128,1,0,228,160,0,0,0,144,0,0,228,128,4,0,0,4,0,0,15,128,3,0,228,160,0,0,170,144,0,0,228,128,
	2,0,0,3,0,0,15,128,0,0,228,128,4,0,228,160,4,0,0,4,0,0,3,192,0,0,255,128,0,0,228,160,0,0,228,128,1,0,0,2,0,0,12,192,0,0,228,128,
	1,0,0,2,0,0,15,224,1,0,228,144,255,255,0,0,83,72,68,82,252,0,0,0,64,0,1,0,63,0,0,0,89,0,0,4,70,142,32,0,0,0,0,0,4,0,0,0,
	95,0,0,3,114,16,16,0,0,0,0,0,95,0,0,3,242,16,16,0,1,0,0,0,101,0,0,3,242,32,16,0,0,0,0,0,103,0,0,4,242,32,16,0,1,0,0,0,
	1,0,0,0,104,0,0,2,1,0,0,0,54,0,0,5,242,32,16,0,0,0,0,0,70,30,16,0,1,0,0,0,56,0,0,8,242,0,16,0,0,0,0,0,86,21,16,0,
	0,0,0,0,70,142,32,0,0,0,0,0,1,0,0,0,50,0,0,10,242,0,16,0,0,0,0,0,70,142,32,0,0,0,0,0,0,0,0,0,6,16,16,0,0,0,0,0,
	70,14,16,0,0,0,0,0,50,0,0,10,242,0,16,0,0,0,0,0,70,142,32,0,0,0,0,0,2,0,0,0,166,26,16,0,0,0,0,0,70,14,16,0,0,0,0,0,
	0,0,0,8,242,32,16,0,1,0,0,0,70,14,16,0,0,0,0,0,70,142,32,0,0,0,0,0,3,0,0,0,62,0,0,1,73,83,71,78,72,0,0,0,2,0,0,0,
	8,0,0,0,56,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,7,7,0,0,65,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,1,0,0,0,
	15,15,0,0,80,79,83,73,84,73,79,78,0,67,79,76,79,82,0,171,79,83,71,78,76,0,0,0,2,0,0,0,8,0,0,0,56,0,0,0,0,0,0,0,0,0,0,0,
	3,0,0,0,0,0,0,0,15,0,0,0,62,0,0,0,0,0,0,0,1,0,0,0,3,0,0,0,1,0,0,0,15,0,0,0,67,79,76,79,82,0,83,86,95,80,111,115,
	105,116,105,111,110,0,171,171
};
const BYTE kPixelShaderCode[]=
{
	68,88,66,67,196,65,213,199,14,78,29,150,87,236,231,156,203,125,244,112,1,0,0,0,32,1,0,0,4,0,0,0,48,0,0,0,124,0,0,0,188,0,0,0,236,0,0,0,
	65,111,110,57,68,0,0,0,68,0,0,0,0,2,255,255,32,0,0,0,36,0,0,0,0,0,36,0,0,0,36,0,0,0,36,0,0,0,36,0,0,0,36,0,1,2,255,255,
	31,0,0,2,0,0,0,128,0,0,15,176,1,0,0,2,0,8,15,128,0,0,228,176,255,255,0,0,83,72,68,82,56,0,0,0,64,0,0,0,14,0,0,0,98,16,0,3,
	242,16,16,0,0,0,0,0,101,0,0,3,242,32,16,0,0,0,0,0,54,0,0,5,242,32,16,0,0,0,0,0,70,30,16,0,0,0,0,0,62,0,0,1,73,83,71,78,
	40,0,0,0,1,0,0,0,8,0,0,0,32,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,15,15,0,0,67,79,76,79,82,0,171,171,79,83,71,78,
	44,0,0,0,1,0,0,0,8,0,0,0,32,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,15,0,0,0,83,86,95,84,65,82,71,69,84,0,171,171
};
*/

#if 0
Texture2D shaderTexture;
SamplerState samplerState;

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 Texture : TEXCOORD;
};

float4 PSMain_sample(PS_INPUT input) : SV_TARGET
{
  return shaderTexture.Sample(samplerState, input.Texture);
}
#endif
const BYTE g_PSMain_sample[] =
{
	 68,  88,  66,  67,  42, 171,
	 68, 189,  81, 136,  62, 236,
	196,  37,  91, 100, 172, 130,
	148, 251,   1,   0,   0,   0,
	 80,   2,   0,   0,   5,   0,
	  0,   0,  52,   0,   0,   0,
	220,   0,   0,   0,  52,   1,
	  0,   0, 104,   1,   0,   0,
	212,   1,   0,   0,  82,  68,
	 69,  70, 160,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   2,   0,   0,   0,
	 28,   0,   0,   0,   0,   4,
	255, 255,   0,   1,   0,   0,
	119,   0,   0,   0,  92,   0,
	  0,   0,   3,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   1,   0,
	  0,   0,   0,   0,   0,   0,
	105,   0,   0,   0,   2,   0,
	  0,   0,   5,   0,   0,   0,
	  4,   0,   0,   0, 255, 255,
	255, 255,   0,   0,   0,   0,
	  1,   0,   0,   0,  12,   0,
	  0,   0, 115,  97, 109, 112,
	108, 101, 114,  83, 116,  97,
	116, 101,   0, 115, 104,  97,
	100, 101, 114,  84, 101, 120,
	116, 117, 114, 101,   0,  77,
	105,  99, 114, 111, 115, 111,
	102, 116,  32,  40,  82,  41,
	 32,  72,  76,  83,  76,  32,
	 83, 104,  97, 100, 101, 114,
	 32,  67, 111, 109, 112, 105,
	108, 101, 114,  32,  49,  48,
	 46,  49,   0, 171,  73,  83,
	 71,  78,  80,   0,   0,   0,
	  2,   0,   0,   0,   8,   0,
	  0,   0,  56,   0,   0,   0,
	  0,   0,   0,   0,   1,   0,
	  0,   0,   3,   0,   0,   0,
	  0,   0,   0,   0,  15,   0,
	  0,   0,  68,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   3,   0,   0,   0,
	  1,   0,   0,   0,   3,   3,
	  0,   0,  83,  86,  95,  80,
	 79,  83,  73,  84,  73,  79,
	 78,   0,  84,  69,  88,  67,
	 79,  79,  82,  68,   0, 171,
	171, 171,  79,  83,  71,  78,
	 44,   0,   0,   0,   1,   0,
	  0,   0,   8,   0,   0,   0,
	 32,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  3,   0,   0,   0,   0,   0,
	  0,   0,  15,   0,   0,   0,
	 83,  86,  95,  84,  65,  82,
	 71,  69,  84,   0, 171, 171,
	 83,  72,  68,  82, 100,   0,
	  0,   0,  64,   0,   0,   0,
	 25,   0,   0,   0,  90,   0,
	  0,   3,   0,  96,  16,   0,
	  0,   0,   0,   0,  88,  24,
	  0,   4,   0, 112,  16,   0,
	  0,   0,   0,   0,  85,  85,
	  0,   0,  98,  16,   0,   3,
	 50,  16,  16,   0,   1,   0,
	  0,   0, 101,   0,   0,   3,
	242,  32,  16,   0,   0,   0,
	  0,   0,  69,   0,   0,   9,
	242,  32,  16,   0,   0,   0,
	  0,   0,  70,  16,  16,   0,
	  1,   0,   0,   0,  70, 126,
	 16,   0,   0,   0,   0,   0,
	  0,  96,  16,   0,   0,   0,
	  0,   0,  62,   0,   0,   1,
	 83,  84,  65,  84, 116,   0,
	  0,   0,   2,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   2,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  1,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   1,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0
};

#if 0
struct VS_INPUT
{
	float4 Position : POSITION;
	float2 Texture : TEXCOORD;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float2 Texture : TEXCOORD;
};

VS_OUTPUT VSMain_coord(VS_INPUT input)
{
	return input;
}
#endif
const BYTE g_VSMain_coord[] =
{
	 68,  88,  66,  67, 119,  76,
	129,  53, 139, 143, 201, 108,
	 78,  31,  90,  10,  57, 206,
	  5,  93,   1,   0,   0,   0,
	 24,   2,   0,   0,   5,   0,
	  0,   0,  52,   0,   0,   0,
	128,   0,   0,   0, 212,   0,
	  0,   0,  44,   1,   0,   0,
	156,   1,   0,   0,  82,  68,
	 69,  70,  68,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	 28,   0,   0,   0,   0,   4,
	254, 255,   0,   1,   0,   0,
	 28,   0,   0,   0,  77, 105,
	 99, 114, 111, 115, 111, 102,
	116,  32,  40,  82,  41,  32,
	 72,  76,  83,  76,  32,  83,
	104,  97, 100, 101, 114,  32,
	 67, 111, 109, 112, 105, 108,
	101, 114,  32,  49,  48,  46,
	 49,   0,  73,  83,  71,  78,
	 76,   0,   0,   0,   2,   0,
	  0,   0,   8,   0,   0,   0,
	 56,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  3,   0,   0,   0,   0,   0,
	  0,   0,  15,  15,   0,   0,
	 65,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  3,   0,   0,   0,   1,   0,
	  0,   0,   3,   3,   0,   0,
	 80,  79,  83,  73,  84,  73,
	 79,  78,   0,  84,  69,  88,
	 67,  79,  79,  82,  68,   0,
	171, 171,  79,  83,  71,  78,
	 80,   0,   0,   0,   2,   0,
	  0,   0,   8,   0,   0,   0,
	 56,   0,   0,   0,   0,   0,
	  0,   0,   1,   0,   0,   0,
	  3,   0,   0,   0,   0,   0,
	  0,   0,  15,   0,   0,   0,
	 68,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  3,   0,   0,   0,   1,   0,
	  0,   0,   3,  12,   0,   0,
	 83,  86,  95,  80,  79,  83,
	 73,  84,  73,  79,  78,   0,
	 84,  69,  88,  67,  79,  79,
	 82,  68,   0, 171, 171, 171,
	 83,  72,  68,  82, 104,   0,
	  0,   0,  64,   0,   1,   0,
	 26,   0,   0,   0,  95,   0,
	  0,   3, 242,  16,  16,   0,
	  0,   0,   0,   0,  95,   0,
	  0,   3,  50,  16,  16,   0,
	  1,   0,   0,   0, 103,   0,
	  0,   4, 242,  32,  16,   0,
	  0,   0,   0,   0,   1,   0,
	  0,   0, 101,   0,   0,   3,
	 50,  32,  16,   0,   1,   0,
	  0,   0,  54,   0,   0,   5,
	242,  32,  16,   0,   0,   0,
	  0,   0,  70,  30,  16,   0,
	  0,   0,   0,   0,  54,   0,
	  0,   5,  50,  32,  16,   0,
	  1,   0,   0,   0,  70,  16,
	 16,   0,   1,   0,   0,   0,
	 62,   0,   0,   1,  83,  84,
	 65,  84, 116,   0,   0,   0,
	  3,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  4,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   1,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   2,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0,   0,   0,   0,   0,
	  0,   0
};

struct VertexData
{
	struct
	{
		FLOAT x;
		FLOAT y;
		FLOAT z;
	} pos;

	struct
	{
		FLOAT u;
		FLOAT v;
	} uv;
};

RenderAPI_D3D11::RenderAPI_D3D11()
	/* : m_Device(NULL)
	, m_VB(NULL)
	, m_CB(NULL)
	, m_VertexShader(NULL)
	, m_PixelShader(NULL)
	, m_InputLayout(NULL)
	, m_RasterState(NULL)
	, m_BlendState(NULL)
	, m_DepthState(NULL)*/
{
}


void RenderAPI_D3D11::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	switch (type)
	{
	case kUnityGfxDeviceEventInitialize:
	{
		IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
		//m_Device = d3d->GetDevice();
		device_ = d3d->GetDevice();
		CreateResources();
		break;
	}
	case kUnityGfxDeviceEventShutdown:
		ReleaseResources();
		break;
	}
}


void RenderAPI_D3D11::CreateResources()
{
	/*D3D11_BUFFER_DESC desc;
	memset(&desc, 0, sizeof(desc));

	// vertex buffer
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = 1024;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	m_Device->CreateBuffer(&desc, NULL, &m_VB);

	// constant buffer
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.ByteWidth = 64; // hold 1 matrix
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	desc.CPUAccessFlags = 0;
	m_Device->CreateBuffer(&desc, NULL, &m_CB);

	// shaders
	HRESULT hr;
	hr = m_Device->CreateVertexShader(kVertexShaderCode, sizeof(kVertexShaderCode), nullptr, &m_VertexShader);
	if (FAILED(hr))
		OutputDebugStringA("Failed to create vertex shader.\n");
	hr = m_Device->CreatePixelShader(kPixelShaderCode, sizeof(kPixelShaderCode), nullptr, &m_PixelShader);
	if (FAILED(hr))
		OutputDebugStringA("Failed to create pixel shader.\n");

	// input layout
	if (m_VertexShader)
	{
		D3D11_INPUT_ELEMENT_DESC s_DX11InputElementDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "Level", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		m_Device->CreateInputLayout(s_DX11InputElementDesc, 2, kVertexShaderCode, sizeof(kVertexShaderCode), &m_InputLayout);
	}

	// render states
	D3D11_RASTERIZER_DESC rsdesc;
	memset(&rsdesc, 0, sizeof(rsdesc));
	rsdesc.FillMode = D3D11_FILL_SOLID;
	rsdesc.CullMode = D3D11_CULL_NONE;
	rsdesc.DepthClipEnable = TRUE;
	m_Device->CreateRasterizerState(&rsdesc, &m_RasterState);

	D3D11_DEPTH_STENCIL_DESC dsdesc;
	memset(&dsdesc, 0, sizeof(dsdesc));
	dsdesc.DepthEnable = TRUE;
	dsdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dsdesc.DepthFunc = GetUsesReverseZ() ? D3D11_COMPARISON_GREATER_EQUAL : D3D11_COMPARISON_LESS_EQUAL;
	m_Device->CreateDepthStencilState(&dsdesc, &m_DepthState);

	D3D11_BLEND_DESC bdesc;
	memset(&bdesc, 0, sizeof(bdesc));
	bdesc.RenderTarget[0].BlendEnable = FALSE;
	bdesc.RenderTarget[0].RenderTargetWriteMask = 0xF;
	m_Device->CreateBlendState(&bdesc, &m_BlendState);*/

	static const D3D_FEATURE_LEVEL feature_levels[] = {
  D3D_FEATURE_LEVEL_11_1,
  D3D_FEATURE_LEVEL_11_0,
  D3D_FEATURE_LEVEL_10_1,
  D3D_FEATURE_LEVEL_10_0,
	};

	ComPtr<IDXGIFactory1> factory;
	auto hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
	assert(SUCCEEDED(hr));

	/* We will use CreateSwapChainForHwnd which requires IDXGIFactory2 interface */
	hr = factory.As(&factory_);
	assert(SUCCEEDED(hr));

	/* Select first (default) device. User can select one among enumerated adapters */
	ComPtr<IDXGIAdapter> adapter;
	hr = factory_->EnumAdapters(0, &adapter);
	assert(SUCCEEDED(hr));

	/*hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN,
		nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, feature_levels,
		G_N_ELEMENTS(feature_levels), D3D11_SDK_VERSION, &device_,
		nullptr, &context_);

	/* Old OS may not understand D3D_FEATURE_LEVEL_11_1, try without it if needed */
	/*if (FAILED(hr)) {
		hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN,
			nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, &feature_levels[1],
			G_N_ELEMENTS(feature_levels) - 1, D3D11_SDK_VERSION, &device_,
			nullptr, &context_);
	}
	assert(SUCCEEDED(hr));*/

	/* Create shader pipeline */
	D3D11_SAMPLER_DESC sampler_desc = { };
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = device_->CreateSamplerState(&sampler_desc, &sampler_);
	assert(SUCCEEDED(hr));

	D3D11_INPUT_ELEMENT_DESC input_desc[2];
	input_desc[0].SemanticName = "POSITION";
	input_desc[0].SemanticIndex = 0;
	input_desc[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	input_desc[0].InputSlot = 0;
	input_desc[0].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	input_desc[0].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	input_desc[0].InstanceDataStepRate = 0;

	input_desc[1].SemanticName = "TEXCOORD";
	input_desc[1].SemanticIndex = 0;
	input_desc[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	input_desc[1].InputSlot = 0;
	input_desc[1].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	input_desc[1].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	input_desc[1].InstanceDataStepRate = 0;
	hr = device_->CreateVertexShader(g_VSMain_coord, sizeof(g_VSMain_coord),
		nullptr, &vs_);
	assert(SUCCEEDED(hr));

	hr = device_->CreateInputLayout(input_desc, G_N_ELEMENTS(input_desc),
		g_VSMain_coord, sizeof(g_VSMain_coord), &layout_);
	assert(SUCCEEDED(hr));

	hr = device_->CreatePixelShader(g_PSMain_sample, sizeof(g_PSMain_sample),
		nullptr, &ps_);
	assert(SUCCEEDED(hr));

	VertexData vertex_data[4];
	/* bottom left */
	vertex_data[0].pos.x = -1.0f;
	vertex_data[0].pos.y = -1.0f;
	vertex_data[0].pos.z = 0.0f;
	vertex_data[0].uv.u = 0.0f;
	vertex_data[0].uv.v = 1.0f;

	/* top left */
	vertex_data[1].pos.x = -1.0f;
	vertex_data[1].pos.y = 1.0f;
	vertex_data[1].pos.z = 0.0f;
	vertex_data[1].uv.u = 0.0f;
	vertex_data[1].uv.v = 0.0f;

	/* top right */
	vertex_data[2].pos.x = 1.0f;
	vertex_data[2].pos.y = 1.0f;
	vertex_data[2].pos.z = 0.0f;
	vertex_data[2].uv.u = 1.0f;
	vertex_data[2].uv.v = 0.0f;

	/* bottom right */
	vertex_data[3].pos.x = 1.0f;
	vertex_data[3].pos.y = -1.0f;
	vertex_data[3].pos.z = 0.0f;
	vertex_data[3].uv.u = 1.0f;
	vertex_data[3].uv.v = 1.0f;

	D3D11_BUFFER_DESC buffer_desc = { };
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.ByteWidth = sizeof(VertexData) * 4;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA subresource_data = { };
	subresource_data.pSysMem = vertex_data;
	subresource_data.SysMemPitch = sizeof(VertexData) * 4;
	hr = device_->CreateBuffer(&buffer_desc, &subresource_data, &vertex_buf_);
	assert(SUCCEEDED(hr));

	const WORD indices[6] = { 0, 1, 2, 3, 0, 2 };
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.ByteWidth = sizeof(WORD) * 6;
	buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	subresource_data.pSysMem = indices;
	subresource_data.SysMemPitch = sizeof(WORD) * 6;
	hr = device_->CreateBuffer(&buffer_desc, &subresource_data, &index_buf_);
	assert(SUCCEEDED(hr));

	D3D11_RASTERIZER_DESC rs_desc = { };
	rs_desc.FillMode = D3D11_FILL_SOLID;
	rs_desc.CullMode = D3D11_CULL_NONE;
	rs_desc.DepthClipEnable = TRUE;

	hr = device_->CreateRasterizerState(&rs_desc, &rs_);
	assert(SUCCEEDED(hr));

	/* Create swapchain */
	/*DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
	//swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchain_desc.SampleDesc.Count = 1;
	swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.BufferCount = 2;
	//swapchain_desc.Scaling = DXGI_SCALING_NONE;
	swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	//swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	//hr = factory_->CreateSwapChainForHwnd(device_.Get(), hwnd,
	//	&swapchain_desc, nullptr, nullptr, &swapchain_);
	hr = factory_->CreateSwapChain(device_.Get(), &swapchain_desc, &swapchain_);
	assert(SUCCEEDED(hr));*/
}


void RenderAPI_D3D11::ReleaseResources()
{
	/*SAFE_RELEASE(m_VB);
	SAFE_RELEASE(m_CB);
	SAFE_RELEASE(m_VertexShader);
	SAFE_RELEASE(m_PixelShader);
	SAFE_RELEASE(m_InputLayout);
	SAFE_RELEASE(m_RasterState);
	SAFE_RELEASE(m_BlendState);
	SAFE_RELEASE(m_DepthState);*/
}


void RenderAPI_D3D11::DrawSimpleTriangles(const float worldMatrix[16], int triangleCount, const void* verticesFloat3Byte4)
{
	/*ID3D11DeviceContext* ctx = NULL;
	m_Device->GetImmediateContext(&ctx);

	// Set basic render state
	ctx->OMSetDepthStencilState(m_DepthState, 0);
	ctx->RSSetState(m_RasterState);
	ctx->OMSetBlendState(m_BlendState, NULL, 0xFFFFFFFF);

	// Update constant buffer - just the world matrix in our case
	ctx->UpdateSubresource(m_CB, 0, NULL, worldMatrix, 64, 0);

	// Set shaders
	ctx->VSSetConstantBuffers(0, 1, &m_CB);
	ctx->VSSetShader(m_VertexShader, NULL, 0);
	ctx->PSSetShader(m_PixelShader, NULL, 0);

	// Update vertex buffer
	const int kVertexSize = 12 + 4;
	ctx->UpdateSubresource(m_VB, 0, NULL, verticesFloat3Byte4, triangleCount * 3 * kVertexSize, 0);

	// set input assembler data and draw
	ctx->IASetInputLayout(m_InputLayout);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = kVertexSize;
	UINT offset = 0;
	ctx->IASetVertexBuffers(0, 1, &m_VB, &stride, &offset);
	ctx->Draw(triangleCount * 3, 0);

	ctx->Release();*/
}


void* RenderAPI_D3D11::BeginModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int* outRowPitch)
{
	/*const int rowPitch = textureWidth * 4;
	// Just allocate a system memory buffer here for simplicity
	unsigned char* data = new unsigned char[rowPitch * textureHeight];
	*outRowPitch = rowPitch;
	return data;*/
	return nullptr;
}


void RenderAPI_D3D11::EndModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr)
{
	/*ID3D11Texture2D* d3dtex = (ID3D11Texture2D*)textureHandle;
	assert(d3dtex);

	ID3D11DeviceContext* ctx = NULL;
	m_Device->GetImmediateContext(&ctx);
	// Update texture data, and free the memory buffer
	ctx->UpdateSubresource(d3dtex, 0, NULL, dataPtr, rowPitch, 0);
	delete[] (unsigned char*)dataPtr;
	ctx->Release();*/
}


void* RenderAPI_D3D11::BeginModifyVertexBuffer(void* bufferHandle, size_t* outBufferSize)
{
	/*ID3D11Buffer* d3dbuf = (ID3D11Buffer*)bufferHandle;
	assert(d3dbuf);
	D3D11_BUFFER_DESC desc;
	d3dbuf->GetDesc(&desc);
	*outBufferSize = desc.ByteWidth;

	ID3D11DeviceContext* ctx = NULL;
	m_Device->GetImmediateContext(&ctx);
	D3D11_MAPPED_SUBRESOURCE mapped;
	ctx->Map(d3dbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	ctx->Release();

	return mapped.pData;*/
	return nullptr;
}


void RenderAPI_D3D11::EndModifyVertexBuffer(void* bufferHandle)
{
	/*ID3D11Buffer* d3dbuf = (ID3D11Buffer*)bufferHandle;
	assert(d3dbuf);

	ID3D11DeviceContext* ctx = NULL;
	m_Device->GetImmediateContext(&ctx);
	ctx->Unmap(d3dbuf, 0);
	ctx->Release();*/
}

void RenderAPI_D3D11::Render(void* texture[2])
{
	ComPtr<ID3D11ShaderResourceView> srv[2];
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = { };
	srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Texture2D.MipLevels = 1;
	HRESULT hr;
	for (UINT i = 0; i < 2; i++) {
		hr = device_->CreateShaderResourceView((ID3D11Texture2D*)texture[i], &srv_desc,
			&srv[i]);
		g_assert(SUCCEEDED(hr));
	}

	ID3D11DeviceContext* ctx = NULL;
	device_->GetImmediateContext(&ctx);

	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D11Buffer* vb[] = { vertex_buf_.Get() };
	UINT offsets = 0;
	UINT vb_stride = sizeof(VertexData);
	ctx->IASetVertexBuffers(0, 1, vb, &vb_stride, &offsets);
	ctx->IASetIndexBuffer(index_buf_.Get(), DXGI_FORMAT_R16_UINT, 0);
	ctx->IASetInputLayout(layout_.Get());

	ctx->VSSetShader(vs_.Get(), nullptr, 0);

	ID3D11SamplerState* sampler[] = { sampler_.Get() };
	ctx->PSSetSamplers(0, 1, sampler);
	ctx->PSSetShader(ps_.Get(), nullptr, 0);

	ID3D11ShaderResourceView* view[] = { srv[0].Get() };
	ctx->PSSetShaderResources(0, 1, view);

	ctx->RSSetState(rs_.Get());
	ctx->RSSetViewports(1, viewport_);

	ctx->OMSetBlendState(nullptr, nullptr, 0xffffffff);
	ID3D11RenderTargetView* rtv[] = { rtv_.Get() };
	ctx->OMSetRenderTargets(1, rtv, nullptr);
	ctx->DrawIndexed(6, 0, 0);

	/* Draw right image */
	view[0] = srv[1].Get();
	ctx->PSSetShaderResources(0, 1, view);
	ctx->RSSetViewports(1, &viewport_[1]);
	ctx->DrawIndexed(6, 0, 0);

	/* Then present */
	//swapchain_->Present(0, 0);
}

#endif // #if SUPPORT_D3D11

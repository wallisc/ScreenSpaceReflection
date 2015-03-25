#include <windows.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "DDSTextureLoader.h"
#include "resource.h"

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT2 Tex;
    XMFLOAT3 Norm;
};

struct CBNeverChanges
{
    XMMATRIX mView;
    XMVECTOR mCamPos;
    XMVECTOR mClipDistance;
};

struct CBChangeOnResize
{
    XMMATRIX mProjection;
    XMVECTOR mDimensions;
};


struct CBMaterial
{
    XMVECTOR Reflectivity;
};

struct CBChangesEveryFrame
{
    XMMATRIX mWorld;
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                           g_hInst = nullptr;
HWND                                g_hWnd = nullptr;
D3D_DRIVER_TYPE                     g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL                   g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*                       g_pd3dDevice = nullptr;
ID3D11Device1*                      g_pd3dDevice1 = nullptr;
ID3D11DeviceContext*                g_pImmediateContext = nullptr;
ID3D11DeviceContext1*               g_pImmediateContext1 = nullptr;
IDXGISwapChain*                     g_pSwapChain = nullptr;
IDXGISwapChain1*                    g_pSwapChain1 = nullptr;
ID3D11RenderTargetView*             g_pGBufferReflRTV = nullptr;
ID3D11RenderTargetView*             g_pGBufferColorRTV = nullptr;
ID3D11RenderTargetView*             g_pGBufferMaterialRTV = nullptr;
ID3D11RenderTargetView*             g_pGBufferCameraPosRTV = nullptr;
ID3D11RenderTargetView*             g_pFinalOutputView = nullptr;
ID3D11DepthStencilView*             g_pDepthStencilView = nullptr;
ID3D11DepthStencilState*            g_pDepthState = nullptr;
ID3D11Texture2D*                    g_pDepthStencil = nullptr;
ID3D11VertexShader*                 g_pVertexShader = nullptr;
ID3D11PixelShader*                  g_pPixelShader = nullptr;
ID3D11InputLayout*                  g_pVertexLayout = nullptr;
ID3D11PixelShader*                  g_pDeferredPixelShader = nullptr;
ID3D11VertexShader*                 g_pDeferredVertexShader = nullptr;
ID3D11InputLayout*                  g_pDeferredVertexLayout = nullptr;
ID3D11Buffer*                       g_pBoxVertexBuffer = nullptr;
ID3D11Buffer*                       g_pPlaneVertexBuffer = nullptr;
ID3D11Buffer*                       g_pBoxIndexBuffer = nullptr;
ID3D11Buffer*                       g_pCBNeverChanges = nullptr;
ID3D11Buffer*                       g_pCBChangeOnResize = nullptr;
ID3D11Buffer*                       g_pCBBoxMaterial = nullptr;
ID3D11Buffer*                       g_pCBPlaneMaterial = nullptr;
ID3D11Buffer*                       g_pCBChangesEveryFrame = nullptr;
ID3D11ShaderResourceView*           g_pGBufferReflSRV = nullptr;
ID3D11ShaderResourceView*           g_pGBufferCameraPosSRV = nullptr;
ID3D11ShaderResourceView*           g_pGBufferColorSRV = nullptr;
ID3D11ShaderResourceView*           g_pGBufferMaterialSRV = nullptr;
ID3D11ShaderResourceView*           g_pPlaneTextureRV = nullptr;
ID3D11ShaderResourceView*           g_pTextureRV = nullptr;
ID3D11SamplerState*                 g_pSamplerLinear = nullptr;
XMMATRIX                            g_World;
XMMATRIX                            g_View;
XMMATRIX                            g_Projection;

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Render();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

    if( FAILED( InitDevice() ) )
    {
        CleanupDevice();
        return 0;
    }

    // Main message loop
    MSG msg = {0};
    while( WM_QUIT != msg.message )
    {
        if( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
        {
            Render();
        }
    }

    CleanupDevice();

    return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
    wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"TutorialWindowClass";
    wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_TUTORIAL1 );
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, 800, 600 };
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    g_hWnd = CreateWindow( L"TutorialWindowClass", L"Direct3D 11 Tutorial 7",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
                           nullptr );
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;

    // Disable optimizations to further improve shader debugging
    dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = nullptr;
    hr = D3DCompileFromFile( szFileName, nullptr, nullptr, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, ppBlobOut, &pErrorBlob );
    if( FAILED(hr) )
    {
        if( pErrorBlob )
        {
            OutputDebugStringA( reinterpret_cast<const char*>( pErrorBlob->GetBufferPointer() ) );
            pErrorBlob->Release();
        }
        return hr;
    }
    if( pErrorBlob ) pErrorBlob->Release();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect( g_hWnd, &rc );
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef DEBUG
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE( driverTypes );

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE( featureLevels );

    for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDevice( nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
                                D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );

        if ( hr == E_INVALIDARG )
        {
            // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
            hr = D3D11CreateDevice( nullptr, g_driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
                                    D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
        }

        if( SUCCEEDED( hr ) )
            break;
    }
    if( FAILED( hr ) )
        return hr;

    // Obtain DXGI factory from device (since we used nullptr for pAdapter above)
    IDXGIFactory1* dxgiFactory = nullptr;
    {
        IDXGIDevice* dxgiDevice = nullptr;
        hr = g_pd3dDevice->QueryInterface( __uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice) );
        if (SUCCEEDED(hr))
        {
            IDXGIAdapter* adapter = nullptr;
            hr = dxgiDevice->GetAdapter(&adapter);
            if (SUCCEEDED(hr))
            {
                hr = adapter->GetParent( __uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory) );
                adapter->Release();
            }
            dxgiDevice->Release();
        }
    }
    if (FAILED(hr))
        return hr;

    // Create swap chain
    IDXGIFactory2* dxgiFactory2 = nullptr;
    hr = dxgiFactory->QueryInterface( __uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2) );
    if ( dxgiFactory2 )
    {
        // DirectX 11.1 or later
        hr = g_pd3dDevice->QueryInterface( __uuidof(ID3D11Device1), reinterpret_cast<void**>(&g_pd3dDevice1) );
        if (SUCCEEDED(hr))
        {
            (void) g_pImmediateContext->QueryInterface( __uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&g_pImmediateContext1) );
        }

        DXGI_SWAP_CHAIN_DESC1 sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.Width = width;
        sd.Height = height;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 1;

        hr = dxgiFactory2->CreateSwapChainForHwnd( g_pd3dDevice, g_hWnd, &sd, nullptr, nullptr, &g_pSwapChain1 );
        if (SUCCEEDED(hr))
        {
            hr = g_pSwapChain1->QueryInterface( __uuidof(IDXGISwapChain), reinterpret_cast<void**>(&g_pSwapChain) );
        }

        dxgiFactory2->Release();
    }
    else
    {
        // DirectX 11.0 systems
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = 1;
        sd.BufferDesc.Width = width;
        sd.BufferDesc.Height = height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = g_hWnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;

        hr = dxgiFactory->CreateSwapChain( g_pd3dDevice, &sd, &g_pSwapChain );
    }

    // Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
    //dxgiFactory->MakeWindowAssociation( g_hWnd, DXGI_MWA_NO_ALT_ENTER );

    dxgiFactory->Release();

    if (FAILED(hr))
        return hr;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &pBackBuffer ) );
    if( FAILED( hr ) )
        return hr;

    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC firstPassTexture;
    ZeroMemory( &firstPassTexture, sizeof(firstPassTexture) );
    firstPassTexture.Width = width;
    firstPassTexture.Height = height;
    firstPassTexture.MipLevels = 1;
    firstPassTexture.ArraySize = 1;
    firstPassTexture.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    firstPassTexture.SampleDesc.Count = 1;
    firstPassTexture.SampleDesc.Quality = 0;
    firstPassTexture.Usage = D3D11_USAGE_DEFAULT;
    firstPassTexture.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    firstPassTexture.CPUAccessFlags = 0;
    firstPassTexture.MiscFlags = 0;

	ID3D11Texture2D* pGBufferTexture;
    hr = g_pd3dDevice->CreateTexture2D( &firstPassTexture, nullptr, &pGBufferTexture);
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateRenderTargetView( pGBufferTexture, nullptr, &g_pGBufferColorRTV );
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateShaderResourceView( pGBufferTexture, nullptr, &g_pGBufferColorSRV);
    if( FAILED( hr ) )
        return hr;
	pGBufferTexture->Release();

    hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &g_pFinalOutputView );
    pBackBuffer->Release();
    if( FAILED( hr ) )
        return hr;

    ID3D11Texture2D *pReflTexture;
    hr = g_pd3dDevice->CreateTexture2D( &firstPassTexture, nullptr, &pReflTexture);
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateShaderResourceView( pReflTexture, nullptr, &g_pGBufferReflSRV);
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateRenderTargetView( pReflTexture, nullptr, &g_pGBufferReflRTV);
    if( FAILED( hr ) )
        return hr;
    pReflTexture->Release();

    ID3D11Texture2D *pWorldPosTexture;
    hr = g_pd3dDevice->CreateTexture2D( &firstPassTexture, nullptr, &pWorldPosTexture);
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateShaderResourceView( pWorldPosTexture, nullptr, &g_pGBufferCameraPosSRV);
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateRenderTargetView( pWorldPosTexture, nullptr, &g_pGBufferCameraPosRTV);
    if( FAILED( hr ) )
        return hr;
    pWorldPosTexture->Release();

    ID3D11Texture2D *pMaterialTexture;
    hr = g_pd3dDevice->CreateTexture2D( &firstPassTexture, nullptr, &pMaterialTexture);
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateShaderResourceView( pMaterialTexture, nullptr, &g_pGBufferMaterialSRV);
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateRenderTargetView( pMaterialTexture, nullptr, &g_pGBufferMaterialRTV);
    if( FAILED( hr ) )
        return hr;
    pMaterialTexture->Release();

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

    hr = g_pd3dDevice->CreateDepthStencilState(&depthStencilDesc, &g_pDepthState);
    if( FAILED( hr ) )
        return hr;

    g_pImmediateContext->OMSetDepthStencilState(g_pDepthState, 0);

    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC descDepth;
    ZeroMemory( &descDepth, sizeof(descDepth) );
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    hr = g_pd3dDevice->CreateTexture2D( &descDepth, nullptr, &g_pDepthStencil );
    if( FAILED( hr ) )
        return hr;

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory( &descDSV, sizeof(descDSV) );
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = g_pd3dDevice->CreateDepthStencilView( g_pDepthStencil, &descDSV, &g_pDepthStencilView );
    if( FAILED( hr ) )
        return hr;

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports( 1, &vp );

    // Compile the vertex shader
    ID3DBlob* pVSBlob = nullptr;
    hr = CompileShaderFromFile( L"GeometryRender.fx", "VS", "vs_4_0", &pVSBlob );
    if( FAILED( hr ) )
    {
        MessageBox( nullptr,
                    L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
        return hr;
    }

    // Create the vertex shader
    hr = g_pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader );
    if( FAILED( hr ) )
    {    
        pVSBlob->Release();
        return hr;
    }

    ID3DBlob* pDeferredVSBlob = nullptr;
    hr = CompileShaderFromFile( L"DeferredRender.fx", "VS", "vs_5_0", &pDeferredVSBlob  );
    if( FAILED( hr ) )
    {
        MessageBox( nullptr,
                    L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
        return hr;
    }
    
    // Create the vertex shader
    hr = g_pd3dDevice->CreateVertexShader( pDeferredVSBlob ->GetBufferPointer(), pDeferredVSBlob ->GetBufferSize(), nullptr, &g_pDeferredVertexShader );
    if( FAILED( hr ) )
    {    
        pDeferredVSBlob ->Release();
        return hr;
    }


    // Define the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = ARRAYSIZE( layout );

    // Create the input layout
    hr = g_pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
                                          pVSBlob->GetBufferSize(), &g_pVertexLayout );

    hr = g_pd3dDevice->CreateInputLayout( layout, 0, pDeferredVSBlob->GetBufferPointer(),
                                          pDeferredVSBlob->GetBufferSize(), &g_pDeferredVertexLayout );
    pVSBlob->Release();
    if( FAILED( hr ) )
        return hr;

    pDeferredVSBlob->Release();
    if( FAILED( hr ) )
        return hr;

    // Compile the pixel shader
    ID3DBlob* pPSBlob = nullptr;
    hr = CompileShaderFromFile( L"GeometryRender.fx", "PS", "ps_4_0", &pPSBlob );
    if( FAILED( hr ) )
    {
        MessageBox( nullptr,
                    L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
        return hr;
    }

    // Create the pixel shader
    hr = g_pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader );
    pPSBlob->Release();
    if( FAILED( hr ) )
        return hr;

    hr = CompileShaderFromFile( L"DeferredRender.fx", "PS", "ps_5_0", &pPSBlob );
    hr = g_pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pDeferredPixelShader );
    pPSBlob->Release();
    if( FAILED( hr ) )
        return hr;

    // Create vertex buffer
    SimpleVertex vertices[] =
    {
        { XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT3(0.0f, 1.0f, 0.0f) },

        { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT3(0.0f, -1.0f, 0.0f) },
        { XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT3(0.0f, -1.0f, 0.0f) },
        { XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT3(0.0f, -1.0f, 0.0f) },
        { XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT3(0.0f, -1.0f, 0.0f) },

        { XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
        { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
        { XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT3(-1.0f, 0.0f, 0.0f) },
        { XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT3(-1.0f, 0.0f, 0.0f) },

        { XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT3(1.0f, 0.0f, 0.0f) },
        { XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT3(1.0f, 0.0f, 0.0f) },
        { XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT3(1.0f, 0.0f, 0.0f) },
        { XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT3(1.0f, 0.0f, 0.0f) },

        { XMFLOAT3( -1.0f, -1.0f, -1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3( 1.0f, -1.0f, -1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3( 1.0f, 1.0f, -1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT3(0.0f, 0.0f, -1.0f) },
        { XMFLOAT3( -1.0f, 1.0f, -1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT3(0.0f, 0.0f, -1.0f) },

        { XMFLOAT3( -1.0f, -1.0f, 1.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3( 1.0f, -1.0f, 1.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3( 1.0f, 1.0f, 1.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT3(0.0f, 0.0f, 1.0f) },
        { XMFLOAT3( -1.0f, 1.0f, 1.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT3(0.0f, 0.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC bd;
    ZeroMemory( &bd, sizeof(bd) );
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( SimpleVertex ) * 24;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(InitData) );
    InitData.pSysMem = vertices;
    hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pBoxVertexBuffer );
    if( FAILED( hr ) )
        return hr;

    // Create index buffer
    // Create vertex buffer
    WORD indices[] =
    {
        3,1,0,
        2,1,3,

        6,4,5,
        7,4,6,

        11,9,8,
        10,9,11,

        14,12,13,
        15,12,14,

        19,17,16,
        18,17,19,

        22,20,21,
        23,20,22
    };

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( WORD ) * 36;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    InitData.pSysMem = indices;
    hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pBoxIndexBuffer );
    if( FAILED( hr ) )
        return hr;

      // Create vertex buffer for the ground plane
    SimpleVertex planeVertices[] =
    {
        { XMFLOAT3( -5.0f, -1.0f, 5.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3( 5.0f, -1.0f, -5.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3( -5.0f, -1.0f, -5.0f ), XMFLOAT2( 1.0f, 0.0f ), XMFLOAT3(0.0f, 1.0f, 0.0f) },

        { XMFLOAT3( 5.0f, -1.0f, 5.0f ), XMFLOAT2( 0.0f, 1.0f ), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3( 5.0f, -1.0f, -5.0f ), XMFLOAT2( 0.0f, 0.0f ), XMFLOAT3(0.0f, 1.0f, 0.0f) },
        { XMFLOAT3( -5.0f, -1.0f, 5.0f ), XMFLOAT2( 1.0f, 1.0f ), XMFLOAT3(0.0f, 1.0f, 0.0f) },
    };

    D3D11_BUFFER_DESC planeBd;
    ZeroMemory( &planeBd, sizeof(planeBd) );
    planeBd.Usage = D3D11_USAGE_DEFAULT;
    planeBd.ByteWidth = sizeof( SimpleVertex ) * 6;
    planeBd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    planeBd.CPUAccessFlags = 0;
    D3D11_SUBRESOURCE_DATA planeInitData;
    ZeroMemory( &planeInitData, sizeof(planeInitData) );
    planeInitData.pSysMem = planeVertices;
    hr = g_pd3dDevice->CreateBuffer( &planeBd, &planeInitData, &g_pPlaneVertexBuffer );
    if( FAILED( hr ) )
        return hr;

    // Set primitive topology
    g_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Create the constant buffers
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(CBNeverChanges);
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    hr = g_pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBNeverChanges );
    if( FAILED( hr ) )
        return hr;
    
    bd.ByteWidth = sizeof(CBChangeOnResize);
    hr = g_pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBChangeOnResize );
    if( FAILED( hr ) )
        return hr;

    CBMaterial PlaneMaterial;
    PlaneMaterial.Reflectivity = XMVectorSet(0.5f, 0, 0, 0);
    bd.ByteWidth = sizeof(CBMaterial);
    D3D11_SUBRESOURCE_DATA Data = {};
    Data.pSysMem = &PlaneMaterial;
    hr = g_pd3dDevice->CreateBuffer( &bd, &Data, &g_pCBPlaneMaterial);
    if( FAILED( hr ) )
        return hr;

    CBMaterial BoxMaterial;
    Data.pSysMem = &BoxMaterial;
    BoxMaterial.Reflectivity = XMVectorSet(0.0f, 0, 0, 0);
    hr = g_pd3dDevice->CreateBuffer( &bd, &Data, &g_pCBBoxMaterial );
    if( FAILED( hr ) )
        return hr;
    
    bd.ByteWidth = sizeof(CBChangesEveryFrame);
    hr = g_pd3dDevice->CreateBuffer( &bd, nullptr, &g_pCBChangesEveryFrame );
    if( FAILED( hr ) )
        return hr;

    // Load the Texture
    hr = CreateDDSTextureFromFile( g_pd3dDevice, L"crate.dds", nullptr, &g_pTextureRV );
    if( FAILED( hr ) )
        return hr;

    hr = CreateDDSTextureFromFile( g_pd3dDevice, L"tile.dds", nullptr, &g_pPlaneTextureRV );
    if( FAILED( hr ) )
        return hr;

    // Create the sample state
    D3D11_SAMPLER_DESC sampDesc;
    ZeroMemory( &sampDesc, sizeof(sampDesc) );
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = g_pd3dDevice->CreateSamplerState( &sampDesc, &g_pSamplerLinear );
    if( FAILED( hr ) )
        return hr;

    // Initialize the world matrices
    g_World = XMMatrixIdentity();

    // Initialize the view matrix
    XMVECTOR Eye = XMVectorSet( 0.0f, 3.0f, -11.0f, 1.0f );
    XMVECTOR At = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    XMVECTOR Up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    g_View = XMMatrixLookAtLH( Eye, At, Up );

	const float nearClip = 0.01f;
	const float farClip = 100.0f;

    CBNeverChanges cbNeverChanges;
    cbNeverChanges.mView = XMMatrixTranspose( g_View );
    cbNeverChanges.mCamPos = Eye;
    cbNeverChanges.mClipDistance = XMVectorSet(farClip, 0, 0, 0);
    g_pImmediateContext->UpdateSubresource( g_pCBNeverChanges, 0, nullptr, &cbNeverChanges, 0, 0 );

    // Initialize the projection matrix
    g_Projection = XMMatrixPerspectiveFovLH( XM_PIDIV4, width / (FLOAT)height, nearClip, farClip);
    
    CBChangeOnResize cbChangesOnResize;
    cbChangesOnResize.mProjection = XMMatrixTranspose( g_Projection );
    cbChangesOnResize.mDimensions = XMVectorSet((float)width, (float)height, 0, 0);
    g_pImmediateContext->UpdateSubresource( g_pCBChangeOnResize, 0, nullptr, &cbChangesOnResize, 0, 0 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if( g_pImmediateContext ) g_pImmediateContext->ClearState();

    if( g_pSamplerLinear ) g_pSamplerLinear->Release();
    if( g_pTextureRV ) g_pTextureRV->Release();
    if( g_pPlaneTextureRV ) g_pPlaneTextureRV->Release();
    if( g_pCBNeverChanges ) g_pCBNeverChanges->Release();
    if( g_pCBChangeOnResize ) g_pCBChangeOnResize->Release();
    if( g_pCBPlaneMaterial ) g_pCBPlaneMaterial->Release();
    if( g_pCBBoxMaterial ) g_pCBBoxMaterial->Release();
    if( g_pCBChangesEveryFrame ) g_pCBChangesEveryFrame->Release();
    if( g_pPlaneVertexBuffer ) g_pPlaneVertexBuffer->Release();
    if( g_pBoxVertexBuffer ) g_pBoxVertexBuffer->Release();
    if( g_pBoxIndexBuffer ) g_pBoxIndexBuffer->Release();
    if( g_pVertexLayout ) g_pVertexLayout->Release();
    if( g_pVertexShader ) g_pVertexShader->Release();
    if( g_pPixelShader ) g_pPixelShader->Release();
    if( g_pDepthStencil ) g_pDepthStencil->Release();
    if( g_pDepthStencilView ) g_pDepthStencilView->Release();
    if( g_pDepthState ) g_pDepthState->Release();
    if( g_pGBufferColorSRV ) g_pGBufferColorSRV->Release();
    if( g_pGBufferColorRTV ) g_pGBufferColorRTV->Release();
    if( g_pSwapChain1 ) g_pSwapChain1->Release();
    if( g_pSwapChain ) g_pSwapChain->Release();
    if( g_pImmediateContext1 ) g_pImmediateContext1->Release();
    if( g_pImmediateContext ) g_pImmediateContext->Release();
    if( g_pd3dDevice1 ) g_pd3dDevice1->Release();
    if( g_pd3dDevice ) g_pd3dDevice->Release();
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch( message )
    {
    case WM_PAINT:
        hdc = BeginPaint( hWnd, &ps );
        EndPaint( hWnd, &ps );
        break;

    case WM_DESTROY:
        PostQuitMessage( 0 );
        break;

        // Note that this tutorial does not handle resizing (WM_SIZE) requests,
        // so we created the window without the resize border.

    default:
        return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
    // Update our time
    static float t = 0.0f;
    if( g_driverType == D3D_DRIVER_TYPE_REFERENCE )
    {
        t += ( float )XM_PI * 0.0125f;
    }
    else
    {
        static ULONGLONG timeStart = 0;
        ULONGLONG timeCur = GetTickCount64();
        if( timeStart == 0 )
            timeStart = timeCur;
        t = ( timeCur - timeStart ) / 1000.0f;
    }

    // Rotate cube around the origin
    g_World = XMMatrixRotationY( -t );

    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), reinterpret_cast<void**>( &pBackBuffer ) );
    if( FAILED( hr ) )
        return;

    //
    // Clear resources
    //
    g_pImmediateContext->ClearRenderTargetView( g_pFinalOutputView, Colors::Gray );
    g_pImmediateContext->ClearRenderTargetView( g_pGBufferColorRTV, Colors::Gray );
    g_pImmediateContext->ClearRenderTargetView( g_pGBufferMaterialRTV, Colors::Black );
    g_pImmediateContext->ClearRenderTargetView( g_pGBufferCameraPosRTV, Colors::Black );
    g_pImmediateContext->ClearRenderTargetView( g_pGBufferReflRTV, Colors::Black);
    g_pImmediateContext->ClearDepthStencilView( g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0 );

    //
    // Update variables that change once per frame
    //
    CBChangesEveryFrame cb;
    cb.mWorld = XMMatrixTranspose( g_World );
    g_pImmediateContext->UpdateSubresource( g_pCBChangesEveryFrame, 0, nullptr, &cb, 0, 0 );

    //
    // Render the cube
    //
    g_pImmediateContext->VSSetShader( g_pVertexShader, nullptr, 0 );
    g_pImmediateContext->VSSetConstantBuffers( 0, 1, &g_pCBNeverChanges );
    g_pImmediateContext->VSSetConstantBuffers( 1, 1, &g_pCBChangeOnResize );
    g_pImmediateContext->VSSetConstantBuffers( 2, 1, &g_pCBChangesEveryFrame );
    g_pImmediateContext->PSSetConstantBuffers( 0, 1, &g_pCBNeverChanges );
    g_pImmediateContext->PSSetConstantBuffers( 2, 1, &g_pCBChangesEveryFrame );
    g_pImmediateContext->PSSetShader( g_pPixelShader, nullptr, 0 );
    g_pImmediateContext->PSSetConstantBuffers( 1, 1, &g_pCBChangeOnResize );
    g_pImmediateContext->PSSetConstantBuffers( 2, 1, &g_pCBChangesEveryFrame );
    g_pImmediateContext->PSSetSamplers( 0, 1, &g_pSamplerLinear );

    UINT stride = sizeof( SimpleVertex );
    UINT offset = 0;
    
    ID3D11RenderTargetView* RenderTargets[] = 
    {
       g_pGBufferColorRTV,
       g_pGBufferReflRTV,
       g_pGBufferMaterialRTV,
       g_pGBufferCameraPosRTV
    };

    g_pImmediateContext->IASetInputLayout( g_pVertexLayout );
    g_pImmediateContext->OMSetRenderTargets( ARRAYSIZE(RenderTargets), RenderTargets, g_pDepthStencilView );

    // Draw the box
    g_pImmediateContext->PSSetShaderResources( 0, 1, &g_pTextureRV );
    g_pImmediateContext->PSSetConstantBuffers( 3, 1, &g_pCBBoxMaterial );
    g_pImmediateContext->IASetVertexBuffers( 0, 1, &g_pBoxVertexBuffer, &stride, &offset );
    g_pImmediateContext->IASetIndexBuffer( g_pBoxIndexBuffer, DXGI_FORMAT_R16_UINT, 0 );
    g_pImmediateContext->DrawIndexed( 36, 0, 0 );

    // Draw the ground plane
    g_pImmediateContext->PSSetShaderResources( 0, 1, &g_pPlaneTextureRV );
    g_pImmediateContext->PSSetConstantBuffers( 3, 1, &g_pCBPlaneMaterial );
    g_pImmediateContext->IASetVertexBuffers( 0, 1, &g_pPlaneVertexBuffer, &stride, &offset );
    g_pImmediateContext->IASetIndexBuffer( NULL, DXGI_FORMAT_R16_UINT, 0 );
    g_pImmediateContext->DrawInstanced(6, 1, 0, 0);

    // Set the input layout
    ID3D11ShaderResourceView *DeferredSRVs[] =
    {
       g_pGBufferColorSRV,
       g_pGBufferReflSRV,
       g_pGBufferMaterialSRV,
       g_pGBufferCameraPosSRV
    };

    g_pImmediateContext->OMSetRenderTargets( 1, &g_pFinalOutputView, nullptr);
    g_pImmediateContext->PSSetShaderResources( 0, ARRAYSIZE(DeferredSRVs), DeferredSRVs);

    // Do the "post process" pass
    g_pImmediateContext->IASetInputLayout( g_pDeferredVertexLayout );
    g_pImmediateContext->VSSetShader( g_pDeferredVertexShader, nullptr, 0 );
    g_pImmediateContext->PSSetShader( g_pDeferredPixelShader, nullptr, 0 );
    g_pImmediateContext->DrawInstanced(3, 1, 0, 0);

    ID3D11ShaderResourceView *NullViews[4] = {};
    g_pImmediateContext->PSSetShaderResources( 0, ARRAYSIZE(NullViews), NullViews);

    //
    // Present our back buffer to our front buffer
    //
    g_pSwapChain->Present( 0, 0 );
}

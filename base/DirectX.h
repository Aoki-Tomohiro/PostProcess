#pragma once
#include "WinApp.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dxgidebug.h>
#include <vector>
#include "DirectXTex.h"
#include "d3dx12.h"
#include <wrl.h>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"dxguid.lib")

class DirectXCommon {
public:
	static uint32_t descriptorSizeSRV;
	static uint32_t descriptorSizeRTV;
	static uint32_t descriptorSizeDSV;
	~DirectXCommon();
	void Initialize(WinApp* winApp);
	void CreateDXGIDevice();
	void CreateCommand();
	void CreateSwapChain();
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(const Microsoft::WRL::ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes);
	void CreateRenderTargetView();
	void CreateSRVDescriptorHeap();
	void CreateShaderResourceView(const Microsoft::WRL::ComPtr<ID3D12Resource>& resource, const DirectX::TexMetadata& metadata, uint32_t index);
	void CreateDepthStencilView();
	void CreateFence();
	void PreDraw();
	void PostDraw();
	DirectX::ScratchImage LoadTexture(const std::string& filePath);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const DirectX::TexMetadata& metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, int32_t width, int32_t height);
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
	Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() { return device_; };
	DXGI_SWAP_CHAIN_DESC1 GetSwapChainDesc() { return swapChainDesc_; };
	D3D12_RENDER_TARGET_VIEW_DESC GetRenderTargetViewDesc() { return rtvDesc_; };
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSRVDescriptorHeap() { return srvDescriptorHeap_; };
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetCommandList() { return commandList_; };
	WinApp* GetWinApp() { return winApp_; };
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, const uint32_t descriptorSize, uint32_t index);
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, const uint32_t descriptorSize, uint32_t index);

	//マルチパスレンダリング
	void CreateFirstPassResource();
	void CreateMultiPassRTV();
	void CreateMultiPassSRV();
	void FirstPassPreDraw();
	void FirstPassPostDraw();
	void PreHorizontalBlur();
	void PostHorizontalBlur();
	void PreVerticalBlur();
	void PostVerticalBlur();
	void PreHorizontalShrinkBlur();
	void PostHorizontalShrinkBlur();
	void PreVerticalShrinkBlur();
	void PostVerticalShrinkBlur();
private:
	//WinApp
	WinApp* winApp_ = nullptr;
	//デバッグレイヤー
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController_ = nullptr;
	//DXGIファクトリー
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
	//アダプタ
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter_ = nullptr;
	//デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device_ = nullptr;
	//コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;
	//コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;
	//コマンドリストを生成
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
	//スワップチェーン
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc_{};
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
	//ディスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;
	//RenderTargetView
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources_[2] = { nullptr };
	//フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_ = nullptr;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;
	//DSV
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_ = nullptr;

	//マルチパスレンダリング
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> multiPassRTVDescriptorHeap_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> firstPassResource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> blurResource_[2];
	Microsoft::WRL::ComPtr<ID3D12Resource> shrinkBlurResource_[2];
	Microsoft::WRL::ComPtr<ID3D12Resource> depthResource_ = nullptr;
};	
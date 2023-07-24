#pragma once
#include "DirectX.h"
#include "MathFunction.h"
#include <dxcapi.h>
#pragma comment(lib,"dxcompiler.lib")

class Model {
public:
	~Model();
	void Initialize(DirectXCommon* directX);
	void InitializeDXCCompiler();
	IDxcBlob* CompileShader(//CompilerするShaderファイルへのパス
		const std::wstring& filePath,
		//compilerに使用するProfile
		const wchar_t* profile,
		//初期化で生成したものを3つ
		IDxcUtils* dxcUtils,
		IDxcCompiler3* dxcCompiler,
		IDxcIncludeHandler* includeHandler);
	void CreatePipelineStateObject();
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateVertexResource(D3D12_VERTEX_BUFFER_VIEW& vertexBufferView, UINT sizeInBytes, VertexData* vertexData, uint32_t vertexCount);
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateMaterialData(Material* color);
	void UpdateMaterialData(const Microsoft::WRL::ComPtr<ID3D12Resource>& materialResource, Material* materialData);
	void UpdateMatrix(const Microsoft::WRL::ComPtr<ID3D12Resource>& WVPResource, TransformationMatrix matrix);
	ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
	MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	void CreateViewport();
	void CreateScissorRect();
	void Draw(D3D12_VERTEX_BUFFER_VIEW* vertexBufferView, UINT vertexCount, const Microsoft::WRL::ComPtr<ID3D12Resource>& materialResource, const Microsoft::WRL::ComPtr<ID3D12Resource>& WVPResource, const Microsoft::WRL::ComPtr<ID3D12Resource>& lightingResource, D3D12_INDEX_BUFFER_VIEW* indexBufferViewSprite, uint32_t textureIndex);
	//マルチパスレンダリング
	void CreateSecondPassPipelineStateObject();
	void SecondPassDraw(const Microsoft::WRL::ComPtr<ID3D12Resource>& fogResource);
	void CreateBlurPipelineStateObject();
	void HorizontalBlur(const Microsoft::WRL::ComPtr<ID3D12Resource> bkResource);
	void VerticalBlur(const Microsoft::WRL::ComPtr<ID3D12Resource> bkResource);
	void HorizontalShrinkBlur(const Microsoft::WRL::ComPtr<ID3D12Resource> bkResource);
	void VerticalShrinkBlur(const Microsoft::WRL::ComPtr<ID3D12Resource> bkResource);
private:
	//DirectX
	DirectXCommon* directX_ = nullptr;
	//dxcCompiler
	IDxcUtils* dxcUtils_ = nullptr;
	IDxcCompiler3* dxcCompiler_ = nullptr;
	IDxcIncludeHandler* includeHandler_ = nullptr;
	//RootSignature
	ID3DBlob* signatureBlob_ = nullptr;
	ID3DBlob* errorBlob_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	//Shaderコンパイル
	IDxcBlob* vertexShaderBlob_{};
	IDxcBlob* pixelShaderBlob_{};
	//PSO
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;
	//ビューポート
	D3D12_VIEWPORT viewport_{};
	//シザー矩形
	D3D12_RECT scissorRect_{};

	//マルチパスレンダリング
	VertexData vertexData[4];
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> secondPassRootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> secondPassGraphicsPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> blurRootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> horizontalBlurGraphicsPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> verticalBlurGraphicsPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> horizontalShrinkBlurGraphicsPipelineState_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> verticalShrinkBlurGraphicsPipelineState_ = nullptr;
};
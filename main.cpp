#include <Windows.h>
#include "WinApp.h"
#include "DirectX.h"
#include "Model.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_DX12.h"
#include "D3DResourceLeakChecker.h"

struct Weights {
	float weight[8];
};

Weights GaussianWeights(float s) {
	Weights weights;
	float total = 0.0f;
	for (int i = 0; i < 8; i++) {
		weights.weight[i] = expf(-(i * i) / (2 * s * s));
		total += weights.weight[i];
	}
	total = total * 2.0f - 1.0f;
	//最終的な合計値で重みをわる
	for (int i = 0; i < 8; i++) {
		weights.weight[i] /= total;
	}
	return weights;
}

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_  HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {

	CoInitializeEx(0, COINIT_MULTITHREADED);

	//リソースリークチェック
	D3DResourceLeakChecker leakCheck;

	//WindowsAPI
	WinApp* winApp = new WinApp;
	winApp->CreateGameWindow(L"CG2WindowClass", winApp->kClientWidth, winApp->kClientHeight);

	//DirectX
	DirectXCommon* directX = new DirectXCommon;
	directX->Initialize(winApp);

	//Model
	Model* model = new Model;
	model->Initialize(directX);

	//モデル読み込み
	ModelData modelData = model->LoadObjFile("resource", "plane.obj");
	//頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = model->CreateBufferResource(directX->GetDevice(), sizeof(VertexData) * modelData.vertices.size());
	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();//リソースの先頭のアドレスから使う
	vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * modelData.vertices.size());//使用するリソースのサイズは頂点のサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	//頂点リソースにデータを書き込む
	VertexData* vertexData = nullptr;
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));//書き込むためのアドレスを取得
	std::memcpy(vertexData, modelData.vertices.data(), sizeof(VertexData)* modelData.vertices.size());

	//モデル読み込み
	ModelData modelData2 = model->LoadObjFile("resource/skydome", "skydome.obj");
	//頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource2 = model->CreateBufferResource(directX->GetDevice(), sizeof(VertexData) * modelData2.vertices.size());
	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView2{};
	vertexBufferView2.BufferLocation = vertexResource2->GetGPUVirtualAddress();//リソースの先頭のアドレスから使う
	vertexBufferView2.SizeInBytes = UINT(sizeof(VertexData) * modelData2.vertices.size());//使用するリソースのサイズは頂点のサイズ
	vertexBufferView2.StrideInBytes = sizeof(VertexData);
	//頂点リソースにデータを書き込む
	VertexData* vertexData2 = nullptr;
	vertexResource2->Map(0, nullptr, reinterpret_cast<void**>(&vertexData2));//書き込むためのアドレスを取得
	std::memcpy(vertexData2, modelData2.vertices.data(), sizeof(VertexData) * modelData2.vertices.size());

	//マテリアルデータ
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = nullptr;
	Material* materialData = new Material;
	materialData->color = { 1.0f,1.0f,1.0f,1.0f };
	materialData->enableLighting = false;
	materialData->uvTransform = MakeIdentity4x4();
	materialResource = model->CreateMaterialData(materialData);
	Transform uvTransform{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f},
	};

	//WVP用リソース
	Transform cameraTransform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,-10.0f} };
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixData = model->CreateBufferResource(directX->GetDevice(), sizeof(TransformationMatrix));
	Transform transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{-1.5f,0.0f,5.0f} };
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixData2 = model->CreateBufferResource(directX->GetDevice(), sizeof(TransformationMatrix));
	Transform transform2 = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{1.5f,0.0f,0.0f} };
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixData3 = model->CreateBufferResource(directX->GetDevice(), sizeof(TransformationMatrix));
	Transform transform3 = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,-10.0f,50.0f} };

	//Lighting
	Microsoft::WRL::ComPtr<ID3D12Resource> lightingResource = model->CreateBufferResource(directX->GetDevice(), sizeof(DirectionalLight));
	DirectionalLight* directionalLight = nullptr;
	DirectionalLight lightingData = { {1.0f,1.0f,1.0f,1.0f},{0.0f,-1.0f,0.0f},1.0f };
	lightingResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLight));
	*directionalLight = lightingData;

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = directX->LoadTexture("resource/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_ = directX->CreateTextureResource(directX->GetDevice(), metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource_ = directX->UploadTextureData(textureResource_, mipImages, directX->GetDevice(), directX->GetCommandList());
	directX->CreateShaderResourceView(textureResource_,metadata, 1);
	//2枚目のテクスチャを読んで転送する
	DirectX::ScratchImage mipImages2 = directX->LoadTexture(modelData2.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2_ = directX->CreateTextureResource(directX->GetDevice(), metadata2);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2_ = directX->UploadTextureData(textureResource2_, mipImages2, directX->GetDevice(), directX->GetCommandList());
	directX->CreateShaderResourceView(textureResource2_, metadata2, 2);

	//Fog
	Microsoft::WRL::ComPtr<ID3D12Resource> fogResource = model->CreateBufferResource(directX->GetDevice(), sizeof(FogParameter));
	FogParameter fogParameter = { 0.1f,30.0f,0.5f,0.5f };

	//ぼかし
	Microsoft::WRL::ComPtr<ID3D12Resource> blurResource = model->CreateBufferResource(directX->GetDevice(), sizeof(Weights));
	Weights* mappedWeight = nullptr;
	float s = 5.0f;
	blurResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedWeight));
	*mappedWeight = GaussianWeights(s);

	//ImGuiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(winApp->GetHwnd());
	ImGui_ImplDX12_Init(directX->GetDevice().Get(),
		directX->GetSwapChainDesc().BufferCount,
		directX->GetRenderTargetViewDesc().Format,
		directX->GetSRVDescriptorHeap().Get(),
		directX->GetSRVDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
		directX->GetSRVDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());

	//ウィンドウの×ボタンが押されるまでループ
	while (true) {
		//メインループを抜ける
		if (winApp->MessageRoop() == true) {
			break;
		}

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		//ゲーム処理
		Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
		Matrix4x4 viewMatrix = Inverse(cameraMatrix);
		Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(45.0f * 3.141592654f / 180.0f, float(winApp->kClientWidth) / float(winApp->kClientHeight), 0.1f, 100.0f);

		Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		TransformationMatrix worldViewProjectionMatrix = { Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix)),worldMatrix };
		model->UpdateMatrix(transformationMatrixData, worldViewProjectionMatrix);

		Matrix4x4 worldMatrix2 = MakeAffineMatrix(transform2.scale, transform2.rotate, transform2.translate);
		TransformationMatrix worldViewProjectionMatrix2 = { Multiply(worldMatrix2, Multiply(viewMatrix, projectionMatrix)),worldMatrix2 };
		model->UpdateMatrix(transformationMatrixData2, worldViewProjectionMatrix2);

		Matrix4x4 worldMatrix3 = MakeAffineMatrix(transform3.scale, transform3.rotate, transform3.translate);
		TransformationMatrix worldViewProjectionMatrix3 = { Multiply(worldMatrix3, Multiply(viewMatrix, projectionMatrix)),worldMatrix3 };
		model->UpdateMatrix(transformationMatrixData3, worldViewProjectionMatrix3);

		//uvTransform
		Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransform.scale);
		uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransform.rotate.z));
		uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransform.translate));
		materialData->uvTransform = uvTransformMatrix;
		model->UpdateMaterialData(materialResource, materialData);

		//lighting
		lightingResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLight));
		*directionalLight = lightingData;

		//FogParameter
		FogParameter* fog = nullptr;
		fogResource->Map(0, nullptr, reinterpret_cast<void**>(&fog));
		*fog = fogParameter;

		//ぼかし
		blurResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedWeight));
		*mappedWeight = GaussianWeights(s);

		ImGui::Begin("Window");
		ImGui::DragFloat3("Camera:scale", &cameraTransform.scale.x, 0.01f);
		ImGui::DragFloat3("Camera:rotate", &cameraTransform.rotate.x, 0.01f);
		ImGui::DragFloat3("Camera:translate", &cameraTransform.translate.x, 0.01f);
		ImGui::DragFloat3("plane1:scale", &transform.scale.x, 0.01f);
		ImGui::DragFloat3("plane1:rotate", &transform.rotate.x, 0.01f);
		ImGui::DragFloat3("plane1:translate", &transform.translate.x, 0.01f);
		ImGui::DragFloat3("plane2:scale", &transform2.scale.x, 0.01f);
		ImGui::DragFloat3("plane2:rotate", &transform2.rotate.x, 0.01f);
		ImGui::DragFloat3("plane2:translate", &transform2.translate.x, 0.01f);
		ImGui::DragFloat("fog:nearClip", &fogParameter.nearClip, 0.01f);
		ImGui::DragFloat("fog:farClip", &fogParameter.farClip, 0.01f);
		ImGui::DragFloat("fog:scale", &fogParameter.scale, 0.01f);
		ImGui::DragFloat("fog:attenuationRate", &fogParameter.attenuationRate, 0.01f);
		ImGui::End();

		//描画始まり
		ImGui::Render();
		//一パス目
		directX->FirstPassPreDraw();
		model->Draw(&vertexBufferView, UINT(modelData.vertices.size()), materialResource, transformationMatrixData, lightingResource, nullptr,1);
		model->Draw(&vertexBufferView, UINT(modelData.vertices.size()), materialResource, transformationMatrixData2, lightingResource, nullptr,1);
		model->Draw(&vertexBufferView2, UINT(modelData2.vertices.size()), materialResource, transformationMatrixData3, lightingResource, nullptr, 2);
		directX->FirstPassPostDraw();

		//横ぼかし
		directX->PreHorizontalBlur();
		model->HorizontalBlur(blurResource);
		directX->PostHorizontalBlur();
		//縦ぼかし
		directX->PreVerticalBlur();
		model->VerticalBlur(blurResource);
		directX->PostVerticalBlur();

		//横縮小ぼかし
		directX->PreHorizontalShrinkBlur();
		model->HorizontalShrinkBlur(blurResource);
		directX->PostHorizontalShrinkBlur();
		//縦縮小ぼかし
		directX->PreVerticalShrinkBlur();
		model->VerticalShrinkBlur(blurResource);
		directX->PostVerticalShrinkBlur();

		//最終レンダリング
		directX->PreDraw();
		model->SecondPassDraw(fogResource);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), directX->GetCommandList().Get());
		directX->PostDraw();
	}

	//解放処理
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	delete model;
	delete directX;
	delete winApp;
	CoUninitialize();

	return 0;
}
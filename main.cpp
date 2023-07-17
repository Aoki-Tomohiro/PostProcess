#include <Windows.h>
#include "WinApp.h"
#include "DirectX.h"
#include "Model.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_DX12.h"
#include "D3DResourceLeakChecker.h"

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

	//Sprite用の頂点リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = nullptr;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	VertexData vertexDataSprite[4];
	vertexDataSprite[0].position = { 0.0f,360.0f,0.0f,1.0f };//左下
	vertexDataSprite[0].texcoord = { 0.0f,1.0f };
	vertexDataSprite[0].normal = { 0.0f,0.0f,-1.0f };
	vertexDataSprite[1].position = { 0.0f,0.0f,0.0f,1.0f };//左上
	vertexDataSprite[1].texcoord = { 0.0f,0.0f };
	vertexDataSprite[1].normal = { 0.0f,0.0f,-1.0f };
	vertexDataSprite[2].position = { 640.0f,360.0f,0.0f,1.0f };//右下
	vertexDataSprite[2].texcoord = { 1.0f,1.0f };
	vertexDataSprite[2].normal = { 0.0f,0.0f,-1.0f };
	vertexDataSprite[3].position = { 640.0f,0.0f,0.0f,1.0f };//右上
	vertexDataSprite[3].texcoord = { 1.0f,0.0f };
	vertexDataSprite[3].normal = { 0.0f,0.0f, -1.0f };
	vertexResourceSprite = model->CreateVertexResource(vertexBufferViewSprite, sizeof(vertexDataSprite), vertexDataSprite, 4);

	//マテリアルデータ
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = nullptr;
	Material* materialDataSphere = new Material;
	materialDataSphere->color = { 1.0f,1.0f,1.0f,1.0f };
	materialDataSphere->enableLighting = true;
	materialDataSphere->uvTransform = MakeIdentity4x4();
	materialResource = model->CreateMaterialData(materialDataSphere);
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = nullptr;
	Material* materialDataSprite = new Material;
	materialDataSprite->color = { 1.0f,1.0f,1.0f,1.0f };
	materialDataSprite->enableLighting = false;
	materialDataSprite->uvTransform = MakeIdentity4x4();
	materialResourceSprite = model->CreateMaterialData(materialDataSprite);
	Transform uvTransformSprite{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f},
	};

	//WVP用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixData = model->CreateBufferResource(directX->GetDevice(), sizeof(TransformationMatrix));
	Transform transform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	Transform cameraTransform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,-10.0f} };
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = model->CreateBufferResource(directX->GetDevice(), sizeof(TransformationMatrix));
	Transform transformSprite = { { 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f } };
	bool useMonsterBall = true;

	//Lighting
	Microsoft::WRL::ComPtr<ID3D12Resource> lightingResource = model->CreateBufferResource(directX->GetDevice(), sizeof(DirectionalLight));
	DirectionalLight* directionalLight = nullptr;
	DirectionalLight lightingData = { {1.0f,1.0f,1.0f,1.0f},{0.0f,-1.0f,0.0f},1.0f };
	lightingResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLight));
	*directionalLight = lightingData;

	//IndexResource
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = model->CreateBufferResource(directX->GetDevice(), sizeof(uint32_t) * 6);
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//リソースの先頭のアドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
	//インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0;
	indexDataSprite[1] = 1;
	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1;
	indexDataSprite[4] = 3;
	indexDataSprite[5] = 2;

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = directX->LoadTexture("resource/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_ = directX->CreateTextureResource(directX->GetDevice(), metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource_ = directX->UploadTextureData(textureResource_, mipImages, directX->GetDevice(), directX->GetCommandList());
	directX->CreateShaderResourceView(textureResource_,metadata, 1);
	//2枚目のテクスチャを読んで転送する
	DirectX::ScratchImage mipImages2 = directX->LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2_ = directX->CreateTextureResource(directX->GetDevice(), metadata2);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2_ = directX->UploadTextureData(textureResource2_, mipImages2, directX->GetDevice(), directX->GetCommandList());
	directX->CreateShaderResourceView(textureResource2_, metadata2, 2);


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
		//三角形
		/*transform.rotate.y += 0.03f;*/
		Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
		Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
		Matrix4x4 viewMatrix = Inverse(cameraMatrix);
		Matrix4x4 projectionMatrix = MakePerspectiveFovMatrix(0.45f, float(winApp->kClientWidth) / float(winApp->kClientHeight), 0.1f, 100.0f);
		TransformationMatrix worldViewProjectionMatrix = { Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix)),worldMatrix };
		model->UpdateMatrix(transformationMatrixData, worldViewProjectionMatrix);

		//sprite
		Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
		Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
		Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(winApp->kClientWidth), float(winApp->kClientHeight), 0.0f, 100.0f);
		TransformationMatrix worldViewProjectionMatrixSprite = { Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite)),worldMatrixSprite };
		model->UpdateMatrix(transformationMatrixResourceSprite, worldViewProjectionMatrixSprite);

		//lighting
		lightingResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLight));
		*directionalLight = lightingData;

		//uvTransform
		Matrix4x4 uvTransformMatrix = MakeScaleMatrix(uvTransformSprite.scale);
		uvTransformMatrix = Multiply(uvTransformMatrix, MakeRotateZMatrix(uvTransformSprite.rotate.z));
		uvTransformMatrix = Multiply(uvTransformMatrix, MakeTranslateMatrix(uvTransformSprite.translate));
		materialDataSprite->uvTransform = uvTransformMatrix;
		model->UpdateMaterialData(materialResourceSprite, materialDataSprite);

		ImGui::Begin("Window");
		ImGui::DragFloat3("SphereTranslate", &transform.translate.x, 0.01f);
		ImGui::DragFloat3("SphereRotate", &transform.rotate.x, 0.01f);
		ImGui::DragFloat3("SphereScale", &transform.scale.x, 0.01f);
		ImGui::Checkbox("useMonsterBall", &useMonsterBall);
		ImGui::DragFloat3("spriteTransform", &transformSprite.translate.x, 1.0f);
		ImGui::DragFloat3("cameraScale", &cameraTransform.scale.x, 0.01f);
		ImGui::DragFloat3("cameraRotate", &cameraTransform.rotate.x, 0.01f);
		ImGui::DragFloat3("cameraTranslate", &cameraTransform.translate.x, 0.01f);
		ImGui::DragFloat3("lightDirection", &lightingData.direction.x, 0.01f);
		ImGui::DragFloat2("UVTranslate", &uvTransformSprite.translate.x, 0.01f, -10.0f, 10.0f);
		ImGui::DragFloat2("UVScale", &uvTransformSprite.scale.x, 0.01f, -10.0f, 10.0f);
		ImGui::SliderAngle("UVRotate", &uvTransformSprite.rotate.z);
		ImGui::End();

		//描画始まり
		ImGui::Render();
		//一パス目
		directX->FirstPassPreDraw();
		//オブジェクトの描画
		model->Draw(&vertexBufferView, UINT(modelData.vertices.size()), materialResource, transformationMatrixData, lightingResource, useMonsterBall, nullptr);
		//model->Draw(&vertexBufferViewSprite, 6, materialResourceSprite, transformationMatrixResourceSprite, lightingResource, false, &indexBufferViewSprite);
		directX->FirstPassPostDraw();

		directX->PreDraw();
		model->Draw();
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
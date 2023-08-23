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

	//頂点データ
	Microsoft::WRL::ComPtr<ID3D12Resource> sphereVertexResource = nullptr;
	D3D12_VERTEX_BUFFER_VIEW sphereVertexBufferView{};
	VertexData sphereVertexData[1536];
	const float pi = 3.14f;
	const uint32_t kSubdivision = 16;
	uint32_t latIndex = 0;
	uint32_t lonIndex = 0;
	//経度分割一つ分の角度φd
	const float kLonEvery = pi * 2.0f / float(kSubdivision);
	//緯度分割一つ分の角度θd
	const float kLatEvery = pi / float(kSubdivision);
	//緯度の方向に分割
	for (latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		float lat = -pi / 2.0f + kLatEvery * latIndex;//θ
		//経度の方向に分割しながら線を描く
		for (lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;
			float lon = lonIndex * kLonEvery;//φ
			//頂点にデータを入力する。基準点a
			sphereVertexData[start].position.x = std::cos(lat) * std::cos(lon);
			sphereVertexData[start].position.y = std::sin(lat);
			sphereVertexData[start].position.z = std::cos(lat) * std::sin(lon);
			sphereVertexData[start].position.w = 1.0f;
			sphereVertexData[start].texcoord.x = float(lonIndex) / float(kSubdivision);
			sphereVertexData[start].texcoord.y = 1.0f - float(latIndex) / float(kSubdivision);
			sphereVertexData[start].normal.x = sphereVertexData[start].position.x;
			sphereVertexData[start].normal.y = sphereVertexData[start].position.y;
			sphereVertexData[start].normal.z = sphereVertexData[start].position.z;
			//残りの５頂点も順番に計算して入力していく
			sphereVertexData[start + 1].position.x = std::cos(lat + kLatEvery) * std::cos(lon);
			sphereVertexData[start + 1].position.y = std::sin(lat + kLatEvery);
			sphereVertexData[start + 1].position.z = std::cos(lat + kLatEvery) * std::sin(lon);
			sphereVertexData[start + 1].position.w = 1.0f;
			sphereVertexData[start + 1].texcoord.x = float(lonIndex) / float(kSubdivision);
			sphereVertexData[start + 1].texcoord.y = 1.0f - float(latIndex + 1) / float(kSubdivision);
			sphereVertexData[start + 1].normal.x = sphereVertexData[start + 1].position.x;
			sphereVertexData[start + 1].normal.y = sphereVertexData[start + 1].position.y;
			sphereVertexData[start + 1].normal.z = sphereVertexData[start + 1].position.z;
			sphereVertexData[start + 2].position.x = std::cos(lat) * std::cos(lon + kLonEvery);
			sphereVertexData[start + 2].position.y = std::sin(lat);
			sphereVertexData[start + 2].position.z = std::cos(lat) * std::sin(lon + kLonEvery);
			sphereVertexData[start + 2].position.w = 1.0f;
			sphereVertexData[start + 2].texcoord.x = float(lonIndex + 1) / float(kSubdivision);
			sphereVertexData[start + 2].texcoord.y = 1.0f - float(latIndex) / float(kSubdivision);
			sphereVertexData[start + 2].normal.x = sphereVertexData[start + 2].position.x;
			sphereVertexData[start + 2].normal.y = sphereVertexData[start + 2].position.y;
			sphereVertexData[start + 2].normal.z = sphereVertexData[start + 2].position.z;
			sphereVertexData[start + 3].position.x = std::cos(lat) * std::cos(lon + kLonEvery);
			sphereVertexData[start + 3].position.y = std::sin(lat);
			sphereVertexData[start + 3].position.z = std::cos(lat) * std::sin(lon + kLonEvery);
			sphereVertexData[start + 3].position.w = 1.0f;
			sphereVertexData[start + 3].texcoord.x = float(lonIndex + 1) / float(kSubdivision);
			sphereVertexData[start + 3].texcoord.y = 1.0f - float(latIndex) / float(kSubdivision);
			sphereVertexData[start + 3].normal.x = sphereVertexData[start + 3].position.x;
			sphereVertexData[start + 3].normal.y = sphereVertexData[start + 3].position.y;
			sphereVertexData[start + 3].normal.z = sphereVertexData[start + 3].position.z;
			sphereVertexData[start + 4].position.x = std::cos(lat + kLatEvery) * std::cos(lon);
			sphereVertexData[start + 4].position.y = std::sin(lat + kLatEvery);
			sphereVertexData[start + 4].position.z = std::cos(lat + kLatEvery) * std::sin(lon);
			sphereVertexData[start + 4].position.w = 1.0f;
			sphereVertexData[start + 4].texcoord.x = float(lonIndex) / float(kSubdivision);
			sphereVertexData[start + 4].texcoord.y = 1.0f - float(latIndex + 1) / float(kSubdivision);
			sphereVertexData[start + 4].normal.x = sphereVertexData[start + 4].position.x;
			sphereVertexData[start + 4].normal.y = sphereVertexData[start + 4].position.y;
			sphereVertexData[start + 4].normal.z = sphereVertexData[start + 4].position.z;
			sphereVertexData[start + 5].position.x = std::cos(lat + kLatEvery) * std::cos(lon + kLonEvery);
			sphereVertexData[start + 5].position.y = std::sin(lat + kLatEvery);
			sphereVertexData[start + 5].position.z = std::cos(lat + kLatEvery) * std::sin(lon + kLonEvery);
			sphereVertexData[start + 5].position.w = 1.0f;
			sphereVertexData[start + 5].texcoord.x = float(lonIndex + 1) / float(kSubdivision);
			sphereVertexData[start + 5].texcoord.y = 1.0f - float(latIndex + 1) / float(kSubdivision);
			sphereVertexData[start + 5].normal.x = sphereVertexData[start + 5].position.x;
			sphereVertexData[start + 5].normal.y = sphereVertexData[start + 5].position.y;
			sphereVertexData[start + 5].normal.z = sphereVertexData[start + 5].position.z;
		}
	}
	sphereVertexResource = model->CreateVertexResource(sphereVertexBufferView, sizeof(sphereVertexData), sphereVertexData, 1536);


	//モデル読み込み
	ModelData modelDataSkydome = model->LoadObjFile("resource/skydome", "skydome.obj");
	//頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSkydome = model->CreateBufferResource(directX->GetDevice(), sizeof(VertexData) * modelDataSkydome.vertices.size());
	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSkydome{};
	vertexBufferViewSkydome.BufferLocation = vertexResourceSkydome->GetGPUVirtualAddress();//リソースの先頭のアドレスから使う
	vertexBufferViewSkydome.SizeInBytes = UINT(sizeof(VertexData) * modelDataSkydome.vertices.size());//使用するリソースのサイズは頂点のサイズ
	vertexBufferViewSkydome.StrideInBytes = sizeof(VertexData);
	//頂点リソースにデータを書き込む
	VertexData* vertexDataSkydome = nullptr;
	vertexResourceSkydome->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSkydome));//書き込むためのアドレスを取得
	std::memcpy(vertexDataSkydome, modelDataSkydome.vertices.data(), sizeof(VertexData)* modelDataSkydome.vertices.size());

	ModelData modelDataSphere = model->LoadObjFile("resource/monsterBall", "monsterBall.obj");
	//頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSphere = model->CreateBufferResource(directX->GetDevice(), sizeof(VertexData) * modelDataSphere.vertices.size());
	//頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere{};
	vertexBufferViewSphere.BufferLocation = vertexResourceSphere->GetGPUVirtualAddress();//リソースの先頭のアドレスから使う
	vertexBufferViewSphere.SizeInBytes = UINT(sizeof(VertexData) * modelDataSphere.vertices.size());//使用するリソースのサイズは頂点のサイズ
	vertexBufferViewSphere.StrideInBytes = sizeof(VertexData);
	//頂点リソースにデータを書き込む
	VertexData* vertexDataSphere = nullptr;
	vertexResourceSphere->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSphere));//書き込むためのアドレスを取得
	std::memcpy(vertexDataSphere, modelDataSphere.vertices.data(), sizeof(VertexData) * modelDataSphere.vertices.size());


	//マテリアルデータ
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSkydome = nullptr;
	Material* materialDataSkydome = new Material;
	materialDataSkydome->color = { 1.0f,1.0f,1.0f,1.0f };
	materialDataSkydome->enableLighting = false;
	materialDataSkydome->uvTransform = MakeIdentity4x4();
	materialResourceSkydome = model->CreateMaterialData(materialDataSkydome);
	Transform uvTransformSkydome{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f},
	};

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSphere = nullptr;
	Material* materialDataSphere = new Material;
	materialDataSphere->color = { 1.0f,1.0f,1.0f,1.0f };
	materialDataSphere->enableLighting = true;
	materialDataSphere->uvTransform = MakeIdentity4x4();
	materialResourceSphere = model->CreateMaterialData(materialDataSphere);
	Transform uvTransformSphere{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f},
	};

	//WVP用リソース
	Transform cameraTransform = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,-10.0f} };
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixDataSphere1 = model->CreateBufferResource(directX->GetDevice(), sizeof(TransformationMatrix));
	Transform transformSphere1 = { {1.0f,1.0f,1.0f},{0.0f,-1.5f,0.0f},{-1.5f,0.0f,5.0f}};
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixDataSphere2 = model->CreateBufferResource(directX->GetDevice(), sizeof(TransformationMatrix));
	Transform transformSphere2 = { {1.0f,1.0f,1.0f},{0.0f,-1.5f,0.0f},{1.5f,0.0f,0.0f} };
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixDataSkydome = model->CreateBufferResource(directX->GetDevice(), sizeof(TransformationMatrix));
	Transform transformSkydome = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,-10.0f,50.0f} };

	//Lighting
	Microsoft::WRL::ComPtr<ID3D12Resource> lightingResource = model->CreateBufferResource(directX->GetDevice(), sizeof(DirectionalLight));
	DirectionalLight* directionalLight = nullptr;
	DirectionalLight lightingData = { {1.0f,1.0f,1.0f,1.0f},{0.0f,-1.0f,0.0f},1.0f };
	lightingResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLight));
	*directionalLight = lightingData;

	//Textureを読んで転送する
	DirectX::ScratchImage mipImages = directX->LoadTexture(modelDataSkydome.material.textureFilePath);
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_ = directX->CreateTextureResource(directX->GetDevice(), metadata);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource_ = directX->UploadTextureData(textureResource_, mipImages, directX->GetDevice(), directX->GetCommandList());
	directX->CreateShaderResourceView(textureResource_,metadata, 1);
	//2枚目のテクスチャを読んで転送する
	DirectX::ScratchImage mipImages2 = directX->LoadTexture(modelDataSphere.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2_ = directX->CreateTextureResource(directX->GetDevice(), metadata2);
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource2_ = directX->UploadTextureData(textureResource2_, mipImages2, directX->GetDevice(), directX->GetCommandList());
	directX->CreateShaderResourceView(textureResource2_, metadata2, 2);

	//Fog
	Microsoft::WRL::ComPtr<ID3D12Resource> fogResource = model->CreateBufferResource(directX->GetDevice(), sizeof(FogParameter));
	FogParameter fogParameter = { false,0.1f,30.0f, 0.5f,1.0f };

	//DoF
	Microsoft::WRL::ComPtr<ID3D12Resource> dofResource = model->CreateBufferResource(directX->GetDevice(), sizeof(FogParameter));
	DofParameter dofParameter = { false };

	//LensDistortion
	Microsoft::WRL::ComPtr<ID3D12Resource> lensDistortionResource = model->CreateBufferResource(directX->GetDevice(), sizeof(LensDistortionParameter));
	LensDistortionParameter lensDistortionParameter = { false,2.5f,-0.1f };

	//Vignette
	Microsoft::WRL::ComPtr<ID3D12Resource> vignetteResource = model->CreateBufferResource(directX->GetDevice(), sizeof(VignetteParameter));
	VignetteParameter vignetteParameter = { false,1.0f };

	//ぼかし
	Microsoft::WRL::ComPtr<ID3D12Resource> blurResource = model->CreateBufferResource(directX->GetDevice(), sizeof(Weights));
	Weights* mappedWeight = nullptr;
	float s = 5.0f;


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

		//sphere1
		Matrix4x4 worldMatrixSphere1 = MakeAffineMatrix(transformSphere1.scale, transformSphere1.rotate, transformSphere1.translate);
		TransformationMatrix worldViewProjectionMatrixSphere1 = { Multiply(worldMatrixSphere1, Multiply(viewMatrix, projectionMatrix)),worldMatrixSphere1 };
		model->UpdateMatrix(transformationMatrixDataSphere1, worldViewProjectionMatrixSphere1);

		//sphere2
		Matrix4x4 worldMatrixSphere2 = MakeAffineMatrix(transformSphere2.scale, transformSphere2.rotate, transformSphere2.translate);
		TransformationMatrix worldViewProjectionMatrixSphere2 = { Multiply(worldMatrixSphere2, Multiply(viewMatrix, projectionMatrix)),worldMatrixSphere2 };
		model->UpdateMatrix(transformationMatrixDataSphere2, worldViewProjectionMatrixSphere2);

		//天球
		Matrix4x4 worldMatrixSkydome = MakeAffineMatrix(transformSkydome.scale, transformSkydome.rotate, transformSkydome.translate);
		TransformationMatrix worldViewProjectionMatrixSkydome = { Multiply(worldMatrixSkydome, Multiply(viewMatrix, projectionMatrix)),worldMatrixSkydome };
		model->UpdateMatrix(transformationMatrixDataSkydome, worldViewProjectionMatrixSkydome);

		//uvTransform
		Matrix4x4 uvTransformMatrixSkydome = MakeScaleMatrix(uvTransformSkydome.scale);
		uvTransformMatrixSkydome = Multiply(uvTransformMatrixSkydome, MakeRotateZMatrix(uvTransformSkydome.rotate.z));
		uvTransformMatrixSkydome = Multiply(uvTransformMatrixSkydome, MakeTranslateMatrix(uvTransformSkydome.translate));
		materialDataSkydome->uvTransform = uvTransformMatrixSkydome;
		model->UpdateMaterialData(materialResourceSkydome, materialDataSkydome);

		Matrix4x4 uvTransformMatrixSphere = MakeScaleMatrix(uvTransformSphere.scale);
		uvTransformMatrixSphere = Multiply(uvTransformMatrixSphere, MakeRotateZMatrix(uvTransformSphere.rotate.z));
		uvTransformMatrixSphere = Multiply(uvTransformMatrixSphere, MakeTranslateMatrix(uvTransformSphere.translate));
		materialDataSphere->uvTransform = uvTransformMatrixSphere;
		model->UpdateMaterialData(materialResourceSphere, materialDataSphere);

		//ぼかし
		blurResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedWeight));
		*mappedWeight = GaussianWeights(s);

		//FogParameter
		FogParameter* fog = nullptr;
		fogResource->Map(0, nullptr, reinterpret_cast<void**>(&fog));
		fog->enable = fogParameter.enable;
		fog->start = fogParameter.start;
		fog->end = fogParameter.end;
		fog->scale = fogParameter.scale;
		fog->attenuationRate = fogParameter.attenuationRate;

		//DofParameter
		DofParameter* dof = nullptr;
		dofResource->Map(0, nullptr, reinterpret_cast<void**>(&dof));
		dof->enable = dofParameter.enable;

		//LensDistortion
		LensDistortionParameter* lensDistortion = nullptr;
		lensDistortionResource->Map(0, nullptr, reinterpret_cast<void**>(&lensDistortion));
		lensDistortion->enable = lensDistortionParameter.enable;
		lensDistortion->tightness = lensDistortionParameter.tightness;
		lensDistortion->strength = lensDistortionParameter.strength;

		//Vignette
		VignetteParameter* vignette = nullptr;
		vignetteResource->Map(0, nullptr, reinterpret_cast<void**>(&vignette));
		vignette->enable = vignetteParameter.enable;
		vignette->intensity = vignetteParameter.intensity;


		ImGui::Begin(" ");
		ImGui::DragFloat3("Sphere1:translate", &transformSphere1.translate.x, 0.01f);
		ImGui::DragFloat3("Sphere2:translate", &transformSphere2.translate.x, 0.01f);
		ImGui::DragFloat("weight:s", &s, 0.01f);
		ImGui::Checkbox("fog:enable", &fogParameter.enable);
		ImGui::DragFloat("fog:start", &fogParameter.start, 0.01f);
		ImGui::DragFloat("fog:end", &fogParameter.end, 0.01f);
		ImGui::DragFloat("fog:scale", &fogParameter.scale, 0.01f);
		ImGui::DragFloat("fog:attenuationRate", &fogParameter.attenuationRate, 0.01f);
		ImGui::Checkbox("dof:enable", &dofParameter.enable);
		ImGui::Checkbox("lensDistortion:enable", &lensDistortionParameter.enable);
		ImGui::DragFloat("lensDistortion:tightness", &lensDistortionParameter.tightness, 0.01f);
		ImGui::DragFloat("lensDistortion:strength", &lensDistortionParameter.strength, 0.01f, -0.1f, 0.1f);
		ImGui::Checkbox("vignette:enable", &vignetteParameter.enable);
		ImGui::DragFloat("vignette:intensity", &vignetteParameter.intensity, 0.01f);
		ImGui::End();

		//描画始まり
		ImGui::Render();
		//一パス目
		directX->FirstPassPreDraw();
		model->Draw(&sphereVertexBufferView, 1536, materialResourceSphere, transformationMatrixDataSphere1, lightingResource, nullptr,2, fogResource);
		model->Draw(&sphereVertexBufferView, 1536, materialResourceSphere, transformationMatrixDataSphere2, lightingResource, nullptr,2, fogResource);
		model->Draw(&vertexBufferViewSkydome, UINT(modelDataSkydome.vertices.size()), materialResourceSkydome, transformationMatrixDataSkydome, lightingResource, nullptr, 1, fogResource);
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
		model->SecondPassDraw(fogResource, dofResource, lensDistortionResource, vignetteResource);
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
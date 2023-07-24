#include "Model.h"
#include <fstream>
#include <sstream>

Model::~Model() {
	signatureBlob_->Release();
	if (errorBlob_) {
		errorBlob_->Release();
	}
	pixelShaderBlob_->Release();
	vertexShaderBlob_->Release();
}

void Model::Initialize(DirectXCommon* directX) {
	directX_ = directX;
	Model::InitializeDXCCompiler();
	Model::CreatePipelineStateObject();
	Model::CreateViewport();
	Model::CreateScissorRect();
	//マルチパスレンダリング
	vertexData[0] = { {-1.0f,-1.0f,0.0,1.0f},{0.0f,1.0f},{-1.0f,-1.0f,0.0f} };
	vertexData[1] = { {-1.0f,1.0f,0.0f,1.0f},{0.0f,0.0f},{-1.0f,1.0f,0.0f} };
	vertexData[2] = { {1.0f,-1.0f,0.0f,1.0f},{1.0f,1.0f},{1.0f,-1.0f,0.0f} };
	vertexData[3] = { {1.0f,1.0f,0.0f,1.0f},{1.0f,0.0f},{1.0f,1.0f,0.0f} };
	vertexResource_ = Model::CreateVertexResource(vertexBufferView_, sizeof(VertexData) * 4, vertexData, 4);
	Model::CreateSecondPassPipelineStateObject();
	Model::CreateBlurPipelineStateObject();
}

void Model::InitializeDXCCompiler() {
	HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr));

	//現時点でincludeはしないが、includeに対応するための設定を行っていく
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr));
}

IDxcBlob* Model::CompileShader(const std::wstring& filePath, const wchar_t* profile,
	IDxcUtils* dxcUtils, IDxcCompiler3* dxcCompiler, IDxcIncludeHandler* includeHandler) {
	//1.hlslファイルを読む
	//これからシェーダーをコンパイルする旨をログに出す
	directX_->GetWinApp()->Log(directX_->GetWinApp()->ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	//hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	//読めなかったら止める
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;//UTF8の文字コードであることを通知


	//2.Compileする
	LPCWSTR arguments[] = {
		filePath.c_str(),//コンパイル対象のhlslファイル名
		L"-E",L"main",//エントリーポイントの指定。基本的にmain以外にはしない
		L"-T",profile,//ShaderProfileの設定
		L"-Zi",L"-Qembed_debug",//デバッグ用の情報を埋め込む
		L"-Od",//最適化を外しておく
		L"-Zpr",//メモリレイアウトは行優先
	};
	//実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,//読み込んだファイル
		arguments,//コンパイルオプション
		_countof(arguments),//コンパイルオプションの数
		includeHandler,//includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult)//コンパイル結果
	);
	//コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));


	//3.警告・エラーが出ていないか確認する
	//警告・エラーが出てたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		directX_->GetWinApp()->Log(shaderError->GetStringPointer());
		//警告・エラーダメゼッタイ
		assert(false);
	}


	//4.Compile結果を受け取って返す
	//コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	//成功したログを出す
	directX_->GetWinApp()->Log(directX_->GetWinApp()->ConvertString(std::format(L"Compile Succeeded,path:{},profile:{}\n", filePath, profile)));
	//もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();
	//実行用のバイナリを返却
	return shaderBlob;
}

void Model::CreatePipelineStateObject() {
	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//DescriptorRange
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;//0から始まる
	descriptorRange[0].NumDescriptors = 1;//数は一つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;//Offsetを自動計算

	//RootParameter作成。複数設定できるので配列。今回は結果一つだけなので長さ１の配列
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderを使う
	rootParameters[0].Descriptor.ShaderRegister = 0;//レジスタ番号０とバインド
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;//VertexShaderを使う
	rootParameters[1].Descriptor.ShaderRegister = 0;//レジスタ番号０を使う
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;//Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);//Tableで利用する数
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1;//レジスタ番号１を使う
	descriptionRootSignature.pParameters = rootParameters;//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ

	//Sampler
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;//ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;//レジスタ番号０を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	//シリアライズしてバイナリにする
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		directX_->GetWinApp()->Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	hr = directX_->GetDevice()->CreateRootSignature(0, signatureBlob_->GetBufferPointer(),
		signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));


	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);


	//BlendStateの設定
	//すべての色要素を書き込む
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


	//RasterizerStateの設定を行う
	//裏面(時計回り)を表示しない
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


	//Shaderをコンパイルする
	vertexShaderBlob_ = CompileShader(L"shader/Object3d.VS.hlsl",
		L"vs_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(vertexShaderBlob_ != nullptr);

	pixelShaderBlob_ = CompileShader(L"shader/Object3d.PS.hlsl",
		L"ps_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(pixelShaderBlob_ != nullptr);


	//DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	//Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;


	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();//RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;//InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(),
	vertexShaderBlob_->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(),
	pixelShaderBlob_->GetBufferSize() };//PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;//BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;//RasterizerState
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	/*graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;*/
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 3;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	graphicsPipelineStateDesc.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//利用するトポロジ(形状)のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むかの設定(気にしなくて良い)
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	//実際に生成
	hr = directX_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<ID3D12Resource> Model::CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes) {
	//頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;//UploadHeapを使う
	//頂点リソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	//バッファリソース。テクスチャの場合はまた別の設定をする
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	//バッファの場合はこれらは１にする決まり
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	//バッファの場合はこれにする決まり
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	//実際に頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Model::CreateVertexResource(D3D12_VERTEX_BUFFER_VIEW& vertexBufferView, UINT sizeInBytes, VertexData* vertexData, uint32_t vertexCount) {
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	//vertexBufferViewを作成
	vertexResource = CreateBufferResource(directX_->GetDevice(), sizeInBytes);

	//リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	//使用するリソースのサイズは頂点三つ分のサイズ
	vertexBufferView.SizeInBytes = sizeInBytes;
	//１頂点当たりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	VertexData* vertexData_ = nullptr;
	//書き込むためのアドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	for (uint32_t i = 0; i < vertexCount; i++) {
		vertexData_[i].position = vertexData[i].position;
		vertexData_[i].texcoord = vertexData[i].texcoord;
		vertexData_[i].normal = vertexData[i].normal;
	}
	return vertexResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Model::CreateMaterialData(Material* color) {
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = nullptr;
	materialResource = CreateBufferResource(directX_->GetDevice(), sizeof(Material));

	//マテリアルにデータを書き込む
	Material* materialData = nullptr;
	//書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	//今回は赤を書き込んでみる
	*materialData = *color;

	return materialResource;
}

void Model::UpdateMaterialData(const Microsoft::WRL::ComPtr<ID3D12Resource>& materialResource, Material* materialData) {
	Material* material = nullptr;
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&material));
	*material = *materialData;
}

void Model::UpdateMatrix(const Microsoft::WRL::ComPtr<ID3D12Resource>& WVPResource, TransformationMatrix matrix) {
	//データを書き込む
	TransformationMatrix* wvpData = nullptr;
	//書き込むためのアドレスを取得
	WVPResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));
	//単位行列を書き込んでおく
	*wvpData = matrix;
}

ModelData Model::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
	ModelData modelData;//構築するModelData
	std::vector<Vector4> positions;//位置
	std::vector<Vector3> normals;//法線
	std::vector<Vector2> texcoords;//テクスチャ座標
	std::string line;//ファイルから読んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;//先頭の識別子を読む

		//identifierに応じた処理
		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.z *= -1.0f;
			position.w = 1.0f;
			positions.push_back(position);
		}
		else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoord.y = 1.0f - texcoord.y;
			texcoords.push_back(texcoord);
		}
		else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normal.z *= -1.0f;
			normals.push_back(normal);
		}
		else if (identifier == "f") {
			VertexData triangle[3];
			//面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				//頂点の要素へのIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');// /区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				//要素へのIndexから、実際の要素の値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				triangle[faceVertex] = { position,texcoord,normal };
			}
			//頂点を逆順で登録することで、回り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		}
		else if (identifier == "mtllib") {
			//materialTempalteLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			//基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
}

MaterialData Model::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData;//構築するMaterialData
	std::string line;//ファイルから読んだ1行を格納するもの
	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//とりあえず開けなかったら止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		//identifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			//連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

void Model::CreateViewport() {
	//クライアント領域のサイズと一緒にして画面全体に表示
	viewport_.Width = float(directX_->GetWinApp()->kClientWidth);
	viewport_.Height = float(directX_->GetWinApp()->kClientHeight);
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
}

void Model::CreateScissorRect() {
	//基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect_.left = 0;
	scissorRect_.right = directX_->GetWinApp()->kClientWidth;
	scissorRect_.top = 0;
	scissorRect_.bottom = directX_->GetWinApp()->kClientHeight;
}

void Model::Draw(D3D12_VERTEX_BUFFER_VIEW* vertexBufferView, UINT vertexCount, const Microsoft::WRL::ComPtr<ID3D12Resource>& materialResource, const Microsoft::WRL::ComPtr<ID3D12Resource>& WVPResource, const Microsoft::WRL::ComPtr<ID3D12Resource>& lightingResource, D3D12_INDEX_BUFFER_VIEW* indexBufferView, uint32_t textureIndex) {
	//GPUハンドルを取得
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = directX_->GetGPUDescriptorHandle(directX_->GetSRVDescriptorHeap(), directX_->descriptorSizeSRV, textureIndex);

	directX_->GetCommandList()->RSSetViewports(1, &viewport_);//viewportを設定
	directX_->GetCommandList()->RSSetScissorRects(1, &scissorRect_);//Scissorを設定
	//RootSignatureを設定。PSOに設定しているけど別途設定が必要
	directX_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
	directX_->GetCommandList()->SetPipelineState(graphicsPipelineState_.Get());//PSOを設定
	directX_->GetCommandList()->IASetVertexBuffers(0, 1, vertexBufferView);//VBVを設定
	if (indexBufferView != nullptr) {
		directX_->GetCommandList()->IASetIndexBuffer(indexBufferView);//IBVを設定
	}
	directX_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
	directX_->GetCommandList()->SetGraphicsRootConstantBufferView(3, lightingResource->GetGPUVirtualAddress());
	directX_->GetCommandList()->SetGraphicsRootConstantBufferView(1, WVPResource->GetGPUVirtualAddress());
	//SRVのDescriptorTableの先頭を設定。２はrootParameter[2]である。
	directX_->GetCommandList()->SetGraphicsRootDescriptorTable(2, srvHandle);
	//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
	directX_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	//描画！(DrawCall/ドローコール)。３頂点で一つのインスタンス、インスタンスについては今後
	if (indexBufferView != nullptr) {
		directX_->GetCommandList()->DrawIndexedInstanced(vertexCount, 1, 0, 0, 0);
	}
	else {
		directX_->GetCommandList()->DrawInstanced(vertexCount, 1, 0, 0);
	}
}

void Model::CreateSecondPassPipelineStateObject() {
	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//ディスクリプタレンジ
	D3D12_DESCRIPTOR_RANGE descriptorRange[3]{};
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[1].BaseShaderRegister = 1;
	descriptorRange[1].NumDescriptors = 1;
	descriptorRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[2].BaseShaderRegister = 2;
	descriptorRange[2].NumDescriptors = 4;

	//RootParameter作成。複数設定できるので配列。今回は結果一つだけなので長さ１の配列
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange[0];
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRange[1];
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRange[2];
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderを使う
	rootParameters[3].Descriptor.ShaderRegister = 0;//レジスタ番号０とバインド
	descriptionRootSignature.pParameters = rootParameters;//ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ

	//Sampler
	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイリニアフィルタ
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//０～１の範囲外をリピート
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	sampler.MaxLOD = D3D12_FLOAT32_MAX;//ありったけのMipmapを使う
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	descriptionRootSignature.NumParameters = _countof(rootParameters);
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumStaticSamplers = 1;
	descriptionRootSignature.pStaticSamplers = &sampler;

	//シリアライズしてバイナリにする
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		directX_->GetWinApp()->Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	hr = directX_->GetDevice()->CreateRootSignature(0, signatureBlob_->GetBufferPointer(),
		signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&secondPassRootSignature_));
	assert(SUCCEEDED(hr));


	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);


	//BlendStateの設定
	//すべての色要素を書き込む
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;


	//RasterizerStateの設定を行う
	//裏面(時計回り)を表示しない
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;


	//Shaderをコンパイルする
	vertexShaderBlob_ = CompileShader(L"shader/SecondPassVS.hlsl",
		L"vs_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(vertexShaderBlob_ != nullptr);

	pixelShaderBlob_ = CompileShader(L"shader/SecondPassPS.hlsl",
		L"ps_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(pixelShaderBlob_ != nullptr);


	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = secondPassRootSignature_.Get();//RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;//InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(),
	vertexShaderBlob_->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(),
	pixelShaderBlob_->GetBufferSize() };//PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;//BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;//RasterizerState
	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//利用するトポロジ(形状)のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むかの設定(気にしなくて良い)
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	//実際に生成
	hr = directX_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&secondPassGraphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

void Model::SecondPassDraw(const Microsoft::WRL::ComPtr<ID3D12Resource>& fogResource) {
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandles[3];
	srvHandles[0] = directX_->GetGPUDescriptorHandle(directX_->GetSRVDescriptorHeap(), directX_->descriptorSizeSRV, 3);
	srvHandles[1] = directX_->GetGPUDescriptorHandle(directX_->GetSRVDescriptorHeap(), directX_->descriptorSizeSRV, 5);
	srvHandles[2] = directX_->GetGPUDescriptorHandle(directX_->GetSRVDescriptorHeap(), directX_->descriptorSizeSRV, 7);

	directX_->GetCommandList()->RSSetViewports(1, &viewport_);//viewportを設定
	directX_->GetCommandList()->RSSetScissorRects(1, &scissorRect_);//Scissorを設定
	//RootSignatureを設定。PSOに設定しているけど別途設定が必要
	directX_->GetCommandList()->SetGraphicsRootSignature(secondPassRootSignature_.Get());
	directX_->GetCommandList()->SetPipelineState(secondPassGraphicsPipelineState_.Get());//PSOを設定
	directX_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);//VBVを設定
	directX_->GetCommandList()->SetGraphicsRootConstantBufferView(3, fogResource->GetGPUVirtualAddress());
	//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
	directX_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	//SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である
	directX_->GetCommandList()->SetGraphicsRootDescriptorTable(0, srvHandles[0]);
	directX_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvHandles[1]);
	directX_->GetCommandList()->SetGraphicsRootDescriptorTable(2, srvHandles[2]);
	//描画！(DrawCall/ドローコール)。３頂点で一つのインスタンス、インスタンスについては今後
	directX_->GetCommandList()->DrawInstanced(4, 1, 0, 0);
}

void Model::CreateBlurPipelineStateObject() {
	//RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//DescriptorRange
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;//0から始まる
	descriptorRange[0].NumDescriptors = 1;//数は一つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;//SRVを使う

	//RootParameter作成。複数設定できるので配列。今回は結果一つだけなので長さ一の配列
	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;//CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//VertexShaderを使う
	rootParameters[0].Descriptor.ShaderRegister = 0;//レジスタ番号０を使う
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;//DescriptorTableを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRange;//Tableの中身の配列を指定
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);//Tableで利用する数
	descriptionRootSignature.pParameters = rootParameters;//ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);//配列の長さ

	//Sampler
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;//バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;//０～１の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;//比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;//ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;//PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	//シリアライズしてバイナリにする
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob_, &errorBlob_);
	if (FAILED(hr)) {
		directX_->GetWinApp()->Log(reinterpret_cast<char*>(errorBlob_->GetBufferPointer()));
		assert(false);
	}
	//バイナリを元に生成
	hr = directX_->GetDevice()->CreateRootSignature(0, signatureBlob_->GetBufferPointer(),
		signatureBlob_->GetBufferSize(), IID_PPV_ARGS(&blurRootSignature_));
	assert(SUCCEEDED(hr));

	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//BlendStateの設定
	//すべての色要素を書き込む
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//RasterizerStateの設定を行う
	//裏面(時計回り)を表示しない
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//Shaderをコンパイルする
	vertexShaderBlob_ = CompileShader(L"Shader/BlurVS.hlsl",
		L"vs_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(vertexShaderBlob_ != nullptr);

	pixelShaderBlob_ = CompileShader(L"Shader/HorizontalBlurPS.hlsl",
		L"ps_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(pixelShaderBlob_ != nullptr);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = blurRootSignature_.Get();//RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;//InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(),
	vertexShaderBlob_->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(),
	pixelShaderBlob_->GetBufferSize() };//PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;//BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;//RasterizerState
	//書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//利用するトポロジ(形状)のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	//どのように画面に色を打ち込むかの設定(気にしなくて良い)
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	//実際に生成
	hr = directX_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&horizontalBlurGraphicsPipelineState_));
	assert(SUCCEEDED(hr));

	//縦ぼかし
	vertexShaderBlob_ = CompileShader(L"Shader/BlurVS.hlsl",
		L"vs_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(vertexShaderBlob_ != nullptr);

	pixelShaderBlob_ = CompileShader(L"Shader/VerticalBlurPS.hlsl",
		L"ps_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(pixelShaderBlob_ != nullptr);

	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(),
	vertexShaderBlob_->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(),
	pixelShaderBlob_->GetBufferSize() };//PixelShader

	hr = directX_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&verticalBlurGraphicsPipelineState_));
	assert(SUCCEEDED(hr));


	//縮小横ぼかし
	vertexShaderBlob_ = CompileShader(L"Shader/BlurVS.hlsl",
		L"vs_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(vertexShaderBlob_ != nullptr);

	pixelShaderBlob_ = CompileShader(L"Shader/HorizontalShrinkBlurPS.hlsl",
		L"ps_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(pixelShaderBlob_ != nullptr);

	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(),
	vertexShaderBlob_->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(),
	pixelShaderBlob_->GetBufferSize() };//PixelShader

	hr = directX_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&horizontalShrinkBlurGraphicsPipelineState_));
	assert(SUCCEEDED(hr));


	//縮小縦ぼかし
	vertexShaderBlob_ = CompileShader(L"Shader/BlurVS.hlsl",
		L"vs_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(vertexShaderBlob_ != nullptr);

	pixelShaderBlob_ = CompileShader(L"Shader/VerticalShrinkBlurPS.hlsl",
		L"ps_6_0", dxcUtils_, dxcCompiler_, includeHandler_);
	assert(pixelShaderBlob_ != nullptr);

	graphicsPipelineStateDesc.VS = { vertexShaderBlob_->GetBufferPointer(),
	vertexShaderBlob_->GetBufferSize() };//VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob_->GetBufferPointer(),
	pixelShaderBlob_->GetBufferSize() };//PixelShader

	hr = directX_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&verticalShrinkBlurGraphicsPipelineState_));
	assert(SUCCEEDED(hr));
}

void Model::HorizontalBlur(const Microsoft::WRL::ComPtr<ID3D12Resource> bkResource) {
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = directX_->GetGPUDescriptorHandle(directX_->GetSRVDescriptorHeap(), directX_->descriptorSizeSRV, 3);
	directX_->GetCommandList()->RSSetViewports(1, &viewport_);//viewportを設定
	directX_->GetCommandList()->RSSetScissorRects(1, &scissorRect_);//Scissorを設定
	//RootSignatureを設定。PSOに設定しているけど別途設定が必要
	directX_->GetCommandList()->SetGraphicsRootSignature(blurRootSignature_.Get());
	directX_->GetCommandList()->SetPipelineState(horizontalBlurGraphicsPipelineState_.Get());//PSOを設定
	directX_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);//VBVを設定
	//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
	directX_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	directX_->GetCommandList()->SetGraphicsRootConstantBufferView(0, bkResource->GetGPUVirtualAddress());
	//SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である
	directX_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvHandle);
	//描画！(DrawCall/ドローコール)。３頂点で一つのインスタンス、インスタンスについては今後
	directX_->GetCommandList()->DrawInstanced(4, 1, 0, 0);
}

void Model::VerticalBlur(const Microsoft::WRL::ComPtr<ID3D12Resource> bkResource) {
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = directX_->GetGPUDescriptorHandle(directX_->GetSRVDescriptorHeap(), directX_->descriptorSizeSRV, 4);
	directX_->GetCommandList()->RSSetViewports(1, &viewport_);//viewportを設定
	directX_->GetCommandList()->RSSetScissorRects(1, &scissorRect_);//Scissorを設定
	//RootSignatureを設定。PSOに設定しているけど別途設定が必要
	directX_->GetCommandList()->SetGraphicsRootSignature(blurRootSignature_.Get());
	directX_->GetCommandList()->SetPipelineState(verticalBlurGraphicsPipelineState_.Get());//PSOを設定
	directX_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);//VBVを設定
	//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
	directX_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	directX_->GetCommandList()->SetGraphicsRootConstantBufferView(0, bkResource->GetGPUVirtualAddress());
	//SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である
	directX_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvHandle);
	//描画！(DrawCall/ドローコール)。３頂点で一つのインスタンス、インスタンスについては今後
	directX_->GetCommandList()->DrawInstanced(4, 1, 0, 0);
}

void  Model::HorizontalShrinkBlur(const Microsoft::WRL::ComPtr<ID3D12Resource> bkResource) {
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = directX_->GetGPUDescriptorHandle(directX_->GetSRVDescriptorHeap(), directX_->descriptorSizeSRV, 3);
	D3D12_VIEWPORT viewport{};
	viewport.Height = float(directX_->GetWinApp()->kClientHeight) / 2;
	viewport.Width = float(directX_->GetWinApp()->kClientWidth) / 2;
	directX_->GetCommandList()->RSSetViewports(1, &viewport);//viewportを設定
	directX_->GetCommandList()->RSSetScissorRects(1, &scissorRect_);//Scissorを設定
	//RootSignatureを設定。PSOに設定しているけど別途設定が必要
	directX_->GetCommandList()->SetGraphicsRootSignature(blurRootSignature_.Get());
	directX_->GetCommandList()->SetPipelineState(horizontalShrinkBlurGraphicsPipelineState_.Get());//PSOを設定
	directX_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);//VBVを設定
	//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
	directX_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	directX_->GetCommandList()->SetGraphicsRootConstantBufferView(0, bkResource->GetGPUVirtualAddress());
	//SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である
	directX_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvHandle);
	//描画！(DrawCall/ドローコール)。３頂点で一つのインスタンス、インスタンスについては今後
	directX_->GetCommandList()->DrawInstanced(4, 1, 0, 0);
}

void Model::VerticalShrinkBlur(const Microsoft::WRL::ComPtr<ID3D12Resource> bkResource) {
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = directX_->GetGPUDescriptorHandle(directX_->GetSRVDescriptorHeap(), directX_->descriptorSizeSRV, 6);
	D3D12_VIEWPORT viewport{};
	viewport.Height = float(directX_->GetWinApp()->kClientHeight) / 2;
	viewport.Width = float(directX_->GetWinApp()->kClientWidth) / 2;
	directX_->GetCommandList()->RSSetViewports(1, &viewport);//viewportを設定
	directX_->GetCommandList()->RSSetScissorRects(1, &scissorRect_);//Scissorを設定
	//RootSignatureを設定。PSOに設定しているけど別途設定が必要
	directX_->GetCommandList()->SetGraphicsRootSignature(blurRootSignature_.Get());
	directX_->GetCommandList()->SetPipelineState(verticalShrinkBlurGraphicsPipelineState_.Get());//PSOを設定
	directX_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView_);//VBVを設定
	//形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
	directX_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	directX_->GetCommandList()->SetGraphicsRootConstantBufferView(0, bkResource->GetGPUVirtualAddress());
	//SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である
	directX_->GetCommandList()->SetGraphicsRootDescriptorTable(1, srvHandle);
	//描画！(DrawCall/ドローコール)。３頂点で一つのインスタンス、インスタンスについては今後
	directX_->GetCommandList()->DrawInstanced(4, 1, 0, 0);
}
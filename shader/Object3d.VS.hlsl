#include "Object3d.hlsli"

struct TransformationMatrix {
	float32_t4x4 WVP;
	float32_t4x4 World;
};

struct Fog {
	bool enable;
	float start;
	float end;
	float scale;
	float attenuationRate;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<Fog> gFogParameter : register(b1);

struct VertexShaderInput {
	float32_t4 position : POSITION0;
	float32_t2 texcoord : TEXCOORD0;
	float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input) {
	VertexShaderOutput output;
	output.position = mul(input.position, gTransformationMatrix.WVP);
	output.texcoord = input.texcoord;
	output.normal = normalize(mul(input.normal, (float32_t3x3)gTransformationMatrix.World));

	//線形深度
	float z = (output.position.z - gFogParameter.start) / (gFogParameter.end - gFogParameter.start);
	output.depth = float32_t4(z, 0.0f, 0.0f, 1.0f);

	return output;
}
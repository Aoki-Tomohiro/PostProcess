#include "Object3d.hlsli"

struct TransformationMatrix {
	float32_t4x4 WVP;
	float32_t4x4 World;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput {
	float32_t4 position : POSITION0;
	float32_t2 texcoord : TEXCOORD0;
	float32_t3 normal : NORMAL0;
};

float4 convRGBA(float depth) {
	float r = depth;
	float g = frac(r * 255.0f);
	float b = frac(g * 255.0f);
	float a = frac(g * 255.0f);
	float coef = 1.0f / 255.0f;
	r -= g * coef;
	g -= b * coef;
	b -= a * coef;
	return float4(r, g, b, a);
}

float convCoord(float depth, float offset) {
	float d = clamp(depth + offset, 0.0f, 1.0f);
	if (d > 0.6) {
		d = 2.5 * (1.0 - d);
	}
	else if (d > 0.4f) {
		d = 1.0f;
	}
	else {
		d *= 2.5f;
	}
	return d;
}

VertexShaderOutput main(VertexShaderInput input) {
	VertexShaderOutput output;
	output.position = mul(input.position, gTransformationMatrix.WVP);
	output.texcoord = input.texcoord;
	output.normal = normalize(mul(input.normal, (float32_t3x3)gTransformationMatrix.World));

	//線形深度
	float z = (output.position.z - 0.1f) / (100.0f - 0.1f);
	output.linerDepth = float32_t4(z, 0.0f, 0.0f, 1.0f);

	//dof深度
	const float near = 0.1f;
	const float far = 30.0f;
	const float linerDepth = 1.0f / (far - near);
	float liner = linerDepth * length(output.position);
	float4 convColor = convRGBA(convCoord(liner, 0.00001f));
	output.dofDepth = convColor;
	return output;
}
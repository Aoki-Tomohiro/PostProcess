#include "Object3d.hlsli"

struct PixelShaderOutput {
	float4 highLum : SV_TARGET0;//高輝度
};

struct Weights {
	float4 bkWeight[2];
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

ConstantBuffer<Weights> gWeights : register(b0);

PixelShaderOutput main(VertexShaderOutput input) {
	PixelShaderOutput output;
	output.highLum = float4(0, 0, 0, 0);
	float4 color = gTexture.Sample(gSampler, input.texcoord);
	float w, h, level;
	//画像の大きさを取得
	gTexture.GetDimensions(0, w, h, level);
	//縮小バッファのサイズに合わせる
	w /= 2;
	h /= 2;
	float dx = 1.0f / w;
	float dy = 1.0f / h;

	output.highLum += gWeights.bkWeight[0] * color;
	output.highLum += gWeights.bkWeight[0][1] * gTexture.Sample(gSampler, input.texcoord + float2(dx * 1, 0));
	output.highLum += gWeights.bkWeight[0][1] * gTexture.Sample(gSampler, input.texcoord + float2(-dx * 1, 0));
	output.highLum += gWeights.bkWeight[0][2] * gTexture.Sample(gSampler, input.texcoord + float2(dx * 2, 0));
	output.highLum += gWeights.bkWeight[0][2] * gTexture.Sample(gSampler, input.texcoord + float2(-dx * 2, 0));
	output.highLum += gWeights.bkWeight[0][3] * gTexture.Sample(gSampler, input.texcoord + float2(dx * 3, 0));
	output.highLum += gWeights.bkWeight[0][3] * gTexture.Sample(gSampler, input.texcoord + float2(-dx * 3, 0));
	output.highLum += gWeights.bkWeight[1][0] * gTexture.Sample(gSampler, input.texcoord + float2(dx * 4, 0));
	output.highLum += gWeights.bkWeight[1][0] * gTexture.Sample(gSampler, input.texcoord + float2(-dx * 4, 0));
	output.highLum += gWeights.bkWeight[1][1] * gTexture.Sample(gSampler, input.texcoord + float2(dx * 5, 0));
	output.highLum += gWeights.bkWeight[1][1] * gTexture.Sample(gSampler, input.texcoord + float2(-dx * 5, 0));
	output.highLum += gWeights.bkWeight[1][2] * gTexture.Sample(gSampler, input.texcoord + float2(dx * 6, 0));
	output.highLum += gWeights.bkWeight[1][2] * gTexture.Sample(gSampler, input.texcoord + float2(-dx * 6, 0));
	output.highLum += gWeights.bkWeight[1][3] * gTexture.Sample(gSampler, input.texcoord + float2(dx * 7, 0));
	output.highLum += gWeights.bkWeight[1][3] * gTexture.Sample(gSampler, input.texcoord + float2(-dx * 7, 0));
	output.highLum.a = color.a;

	return output;
}
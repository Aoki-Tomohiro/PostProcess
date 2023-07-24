#include "Object3d.hlsli"

struct Fog {
	float nearClip;
	float farClip;
	float scale;
	float attenuationRate;
};

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t4> gBlurTexture : register(t1);
Texture2D<float32_t4> gShrinkBlurTexture : register(t2);
Texture2D<float32_t> gLinearDepthTexture : register(t3);
Texture2D<float32_t4> gDofDepthTexture : register(t4);
Texture2D<float32_t> gDepthTexture : register(t5);
SamplerState gSampler : register(s0);
ConstantBuffer<Fog> gFogParameter : register(b0);

struct PixelShaderOutput {
	float32_t4 color : SV_TARGET0;
};

float restDepth(float4 RGBA) {
	const float rMask = 1.0f;
	const float gMask = 1.0f / 255.0f;
	const float bMask = 1.0f / (255.0f * 255.0f);
	const float aMask = 1.0f / (255.0f * 255.0f * 255.0f);
	float depth = dot(RGBA, float4(rMask, gMask, bMask, aMask));
	return depth;
}

PixelShaderOutput main(VertexShaderOutput input) {
	PixelShaderOutput output;
	float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
	float32_t4 blurColor = gBlurTexture.Sample(gSampler, input.texcoord);
	float32_t4 shrinkBlurColor = gShrinkBlurTexture.Sample(gSampler, input.texcoord);
	float32_t linearDepthColor = gLinearDepthTexture.Sample(gSampler, input.texcoord);
	float32_t4 dofDepthColor = gDofDepthTexture.Sample(gSampler, input.texcoord);
	float32_t depthColor = gDepthTexture.Sample(gSampler, input.texcoord);

	//Fog
	float fogWeight = 0.0f;
	fogWeight += gFogParameter.scale * max(0.0f, 1.0f - exp(-gFogParameter.attenuationRate * linearDepthColor));
	//const float3 bgColor = gTexture.Sample(gSampler, input.texcoord).rgb;
	//const float3 bgBlurColor = gBlurTexture.Sample(gSampler, input.texcoord).rgb;
	//const float3 bgShrinkBlurColor = gShrinkBlurTexture.Sample(gSampler, input.texcoord).rgb;
	const float3 bgColor = textureColor.rgb;
	const float3 bgBlurColor = blurColor.rgb;
	const float3 bgShrinkBlurColor = shrinkBlurColor.rgb;
	const float3 fogColor = 0.8f;
	textureColor.rgb = lerp(bgColor, fogColor, fogWeight);
	blurColor.rgb = lerp(bgBlurColor, fogColor, fogWeight);
	shrinkBlurColor.rgb = lerp(bgShrinkBlurColor, fogColor, fogWeight);

	//float x = (1.0f - gFogParameter.farClip / gFogParameter.nearClip);
	//float y = (gFogParameter.farClip / gFogParameter.nearClip);
	//float linearDepth = 1.0f / (x * depthColor + y);
	//float fogWeight = 0.0f;
	//fogWeight += gFogParameter.scale * max(0.0f, 1.0f - exp(-gFogParameter.attenuationRate * linearDepth));
	////const float3 bgColor = gTexture.Sample(gSampler, input.texcoord).rgb;
	////const float3 bgBlurColor = gBlurTexture.Sample(gSampler, input.texcoord).rgb;
	////const float3 bgShrinkBlurColor = gShrinkBlurTexture.Sample(gSampler, input.texcoord).rgb;
	//const float3 bgColor = textureColor.rgb;
	//const float3 bgBlurColor = blurColor.rgb;
	//const float3 bgShrinkBlurColor = shrinkBlurColor.rgb;
	//const float3 fogColor = 0.8f;
	//textureColor.rgb = lerp(bgColor, fogColor, fogWeight);
	//blurColor.rgb = lerp(bgBlurColor, fogColor, fogWeight);
	//shrinkBlurColor.rgb = lerp(bgShrinkBlurColor, fogColor, fogWeight);


	//被写界深度
	float d = restDepth(gDofDepthTexture.Sample(gSampler, input.texcoord));
	float coef = 1.0 - d;
	float blur1Coef = coef * d;
	float blur2Coef = coef * coef;
	/*float4 sceneColor = gTexture.Sample(gSampler, input.texcoord);
	float4 blur1Color = gBlurTexture.Sample(gSampler, input.texcoord);
	float4 blur2Color = gShrinkBlurTexture.Sample(gSampler, input.texcoord);*/
	float4 sceneColor = textureColor;
	float4 blur1Color = blurColor;
	float4 blur2Color = shrinkBlurColor;
	float4 destColor = sceneColor * d + blur1Color * blur1Coef + blur2Color * blur2Coef;
	textureColor = destColor;

	//float depthDiff = abs(gDepthTexture.Sample(gSampler, float2(0.5, 0.5)).r - gDepthTexture.Sample(gSampler, input.texcoord).r);
	//depthDiff = pow(depthDiff, 0.5f);
	//float t = depthDiff * 8;
	//float no;
	//t = modf(t, no);
	//float4 retColor[2];
	//retColor[0] = textureColor;
	//if (no == 0.0f) {
	//	retColor[1] = blurColor;
	//}
	//else {
	//	for (int i = 1; i <= 8; ++i) {
	//		if (i - no < 0)continue;
	//		retColor[i - no] = blurColor;
	//		if (i - no > 1) {
	//			break;
	//		}
	//	}
	//}
	//textureColor = lerp(retColor[0], retColor[1], t);


	//ビネット
	//float vignette = length(float2(0.5f, 0.5f) - input.texcoord);
	//vignette = clamp(vignette - 0.2f, 0, 1);
	//textureColor.rgb -= vignette;

	float2 uv = input.texcoord;
	float4 col = textureColor;
	uv = 1.0f * uv - 0.5f;
	textureColor *= 1.0 - dot(uv, uv);

	output.color = textureColor;
	return output;
}
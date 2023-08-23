#include "Object3d.hlsli"

struct Fog {
	bool enable;
	float start;
	float end;
	float scale;
	float attenuationRate;
};

struct DoF {
	bool enable;
};

struct LensDistortion {
	bool enable;
	float tightness;
	float strength;
};

struct Vignette {
	bool enable;
	float intensity;
};

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t4> gBlurTexture : register(t1);
Texture2D<float32_t4> gShrinkBlurTexture : register(t2);
Texture2D<float32_t4> gLinearDepthTexture : register(t3);
Texture2D<float32_t> gDepthTexture : register(t4);
SamplerState gSampler : register(s0);
ConstantBuffer<Fog> gFogParameter : register(b0);
ConstantBuffer<DoF> gDofParameter : register(b1);
ConstantBuffer<LensDistortion> gLensDistortionParameter : register(b2);
ConstantBuffer<Vignette> gVignetteParameter : register(b3);

struct PixelShaderOutput {
	float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
	PixelShaderOutput output;
	float32_t4 textureColor = gTexture.Sample(gSampler, input.texcoord);
	float32_t4 blurColor = gBlurTexture.Sample(gSampler, input.texcoord);
	float32_t4 shrinkBlurColor = gShrinkBlurTexture.Sample(gSampler, input.texcoord);
	float32_t linearDepthColor = gLinearDepthTexture.Sample(gSampler, input.texcoord).r;
	float32_t depthColor = gDepthTexture.Sample(gSampler, input.texcoord);

	//レンズディストーション
	if (gLensDistortionParameter.enable == true) {
		const float2 uvNormalized = input.texcoord * 2 - 1;
		const float distortionMagnitude = abs(uvNormalized.x * uvNormalized.y);
		const float smoothDistortionMagnitude = pow(distortionMagnitude, gLensDistortionParameter.tightness);
		//const float smoothDistortionMagnitude = 1 - sqrt(1 - pow(distortionMagnitude, gLensDistortionParameter.tightness));
		//const float smoothDistortionMagnitude = pow(sin(1.57079632679f * distortionMagnitude), gLensDistortionParameter.tightness);
		float2 uvDistorted = input.texcoord + uvNormalized * smoothDistortionMagnitude * gLensDistortionParameter.strength;
		if (uvDistorted[0] < 0 || uvDistorted[0] > 1 || uvDistorted[1] < 0 || uvDistorted[1] > 1) {
			textureColor = 0.0f;
			blurColor = 0.0f;
			shrinkBlurColor = 0.0f;
			linearDepthColor = 0.0f;
			depthColor = 0.0f;
		}
		else {
			textureColor = gTexture.Sample(gSampler, uvDistorted);
			blurColor = gBlurTexture.Sample(gSampler, uvDistorted);
			shrinkBlurColor = gShrinkBlurTexture.Sample(gSampler, uvDistorted);
			linearDepthColor = gLinearDepthTexture.Sample(gSampler, uvDistorted).r;
			depthColor = gDepthTexture.Sample(gSampler, uvDistorted);
		}
	}


	//Fog
	if (gFogParameter.enable == true) {
		float fogWeight = 0.0f;
		//float x = (1.0f - 30.0f / 0.1f);
		//float y = (30.0f / 0.1f);
		//float linearDepth = 1.0f / (x * depthColor + y);
		//fogWeight += gFogParameter.scale * max(0.0f, 1.0f - exp(-gFogParameter.attenuationRate * linearDepth));
		fogWeight += gFogParameter.scale * max(0.0f, 1.0f - exp(-gFogParameter.attenuationRate * linearDepthColor));
		const float3 bgColor = textureColor.rgb;
		const float3 bgBlurColor = blurColor.rgb;
		const float3 bgShrinkBlurColor = shrinkBlurColor.rgb;
		const float3 fogColor = 0.8f;
		textureColor.rgb = lerp(bgColor, fogColor, fogWeight);
		blurColor.rgb = lerp(bgBlurColor, fogColor, fogWeight);
		shrinkBlurColor.rgb = lerp(bgShrinkBlurColor, fogColor, fogWeight);
	}
	

	//被写界深度
	if (gDofParameter.enable == true) {
		float d = 0.0f;
		if (linearDepthColor > 0.0f && linearDepthColor < 0.5f) {
			d = 2 * linearDepthColor;
		}
		else if (linearDepthColor > 0.5f && linearDepthColor < 1.0f) {
			d = 2 * (1 - linearDepthColor);
		}
		float coef = 1.0 - d;
		float blur1Coef = coef * d;
		float blur2Coef = coef * coef;
		float4 sceneColor = textureColor;
		float4 blur1Color = blurColor;
		float4 blur2Color = shrinkBlurColor;
		textureColor = sceneColor * d + blur1Color * blur1Coef + blur2Color * blur2Coef;
	}
	

	//ビネット
	if (gVignetteParameter.enable) {
		float2 uv = input.texcoord;
		float4 col = textureColor;
		uv = gVignetteParameter.intensity * uv - gVignetteParameter.intensity / 2;
		textureColor *= 1.0 - dot(uv, uv);
	}

	output.color = textureColor;
	return output;
}
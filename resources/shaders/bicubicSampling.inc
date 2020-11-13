float catmullRomWeight1D(float d){
	float d1 = abs(d);
	float d2 = d1 * d1;
	float d3 = d2 * d1;
	if(d1 <= 1){
		return (1.f / 6.f) * (9*d3 - 15*d2 + 6);
	}
	else if(d1 <= 2){
		return (1.f / 6.f) * (-3*d3 + 15*d2 - 24*d + 12);
	}
	else{
		return 0.f;
	}
}

vec2 catmullRomWeight2D(vec2 d){
	return vec2(
		catmullRomWeight1D(d.x),
		catmullRomWeight1D(d.y));
}

//reference: https://vec3.ca/bicubic-filtering-in-fewer-taps/
//naive full 16-tap bicubic sampling using catmull rom weights
//should only be used as reference to compare faster versions
vec3 bicubicSample(texture2D srcTexture, sampler srcSampler, vec2 iUV, vec2 texelSize){
	vec2 uvTrunc = floor(iUV - 0.5) + 0.5f;
	vec2 d = iUV - uvTrunc;

	vec2 w0 = catmullRomWeight2D(abs(d) + 1);
	vec2 w1 = catmullRomWeight2D(abs(d));
	vec2 w2 = catmullRomWeight2D(1 - abs(d));
	vec2 w3 = catmullRomWeight2D(2 - abs(d));

	vec2 uv0 = uvTrunc - 1;
	vec2 uv1 = uvTrunc;
	vec2 uv2 = uvTrunc + 1;
	vec2 uv3 = uvTrunc + 2;

	uv0 *= texelSize;
	uv1 *= texelSize;
	uv2 *= texelSize;
	uv3 *= texelSize;

	return
		texture(sampler2D(srcTexture, srcSampler), vec2(uv0.x, uv0.y)).rgb * w0.x * w0.y + 
		texture(sampler2D(srcTexture, srcSampler), vec2(uv1.x, uv0.y)).rgb * w1.x * w0.y + 
		texture(sampler2D(srcTexture, srcSampler), vec2(uv2.x, uv0.y)).rgb * w2.x * w0.y + 
		texture(sampler2D(srcTexture, srcSampler), vec2(uv3.x, uv0.y)).rgb * w3.x * w0.y + 

		texture(sampler2D(srcTexture, srcSampler), vec2(uv0.x, uv1.y)).rgb * w0.x * w1.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uv1.x, uv1.y)).rgb * w1.x * w1.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uv2.x, uv1.y)).rgb * w2.x * w1.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uv3.x, uv1.y)).rgb * w3.x * w1.y +

		texture(sampler2D(srcTexture, srcSampler), vec2(uv0.x, uv2.y)).rgb * w0.x * w2.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uv1.x, uv2.y)).rgb * w1.x * w2.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uv2.x, uv2.y)).rgb * w2.x * w2.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uv3.x, uv2.y)).rgb * w3.x * w2.y +

		texture(sampler2D(srcTexture, srcSampler), vec2(uv0.x, uv3.y)).rgb * w0.x * w3.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uv1.x, uv3.y)).rgb * w1.x * w3.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uv2.x, uv3.y)).rgb * w2.x * w3.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uv3.x, uv3.y)).rgb * w3.x * w3.y; 
}

//reference: https://vec3.ca/bicubic-filtering-in-fewer-taps/
//optimized 9-tap bicubic sampling using catmull rom weights
//full quality but still pretty heavy
vec3 bicubicSample9Tap(texture2D srcTexture, sampler srcSampler, vec2 iUV, vec2 texelSize){

	vec2 uvTrunc = floor(iUV - 0.5) + 0.5f;
	vec2 f = iUV - uvTrunc;
	vec2 f2 = f*f;
	vec2 f3 = f2*f;
	
	vec2 w0 = -0.5*f3 +     f2 - 0.5*f;
	vec2 w1 =  1.5*f3 - 2.5*f2 + 1.f;
	vec2 w2 = -1.5*f3 + 2.f*f2 + 0.5*f;
	vec2 w3 =  0.5*f3 - 0.5*f2;
	
	vec2 wB = w1 + w2;
	vec2 t = w2 / wB;

	vec2 uv0 = uvTrunc - 1;
	vec2 uvT = uvTrunc + t;
	vec2 uv3 = uvTrunc + 2;
	
	uv0 *= texelSize;
	uvT *= texelSize;
	uv3 *= texelSize;

	return 
		texture(sampler2D(srcTexture, srcSampler), vec2(uv0.x, uv0.y)).rgb * w0.x * w0.y + 
		texture(sampler2D(srcTexture, srcSampler), vec2(uv0.x, uvT.y)).rgb * w0.x * wB.y + 
		texture(sampler2D(srcTexture, srcSampler), vec2(uv0.x, uv3.y)).rgb * w0.x * w3.y +
	
		texture(sampler2D(srcTexture, srcSampler), vec2(uvT.x, uv0.y)).rgb * wB.x * w0.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uvT.x, uvT.y)).rgb * wB.x * wB.y +  
		texture(sampler2D(srcTexture, srcSampler), vec2(uvT.x, uv3.y)).rgb * wB.x * w3.y + 
	
		texture(sampler2D(srcTexture, srcSampler), vec2(uv3.x, uv0.y)).rgb * w3.x * w0.y +
		texture(sampler2D(srcTexture, srcSampler), vec2(uv3.x, uvT.y)).rgb * w3.x * wB.y + 
		texture(sampler2D(srcTexture, srcSampler), vec2(uv3.x, uv3.y)).rgb * w3.x * w3.y;
}

//reference: "Filmic SMAA" Siggraph presentation, page 90
//5-tap bicubic sampling using catmull rom weights, ignoring corner weights
//faster but slightly reduced quality
vec3 bicubicSample5Tap(texture2D srcTexture, sampler srcSampler, vec2 iUV, vec2 texelSize){

	vec2 uvTrunc = floor(iUV - 0.5) + 0.5f;
	vec2 f = iUV - uvTrunc;
	vec2 f2 = f*f;
	vec2 f3 = f2*f;
	
	vec2 w0 = -0.5*f3 +     f2 - 0.5*f;
	vec2 w1 =  1.5*f3 - 2.5*f2 + 1.f;
	vec2 w2 = -1.5*f3 + 2.f*f2 + 0.5*f;
	vec2 w3 =  0.5*f3 - 0.5*f2;
	
	vec2 wB = w1 + w2;
	vec2 t = w2 / wB;

	vec2 uv0 = uvTrunc - 1;
	vec2 uvT = uvTrunc + t;
	vec2 uv3 = uvTrunc + 2;
	
	uv0 *= texelSize;
	uvT *= texelSize;
	uv3 *= texelSize;

	vec4 result =  
		vec4(texture(sampler2D(srcTexture, srcSampler), vec2(uv0.x, uvT.y)).rgb, 1.f) * w0.x * wB.y + 
		vec4(texture(sampler2D(srcTexture, srcSampler), vec2(uvT.x, uv0.y)).rgb, 1.f) * wB.x * w0.y +
		vec4(texture(sampler2D(srcTexture, srcSampler), vec2(uvT.x, uvT.y)).rgb, 1.f) * wB.x * wB.y +  
		vec4(texture(sampler2D(srcTexture, srcSampler), vec2(uvT.x, uv3.y)).rgb, 1.f) * wB.x * w3.y + 
		vec4(texture(sampler2D(srcTexture, srcSampler), vec2(uv3.x, uvT.y)).rgb, 1.f) * w3.x * wB.y;

	//normalize by dividing trough total weight
	//total weight is stored in alpha
	return result.rgb / result.a;
}
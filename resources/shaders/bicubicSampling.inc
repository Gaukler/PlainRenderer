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
	vec2 uvTrunc = ceil(iUV - 0.5) + 0.5;
	vec2 d = uvTrunc - iUV; 

	vec2 w0 = catmullRomWeight2D(d - 1);
	vec2 w1 = catmullRomWeight2D(d);
	vec2 w2 = catmullRomWeight2D(d + 1);
	vec2 w3 = catmullRomWeight2D(d + 2);

	float weightTotal = 
		 w0.x * w0.y +
		 w1.x * w0.y +
		 w2.x * w0.y +
		 w3.x * w0.y +
		
		 w0.x * w1.y +
		 w1.x * w1.y +
		 w2.x * w1.y +
		 w3.x * w1.y +
		
		 w0.x * w2.y +
		 w1.x * w2.y +
		 w2.x * w2.y +
		 w3.x * w2.y +
		
		 w0.x * w3.y +
		 w1.x * w3.y +
		 w2.x * w3.y +
		 w3.x * w3.y;

	vec2 uv0 = uvTrunc - 1;
	vec2 uv1 = uvTrunc;
	vec2 uv2 = uvTrunc + 1;
	vec2 uv3 = uvTrunc + 2;

	uv0 *= texelSize;
	uv1 *= texelSize;
	uv2 *= texelSize;
	uv3 *= texelSize;

	return (
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
		texture(sampler2D(srcTexture, srcSampler), vec2(uv3.x, uv3.y)).rgb * w3.x * w3.y) 
		/ weightTotal; 
}
vec3 calculateViewDirectionFromPixel(vec2 pixelNDC, vec3 cameraForward, vec3 cameraUp, vec3 cameraRight, float cameraTanFovHalf, float aspectRatio){
	vec3 V = -cameraForward;
	V += cameraUp * cameraTanFovHalf * pixelNDC.y;
	V += cameraRight * cameraTanFovHalf * aspectRatio * pixelNDC.x;
	return normalize(V);
}
//see: https://en.wikipedia.org/wiki/Spherical_cap#Volume_and_surface_area
float computeSphericalCapArea(float radius){
    return 2 * pi - 2 * pi * cos(radius);
}

//from: "Ambient Aperture Lighting"
float sphericalCapIntersectionAreaFast ( float fRadius0, float fRadius1, float fDist )
{
    float fArea;
    if( fDist  <= max(fRadius0, fRadius1) -min(fRadius0, fRadius1))
    {
        // One cap in completely inside the other
        fArea = computeSphericalCapArea(min(fRadius0, fRadius1));
    }
    else if( fDist >= fRadius0 + fRadius1 )
    {
        // No intersection exists
        fArea = 0;
    }else
    {
        float fDiff = abs(fRadius0 - fRadius1);
        fArea = smoothstep(0.0, 1.0,1.0-clamp((fDist-fDiff)/(fRadius0+fRadius1-fDiff), 0, 1));
        fArea *= 6.283185308 - 6.283185308 * cos( min(fRadius0,fRadius1) );
    }
    return fArea;
}

//intersection ratio of specular and visibility cone
//see "Practical Realtime Strategies for Accurate Indirect Occlusion" for overview
float computeSpecularOcclusion(vec3 reflectionVector, vec3 bentNormal, float roughness, float aoFactor){
    vec3 visibilityConeDirection = bentNormal;
    vec3 reflectionConeDirection = reflectionVector;
    
    //GGX fit from: "Deferred Lighting in Uncharted 4"
    float reflectionConeAngle = roughness <= 0.5656 ? 
        acos(0.1925 * log2(-72.56 * roughness + 42.03)) :
        acos(0.0005);
    
    //see "Practical Realtime Strategies for Accurate Indirect Occlusion", equation 18
    float visibilityConeAngle = acos(sqrt(1 - aoFactor));
    
    float coneCentroidDistance = acos(max(dot(visibilityConeDirection, reflectionConeDirection), 0));
    
    float intersectionArea = sphericalCapIntersectionAreaFast(visibilityConeAngle, reflectionConeAngle, coneCentroidDistance);
    float reflectionConeArea = computeSphericalCapArea( reflectionConeAngle );
    float specularOcclusion = intersectionArea / reflectionConeArea;
    specularOcclusion = clamp(specularOcclusion, 0, 1); //FIXME: fix actual reason why occlusion is not in range [0, 1]
    return specularOcclusion;
}

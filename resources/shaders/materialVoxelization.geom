#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "global.inc"

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout(location = 0) in vec3 passPosIn[3];
layout(location = 1) in vec2 passUVIn[3];

layout(location = 0) out vec3 passPosOut;
layout(location = 1) out vec2 passUVOut;

layout(push_constant) uniform MatrixBlock {
	mat4 projection;
	mat4 model;
} translation;

layout(set=1, binding = 1, std140) uniform sdfVolumeData{
    vec4 sdfVolumeExtends;
    vec4 sdfVolumeOffset;
};

void main(){

	vec3 N = cross(passPosIn[0] - passPosIn[1], passPosIn[0] - passPosIn[2]);

	vec3 ndc[3] = {
		(passPosIn[0] - sdfVolumeOffset.xyz) / sdfVolumeExtends.xyz,
		(passPosIn[1] - sdfVolumeOffset.xyz) / sdfVolumeExtends.xyz,
		(passPosIn[2] - sdfVolumeOffset.xyz) / sdfVolumeExtends.xyz
	};

	if(abs(N.x) > abs(N.y) && abs(N.x) > abs(N.z)){
		//project on x axis
		for(int i = 0; i < 3; i++){
			ndc[i] = ndc[i].zyx;
		}
	}
	else if(abs(N.y) > abs(N.z)){
		//project on y axiszy
		for(int i = 0; i < 3; i++){
			ndc[i] = ndc[i].xzy;
		}
	}
	else{
		//project on z axis
		//is default case, no swizzling necessary
	}

	for(int i = 0; i < 3; i++){
		gl_Position = vec4(ndc[i].xy, 1.f, 1.f); 
		passPosOut = passPosIn[i];
		passUVOut = passUVIn[i];
		EmitVertex();
	}    
    EndPrimitive();
}
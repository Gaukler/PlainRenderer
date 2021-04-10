#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec2 passQuadPos;
layout(location = 1) out vec3 passWorldPos;

layout(push_constant) uniform MatrixBlock {
    mat4 modelMatrix;
    mat4 mvp;
};

void main(){
    passQuadPos = inPos.xy;
    passWorldPos = mat3(modelMatrix) * inPos;
    gl_Position = mvp * vec4(inPos, 0.f);
    gl_Position.z = 0.f;
}
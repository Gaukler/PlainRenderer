#ifndef MAIN_PASS_MATRICES_INC
#define MAIN_PASS_MATRICES_INC

struct MainPassMatrices {
    mat4 model;
    mat4 mvp;
    mat4 mvpPrevious;
};

#endif // #ifndef MAIN_PASS_MATRICES_INC
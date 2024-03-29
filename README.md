# Plain renderer

Plain renderer is a real-time C++ Vulkan renderer, written for educational purposes and for fun.  
It features modern rendering features such as TAA, signed distance field based diffuse global illumination and physically based sky rendering. 
A separate asset pipeline is used for preprocessing of meshes and SDF creation.

![Sponza screenshot](https://raw.githubusercontent.com/Gaukler/gaukler.github.io/gh-pages/docs/images/Plain/sponza01.png)

## Platforms

The project has been written and tested in C++ 17 using Visual Studio 19 on Windows 10.

## Build instructions

The project is build using Cmake, requiring at least version 3.16. 
Download the project with submodules using: 
```
git clone https://github.com/Gaukler/PlainRenderer.git --recursive
```
Then build the Visual Studio solution using Cmake. The Vulkan SDK must be installed.  
Besides ```Debug``` and ```Release``` modes there is a ```Development``` mode which uses optimizations, but retains debug symbols and keeps the Vulkan validation layers enabled.

## Usage

Models are prepared using a separate asset pipeline.  
The asset pipeline takes in models the glTF 2.0 format. Models are required to provide positions, normals, tangents and one UV set. If a mesh has a custom 'noSDF' attribute SDF generation is skipped.   
Textures can be PNG, Jpeg and DDS. Three textures can be used per material, following standard PBR conventions: base color, normal map and a metal/roughness map. The normal map may have only two channels, with the third being reconstructed in the shader. The metal/roughness map should contain the roughness the roughness in the G and metal in the B channel.  
A binary format is used to store processed meshes using the '.plain' extension.  
Besides processing the mesh the pipeline computes a signed distance field per mesh. These are saved as DDS files.  

All assets must be placed in the resource folder, located in ```Plain/resouces```, with provided paths being relative to that.

The asset pipeline takes a single command line argument, a path to a glTF file to be processed. The resulting ```.plain``` will be written into the same directory. All SDF textures will be written into ```'modelDirectory'/sdfTextures/```.  

The renderer takes three command line arguments:
 1. initial window width in pixels
 2. initial window height in pixels
 3. path to the ```.plain``` scene file to load 

Scenes should be divided into single meshes, as the SDF is computed per mesh. If the entire scene consists of a single mesh the scene's SDF representation will have a low effective resolution.  
If the scene uses many copied objects, instancing should be used to lessen the memory footpring of the SDFs. If the scene uses many small objects, which don't contribute much to the indirect lighting, they should be tagged with the custom 'noSDF' attribute, as the indirect lighting's tracing cost scales with the number of SDF instances.

## Features

* Real time diffuse GI, by tracing SDF representation of scene and denoising, inspired by [[Wr15]](#wr15) and [[Ar19]](#ar19)
* Cook-Torrance BRDF using GGX-distribution, correlated smith geometry term and multiscattering as shown in [[MA19]](#ma19) and [[La18]](#la18) 
* Temporal Anti Aliasing using an exponential history buffer [[Ka14]](#ka14) and bicubic sampling [[Ji17]](#ji17)
* Physically based sky rendering using luts and a multiscattering approximation according to [[Hi20]](#hi20)
* Physically based light and camera units and histogram based automatic exposure [[Ch18]](#ch18)
* Volumetric lighting using froxels [[Hi15]](hi15)
* Bloom and tonemapping
* Single pass min/max hierarchical depth buffer generation
* Cascaded shadow maps for the sun, tightly fitted to depth buffer
* Blue noise generation using the void and cluster method
* 3D perlin noise generation
* Simple job system for multithreading, used for accelerating SDF generation, texture loading and multithreaded drawcall recording
* Custom Vulkan memory allocator
* Separate asset pipeline producing mesh data in a binary format for efficient loading and SDF generation 
* Shader hot reloading

## Gallery

![Profiling and UI](https://raw.githubusercontent.com/Gaukler/gaukler.github.io/gh-pages/docs/images/Plain/profiling-UI.png)
![Sky screenshot](https://raw.githubusercontent.com/Gaukler/gaukler.github.io/gh-pages/docs/images/Plain/sky.png)
![Indirect lighting gif](https://raw.githubusercontent.com/Gaukler/gaukler.github.io/gh-pages/docs/images/Plain/indirectLighting.gif)

## References

This list is a non-exhaustive. 

###### Hi20 
Hillaire, Sébastien. (2020). A Scalable and Production Ready Sky and Atmosphere Rendering Technique. Computer Graphics Forum. 39. 13-22. 10.1111/cgf.14050. 

###### MA19 
McAuley, Stephen. (2019). A Journey Through Implementing Multiscattering BRDFs and Area Lights. Advances in RealTime Rendering, ACM SIGGRAPH 2019 Courses.  

###### La18
Lagarde, Sébastien. (2018). The Road toward Unified Rendering with Unity’s High Definition Render Pipeline. Advances in RealTime Rendering, ACM SIGGRAPH 2018 Courses.  
 
###### Wr15
Wright, Daniel. (2015). Dynamic Occlusion with Signed Distance Fields. Advances in RealTime Rendering, ACM SIGGRAPH 2015 Courses.  
 
###### Ar19
Archard, Ben. (2019). Exploring the Ray Traced Future in 'Metro Exodus' (Presented by NVIDIA). Game Developers Conference.
 
###### Ka14
Karis, Brian. (2014). High-Quality Temporal Supersampling. Advances in RealTime Rendering, ACM SIGGRAPH 2014 Courses.  
 
###### Ji16
Jimenez Jorge. (2016). Filmic SMAA: Sharp Morphological and Temporal Antialiasing. Advances in RealTime Rendering, ACM SIGGRAPH 2016 Courses.  

###### Ji17
Jimenez Jorge. (2017). Dynamic Temporal Antialiasing in Call of Duty: Infinite Warfare. Advances in RealTime Rendering, ACM SIGGRAPH 2017 Courses.  
 
###### Ch18
Chan, Danny. (2018). Real-World Measurements forCall of Duty: Advanced Warfare. 

###### Hi15
Hillaire, Sébastien. (2015). Towards Unified and Physically-Based Volumetric Lighting in Frostbite. Advances in RealTime Rendering, ACM SIGGRAPH 2015 Courses.  

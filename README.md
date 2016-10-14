# Render-Terrain
DX12 Terrain Renderer
Author: Chris Serson (aka Traagen)
Date: October 14, 2016

Description:
This project implements a simple terrain engine in DirectX 12.
It is currently not multi-threaded.
The engine uses a static mesh, centered on the origin, to represent the terrain.
Dynamic Level of Detail is implemented using tessellation.
Adds detail using displacement mapping and bump mapping.
Uses Cascaded Shadow Maps to shadow the entire terrain.
Can be toggled using a simple colour palette and diffuse texture splatting.

Controls:
Movement is relative to the direction you are currently facing.
Mouse look to rotate camera.
A - strafe left
D - strafe right
W - forward
S - backward
Q - strafe up
Z - strafe down

1 - 2D view
2 - 3D view
T - toggle textures on and off
L - toggle whether the camera is locked to the Terrain
ESC - exit

File Resources:
All files currently being loaded are PNG files containing RGBA data.
The engine will take an arbitrary greyscale PNG as a height maps.
There are multiple heightmap#.png files included that all work with the engine.
To switch which is being loaded, change the filename in the Scene constructor.
The displacement map contains a normal map in the RGB channels and a height map 
in the A channel of the PNG file.
Requires 4 normal maps and 4 diffuse maps be specified for height and slope based
normal and diffuse mapping.
To specify diffuse and normal maps, specify the colour/normal in the RGB channels
and place a greyscale depth/height value in the A channel of the PNG.
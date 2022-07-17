# D3D12-Tech-Demo
Direct3D 12 Technology Demonstration<br>
Copyright © 2022 by Denis Reischl<br>

SPDX-License-Identifier: MIT<br>

## 1. Procedural Heightmap: Hex-tiled mesh (near), volume-ray-cast (far)

For any uneven primitive it is essential to use a hex tiled grid for its mesh in 3D. As we see by a (european) football, manufacturing it would be impossible using square leather pieces.

[<img src="https://upload.wikimedia.org/wikipedia/commons/0/0c/Fussball.jpg">]

The hexagonal coordinate system has the decisive advantage that each individual point is always at the same distance to each of its neighboring points. That is also of use for many other game-relevant calculations, like path finding or fundamental AI.

For closer terrain this is the best option, but for distant terrain this would need too many triangles to be rendered.

And so since distant terrain has too many triangles to render i did choose the volume ray casting (raymarching) method for distant terrain. Raymarching means, other than raytracing, that the ray "walks" step by step throug the viewport. This technique is used in the compute shader during post processing.

Missing here : LOD can be easily applied by intersecting the hex triangles for closer tiles

Video Link : https://youtu.be/DTHR7ZxROHY

## 2. Candy Land [DXR] : Raytraced endless bent cylinder

[WORK IN PROGRESS]

[<img src="https://github.com/EisernSchild/D3D12-Tech-Demo/blob/main/media/Candy_land_01.PNG">]

### Gamepad Controls

LT / RT - steer on Y axis<br>
Left Stick - steer on XZ axis<br>
Right Stick - pitch / yaw<br>
START - switch rendering technique<br>

# D3D12-Tech-Demo
Direct3D 12 Technology Demonstration<br>
"Procedural Heightmap in 2 approaches"<br>
Copyright © 2022 by Denis Reischl<br>

SPDX-License-Identifier: MIT<br>

## 1. Terrain as hex-tiled grid, tiles shifted by compute shader

For any uneven primitive it is essential to use a hex tiled grid for its mesh in 3D. As we see by a (european) football, manufacturing it would be impossible using square leather pieces.

[<img src="https://upload.wikimedia.org/wikipedia/commons/0/0c/Fussball.jpg">]

The hexagonal coordinate system has the decisive advantage that each individual point is always at the same distance to each of its neighboring points. That is also of use for many other game-relevant calculations, like path finding or fundamental AI.

Missing here : LOD can be easily applied by intersecting the hex triangles for closer tiles

## 2. Raymarched terrain computed in DXR intersection method

Since terrain has too many triangles to render using real-time raytracing, i did choose the raymarching method here. Raymarching means, other than raytracing, that the ray "walks" step by step throug the viewport.

Now this technique - as seen in the sample - provides flaws, especially when close to the rendered object, in this case the terrain. In addition, applying full screen raymarching should be executed in the compute shader instead of using DXR to avoid unnessesary calculations.

Missing here : Heightmap should be precomputed to minimize calculations during raymarching

## Proposal for solution

1) render the whole terrain as hex tiled grid, no lighting applied here
2) execute raytracing (DXR) rendering water (lake, sea, river, waterfall), applying lighting here providing depth and normal information from the previously rendered terrain
3) use raymarching only for GPUs not supporting DXR, in DXR only for local objects, not for (global) terrain

### Gamepad Controls

LT / RT - steer on Y axis
Left Stick - steer on XZ axis
Right Stick - pitch / yaw
START - switch rendering technique

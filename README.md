# D3D12-Tech-Demo
Direct3D 12 Technology Demonstration<br>
Copyright © 2022 by Denis Reischl<br>

SPDX-License-Identifier: MIT<br>

## 1. Procedural Heightmap: Hex-tiled mesh (near), volume-ray-cast (far)

For any uneven primitive it is essential to use a hex tiled grid for its mesh in 3D. As we see by a (european) football, manufacturing it would be impossible using square leather pieces.

The hexagonal coordinate system has the decisive advantage that each individual point is always at the same distance to each of its neighboring points. That is also of use for many other game-relevant calculations, like path finding [1] or fundamental AI.

For closer terrain this is the best option, but for distant terrain this would need too many triangles to be rendered.

And so since that is the case i did choose the volume ray casting (raymarching) method for distant terrain. Raymarching means, other than raytracing, that the ray "walks" step by step through the viewport. This technique is used in the compute shader during post processing.

Missing here : LOD can be easily applied by intersecting the hex triangles for closer tiles

Video Link : https://youtu.be/DTHR7ZxROHY <br>

[<img src="https://github.com/EisernSchild/D3D12-Tech-Demo/blob/main/media/Proc_heightmap_01.PNG">]

## 2. Candy Land [DXR] : Raytraced procedural sweets

* Scotch mints, Taffy candy bar, Marshmallow

Since these are very simple primitives (ellipsoids, rounded box, capsule), we can use "ray intersection" functions here. That is, it can be calculated mathematically if and where the ray intersects the primitive to calculate the final position and normal of the resulting fragment. These intersectors work much faster than volume ray casting.

* Candy loop

Since the (endless) cylinder is intensely curved, we cannot use an intersector here, that would be very complicated, at least mathematically. So we also use volume ray casting as we did in the first example.

Video Link : https://youtu.be/SRQhpNWdLUs <br>

[<img src="https://github.com/EisernSchild/D3D12-Tech-Demo/blob/main/media/Candy_land_03.PNG">]

### Gamepad Controls

LT / RT - steer on Y axis<br>
Left Stick - steer on XZ axis<br>
Right Stick - pitch / yaw<br>
START - switch rendering technique<br>

### References

1. Hex Tiles path finding : https://www.shadertoy.com/view/ssyfWm <br>

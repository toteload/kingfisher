## Sponza scene

Load the Sponza scene.
For this the .OBJ triangle reader needs to be updated.
The Sponza file has multiple groups, whereas the reader currently only supports files with a single group.

## Bounded multiple bounces for indirect light

Assume that everything is uniformly lit with a simple light.
Basically the skybox is one big uniform light.

## Extend debugging UI

- Add perspective controls; switch between orthographic and pinhole. Make the perspective variables tweakable.
- Make the screenbuffer a fixed subview of the debugging UI.

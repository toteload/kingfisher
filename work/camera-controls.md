# Interactive camera controls

## Functional specification
- Move around using WASD. This is relative to your viewing direction.
- Q moves you up along the y-axis and E moves you down along the y-axis.
- Hold Shift while moving to move faster.
- Look around using the arrow keys.
- No mouse control.
- No default animation for the camera.

## Technical specification
- Camera related variables are all grouped together in a struct.
- Viewing angles are stored in radians.
- There are functions to conveniently manipulate the camera variables.

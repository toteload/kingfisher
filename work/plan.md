# Sample Accumulation Implementation Plan

## Goal
Implement progressive sample accumulation in the XYZ color space that accumulates samples while the camera is stationary and resets when the camera moves.

## Current Architecture Analysis

The current rendering pipeline:
1. Ray trace → wavelength + power per pixel
2. Convert to XYZ via CIE lookup tables
3. Normalize XYZ
4. Convert to linear RGB
5. Apply gamma correction to sRGB
6. Display single-sample result

## Proposed Architecture Changes

### 1. Data Structures

**Accumulation Buffer:**
- Add `vec3 *accumulation_buffer` to store accumulated XYZ values per pixel
- Add `u32 sample_count` to track number of accumulated samples
- Size: `SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(vec3)`

**Camera State Tracking:**
- Store previous camera position/orientation to detect movement
- For `CameraPinhole`: track `pos`, `forward`, `right`, `up` vectors
- Simple comparison: if any component changes, camera has moved

### 2. Modified Rendering Pipeline

**Per Frame:**
1. **Detect Camera Movement**
   - Compare current camera state with previous frame
   - If different: reset accumulation buffer and sample count

2. **Ray Tracing (unchanged)**
   - Generate ray per pixel
   - Compute intersections
   - Sample lighting
   - Result: wavelength + power

3. **Accumulation in XYZ Space**
   - Convert wavelength + power to XYZ using existing `cie_xyz_lut` functions
   - Add XYZ result to `accumulation_buffer[pixel_index]`
   - Increment `sample_count`

4. **Display Conversion**
   - For each pixel: `average_xyz = accumulation_buffer[pixel] / sample_count`
   - Normalize XYZ (existing function)
   - Convert to linear RGB (existing function)
   - Apply sRGB gamma (existing function)
   - Write to SDL texture

### 3. Implementation Steps

#### Step 1: Add Accumulation Buffer
- Allocate `accumulation_buffer` at startup (after SDL initialization)
- Initialize to zero
- Free at shutdown

#### Step 2: Add Camera State Tracking
- Add struct to hold previous camera state (position + orientation vectors)
- Initialize with current camera state
- Add function `bool camera_has_moved(CameraPinhole *current, CameraPinhole *previous)`
  - Compare all relevant fields with small epsilon for floating point

#### Step 3: Implement Reset Logic
- Add `reset_accumulation()` function:
  - Zero out accumulation buffer
  - Set sample_count to 0
- Call when camera movement detected

#### Step 4: Modify Main Render Loop
- Before rendering: check if camera moved
  - If yes: call `reset_accumulation()`, update previous camera state
- During ray tracing: accumulate XYZ instead of immediate RGB conversion
  - Loop structure remains the same
  - After getting wavelength + power, convert to XYZ
  - Add to accumulation buffer
- After all pixels traced: increment sample_count
- For display: divide accumulated XYZ by sample_count, then convert to sRGB

#### Step 5: Handle Edge Cases
- First frame: sample_count is 0, so special case to avoid division by zero
- Window resize: reallocate accumulation buffer and reset
- Very large sample counts: consider using double precision for accumulation or periodic normalization

### 4. Code Changes Summary

**main.c modifications:**
- Add global `vec3 *accumulation_buffer` and `u32 sample_count`
- Add `CameraPinhole camera_prev` for tracking
- Add `camera_has_moved()` function
- Add `reset_accumulation()` function
- Modify main loop:
  - Camera movement check before rendering
  - XYZ accumulation during ray tracing
  - Averaged XYZ → RGB conversion for display

**kingfisher.h modifications (if needed):**
- Potentially add helper functions for XYZ accumulation
- No major changes expected - existing spectral conversion functions work as-is

### 5. Testing Strategy

1. **Static camera test:**
   - Disable camera animation
   - Verify samples accumulate (image should become cleaner over time)
   - Check sample count increases each frame

2. **Moving camera test:**
   - Enable camera animation
   - Verify accumulation resets each frame when camera moves
   - Sample count should stay at 1

3. **Visual quality test:**
   - Static scene should show progressive noise reduction
   - Compare quality at sample_count = 1, 10, 100, 1000

### 6. Performance Considerations

- Memory: Additional ~width × height × 12 bytes for XYZ accumulation buffer
- Computation: Minimal overhead (one vector addition + division per pixel)
- No expected performance degradation

### 7. Future Enhancements (out of scope)

- Display sample count on screen
- Manual reset via keyboard input
- Save accumulated result to file
- Adaptive sample count (stop at convergence threshold)
- Temporal anti-aliasing when camera moves

## Expected Outcome

When stationary, the renderer will progressively accumulate samples in XYZ space, producing cleaner images with reduced noise. When the camera moves, accumulation resets to provide responsive feedback. The XYZ accumulation preserves spectral accuracy through the accumulation process.

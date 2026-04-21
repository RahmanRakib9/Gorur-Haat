# Gorur Haat - Eid Cattle Market (OpenGL Project)

## Project Overview
This is a Computer Graphics Lab project built with OpenGL/FreeGLUT in C++.  
The scene represents a Bangladeshi Eid cattle market (`Gorur Haat`) with a central path, stalls, cows, people, hills, sky, clouds, and daylight environment.

## Theme
- Real-world cultural scene: **Eid cattle market**
- Environment includes:
  - Market stalls and temporary shades
  - Cows with rope/tether logic
  - Buyers/sellers and walking villagers
  - Natural sky, static sun, and clouds

## Core Graphics Features
- Basic primitives: points, lines, polygons, quads, triangles
- Classic graphics algorithms:
  - DDA line algorithm
  - Bresenham line algorithm
  - Midpoint circle algorithm
- 2D transformations used:
  - Translation
  - Rotation
  - Scaling
  - Reflection
- Animation:
  - Moving clouds
  - Human walking/bargaining gestures
  - Cow idle motion (head/tail)
  - Walking cow with leaders

## Project Structure (single source file)
- `main.cpp`
  - Environment drawing (sky, ground, path, hills, trees, stalls)
  - Character/cow drawing modules
  - Row composition and crowd placement
  - Display pipeline (back-to-front rendering)
  - Timer-based animation loop

## Build and Run (Windows / MinGW)
```bash
g++ main.cpp -o gorur_hut -lfreeglut -lopengl32 -lglu32
./gorur_hut
```

## Uploaded Real-Time Screenshots
### Screenshot 1
![Project Screenshot 1](assets/img-1.png)

### Screenshot 2
![Project Screenshot 2](assets/img-2.png)

## Notes
- Sun is kept static for a realistic distant-light effect.
- Variable names and comments are intentionally detailed for readability and presentation.

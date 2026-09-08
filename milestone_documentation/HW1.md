# HW1

## Task 1
Be able to open an SDL3 window and render to the display

### Relevant Files
- src/application.cpp
- include/svanes/application.hpp
- include/svanes/game.hpp

## Task 2
A generic entity system representing the objects in your game

### Relevant Files
Main
- src/registry.cpp
- include/svanes/registry.hpp
- include/svanes/entity.hpp

Rendering
- src/render/render_queue_executor.cpp
- src/render/render_queue_executor.hpp
- src/redner/render_queue.cpp
- src/render/render_system,.cpp
- src/render/texture_manager_internal.hpp
- src/render/texture_manager.cpp
- include/svanes/render/basic_render_types.hpp
- include/svanes/render/render_queue.hpp
- include/svanes/render/render_system.hpp
- include/svanes/render/texture_manager.hpp

Animation
- src/sprite_animation_system.cpp
- include/svanes/sprite_animation_system.hpp

## Task 3
A physics system which provides a directional gravitational force

### Relevant Files
- src/kinematic_system.cpp
- include/svanes/kinematic_system.hpp

## Task 4
An input handling system which allows for controlling the game window and game entities

### Relevant Files
- src/input.cpp
- src/input_manager_internal.hpp
- include/svanes/input.hpp

## Task 5
A collision detection system, detecting when entities collide and/or overlap

### Relevant Files
Main
- src/collision_system.cpp
- include/svanes/collision_system.hpp

Geometry
- src/geometry.cpp
- include/svanes/geometry.hpp
- src/rectangle_geometry.cpp
- src/triangle_geometry.cpp
- src/circle_geometry.cpp
- src/convex_polygon_geometry.cpp
- include/svanes/rectangle_geometry.hpp
- include/svanes/triangle_geometry.hpp
- include/svanes/circle_geometry.hpp
- include/svanes/convex_polygon_geometry.hpp

## Task 6
A system to scale the window size and switch between scaling modes

### Relevant Files
- src/camera2d.cpp
- include/svanes/camera2d.hpp
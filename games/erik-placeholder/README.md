# HW1 Decisions

This document covers what we built for each of the six engine tasks and the decisions we made along the way. milestone_documentation/HW1.md lists the files for each task; this one explains the reasoning behind them.

## Task 1: Core Graphics Setup

Application owns SDL. It initializes SDL3, creates the window and renderer from an ApplicationSettings struct that defaults to 1920x1080, and runs the loop until the game reports it should quit.

SDL stays inside the engine. A game includes svanes/application.hpp, implements IGame, and never sees an SDL type. This is what makes the rest of the tasks replaceable later, since swapping the backend touches the engine and not the games.

The window size is settings-driven rather than fixed. The 1920x1080 requirement is the default value of ApplicationSettings, not a constant in the loop.

The loop runs on variable delta time, measured from SDL ticks and handed to the game as FrameContext::delta_seconds, with a 1ms delay each iteration so an uncapped frame rate does not busy-wait. We chose variable delta time because it is the simpler thing that works; a fixed timestep is worth revisiting when we get to networking and time management.

## Task 2: Generic Entity System

We went property-centric instead of building an entity class. An entity is just an ID, and everything about it lives in components stored in a Registry. Systems then operate on whatever entities happen to hold the components they care about. src/entity_components.md covers how components work in more detail.

Components are data, and systems are free functions. No component has behavior. The logic lives in functions like AdvanceKinematics(world, delta_seconds, gravity) that take the registry and act on every entity matching a component signature. This keeps a system readable in one place instead of spread across the types it touches.

ForEach matches on component types, so systems never maintain their own entity lists, and adding or removing a component changes what an entity participates in immediately.

## Task 3: Physics System

AdvanceKinematics integrates velocity and acceleration into each entity's Transform. An entity opts into physics by holding Kinematic2D, and opts into gravity by additionally holding the Gravity tag component.

Gravity is a configurable vector rather than a downward float. The task asks for a settable constant, and we made it a Vector2D on the context that a game can write at any time. Direction costs nothing extra to support here and leaves room for games that want sideways or inverted gravity without engine changes.

Gravity applies per entity, not globally. A game object is only affected if it carries Gravity, which is how the enemy hangs in the air while the goose falls, without either of them being special-cased in the engine.

## Task 4: Input Handling System

InputManager exposes the keyboard and mouse through the engine's own Key and MouseButton
enums, with per-frame state for held, pressed and released.

The engine defines its own Key enum instead of exposing SDL scancodes. The task suggests a call like isKeyPressed(SDL_SCANCODE_W), but that would put an SDL type in every game's source. InputManager translates SDL scancodes into svanes::Key internally, so a game spells it frame.input.IsDown(svanes::Key::W) and stays free of SDL.

Input is built from events rather than polling the keyboard state. A current and previous array are kept per frame, which gives edge detection: WasPressed and WasReleased, not just IsDown. Reading only the current keyboard state cannot tell the difference between a key being newly pressed and one being held, and we needed that distinction for things like the scaling toggle, which must fire once per press.

## Task 5: Collision Detection System

DetectCollisions takes two geometries with their transforms and returns every collision between them, using the Separating Axis Theorem.

This goes past bounding boxes to SAT over arbitrary convex shapes. SAT handles rectangles, triangles, circles and convex polygons, plus composite shapes built from several parts, through one entry point. Geometry is a std::variant, so a caller passes a Geometry2D and does not branch on shape type. It also keeps collision independent of SDL, which SDL_HasIntersection would not have.

It returns contact information rather than a bool. Each Collision2D carries the unit normal to separate along and the penetration depth along it. A bool answers whether two things overlap, but not what to do about it, and everything we wanted to build needed the second answer.

Collision is a component, so it is opt-in. Only entities with Collider2D participate, and the collider's geometry is separate from the geometry it is drawn with, so the two can differ if a game needs that.

## Task 6: Scaling System

Camera2D owns the conversion between world and screen coordinates and supports two scale modes, toggled in our game by pressing Tab.

Scaling lives in the camera rather than in each render call. Camera2D derives its scale from the current output size, and the render systems ask it to convert geometry as they submit. That means position, zoom and scale mode are all applied in one place, and a new drawable type gets scaling for free.

The toggle is the game's to bind, not the engine's. The engine exposes camera.scale_mode as a plain field and never binds a key to it. We chose Tab, and used WasPressed so holding the key does not flip the mode every frame.

## Where the six tasks appear in our game

The engine-side files for each task are listed in milestone_documentation/HW1.md. In the
game, each task is tagged in a comment at the code that demonstrates it, so searching this
directory for TASK finds all of them.

Task 1, running the engine, is the game being handed to svanes::Application in main.cpp.

Task 2A, the static entity, is the ground, created in ErikGame::Initialize in
erik_game.cpp. Task 2B, the controllable entity, is the goose, spawned in the same
function. Task 2C, the auto-moving entity, is the enemy, also spawned there and walked
along its path in ErikGame::Update.

Task 3, physics, is the Kinematic2D and Gravity components put on the goose in Goose::Spawn
in goose.cpp.

Task 4, controls, is the input read into a GooseIntent in ErikGame::Update in erik_game.cpp.

Task 5, collision response, is Goose::ResolveCollisions in goose.cpp.

Task 6, scaling, is the Tab key toggling the camera's scale mode in ErikGame::Update in
erik_game.cpp.

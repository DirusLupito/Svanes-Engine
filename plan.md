The cleanest path is to separate “hide SDL” from “automatically render entities.” Trying both simultaneously would make the refactor harder to debug.

Each stage below has a concrete stopping point.

## 1. Define backend-neutral rendering types

Add a public rendering header containing only engine types:

- `Color`
- `Point` or `Vector2`
- `Rectangle`
- `ImageData`
- `TextureHandle`

For example:

```cpp
struct TextureHandle {
    uint32_t id = 0;
};

struct ImageData {
    int32_t width;
    int32_t height;
    std::vector<uint8_t> rgba_pixels;
};
```

Do not change existing game code yet.

Success criteria:

- The new header contains no SDL includes or SDL types.
- `just build` still passes.

## 2. Add an engine-owned texture manager

Create something like `AssetManager` or `TextureManager`.

Its public API should accept engine data and return handles:

```cpp
class AssetManager {
public:
    TextureHandle LoadTexture(std::string_view path);
    TextureHandle CreateTexture(const ImageData& image);
};
```

Internally, its `.cpp` file can:

- Include SDL and SDL_image.
- Convert `ImageData` into `SDL_Texture`.
- Store `TextureHandle → SDL_Texture*`.
- Destroy textures before the SDL renderer is destroyed.

Keep the actual SDL lookup private:

```cpp
SDL_Texture* ResolveTexture(TextureHandle handle);
```

That function should only be accessible to the engine renderer.

Success criteria:

- No game needs to own or destroy textures created through `AssetManager`.
- Both file-backed and generated textures are supported.
- Invalid handles and failed loads produce clear errors.

## 3. Add a backend-neutral rendering interface

Introduce a small `RenderQueue` or `Renderer` API:

```cpp
renderer.Clear(Color{17, 24, 39, 255});
renderer.DrawRectangle(destination, Color{56, 189, 248, 255});
renderer.DrawTexture(texture, source, destination);
```

I would favor a `RenderQueue`: game and engine systems submit descriptions, then the engine executes them afterward.

```text
Game/system → RenderQueue → SDL renderer → Present
```

The queue contains engine types and texture handles. Only its implementation translates commands into SDL calls.

Avoid attempting to model every possible graphical operation. Implement only what the current games need:

- Clear
- Solid rectangle
- Texture

Success criteria:

- The public queue header contains no SDL.
- An internal executor can translate every command into SDL.
- Commands preserve submission order.

## 4. Introduce proper initialization context

Textures cannot be loaded cleanly with the current interface because the game receives renderer access only during `OnRender()`. That is why Erik and Orbital Escalation lazily create textures while rendering.

Add initialization explicitly:

```cpp
struct GameContext {
    AssetManager& assets;
};

struct FrameContext {
    const InputManager& input;
    float delta_seconds;
};

class IGame {
public:
    virtual ~IGame() = default;

    virtual void Initialize(GameContext& context) = 0;
    virtual void Update(const FrameContext& frame) = 0;
    virtual void BuildRenderQueue(RenderQueue& queue) = 0;
};
```

`Application::run()` should then perform:

```text
Initialize game
While running:
    collect input
    update game
    collect render commands
    execute render commands
    present
Destroy game resources
Destroy renderer
Quit SDL
```

You can keep `ShouldQuit()` temporarily to limit the size of this step.

Success criteria:

- Initialization happens after the SDL renderer exists.
- Engine-managed textures are destroyed before the renderer.
- No resource creation needs to be hidden inside rendering.

## 5. Migrate the games away from direct SDL rendering

Migrate them in increasing order of complexity.

### Sandbox and Chris

Replace:

- `SDL_SetRenderDrawColor`
- `SDL_RenderClear`
- `SDL_RenderFillRect`

With:

- `RenderQueue::Clear`
- `RenderQueue::DrawRectangle`

### Orbital Escalation

Keep its gradient-generation algorithm, but have it produce `ImageData` instead of calling `SDL_CreateTexture`.

During initialization:

```cpp
gradient_texture = context.assets.CreateTexture(CreateGradientImage());
```

During rendering:

```cpp
queue.DrawTexture(gradient_texture, {}, destination);
```

The game still decides exactly what the gradient looks like. The engine decides how those pixels become a GPU texture.

### Erik

Replace `IMG_LoadTexture` with:

```cpp
orb_texture = context.assets.LoadTexture(
    "darkworld_spawn_swirlingorb_idle.png"
);
```

Change `SpriteAnimation::texture` from:

```cpp
SDL_Texture*
```

to:

```cpp
TextureHandle
```

Change `RenderSprites()` so it submits `DrawTexture` commands instead of accepting an `SDL_Renderer*`.

Finally:

- Remove SDL and SDL_image includes from the games.
- Move `SDL3_image::SDL3_image` from Erik’s CMake target to the engine target.
- Remove manual calls to `SDL_DestroyTexture`.

Success criteria:

```powershell
rg "SDL_|SDL3" games apps
```

should return nothing.

## 6. Make rendering entity-driven

Once games can submit backend-neutral commands, move command submission into engine systems.

Add visual components such as:

```cpp
struct Sprite {
    TextureHandle texture;
    Rectangle source;
};

struct SolidRectangle {
    Color color;
};
```

Continue using `Transform` for position and size.

Then implement engine systems:

```cpp
void SubmitSprites(const Registry& world, RenderQueue& queue);
void SubmitRectangles(const Registry& world, RenderQueue& queue);
```

These systems find the appropriate component combinations:

```text
Transform + Sprite         → texture command
Transform + SolidRectangle → rectangle command
```

At this point, a game supplies what an entity looks like by assigning components. It does not explicitly render the entity every frame.

Success criteria:

- Each game creates visual entities during initialization.
- Updates only mutate state and components.
- Engine rendering systems submit every visible entity.
- Game `BuildRenderQueue()` functions become empty or unnecessary.

## 7. Let the engine own the world and remove `OnRender`

Currently Erik owns its own [Registry](C:/Users/dirus/git/Svanes-Engine/games/erik-placeholder/erik_game.hpp:17). For automatic engine systems, give each running application an engine-owned `Registry`.

Expose it through the contexts:

```cpp
struct GameContext {
    Registry& world;
    AssetManager& assets;
};

struct FrameContext {
    Registry& world;
    const InputManager& input;
    float delta_seconds;
};
```

The loop can then become:

```cpp
game.Update(frame);

AdvanceSpriteAnimations(world, delta_seconds);
SubmitRectangles(world, render_queue);
SubmitSprites(world, render_queue);

ExecuteRenderQueue(render_queue);
```

Remove `OnRender` or `BuildRenderQueue` from `IGame`.

Success criteria:

- No game contains a rendering callback.
- Games create entities and update components.
- The engine renders the world automatically.

## 8. Remove SDL from input and reconsider `IGame`

Rendering is not the only SDL leak. [input.hpp](C:/Users/dirus/git/Svanes-Engine/include/svanes/input.hpp:1) exposes scancodes, mouse button values, points, and windows from SDL.

Replace those with engine types:

```cpp
enum class Key {
    Escape,
    Left,
    Right,
    Space,
};

bool IsDown(Key key) const;
```

SDL event translation remains private inside `input.cpp`.

Then reconsider what remains:

```cpp
class IGame {
public:
    virtual void Initialize(GameContext& context) = 0;
    virtual void Update(FrameContext& frame) = 0;
};
```

At that point you can make an informed choice:

- Keep it and rename it `GameModule` or `ApplicationClient`.
- Replace it with a compile-time concept.
- Replace it with callbacks.

I would postpone that choice. First make the boundary correct; then decide whether virtual dispatch is earning its place.

## Definition of done

The refactor is complete when:

- `games/` and `apps/` contain no SDL calls or includes.
- Public engine headers contain no SDL types.
- SDL and SDL_image are implementation details of the engine.
- Games provide appearance through engine components and image data.
- The engine owns textures and destroys them in the correct order.
- Games update state; engine systems render it.
- All four commands work:

```sh
just run
just run erik
just run orbitalEscalation
just run chris
```

The most useful first milestone is steps 1–5: hide SDL without changing the overall game-loop architecture. Once that works, entity-driven rendering in steps 6–7 becomes a much smaller and safer change.
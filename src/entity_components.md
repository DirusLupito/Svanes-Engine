## Entity Components

In our engine, our entities are composed of various components that define their behavior and properties.

This is opposed to a traditional inheritance-based approach, where entities would inherit from a base class and have their behavior defined by that class hierarchy.

### Components

Components themselves are data structs, and nothing more. They do not contain any logic or behavior.
Their logic lies in utility functions specific to that component type, which operate on the data contained within the component.

### What this looks like

Creating a component is simply defining a struct with the data you want to store.
For example the [SpriteAnimation](../include/svanes/sprite_animation_system.hpp) component we have is defined like this:

```cpp
struct SpriteAnimation {
    std::int32_t frame_width = 0;
    std::int32_t frame_height = 0;
    std::int32_t frame_count = 1;
    std::int32_t current_frame = 0;
    float seconds_per_frame = 0.1F;
    float elapsed_seconds = 0.0F;
};
```

Like stated above, this struct only contains the data that we want to store and action on. The entity's `Sprite` component owns the texture handle and source rectangle used for rendering; `SpriteAnimation` updates that source rectangle as frames advance.

The logic for this component is contained in a utility function that operates on the data contained within the struct.
For example the [AdvanceSpriteAnimations](sprite_animation_system.cpp) function is roughly defined like this:

```cpp
void AdvanceSpriteAnimations(Registry& registry, float delta_seconds)
{
    // Run a ForEach on our registry to get every entity that has both a <SpriteAnimation> and <Sprite> component.
    registry.ForEach<SpriteAnimation, Sprite>([delta_seconds](Entity, SpriteAnimation& animation, Sprite& sprite) {

        // Accumulate elapsed_seconds, advance the frame, and update sprite.source.
    });
}
```

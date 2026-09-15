## Entity Components

In our engine, our entities are composed of various components that define their behavior and properties.

This is opposed to a traditional inheritance-based approach, where entities would inherit from a base class and have their behavior defined by that class hierarchy.

### Components

Most components are data structs. Systems operate on entities containing the components they need.
A component can also be a class when it needs to maintain its own invariants: `Timeline`, for example, keeps its rational tic size and incomplete progress private. The timeline system advances these components before movement or animation reads their published local time.

### What this looks like

Creating a component is simply defining a struct with the data you want to store.
For example the [SpriteAnimation](../include/svanes/sprite_animation_system.hpp) component we have is defined like this:

```cpp
struct SpriteAnimation {
    std::int32_t frame_width = 0;
    std::int32_t frame_height = 0;
    std::int32_t frame_count = 1;
    std::int32_t current_frame = 0;
    TicCount tics_per_frame = SecondsToTics(0.1);
    TicCount elapsed_tics = 0;
};
```

Like stated above, this struct only contains the data that we want to store and action on. The entity's `Sprite` component owns the texture handle and source rectangle used for rendering; `SpriteAnimation` updates that source rectangle as frames advance.

The logic for this component is contained in a utility function that operates on the data contained within the struct.
For example the [AdvanceSpriteAnimations](sprite_animation_system.cpp) function is roughly defined like this:

```cpp
void AdvanceSpriteAnimations(Registry& registry)
{
    // Run a ForEach on our registry to get every entity that has SpriteAnimation, Sprite, and Timeline components.
    registry.ForEach<SpriteAnimation, Sprite, Timeline>([](Entity, SpriteAnimation& animation, Sprite& sprite, const Timeline& timeline) {

        // Accumulate timeline.GetDeltaTics(), advance the frame, and update sprite.source.
    });
}
```

Call `AdvanceTimelines` before these systems. Entities without a `Timeline` are excluded from time-dependent iteration. A root timeline uses real microseconds; a child uses its parent's local tics. Tic size `2/1` runs at half the parent's speed, while `1/2` runs at double speed.

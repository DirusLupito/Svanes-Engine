# Writing comments in Svanes Engine

If as an agent you have been allowed to write comments, this is the guide for you. Follow the style of the surrounding engine code. Each numbered section starts with the exact example the team flagged, followed by the reasoning and a revision.

## Learn the style from the engine

Read `CLAUDE.md` for the repository conventions. The team's recommendation was to read 10–20 engine headers and implementations together. These are the eight pairs used in the original review. Open each header alongside its implementation and notice which facts a caller needs and which facts help someone working on the algorithm.

| Header | Implementation | What to look for |
| --- | --- | --- |
| `include/svanes/utility/file_io.hpp` | `src/utility/file_io.cpp` | The header says what an empty file returns and which failures throw. The implementation explains why it checks the stream after reading. |
| `include/svanes/render/texture_manager.hpp` | `src/render/texture_manager.cpp` | Callers need the texture ownership rules. The byte pitch of an SDL upload belongs beside the upload. |
| `include/svanes/sprite_animation_system.hpp` | `src/sprite_animation_system.cpp` | The header explains when to advance animations and why applying the same timeline delta twice would be wrong. |
| `include/svanes/geometry/rectangle_geometry.hpp` | `src/geometry/rectangle_geometry.cpp` | The header defines what the coordinates mean. The bounds calculation explains how rounding its midpoint can move an edge. |
| `include/svanes/stable_id.hpp` | `src/stable_id.cpp` | The ordering guarantee matters across peers. The template definition lives in the header alongside its contract. |
| `include/svanes/utility/hash.hpp` | `src/utility/hash.cpp` | Naming the hash algorithm is useful when callers need reproducible values. |
| `include/svanes/network/message_serialization.hpp` | `src/network/message_serialization.cpp` | Byte order is part of the wire contract. The reason for checking a whole field before moving the cursor belongs beside the read. |
| `include/svanes/kinematic_system.hpp` | `src/kinematic_system.cpp` | The header gives component fields and their units. File-local helpers are documented at their definitions. |

Put the contract for a function, class, struct, or enum at its declaration in the header. For a static function used only in a `.cpp` file, put its documentation at the definition. For structs and enums, use the exact field names and enum values so readers can connect each description to the code. Inside a function, use a short comment where a choice, constraint, or edge case needs explaining.

## 1. A contract should survive a change of data

**Original example**

```sql
 * Builds the southern Red Sea map from the packaged elevation grid.
 * Each pixel represents one square kilometer, with north at the top.
```

> The pattern for you to learn is generality of comments so that they offer a (more or less) immutable contract (subject to change of course, so this immutability is a desire rather than a reality).

The test the team gave was to substitute the Gulf of Mexico, Hudson Bay, or randomly generated terrain. In all three cases the caller still wants an image it can display. The region, pixel scale, orientation, and source describe the current asset. A caller that actually relies on a particular unit, coordinate convention, or output format should find that guarantee in the contract.

```cpp
/**
 * Builds the image the client uses to display the terrain.
 *
 * @return An opaque RGBA image ready for TextureManager::CreateTexture().
 */
```

This excerpt illustrates the body and return value. Keep applicable `@throws` entries in the full declaration. When documenting the decoder itself, signed elevations and byte order matter because they explain how to interpret the bytes. Tuning values that are already clear from named constants seldom need repeating in prose.

## 2. A lifecycle contract should hold beyond the prototype

**Original example**

```cpp
    /**
     * Keeps the entire map in view as the window resizes and handles quit
     * input.
     *
     * @param context The current frame's input and engine services.
     */
    void Update(const svanes::ClientFrameContext &context) override;
```

Even after removing the region name, this description still treats Marching Orders as a terrain viewer. It is intended to be a War of Dots–like strategy game. Think through what happens when the player can select units or issue orders. The camera may follow one part of the battlefield, and the same callback will still process input and update local state. The engine's `IClient` interface tells us when that happens.

```cpp
    /**
     * Processes player input and updates client-side game state before
     * rendering each frame.
     *
     * @param context The current frame's input and engine services.
     */
    void Update(const svanes::ClientFrameContext &context) override;
```

Use the intended game to test how long a description will hold. Only promise behavior the code supports today. The same reasoning applies to initialization, joining, frame pacing, and shutdown. For example, a caller needs to know when to leave its frame loop even if the quit key changes.

## 3. Every documentation block needs a prose body

**Original example**

```cpp
    /**
     * @return True if the client should exit its frame loop, false otherwise.
     */
    bool ShouldQuit() const override;

    /**
     * @return The target interval between client frames, in microseconds.
     */
    svanes::TicCount GetFrameIntervalTics() const override;
```

Start every Doxygen block with a sentence describing the declaration's responsibility. Leave a blank comment line before tags such as `@param`, `@tparam`, `@return`, and `@throws`. This also applies to a simple getter or an override, even where an older engine example starts directly with a tag. Let the body explain the purpose and the tags supply details such as units, the meaning of a result, or a failure a caller must handle.

```cpp
    /**
     * Reports whether the client has requested shutdown.
     *
     * @return True to end the frame loop, false to continue.
     */
    bool ShouldQuit() const override;

    /**
     * Provides the interval the engine uses to pace the client.
     *
     * @return The target interval between client frames, in microseconds.
     */
    svanes::TicCount GetFrameIntervalTics() const override;
```

## 4. Comment each global and particularly important locals

**Original example**

```cpp
// BIL stores one signed 16-bit sample per cell. The output image stores
// one byte per channel in RGBA order.
constexpr auto kBytesPerElevation = sizeof(std::int16_t);
constexpr auto kBitsPerByte = std::numeric_limits<std::uint8_t>::digits;
constexpr std::size_t kChannelsPerPixel = 4;

// Water starts at sea level and reaches its deepest color at this depth.
constexpr float kSeaLevelMeters = 0.0F;
constexpr float kDeepWaterDepthMeters = 1300.0F;
constexpr svanes::Color kShallowWaterColor{
    .red = 105, .green = 174, .blue = 178};
constexpr svanes::Color kDeepWaterColor{.red = 33, .green = 88, .blue = 121};

// Land starts at the sea-level temperature and cools with elevation.
constexpr float kSeaLevelCelsius = 30.0F;
constexpr float kCoolingCelsiusPerMeter = 0.0065F;

// Land colors blend from lowland at sea level, through upland at its
// temperature threshold, to ridge at the coldest threshold.
constexpr float kUplandCelsius = 16.0F;
constexpr float kRidgeCelsius = 0.0F;
constexpr svanes::Color kLowlandColor{.red = 222, .green = 200, .blue = 151};
constexpr svanes::Color kUplandColor{.red = 146, .green = 159, .blue = 122};
constexpr svanes::Color kRidgeColor{.red = 203, .green = 201, .blue = 185};

// A larger rise scale gives the same slope less contrast. The adjustment
// limits are fractions of the base land color's brightness.
constexpr float kHillshadeRiseScaleMeters = 1050.0F;
constexpr float kMinHillshadeAdjustment = -0.26F;
constexpr float kMaxHillshadeAdjustment = 0.22F;
```

For globals, the team’s rule is “Every individual var gets its own inline comment. We do not give a single comment to multiple vars.” Place a separate `//` comment directly above each global variable, including each constant in an anonymous namespace. Describe what that particular value represents. Include units, a reference point, or the effect of changing it when that helps the reader.

Inside a function, reserve individual comments for particularly important locals. For example, the square root in the terrain depth calculation gives shallow coastal waters more of the color range. That choice deserves an explanation. Routine loop indices, byte offsets, and other locals with clear names can stand on their own.

**Example the team approved**

```cpp

// Samples along each edge of the packaged square elevation grid.
constexpr std::int32_t kTerrainSide = 880;
```

For example, the two water levels answer different questions. Document each where it is declared.

```cpp
// Highest elevation, in meters, that the map treats as water.
constexpr float kSeaLevelMeters = 0.0F;

// Depth below sea level, in meters, where the deep-water color is reached.
constexpr float kDeepWaterDepthMeters = 1300.0F;
```

When reviewing a comment, ask what its reader can rely on. If changing the asset, controls, or implementation would force a rewrite, check whether that detail is a real guarantee. If the guarantee itself changes, update the comment with the code.

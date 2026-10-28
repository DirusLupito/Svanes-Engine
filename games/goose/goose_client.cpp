#include "arena.hpp"
#include "bullets.hpp"
#include "enemy.hpp"
#include "goose.hpp"
#include "minions.hpp"
#include "pause_menu.hpp"
#include "protocol.hpp"

#include <svanes/asset_path.hpp>
#include <svanes/camera2d.hpp>
#include <svanes/input.hpp>
#include <svanes/network/clientserver/IClient.hpp>
#include <svanes/network/clientserver/network_client.hpp>
#include <svanes/registry.hpp>
#include <svanes/render/render_system.hpp>
#include <svanes/render/texture_manager.hpp>
#include <svanes/sprite_animation_system.hpp>
#include <svanes/timeline_system.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace {

// how much larger than the view the sky and death tint are drawn, so their edges
// stay offscreen
constexpr float kSkyMargin = 1.5F;

// drawn over the world while the player's goose is dead, but under the pause menu
constexpr svanes::Color kDeathTintColor{
    .red = 90, .green = 90, .blue = 90, .alpha = 150};
constexpr std::int32_t kDeathTintZ = 500;

constexpr float kBossBarWidthFraction = 0.45F;
constexpr float kBossBarHeight = 28.0F;
constexpr float kBossBarTop = 30.0F;
constexpr std::int32_t kBossBarZ = 600;

// where in the window the goose is held, as a fraction of the output size, placing
// it centered horizontally and below center vertically to show more of what is ahead
constexpr float kCameraAnchorX = 0.5F;
constexpr float kCameraAnchorY = 0.68F;

constexpr std::int32_t kGooseFrameWidth = 29;
constexpr std::int32_t kGooseFrameHeight = 27;
constexpr std::int32_t kGooseWalkFrames = 4;
constexpr double kWalkSecondsPerFrame = 0.1;
constexpr double kFlySecondsPerFrame = 0.06;

constexpr svanes::Color kEnemyColor{200, 60, 60, 255};
constexpr svanes::Color kGooseBulletColor{255, 230, 120, 255};
constexpr svanes::Color kMinionColor{.red = 230, .green = 120, .blue = 30};

// the health bar spans the goose's width and floats just above its head
constexpr float kHealthBarHeight = 5.0F;
constexpr float kHealthBarGap = 6.0F;
constexpr svanes::Color kHealthColor{.red = 60, .green = 200, .blue = 60};
constexpr svanes::Color kMissingHealthColor{.red = 200, .green = 40, .blue = 40};
constexpr svanes::Color kEnemyBulletColor{255, 90, 90, 255};

const svanes::TicCount kDoubleTapTics = svanes::SecondsToTics(0.25);

/**
 * Subtracts elapsed time from a countdown without unsigned underflow.
 *
 * @param remaining The countdown to advance.
 * @param delta_tics The elapsed tics.
 */
void AdvanceCountdown(svanes::TicCount &remaining,
                      svanes::TicCount delta_tics) {
    remaining -= std::min(remaining, delta_tics);
}

/**
 * Turns a second press of a key within the double-tap window into a dash.
 *
 * @param pressed Whether the key was pressed this frame.
 * @param window The time left to press it again, counted down by the caller.
 * @return Whether this press completed a double tap.
 */
bool CompletesDoubleTap(bool pressed, svanes::TicCount &window) {
    if (!pressed) {
        return false;
    }
    if (window > 0) {
        window = 0;
        return true;
    }
    window = kDoubleTapTics;
    return false;
}

/**
 * A goose the client draws.
 *
 * FIELDS:
 * - entity: The entity drawing it.
 * - state: The animation its sprite is set up for.
 * - health_bar: The entity drawing the left part of its health bar, sized to
 *   the health it has.
 * - missing_health_bar: The entity drawing the right part of its health bar,
 *   sized to the health it has lost.
 */
struct DrawnGoose {
    svanes::Entity entity;
    GooseState state;
    svanes::Entity health_bar;
    svanes::Entity missing_health_bar;
};

/**
 * Moves and resizes one part of a health bar.
 *
 * @param world The registry holding the part.
 * @param entity The part's entity.
 * @param area The part's world center and size.
 */
void PlaceHealthBarPart(svanes::Registry &world, svanes::Entity entity,
                        const svanes::Rectangle2D &area) {
    svanes::Transform &transform = world.GetComponent<svanes::Transform>(entity);
    transform.x = area.x;
    transform.y = area.y;
    world.GetComponent<svanes::SolidShape>(entity).geometry =
        svanes::Rectangle2D{.width = area.width, .height = area.height};
}

/**
 * Lays out a health bar, split between the health remaining on the left and
 * the health lost on the right.
 *
 * @param world The registry holding the bar's parts.
 * @param filled_part The entity drawing the health remaining.
 * @param missing_part The entity drawing the health lost.
 * @param bar The whole bar's world center and size.
 * @param health The health remaining, from zero up to max_health.
 * @param max_health The health a full bar stands for.
 */
void PlaceHealthBar(svanes::Registry &world, svanes::Entity filled_part,
                    svanes::Entity missing_part, const svanes::Rectangle2D &bar,
                    std::uint32_t health, std::uint32_t max_health) {
    const float filled = bar.width * static_cast<float>(health) /
                         static_cast<float>(max_health);
    const float missing = bar.width - filled;
    const float left = bar.x - bar.width * 0.5F;

    PlaceHealthBarPart(world, filled_part,
                       {left + filled * 0.5F, bar.y, filled, bar.height});
    PlaceHealthBarPart(world, missing_part, {left + filled + missing * 0.5F,
                                             bar.y, missing, bar.height});
}

/**
 * The goose game's window. It sends the server what the player wants each
 * frame and draws the latest snapshot the server sent back. It simulates
 * nothing; only animations run locally, on a clock that copies the server's.
 */
class GooseClient final : public svanes::IClient {
public:
    /**
     * @param server_address The server's address, in IPv4:PORT form.
     * @param text_snapshots Whether the server sends snapshots as text rather
     * than binary.
     */
    GooseClient(std::string server_address, bool text_snapshots)
        : server_address(std::move(server_address)),
          text_snapshots(text_snapshots) {}

    void Initialize(svanes::ClientContext &context) override {
        svanes::Registry &world = context.world;

        background = world.CreateEntity();
        world.AddComponent<svanes::Transform>(background);
        world.AddComponent<svanes::SolidShape>(
            background,
            svanes::SolidShape{.color = svanes::Color{.blue = 255},
                               .geometry = svanes::Rectangle2D{}});
        // drawn behind everything else, and resized to cover the view each frame in Update
        world.AddComponent<svanes::ZOrder>(background, svanes::ZOrder{-100});

        death_tint = world.CreateEntity();
        world.AddComponent<svanes::Transform>(death_tint);
        svanes::Color hidden_tint = kDeathTintColor;
        hidden_tint.alpha = 0;
        world.AddComponent<svanes::SolidShape>(
            death_tint, svanes::SolidShape{.color = hidden_tint,
                                           .geometry = svanes::Rectangle2D{}});
        world.AddComponent<svanes::ZOrder>(death_tint,
                                           svanes::ZOrder{kDeathTintZ});

        boss_bar = CreateHealthBarPart(world, kHealthColor);
        missing_boss_bar = CreateHealthBarPart(world, kMissingHealthColor);
        world.AddComponent<svanes::ZOrder>(boss_bar, svanes::ZOrder{kBossBarZ});
        world.AddComponent<svanes::ZOrder>(missing_boss_bar,
                                           svanes::ZOrder{kBossBarZ});

        clock = world.CreateEntity();
        world.AddComponent<svanes::Timeline>(clock);

        CreateArena(world);
        CreateOrb(context);

        enemy = world.CreateEntity();
        world.AddComponent<svanes::Transform>(enemy);
        world.AddComponent<svanes::SolidShape>(
            enemy, svanes::SolidShape{
                       .color = kEnemyColor,
                       .geometry = svanes::Rectangle2D{.width = EnemySize,
                                                       .height = EnemySize}});

        const std::string assets = svanes::AssetPath(GOOSE_ASSETS_DIR);
        idle_texture = context.assets.LoadTexture(assets + "/goose.png");
        walk_texture = context.assets.LoadTexture(assets + "/goose_walk.png");

        menu.emplace(context);

        context.network.Connect(server_address);
        std::cout << "Joining the goose server at " << server_address << ".\n";
    }

    svanes::NetworkMessage CreateJoinRequest() override { return {}; }

    /**
     * @throws std::runtime_error if the server rejected this client or did not
     * answer.
     */
    void HandleJoinResult(const svanes::JoinResult &result) override {
        if (result.status != svanes::JoinStatus::Accepted) {
            throw std::runtime_error("Could not join the goose server at " +
                                     server_address + ".");
        }
        player = result.client_id;
        std::cout << "Joined as player " << player << ".\n";
    }

    /**
     * @throws std::runtime_error always, since the goose game cannot run
     * without its server.
     */
    void HandleDisconnect() override {
        throw std::runtime_error("Lost the goose server at " + server_address +
                                 ".");
    }

    void Update(const svanes::ClientFrameContext &frame) override {
        const auto snapshots = frame.network.PollMessages();
        if (!snapshots.empty()) {
            ApplySnapshot(frame.world,
                          text_snapshots ? DecodeSnapshotText(snapshots.back())
                                         : DecodeSnapshot(snapshots.back()));
        }

        svanes::AdvanceTimelines(frame.world, frame.real_delta_tics);
        svanes::AdvanceSpriteAnimations(frame.world);

        const bool alone = geese.size() == 1;
        if (!alone) {
            menu->ResetSpeed();
        }
        menu->HandleInput(frame, alone);
        should_quit = should_quit || menu->TakeQuitRequest();
        if (const std::optional<bool> request =
                menu->TakeMinionSpawningRequest()) {
            pending_minion_spawning = request;
        }

        if (frame.input.WasPressed(svanes::Key::Tab)) {
            frame.camera.scale_mode =
                frame.camera.scale_mode == svanes::ScaleMode::Constant
                    ? svanes::ScaleMode::Proportional
                    : svanes::ScaleMode::Constant;
        }

        if (should_quit && frame.network.IsConnected()) {
            frame.network.Disconnect();
        }

        const PlayerInput input{
            ReadIntent(frame),
            ClockSetting{.speed = menu->Speed(), .paused = menu->IsOpen()},
            pending_minion_spawning};
        if (frame.network.IsConnected()) {
            frame.network.Send(EncodePlayerInput(input));
        }

        UpdateCamera(frame);
        PlaceBossBar(frame);
        menu->Draw(frame, alone);
    }

    bool ShouldQuit() const override { return should_quit; }

    svanes::TicCount GetFrameIntervalTics() const override {
        switch (menu->LoopRate()) {
        case GooseSpeed::Half:
            return svanes::SecondsToTics(1.0 / 15.0);
        case GooseSpeed::Normal:
            return svanes::SecondsToTics(1.0 / 60.0);
        case GooseSpeed::Double:
            return svanes::SecondsToTics(1.0 / 120.0);
        }
        throw std::invalid_argument("GooseClient received an unknown loop rate.");
    }

private:
    /**
     * Creates the animated orb drawn in the sky.
     *
     * @param context The world to create it in and the textures to load.
     */
    void CreateOrb(svanes::ClientContext &context) {
        const svanes::TextureHandle texture = context.assets.LoadTexture(
            svanes::AssetPath(GOOSE_ASSETS_DIR) +
            "/darkworld_spawn_swirlingorb_idle.png");

        const svanes::Entity orb = context.world.CreateEntity();
        context.world.AddComponent<svanes::Transform>(
            orb, svanes::Transform{.x = 960.0F, .y = 540.0F});
        context.world.AddComponent<svanes::Sprite>(
            orb, svanes::Sprite{
                     .texture = texture,
                     .source = svanes::Rectangle2D{64.0F, 64.0F, 128.0F,
                                                   128.0F},
                     .geometry = svanes::Rectangle2D{.width = 128.0F,
                                                     .height = 128.0F}});
        context.world.AddComponent<svanes::Timeline>(orb,
                                                     svanes::Timeline{clock});
        context.world.AddComponent<svanes::SpriteAnimation>(
            orb, svanes::SpriteAnimation{
                     .frame_width = 128,
                     .frame_height = 128,
                     .frame_count = 4,
                     .tics_per_frame = svanes::SecondsToTics(0.12)});
    }

    /**
     * Reads what the player wants from the keyboard and mouse. While the menu
     * is open the goose gets no input, so the menu's keys do not move it.
     *
     * @param frame The frame's input and camera.
     * @return The player's intent for this frame.
     */
    GooseIntent ReadIntent(const svanes::ClientFrameContext &frame) {
        AdvanceCountdown(left_tap_window, frame.real_delta_tics);
        AdvanceCountdown(right_tap_window, frame.real_delta_tics);

        const svanes::InputManager &keys = frame.input;
        if (menu->IsOpen()) {
            return {};
        }

        GooseIntent intent{};
        intent.move = (keys.IsDown(svanes::Key::D) ? 1.0F : 0.0F) -
                      (keys.IsDown(svanes::Key::A) ? 1.0F : 0.0F);
        intent.jump = keys.IsDown(svanes::Key::Space) ||
                      keys.WasPressed(svanes::Key::Space);
        intent.fire =
            keys.IsMouseButtonDown(svanes::MouseButton::Left) ||
            keys.WasMouseButtonPressed(svanes::MouseButton::Left);

        if (CompletesDoubleTap(keys.WasPressed(svanes::Key::A),
                               left_tap_window)) {
            intent.dash = -1.0F;
        }
        if (CompletesDoubleTap(keys.WasPressed(svanes::Key::D),
                               right_tap_window)) {
            intent.dash = 1.0F;
        }

        const auto own = geese.find(player);
        if (intent.fire && own != geese.end()) {
            const svanes::Vector2D mouse = keys.MousePosition();
            const svanes::Rectangle2D aim =
                frame.camera.ScreenToWorld({mouse.x, mouse.y, 0.0F, 0.0F});
            const svanes::Transform &goose =
                frame.world.GetComponent<svanes::Transform>(own->second.entity);
            intent.aim = {aim.x - goose.x, aim.y - goose.y};
        }
        return intent;
    }

    /**
     * Makes the drawn world and the menu match a snapshot, and stops sending a
     * minion spawning request once the server has carried it out.
     *
     * @param world The registry holding everything drawn.
     * @param snapshot The latest snapshot from the server.
     */
    void ApplySnapshot(svanes::Registry &world, const Snapshot &snapshot) {
        menu->ShowMinionSpawning(snapshot.minion_spawning);
        if (pending_minion_spawning == snapshot.minion_spawning) {
            pending_minion_spawning.reset();
        }

        svanes::Timeline &timeline = world.GetComponent<svanes::Timeline>(clock);
        timeline.SetTicSize(TicSize(snapshot.clock.speed));
        if (snapshot.clock.paused) {
            timeline.Pause();
        } else {
            timeline.Unpause();
        }

        std::unordered_map<svanes::ClientId, DrawnGoose> remaining =
            std::move(geese);
        geese.clear();
        for (const SnapshotGoose &goose : snapshot.geese) {
            const auto found = remaining.find(goose.client);
            DrawnGoose drawn{};
            if (found == remaining.end()) {
                drawn = CreateGoose(world);
            } else {
                drawn = found->second;
                remaining.erase(found);
            }

            svanes::Transform &transform =
                world.GetComponent<svanes::Transform>(drawn.entity);
            transform.x = goose.position.x;
            transform.y = goose.position.y;
            if (drawn.state != goose.state) {
                ShowState(world, drawn.entity, goose.state);
                drawn.state = goose.state;
            }
            const svanes::Rectangle2D above_goose{
                goose.position.x,
                goose.position.y - GooseHeight * 0.5F - kHealthBarGap -
                    kHealthBarHeight * 0.5F,
                GooseWidth, kHealthBarHeight};
            PlaceHealthBar(world, drawn.health_bar, drawn.missing_health_bar,
                           above_goose, goose.health, GooseMaxHealth);
            if (goose.client == player) {
                world.GetComponent<svanes::SolidShape>(death_tint).color.alpha =
                    goose.health == 0 ? kDeathTintColor.alpha : 0;
            }
            geese.emplace(goose.client, drawn);
        }
        for (const auto &[client, departed] : remaining) {
            world.DestroyEntity(departed.entity);
            world.DestroyEntity(departed.health_bar);
            world.DestroyEntity(departed.missing_health_bar);
        }

        if (snapshot.enemy) {
            svanes::Transform &enemy_transform =
                world.GetComponent<svanes::Transform>(enemy);
            enemy_transform.x = snapshot.enemy->position.x;
            enemy_transform.y = snapshot.enemy->position.y;
        }
        boss_health = snapshot.enemy ? snapshot.enemy->health : 0;
        world.GetComponent<svanes::SolidShape>(enemy).color.alpha =
            snapshot.enemy ? 255 : 0;

        for (const svanes::Entity bullet : bullets) {
            world.DestroyEntity(bullet);
        }
        bullets.clear();
        for (const SnapshotBullet &shot : snapshot.bullets) {
            const svanes::Entity bullet = world.CreateEntity();
            world.AddComponent<svanes::Transform>(
                bullet,
                svanes::Transform{.x = shot.position.x, .y = shot.position.y});
            world.AddComponent<svanes::SolidShape>(
                bullet,
                svanes::SolidShape{
                    .color =
                        shot.from_enemy ? kEnemyBulletColor : kGooseBulletColor,
                    .geometry = svanes::Rectangle2D{.width = BulletSize,
                                                    .height = BulletSize}});
            bullets.push_back(bullet);
        }

        for (const svanes::Entity part : minion_parts) {
            world.DestroyEntity(part);
        }
        minion_parts.clear();
        for (const SnapshotMinion &minion : snapshot.minions) {
            const bool archer = minion.kind == MinionKind::Archer;
            const float width = archer ? ArcherMinionWidth : MeleeMinionWidth;
            const float height =
                archer ? ArcherMinionHeight : MeleeMinionHeight;

            const svanes::Entity body = world.CreateEntity();
            world.AddComponent<svanes::Transform>(
                body, svanes::Transform{.x = minion.position.x,
                                        .y = minion.position.y});
            world.AddComponent<svanes::SolidShape>(
                body, svanes::SolidShape{
                          .color = kMinionColor,
                          .geometry = svanes::Rectangle2D{.width = width,
                                                          .height = height}});
            minion_parts.push_back(body);

            if (minion.health < MinionMaxHealth) {
                const svanes::Entity filled =
                    CreateHealthBarPart(world, kHealthColor);
                const svanes::Entity missing =
                    CreateHealthBarPart(world, kMissingHealthColor);
                const svanes::Rectangle2D above_minion{
                    minion.position.x,
                    minion.position.y - height * 0.5F - kHealthBarGap -
                        kHealthBarHeight * 0.5F,
                    width, kHealthBarHeight};
                PlaceHealthBar(world, filled, missing, above_minion,
                               minion.health, MinionMaxHealth);
                minion_parts.push_back(filled);
                minion_parts.push_back(missing);
            }
        }
    }

    /**
     * @param world The registry to create it in.
     * @return A newly drawn goose, standing idle, with an unplaced health bar.
     */
    DrawnGoose CreateGoose(svanes::Registry &world) {
        const svanes::Entity entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(entity);
        world.AddComponent<svanes::Sprite>(
            entity,
            svanes::Sprite{.texture = idle_texture,
                           .geometry = svanes::Rectangle2D{
                               .width = GooseWidth, .height = GooseHeight}});
        world.AddComponent<svanes::Timeline>(entity, svanes::Timeline{clock});
        return {entity, GooseState::Idle,
                CreateHealthBarPart(world, kHealthColor),
                CreateHealthBarPart(world, kMissingHealthColor)};
    }

    /**
     * @param world The registry to create it in.
     * @param color The color to draw it in.
     * @return A part of a health bar, placed by PlaceHealthBar.
     */
    svanes::Entity CreateHealthBarPart(svanes::Registry &world,
                                       svanes::Color color) {
        const svanes::Entity entity = world.CreateEntity();
        world.AddComponent<svanes::Transform>(entity);
        world.AddComponent<svanes::SolidShape>(
            entity, svanes::SolidShape{.color = color,
                                       .geometry = svanes::Rectangle2D{}});
        return entity;
    }

    /**
     * Swaps a goose's texture and animation to show a state.
     *
     * @param world The registry holding the goose.
     * @param entity The goose's entity.
     * @param state The state to show.
     */
    void ShowState(svanes::Registry &world, svanes::Entity entity,
                   GooseState state) {
        svanes::Sprite &sprite = world.GetComponent<svanes::Sprite>(entity);
        if (state == GooseState::Idle) {
            sprite.texture = idle_texture;
            sprite.source = std::nullopt;
            world.RemoveComponent<svanes::SpriteAnimation>(entity);
            return;
        }

        const double seconds_per_frame = state == GooseState::Flying
                                             ? kFlySecondsPerFrame
                                             : kWalkSecondsPerFrame;
        sprite.texture = walk_texture;
        sprite.source = svanes::Rectangle2D{
            kGooseFrameWidth * 0.5F, kGooseFrameHeight * 0.5F,
            static_cast<float>(kGooseFrameWidth),
            static_cast<float>(kGooseFrameHeight)};
        world.AddComponent<svanes::SpriteAnimation>(
            entity, svanes::SpriteAnimation{
                        .frame_width = kGooseFrameWidth,
                        .frame_height = kGooseFrameHeight,
                        .frame_count = kGooseWalkFrames,
                        .tics_per_frame =
                            svanes::SecondsToTics(seconds_per_frame)});
    }

    /**
     * Pins the boss's health bar to the top center of the view, hidden once
     * the boss is dead. Call after the camera has moved.
     *
     * @param frame The frame's world and camera.
     */
    void PlaceBossBar(const svanes::ClientFrameContext &frame) {
        const svanes::Rectangle2D viewport = frame.camera.Viewport();
        const float width =
            boss_health > 0 ? viewport.width * kBossBarWidthFraction : 0.0F;
        const svanes::Rectangle2D bar = frame.camera.ScreenToWorld(
            {viewport.x,
             viewport.y - viewport.height * 0.5F + kBossBarTop +
                 kBossBarHeight * 0.5F,
             width, kBossBarHeight});
        PlaceHealthBar(frame.world, boss_bar, missing_boss_bar, bar,
                       boss_health, EnemyMaxHealth);
    }

    /**
     * Keeps the camera on the player's goose, once it exists, and sizes the
     * sky and death tint to the view.
     *
     * @param frame The frame's world and camera.
     */
    void UpdateCamera(const svanes::ClientFrameContext &frame) {
        const auto own = geese.find(player);
        if (own != geese.end()) {
            // the camera follows by finding which world position currently sits at the
            // anchor point on screen, then shifting by however far the goose is from it.
            // Working through the anchor this way keeps the follow correct at any zoom,
            // scale mode or window size, since the camera itself does that conversion.
            const svanes::Transform &goose =
                frame.world.GetComponent<svanes::Transform>(own->second.entity);
            const svanes::Rectangle2D anchor = frame.camera.ScreenToWorld(
                {frame.output_width * kCameraAnchorX,
                 frame.output_height * kCameraAnchorY, 0.0F, 0.0F});
            frame.camera.x += goose.x - anchor.x;
            frame.camera.y += goose.y - anchor.y;
        }

        const svanes::Rectangle2D view = frame.camera.ScreenToWorld(
            {frame.output_width * 0.5F, frame.output_height * 0.5F,
             static_cast<float>(frame.output_width),
             static_cast<float>(frame.output_height)});

        for (const svanes::Entity cover : {background, death_tint}) {
            svanes::Transform &transform =
                frame.world.GetComponent<svanes::Transform>(cover);
            transform.x = view.x;
            transform.y = view.y;

            svanes::Rectangle2D &body = std::get<svanes::Rectangle2D>(
                frame.world.GetComponent<svanes::SolidShape>(cover).geometry);
            body.width = view.width * kSkyMargin;
            body.height = view.height * kSkyMargin;
        }
    }

    std::string server_address;
    bool text_snapshots;
    svanes::ClientId player = 0;
    std::optional<PauseMenu> menu;
    bool should_quit = false;
    std::optional<bool> pending_minion_spawning;

    svanes::TicCount left_tap_window = 0;
    svanes::TicCount right_tap_window = 0;

    svanes::TextureHandle idle_texture{};
    svanes::TextureHandle walk_texture{};

    svanes::Entity clock = 0;

    // the sky, kept centered on the camera and sized to cover the view every frame
    svanes::Entity background = 0;

    // the gray shown over the view while the player's goose is dead, kept covering
    // the view the same way as the sky
    svanes::Entity death_tint = 0;

    svanes::Entity enemy = 0;
    svanes::Entity boss_bar = 0;
    svanes::Entity missing_boss_bar = 0;
    std::uint32_t boss_health = 0;
    std::unordered_map<svanes::ClientId, DrawnGoose> geese;
    std::vector<svanes::Entity> bullets;
    std::vector<svanes::Entity> minion_parts;
};

} // namespace

int32_t main(int32_t argc, char **argv) {
    const bool text_snapshots =
        argc == 3 && std::string_view{argv[2]} == "--text";
    if (argc < 2 || argc > 3 || (argc == 3 && !text_snapshots) ||
        std::string_view{argv[1]} == "--help") {
        std::cerr << "Usage: svanes_game_goose IPv4:PORT [--text]\n"
                  << "Joins the goose server at that address. The server "
                     "listens on port 46000. Pass --text if the server "
                     "was started with --text.\n";
        return argc == 2 ? 0 : 1;
    }

    try {
        svanes::NetworkClient network({
            .title = "Titled Goose Game",
            .width = 640,
            .height = 900,
        });
        GooseClient client(argv[1], text_snapshots);
        return network.run(client);
    } catch (const std::exception &error) {
        std::cerr << "Titled Goose Game: " << error.what() << '\n';
        return 1;
    }
}

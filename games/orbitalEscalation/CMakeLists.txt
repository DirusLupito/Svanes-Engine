add_library(
    svanes_orbital_common STATIC
    player_events.cpp
    player_events.hpp
    world_snapshot_event.cpp
    world_snapshot_event.hpp
    attachment_system.cpp
    attachment_system.hpp
    collision_response.cpp
    collision_response.hpp
    propulsion_system.cpp
    propulsion_system.hpp
    combat/damage_system.cpp
    combat/damage_system.hpp
    game_objects/owned_entity.cpp
    game_objects/owned_entity.hpp
    game_objects/spawn_zones.cpp
    game_objects/spawn_zones.hpp
    game_objects/deletion_boxes.cpp
    game_objects/deletion_boxes.hpp
    serialization/geometry_serialization.cpp
    serialization/geometry_serialization.hpp
    serialization/world_update.cpp
    serialization/world_update.hpp
    serialization/json.cpp
    serialization/json.hpp
    combat/weapon_system.cpp
    combat/weapon_system.hpp
    combat/missile_system.cpp
    combat/missile_system.hpp
    effects/collision_flashes.cpp
    effects/collision_flashes.hpp
    game_objects/dynamic_object/missile_launcher.cpp
    game_objects/dynamic_object/missile_launcher.hpp
    serialization/missile_launcher_serialization.cpp
    serialization/missile_launcher_serialization.hpp
    serialization/asset_catalog.cpp
    serialization/asset_catalog.hpp
    serialization/dynamic_object_serialization.cpp
    serialization/dynamic_object_serialization.hpp
    serialization/missile_serialization.cpp
    serialization/missile_serialization.hpp
    game_objects/dynamic_object/missile.cpp
    game_objects/dynamic_object/missile.hpp
    serialization/ship_serialization.cpp
    serialization/ship_serialization.hpp
    game_objects/dynamic_object/dynamic_object.cpp
    game_objects/dynamic_object/dynamic_object.hpp
    game_objects/dynamic_object/ship.cpp
    game_objects/dynamic_object/ship.hpp
    game_objects/planet.cpp
    game_objects/planet.hpp
    game_objects/world_obstacles.cpp
    game_objects/world_obstacles.hpp
    controls.cpp
    controls.hpp
    game_objects/visuals.cpp
    game_objects/visuals.hpp
)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/nlohmann_json-src"
)
FetchContent_MakeAvailable(nlohmann_json)
target_link_libraries(svanes_orbital_common PUBLIC
    Svanes::Engine nlohmann_json::nlohmann_json
)
add_executable(svanes_game_orbital_escalation
    client_main.cpp
    orbital_escalation_client.cpp
    orbital_escalation_client.hpp
)
target_link_libraries(svanes_game_orbital_escalation PRIVATE svanes_orbital_common)
add_executable(svanes_game_orbital_escalation_server
    server_main.cpp
    OrbitalEscalationServer.hpp
    OrbitalEscalationServer.cpp
)
target_link_libraries(svanes_game_orbital_escalation_server PRIVATE svanes_orbital_common)


svanes_game_package(svanes_game_orbital_escalation orbitalEscalation orbitalEscalation orbitalEscalation)
add_dependencies(svanes_game_orbital_escalation_server svanes_game_orbital_escalation_assets)

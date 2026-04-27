FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.12.0)
FetchContent_MakeAvailable(json)

add_executable(server
        server.cpp
        ../src/serializer.cpp
)
target_link_libraries(server PRIVATE core_base nlohmann_json::nlohmann_json)
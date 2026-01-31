set(SERVER_NAME "server")

# collect all file implementations (src and headers) for subproject
file(GLOB_RECURSE SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB_RECURSE HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# Find the packages provided by vcpkg.
find_package(dpp CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
# Added missing JSON dependency
find_package(nlohmann_json CONFIG REQUIRED) 

# Create executable with given src files
add_executable(server
    ${SOURCES}
)

# Make header files in this directory available to current project.
target_include_directories(${SERVER_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# Link server executable against dependencies
target_link_libraries(${SERVER_NAME} PRIVATE
    dpp::dpp
    unofficial::sqlite3::sqlite3
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

set(CONFIG_FILE "${CMAKE_SOURCE_DIR}/LeagueOfGains.cfg")

add_custom_command(TARGET ${SERVER_NAME} POST_BUILD
    # A) Copy the Config File
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CONFIG_FILE}"
        "$<TARGET_FILE_DIR:${SERVER_NAME}>/LeagueOfGains.cfg"
    
    # B) Copy Dependency DLLs (DPP, ZLib, SSL, etc.)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${SERVER_NAME}>
        $<TARGET_FILE_DIR:${SERVER_NAME}>
    
    COMMAND_EXPAND_LISTS
    COMMENT "Copying Configuration and Runtime DLLs to binary directory..."
)
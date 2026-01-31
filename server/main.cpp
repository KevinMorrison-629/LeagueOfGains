#include "server/core/TaskManager.h"
#include "server/database/Database.h"
#include "server/discord/Bot.h"
#include "server/riot/RiotClient.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Config
{
    std::string bot_token;
    std::string application_id;
    std::string riot_key;
    std::string db_file;
    int thread_count = 4;
    std::vector<Server::DB::ExerciseDefinition> exercises;
};

// Helper to load config
Config LoadConfig(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open configuration file: " + path);
    }

    json j;
    file >> j;

    Config cfg;
    try
    {
        cfg.bot_token = j.at("bot_token").get<std::string>();
        cfg.riot_key = j.at("riot_api_key").get<std::string>();

        // Optional fields with defaults
        cfg.application_id = j.value("application_id", "");
        cfg.db_file = j.value("database_file", "league_fitness.db");
        cfg.thread_count = j.value("thread_pool_size", 4);

        if (j.contains("exercises") && j["exercises"].is_array())
        {
            for (const auto &item : j["exercises"])
            {
                Server::DB::ExerciseDefinition ex;
                ex.id = 0; // ID is auto-assigned by DB
                ex.name = item.value("name", "Unnamed Exercise");
                ex.set_count = item.value("count", 10);
                ex.type = item.value("type", "core");
                cfg.exercises.push_back(ex);
            }
        }
    }
    catch (const json::exception &e)
    {
        throw std::runtime_error("Config parsing error: " + std::string(e.what()));
    }

    return cfg;
}

int main()
{
    try
    {
        // 1. Load Configuration
        std::cout << "Loading configuration from LeagueOfGains.cfg..." << std::endl;
        Config cfg = LoadConfig("LeagueOfGains.cfg");

        if (cfg.bot_token == "YOUR_DISCORD_BOT_TOKEN_HERE" || cfg.riot_key == "YOUR_RIOT_API_KEY_HERE")
        {
            std::cerr << "⚠️  Please update LeagueOfGains.cfg with your actual credentials." << std::endl;
            return 1;
        }

        // 2. Initialize Core Components
        std::cout << "Initializing Discord Cluster..." << std::endl;
        auto botCluster = std::make_shared<dpp::cluster>(cfg.bot_token);

        std::cout << "Initializing Database..." << std::endl;
        auto db = std::make_shared<Server::DB::Database>(cfg.db_file);

        // Seed exercises from config
        db->SeedExercises(cfg.exercises);

        std::cout << "Initializing Riot Client..." << std::endl;
        auto riot = std::make_shared<Server::Riot::RiotClient>(botCluster, cfg.riot_key);

        // 3. Shared Context
        auto ctx = std::make_shared<Core::Utils::AppContext>();
        ctx->bot = botCluster;
        ctx->db = db;
        ctx->riot = riot;

        // 4. Task Manager
        std::cout << "Starting Task Manager with " << cfg.thread_count << " threads..." << std::endl;
        auto taskManager = std::make_shared<Core::Utils::TaskManager>(cfg.thread_count, ctx);

        // 5. Bot Wrapper
        Core::Discord::Bot botApp;
        botApp.Initialize(botCluster, ctx, taskManager);

        // 6. Run
        std::cout << "Starting Bot..." << std::endl;
        botApp.Run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
#pragma once

#include "server/database/Database.h"
#include "server/riot/RiotClient.h"
#include <dpp/dpp.h>
#include <functional>
#include <memory>

namespace Core::Utils
{
    // Forward declaration of Task to avoid circular include of TaskManager
    class Task;

    // Shared Resources passed to tasks and commands
    struct AppContext
    {
        std::shared_ptr<dpp::cluster> bot;
        std::shared_ptr<Server::DB::Database> db;
        std::shared_ptr<Server::Riot::RiotClient> riot;

        // Helper to add tasks back to queue (implementation in TaskManager)
        std::function<void(std::unique_ptr<Task>)> submitTask;
    };
} // namespace Core::Utils
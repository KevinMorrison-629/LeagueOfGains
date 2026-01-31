#pragma once
#include "server/commands/CommandSystem.h"
#include "server/core/TaskManager.h"
#include <chrono>
#include <map>
#include <mutex>

namespace Core::Commands::Impl
{
    class CmdForceFetch : public ICommand
    {
    private:
        // Mutex to protect the cooldown map from concurrent access
        std::mutex m_mtx;
        std::map<int64_t, std::chrono::steady_clock::time_point> m_cooldowns;

    public:
        std::string GetName() const override { return "forcefetch"; }
        std::string GetDescription() const override { return "Force update from Riot (1m Cooldown)"; }

        void Execute(const dpp::interaction_create_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) override
        {
            int64_t guild_id = event.command.guild_id;
            auto now = std::chrono::steady_clock::now();

            {
                // Lock the mutex for thread safety
                std::lock_guard<std::mutex> lock(m_mtx);
                if (m_cooldowns.find(guild_id) != m_cooldowns.end())
                {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_cooldowns[guild_id]).count();
                    if (elapsed < 60)
                    {
                        event.edit_original_response(
                            dpp::message("⏳ Please wait " + std::to_string(60 - elapsed) + "s before fetching again."));
                        return;
                    }
                }
                m_cooldowns[guild_id] = now;
            } // Mutex is released here

            auto task = std::make_unique<Core::Utils::TaskTrackerUpdate>();
            task->ctx = ctx;
            task->priority = Core::Utils::TaskPriority::High;

            ctx->submitTask(std::move(task));

            event.edit_original_response(dpp::message("🚀 Update queued!"));
        }
    };
} // namespace Core::Commands::Impl
#include "server/core/TaskManager.h"
#include "server/commands/CommandSystem.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

namespace Core::Utils
{
    TaskManager::TaskManager(size_t num_threads, std::shared_ptr<AppContext> context) : m_done(false), m_ctx(context)
    {
        m_ctx->submitTask = [this](std::unique_ptr<Task> t) { this->submit(std::move(t)); };

        for (size_t i = 0; i < num_threads; ++i)
        {
            m_workers.emplace_back(&TaskManager::WorkerLoop, this);
        }
    }

    TaskManager::~TaskManager()
    {
        m_done = true;
        for (auto &worker : m_workers)
        {
            if (worker.joinable())
                worker.join();
        }
    }

    void TaskManager::submit(std::unique_ptr<Task> task)
    {
        if (!task)
            return;
        switch (task->priority)
        {
        case TaskPriority::High:
            m_highQueue.push(std::move(task));
            break;
        case TaskPriority::Standard:
            m_stdQueue.push(std::move(task));
            break;
        case TaskPriority::Low:
            m_lowQueue.push(std::move(task));
            break;
        }
    }

    // -------------------------------------------------------------------------
    // SLASH COMMAND PROCESSING
    // -------------------------------------------------------------------------
    void TaskSlashCommand::process()
    {
        std::string commandName = event.command.get_command_name();
        auto cmd = Core::Commands::CommandRegistry::Instance().Get(commandName);

        if (cmd)
        {
            try
            {
                cmd->Execute(event, ctx);
            }
            catch (const std::exception &e)
            {
                event.edit_original_response(dpp::message("⚠️ Error executing command: " + std::string(e.what())));
            }
        }
        else
        {
            event.edit_original_response(dpp::message("❌ Unknown command: " + commandName));
        }
    }

    // -------------------------------------------------------------------------
    // BUTTON CLICK PROCESSING
    // -------------------------------------------------------------------------
    void TaskButtonClick::process()
    {
        std::string customId = event.custom_id;

        // Simple routing: if customId starts with "penance_", route to Penance command
        std::string cmdName = "";
        if (customId.find("penance_") == 0)
            cmdName = "penance";
        // Add other mappings here as needed

        if (!cmdName.empty())
        {
            auto cmd = Core::Commands::CommandRegistry::Instance().Get(cmdName);
            if (cmd)
            {
                try
                {
                    cmd->OnButton(event, ctx);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Button Error: " << e.what() << std::endl;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // TRACKER DISPATCHER
    // -------------------------------------------------------------------------
    void TaskTrackerUpdate::process()
    {
        auto users = ctx->db->GetAllUsers();
        for (const auto &user : users)
        {
            auto task = std::make_unique<TaskCheckUserMatch>();
            task->ctx = ctx;
            task->user = user;
            task->priority = TaskPriority::Low;
            ctx->submitTask(std::move(task));
        }
    }

    // -------------------------------------------------------------------------
    // INDIVIDUAL USER MATCH CHECK (ROBUST LOGIC)
    // -------------------------------------------------------------------------
    void TaskCheckUserMatch::process()
    {
        // 1. Fetch last 15 matches (Riot defaults to Newest -> Oldest)
        auto matches = ctx->riot->GetLastMatches(user.riot_puuid, user.region, 15);

        if (matches.empty())
            return;

        // 2. Reverse to process Oldest -> Newest
        std::reverse(matches.begin(), matches.end());

        // 3. Iterate and check against DB
        for (const auto &match_id : matches)
        {
            if (ctx->db->IsMatchProcessed(user.discord_id, match_id))
            {
                continue; // Already processed, skip it.
            }

            // 4. It's a new match! Analyze it.
            auto stats = ctx->riot->AnalyzeMatch(match_id, user.riot_puuid, user.region);

            if (stats.valid)
            {
                ctx->db->LogGame(user.discord_id, match_id, stats.timestamp, stats.gameDuration, stats.champion_name, stats.kills, stats.deaths,
                                 stats.assists, stats.kp_percent, stats.cs, stats.cs_min);

                if (stats.deaths > 0)
                {
                    auto exOpt = ctx->db->GetRandomExercise();
                    std::string exName = "Pushups";
                    int baseReps = 10;
                    std::string type = "upper";

                    if (exOpt)
                    {
                        exName = exOpt->name;
                        baseReps = exOpt->set_count;
                        type = exOpt->type;
                    }

                    double multiplier = 1.0;
                    if (type == "lower")
                        multiplier = user.mult_lower;
                    else if (type == "core")
                        multiplier = user.mult_core;
                    else
                        multiplier = user.mult_upper;

                    int totalReps = static_cast<int>(stats.deaths * baseReps * multiplier);
                    if (totalReps < 1)
                        totalReps = 1;

                    ctx->db->AddToQueue(user.discord_id, match_id, exName, totalReps, stats.deaths);

                    ctx->bot->direct_message_create(
                        user.discord_id, dpp::message("💀 **New Match Detected** (" + user.riot_name +
                                                      ")\nDeaths: " + std::to_string(stats.deaths) + "\nPenance: " +
                                                      std::to_string(totalReps) + " " + exName + " (" + type + ")"));
                }
                ctx->db->UpdateLastMatch(user.discord_id, user.riot_puuid, match_id);
            }
            else
            {
                std::cerr << "Failed to analyze match " << match_id << " for user " << user.riot_name << std::endl;
            }
        }
    }

    // -------------------------------------------------------------------------
    // WORKER LOOP
    // -------------------------------------------------------------------------
    void TaskManager::WorkerLoop()
    {
        while (!m_done)
        {
            std::unique_ptr<Task> task;
            if (TryPopWeighted(task))
            {
                try
                {
                    task->process();
                }
                catch (const std::exception &e)
                {
                    std::cerr << "CRITICAL: Worker Thread Exception: " << e.what() << std::endl;
                }
                catch (...)
                {
                    std::cerr << "CRITICAL: Worker Thread Unknown Exception" << std::endl;
                }
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }

    bool TaskManager::TryPopWeighted(std::unique_ptr<Task> &task)
    {
        if (m_highQueue.try_pop(task))
            return true;
        if (m_stdQueue.try_pop(task))
            return true;
        if (m_lowQueue.try_pop(task))
            return true;
        return false;
    }
} // namespace Core::Utils
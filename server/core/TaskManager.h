#pragma once

#include "server/core/AppContext.h" // Includes DB, Riot, DPP
#include "server/core/ThreadsafeQueue.h"
#include <memory>
#include <variant>

namespace Core::Utils
{
    enum class TaskPriority
    {
        Low,
        Standard,
        High
    };
    enum class TaskType
    {
        GENERIC,
        SLASH_COMMAND,
        BUTTON_CLICK, // New
        SELECT_CLICK, // Newer
        TRACKER_UPDATE,
        CHECK_USER_MATCH
    };

    // Abstract Task
    class Task
    {
    public:
        virtual ~Task() = default;
        virtual void process() = 0;

        TaskPriority priority = TaskPriority::Standard;
        TaskType type = TaskType::GENERIC;
    };

    // ---------------------------------------------------------
    // Task Implementations
    // ---------------------------------------------------------

    // 1. Slash Command Task
    class TaskSlashCommand : public Task
    {
    public:
        dpp::interaction_create_t event;
        std::shared_ptr<AppContext> ctx;

        void process() override;
    };

    // 2. Button Click Task (New)
    class TaskButtonClick : public Task
    {
    public:
        dpp::button_click_t event;
        std::shared_ptr<AppContext> ctx;

        void process() override;
    };

    // 2.5 Select Menu Click Task
    class TaskSelectClick : public Task
    {
    public:
        dpp::select_click_t event;
        std::shared_ptr<AppContext> ctx;

        void process() override;
    };

    // 3. Background Tracker Dispatcher
    // This task fetches all users and spawns TaskCheckUserMatch for each
    class TaskTrackerUpdate : public Task
    {
    public:
        std::shared_ptr<AppContext> ctx;

        void process() override;
    };

    // 4. Individual User Match Check
    // This task handles the API call and logic for a single user
    class TaskCheckUserMatch : public Task
    {
    public:
        std::shared_ptr<AppContext> ctx;
        Server::DB::User user;

        void process() override;
    };

    // ---------------------------------------------------------
    // Task Manager (Worker Pool)
    // ---------------------------------------------------------
    class TaskManager
    {
    public:
        TaskManager(size_t num_threads, std::shared_ptr<AppContext> context);
        ~TaskManager();

        void submit(std::unique_ptr<Task> task);

    private:
        void WorkerLoop();
        bool TryPopWeighted(std::unique_ptr<Task> &task);

        ThreadsafeQueue<std::unique_ptr<Task>> m_highQueue;
        ThreadsafeQueue<std::unique_ptr<Task>> m_stdQueue;
        ThreadsafeQueue<std::unique_ptr<Task>> m_lowQueue;

        std::atomic<bool> m_done;
        std::vector<std::thread> m_workers;
        std::shared_ptr<AppContext> m_ctx;
    };
} // namespace Core::Utils
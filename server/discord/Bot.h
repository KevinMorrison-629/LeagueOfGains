#pragma once
#include "server/core/TaskManager.h"
#include <dpp/dpp.h>

namespace Core::Discord
{
    class Bot
    {
    public:
        void Initialize(std::shared_ptr<dpp::cluster> bot, std::shared_ptr<Utils::AppContext> ctx,
                        std::shared_ptr<Utils::TaskManager> tm);
        void Run();

    private:
        std::shared_ptr<dpp::cluster> m_bot;
        std::shared_ptr<Utils::TaskManager> m_taskManager;
        std::shared_ptr<Utils::AppContext> m_ctx;

        void OnReady(const dpp::ready_t &event);
        void OnSlashCommand(const dpp::interaction_create_t &event);
        void OnButtonClick(const dpp::button_click_t &event); 
        void OnSelectClick(const dpp::select_click_t &event); 
        void RegisterCommands();
    };
} // namespace Core::Discord
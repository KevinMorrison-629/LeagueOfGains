#include "server/discord/Bot.h"
#include "server/commands/CommandSystem.h"

// Include Command Implementations
#include "server/commands/impl/ForceFetch.h"
#include "server/commands/impl/Link.h"
#include "server/commands/impl/Penance.h"
#include "server/commands/impl/Stats.h"
#include "server/commands/impl/Leaderboard.h"
#include "server/commands/impl/Wimp.h"

namespace Core::Discord
{
    void Bot::Initialize(std::shared_ptr<dpp::cluster> bot, std::shared_ptr<Utils::AppContext> ctx,
                         std::shared_ptr<Utils::TaskManager> tm)
    {
        m_bot = bot;
        m_ctx = ctx;
        m_taskManager = tm;

        // Register Commands into the Registry
        auto &reg = Core::Commands::CommandRegistry::Instance();
        reg.Register(std::make_shared<Core::Commands::Impl::CmdLink>());
        reg.Register(std::make_shared<Core::Commands::Impl::CmdPenance>());
        reg.Register(std::make_shared<Core::Commands::Impl::CmdStats>());
        reg.Register(std::make_shared<Core::Commands::Impl::CmdForceFetch>());
        reg.Register(std::make_shared<Core::Commands::Impl::CmdWimp>());
        reg.Register(std::make_shared<Core::Commands::Impl::CmdLeaderboard>());

        m_bot->on_log(dpp::utility::cout_logger());
        m_bot->on_ready([this](const dpp::ready_t &event) { this->OnReady(event); });
        m_bot->on_slashcommand([this](const dpp::interaction_create_t &event) { this->OnSlashCommand(event); });

        // Register Button Click Handler
        m_bot->on_button_click([this](const dpp::button_click_t &event) { this->OnButtonClick(event); });

        // Register Select Click Handler
        m_bot->on_select_click([this](const dpp::select_click_t &event) { this->OnSelectClick(event); });
    }

    void Bot::Run() { m_bot->start(dpp::st_wait); }

    void Bot::OnReady(const dpp::ready_t &event)
    {
        std::cout << "Bot is online as " << m_bot->me.username << std::endl;

        // 1. Run immediately on startup
        auto initialTask = std::make_unique<Utils::TaskTrackerUpdate>();
        initialTask->priority = Utils::TaskPriority::Low;
        initialTask->ctx = m_ctx;
        m_taskManager->submit(std::move(initialTask));

        // 2. Schedule recurring timer (300 seconds = 5 minutes)
        m_bot->start_timer(
            [this](const dpp::timer &timer)
            {
                auto task = std::make_unique<Utils::TaskTrackerUpdate>();
                task->priority = Utils::TaskPriority::Low;
                task->ctx = m_ctx;
                m_taskManager->submit(std::move(task));
            },
            300);

        if (dpp::run_once<struct RegisterBotCommands>())
        {
            RegisterCommands();
        }
    }

    void Bot::RegisterCommands()
    {
        std::vector<dpp::slashcommand> cmds;

        auto &reg = Core::Commands::CommandRegistry::Instance();
        for (const auto &[name, cmd] : reg.GetAll())
        {
            dpp::slashcommand dppCmd(name, cmd->GetDescription(), m_bot->me.id);
            cmd->RegisterParams(dppCmd);
            cmds.push_back(dppCmd);
            std::cout << "Registered command: " << name << std::endl;
        }

        m_bot->global_bulk_command_create(cmds);
    }

    void Bot::OnSlashCommand(const dpp::interaction_create_t &event)
    {
        event.thinking();

        auto task = std::make_unique<Utils::TaskSlashCommand>();
        task->type = Utils::TaskType::SLASH_COMMAND;
        task->priority = Utils::TaskPriority::High;
        task->event = event;
        task->ctx = m_ctx;

        m_taskManager->submit(std::move(task));
    }

    void Bot::OnButtonClick(const dpp::button_click_t &event)
    {
        // Don't call event.thinking() automatically for buttons unless you know it takes time,
        // as we often want to do event.reply() or event.update() directly.
        // We'll let the specific command implementation handle the response type.

        auto task = std::make_unique<Utils::TaskButtonClick>();
        task->type = Utils::TaskType::BUTTON_CLICK;
        task->priority = Utils::TaskPriority::High; // UI interactions should be snappy
        task->event = event;
        task->ctx = m_ctx;

        m_taskManager->submit(std::move(task));
    }

    void Bot::OnSelectClick(const dpp::select_click_t &event)
    {
        auto task = std::make_unique<Utils::TaskSelectClick>();
        task->type = Utils::TaskType::SELECT_CLICK;
        task->priority = Utils::TaskPriority::High;
        task->event = event;
        task->ctx = m_ctx;

        m_taskManager->submit(std::move(task));
    }
} // namespace Core::Discord
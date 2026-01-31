#pragma once

#include "server/core/AppContext.h"
#include <dpp/dpp.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Core::Commands
{
    // Abstract Base Class for all Slash Commands
    class ICommand
    {
    public:
        virtual ~ICommand() = default;

        // The name of the slash command (e.g., "link")
        virtual std::string GetName() const = 0;

        // The description shown in Discord
        virtual std::string GetDescription() const = 0;

        // Define parameters (options) for the command
        virtual void RegisterParams(dpp::slashcommand &command) const {}

        // The logic to run when the command is triggered
        virtual void Execute(const dpp::interaction_create_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) = 0;

        // Handle button clicks related to this command
        virtual void OnButton(const dpp::button_click_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) {}

        // Handle select menu interactions
        virtual void OnSelect(const dpp::select_click_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) {}
    };

    // Registry to manage commands
    class CommandRegistry
    {
    public:
        static CommandRegistry &Instance()
        {
            static CommandRegistry instance;
            return instance;
        }

        void Register(std::shared_ptr<ICommand> cmd) { m_commands[cmd->GetName()] = cmd; }

        std::shared_ptr<ICommand> Get(const std::string &name)
        {
            if (m_commands.find(name) != m_commands.end())
                return m_commands[name];
            return nullptr;
        }

        const std::unordered_map<std::string, std::shared_ptr<ICommand>> &GetAll() const { return m_commands; }

    private:
        CommandRegistry() = default;
        std::unordered_map<std::string, std::shared_ptr<ICommand>> m_commands;
    };
} // namespace Core::Commands
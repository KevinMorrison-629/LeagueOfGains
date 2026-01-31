#pragma once
#include "server/commands/CommandSystem.h"

namespace Core::Commands::Impl
{
    class CmdReroll : public ICommand
    {
    public:
        std::string GetName() const override { return "reroll"; }
        std::string GetDescription() const override { return "Reroll a punishment for a specific game"; }

        void RegisterParams(dpp::slashcommand &command) const override
        {
            command.add_option(dpp::command_option(dpp::co_string, "gameid", "The Game ID to reroll", true));
        }

        void Execute(const dpp::interaction_create_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) override
        {
            std::string gameId = std::get<std::string>(event.get_parameter("gameid"));
            auto user = event.command.get_issuing_user();
            int64_t guild_id = event.command.guild_id;

            auto task = ctx->db->GetPenanceByGameID(user.id, gameId);

            if (task)
            {
                auto newExOpt = ctx->db->GetRandomExercise();
                if (!newExOpt)
                {
                    event.edit_original_response(dpp::message("❌ No exercises defined for this server. Use /add first."));
                    return;
                }

                auto newEx = *newExOpt;
                double multiplier = ctx->db->GetUserMultiplier(user.id, newEx.type);

                // Recalculate based on original deaths
                int totalReps = static_cast<int>(task->original_deaths * newEx.set_count * multiplier);

                ctx->db->UpdatePenance(task->id, newEx.name, totalReps);

                event.edit_original_response(
                    dpp::message("🎲 Rerolled! New task: **" + std::to_string(totalReps) + " " + newEx.name + "**"));
            }
            else
            {
                event.edit_original_response(dpp::message("❌ No pending punishment found for Game ID: " + gameId));
            }
        }
    };
} // namespace Core::Commands::Impl
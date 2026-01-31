#pragma once
#include "server/commands/CommandSystem.h"

namespace Core::Commands::Impl
{
    class CmdComplete : public ICommand
    {
    public:
        std::string GetName() const override { return "complete"; }
        std::string GetDescription() const override { return "Mark a game punishment as complete"; }

        void RegisterParams(dpp::slashcommand &command) const override
        {
            command.add_option(
                dpp::command_option(dpp::co_string, "gameid", "The Game ID (from /penance) to complete", true));
        }

        void Execute(const dpp::interaction_create_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) override
        {
            std::string gameId = std::get<std::string>(event.get_parameter("gameid"));
            auto user = event.command.get_issuing_user();

            auto task = ctx->db->GetPenanceByGameID(user.id, gameId);

            if (task)
            {
                ctx->db->CompletePenance(user.id, gameId);
                event.edit_original_response(dpp::message("✅ Completed **" + std::to_string(task->reps) + " " +
                                                          task->exercise_name + "** for Game " + gameId));
            }
            else
            {
                event.edit_original_response(dpp::message("❌ No pending punishment found for Game ID: " + gameId));
            }
        }
    };
} // namespace Core::Commands::Impl
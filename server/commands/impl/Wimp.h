#pragma once
#include "server/commands/CommandSystem.h"
#include <cmath>
#include <variant>

namespace Core::Commands::Impl
{
    class CmdWimp : public ICommand
    {
    public:
        std::string GetName() const override { return "wimp"; }
        std::string GetDescription() const override
        {
            return "Set exercise difficulty multiplier (global or per muscle group)";
        }

        void RegisterParams(dpp::slashcommand &command) const override
        {
            // Mandatory multiplier
            command.add_option(
                dpp::command_option(dpp::co_number, "multiplier", "Multiplier (e.g. 0.5 for half reps)", true));

            // Optional Type
            dpp::command_option typeOpt(dpp::co_string, "type", "Specific Muscle Group (Optional)", false);
            typeOpt.add_choice(dpp::command_option_choice("Upper Body", "upper"));
            typeOpt.add_choice(dpp::command_option_choice("Lower Body", "lower"));
            typeOpt.add_choice(dpp::command_option_choice("Core", "core"));
            command.add_option(typeOpt);
        }

        void Execute(const dpp::interaction_create_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) override
        {
            double mult = std::get<double>(event.get_parameter("multiplier"));

            // Handle optional parameter
            std::string type = "";
            auto typeParam = event.get_parameter("type");
            if (std::holds_alternative<std::string>(typeParam))
            {
                type = std::get<std::string>(typeParam);
            }

            auto user = event.command.get_issuing_user();

            if (mult <= 0.0)
            {
                event.edit_original_response(dpp::message("❌ Multiplier must be positive."));
                return;
            }

            ctx->db->SetUserMultiplier(user.id, mult, type);

            std::string mode = "Wimp mode";
            if (mult > 1.0)
            {
                mode = "GigaChad mode";
            }

            if (type.empty())
            {
                event.edit_original_response(
                    dpp::message("✅ " + mode + " mode set to **" + std::to_string(mult) + "x** for all exercises."));
            }
            else
            {
                event.edit_original_response(dpp::message("✅ " + mode + " set to **" + std::to_string(mult) + "x** for **" +
                                                          type + "** exercises."));
            }
        }
    };
} // namespace Core::Commands::Impl
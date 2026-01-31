#pragma once
#include "server/commands/CommandSystem.h"
#include <iomanip>
#include <sstream>

namespace Core::Commands::Impl
{
    class CmdLeaderboard : public ICommand
    {
    public:
        std::string GetName() const override { return "leaderboard"; }
        std::string GetDescription() const override { return "Show top users by category"; }

        void RegisterParams(dpp::slashcommand &command) const override
        {
            dpp::command_option typeOpt(dpp::co_string, "category", "Ranking category (default: reps)", false);
            typeOpt.add_choice(dpp::command_option_choice("Reps Completed", "reps"));
            typeOpt.add_choice(dpp::command_option_choice("Total Deaths", "deaths"));
            typeOpt.add_choice(dpp::command_option_choice("Average KDA", "kda"));
            command.add_option(typeOpt);
        }

        void Execute(const dpp::interaction_create_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) override
        {
            std::string type = "reps";
            if (event.get_parameter("category").index() != 0) // if exists
                type = std::get<std::string>(event.get_parameter("category"));

            auto weights = ctx->db->GetLeaderboard(type);

            std::string title = "🏆 Leaderboard: ";
            std::string fieldName = "Score";
            if (type == "reps") { title += "Gym Rats (Reps)"; fieldName = "Reps"; }
            if (type == "deaths") { title += "Feeders (Deaths)"; fieldName = "Deaths"; }
            if (type == "kda") { title += "Carries (KDA)"; fieldName = "Average KDA"; }

            dpp::embed embed = dpp::embed()
                                   .set_title(title)
                                   .set_color(0xFFD700); // Gold

            if (weights.empty())
            {
                embed.set_description("No data recorded yet.");
            }
            else
            {
                std::stringstream ss;
                int rank = 1;
                for (const auto &p : weights)
                {
                    std::string medal = "";
                    if (rank == 1) medal = "🥇";
                    else if (rank == 2) medal = "🥈";
                    else if (rank == 3) medal = "🥉";
                    else medal = "#" + std::to_string(rank);

                    if (type == "kda")
                    {
                        double kda = (double)p.second / 100.0;
                         std::stringstream kdaSS;
                         kdaSS << std::fixed << std::setprecision(2) << kda;
                         ss << "**" << medal << "** " << p.first << " — **" << kdaSS.str() << "**\n";
                    }
                    else
                    {
                        ss << "**" << medal << "** " << p.first << " — **" << p.second << "**\n";
                    }
                    rank++;
                }
                embed.set_description(ss.str());
            }

            event.edit_original_response(dpp::message(embed));
        }
    };
} // namespace Core::Commands::Impl

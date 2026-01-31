#pragma once
#include "server/commands/CommandSystem.h"
#include <iomanip>
#include <sstream>

namespace Core::Commands::Impl
{
    class CmdStats : public ICommand
    {
    public:
        std::string GetName() const override { return "stats"; }
        std::string GetDescription() const override { return "Display your fitness and feeding statistics"; }

        void Execute(const dpp::interaction_create_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) override
        {
            auto user = event.command.get_issuing_user();
            auto stats = ctx->db->GetUserStats(user.id);

            dpp::embed embed = dpp::embed().set_title(user.username + "'s Stats").set_color(0x0099FF);

            embed.add_field("💀 Total Deaths", std::to_string(stats.total_deaths), true);
            embed.add_field("🎮 Games Tracked", std::to_string(stats.total_games), true);

            std::stringstream kda;
            kda << std::fixed << std::setprecision(2) << stats.lowest_kda;
            embed.add_field("📉 Lowest KDA", kda.str(), true);

            embed.add_field("🔥 Max Deaths (1 Game)", std::to_string(stats.most_deaths_single), true);
            embed.add_field("🏋️ Pending Tasks", std::to_string(stats.pending_penance_count), true);

            std::string topChamps = "";
            for (const auto &p : stats.top_death_champs)
            {
                topChamps += p.first + " (" + std::to_string(p.second) + ")\n";
            }
            if (topChamps.empty())
                topChamps = "None";
            embed.add_field("⚰️ Top Death Champs", topChamps, false);

            std::string exercises = "";
            for (const auto &p : stats.exercise_counts)
            {
                exercises += p.first + ": " + std::to_string(p.second) + "\n";
            }
            if (exercises.empty())
                exercises = "None";
            embed.add_field("💪 Reps Completed", exercises, false);

            // Chart Logic
            auto recentGames = ctx->db->GetRecentGames(user.id, 10);
            if (!recentGames.empty())
            {
                std::stringstream labels_ss;
                std::stringstream data_ss;
                labels_ss << "[";
                data_ss << "[";
                
                // Oldest first for the graph
                for (int i = recentGames.size() - 1; i >= 0; --i)
                {
                    labels_ss << "'" << i + 1 << "'"; // Just using indices 1..10
                    data_ss << recentGames[i].deaths;
                    if (i > 0) {
                        labels_ss << ",";
                        data_ss << ",";
                    }
                }
                labels_ss << "]";
                data_ss << "]";

                // Construct URL manually
                // Warning: QuickChart URLs can get long, we need to keep it simple.
                // Using a darker background theme for Discord.
                std::string chartUrl = "https://quickchart.io/chart?c={type:'line',data:{labels:" + labels_ss.str() + 
                                       ",datasets:[{label:'Deaths',data:" + data_ss.str() + 
                                       ",borderColor:'red',fill:false}]},options:{legend:{labels:{fontColor:'white'}},scales:{yAxes:[{ticks:{fontColor:'white',beginAtZero:true}}],xAxes:[{ticks:{fontColor:'white'}}]}}}";
                
                // URL Encode is ideally needed but specific chars like {} are usually handled by browsers, 
                // but Discord might be picky. QuickChart works well usually.
                // Replaced space with %20 just in case, though we have none.
                
                embed.set_image(chartUrl);
            }

            event.edit_original_response(dpp::message(embed));
        }
    };
} // namespace Core::Commands::Impl
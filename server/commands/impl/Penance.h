#pragma once
#include "server/commands/CommandSystem.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Core::Commands::Impl
{
    class CmdPenance : public ICommand
    {
    public:
        std::string GetName() const override { return "penance"; }
        std::string GetDescription() const override { return "View all pending game punishments (with stats)"; }

        // Helper to format duration (min:sec)
        std::string FormatDuration(double cs, double cs_min)
        {
            if (cs_min <= 0.01)
                return "?:??";
            double minutes = cs / cs_min;
            int mins = static_cast<int>(minutes);
            int secs = static_cast<int>((minutes - mins) * 60);
            char buf[16];
            snprintf(buf, sizeof(buf), "%d:%02d", mins, secs);
            return std::string(buf);
        }

        // Helper to clean champion names for DDragon URLs
        // DDragon expects names like "LeeSin", "MissFortune", "KaiSa"
        // The Riot API 'championName' field is usually already in this format (e.g. "MonkeyKing" for Wukong)
        // keeping this simple but robust against spaces just in case.
        std::string CleanChampName(std::string input)
        {
            if (input == "FiddleSticks")
                return "Fiddlesticks"; // Common edge case

            // Remove spaces and apostrophes
            input.erase(std::remove(input.begin(), input.end(), ' '), input.end());
            input.erase(std::remove(input.begin(), input.end(), '\''), input.end());
            return input;
        }

        // Helper to build the message for a specific page
        dpp::message BuildMessage(const std::vector<Server::DB::PenanceDisplayInfo> &allTasks, int page)
        {
            const int ITEMS_PER_PAGE = 5;
            int totalPages = (allTasks.empty()) ? 1 : (allTasks.size() + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

            // Clamp page
            if (page < 0)
                page = 0;
            if (page >= totalPages)
                page = totalPages - 1;

            dpp::message msg;

            // 1. Header Embed
            dpp::embed header = dpp::embed()
                                    .set_title("🏋️ Penance List")
                                    .set_color(0xFFA500) // Orange
                                    .set_description("Total Pending: **" + std::to_string(allTasks.size()) + "**\nPage " +
                                                     std::to_string(page + 1) + "/" + std::to_string(totalPages));

            msg.add_embed(header);

            if (allTasks.empty())
            {
                header.set_description("🎉 You are free! No pending exercises.");
                return msg;
            }

            int startIdx = page * ITEMS_PER_PAGE;
            int endIdx = std::min(startIdx + ITEMS_PER_PAGE, (int)allTasks.size());

            // Select Menus
            dpp::component selectMenuComplete;
            selectMenuComplete.set_type(dpp::cot_selectmenu);
            selectMenuComplete.set_placeholder("Select Task to Complete...");
            selectMenuComplete.set_id("penance_completion_menu");

            dpp::component selectMenuReroll;
            selectMenuReroll.set_type(dpp::cot_selectmenu);
            selectMenuReroll.set_placeholder("Select Task to Reroll...");
            selectMenuReroll.set_id("penance_reroll_menu");

            bool hasItems = false;

            // 2. Item Embeds (One per game)
            for (int i = startIdx; i < endIdx; ++i)
            {
                hasItems = true;
                const auto &task = allTasks[i];
                dpp::embed itemEmbed = dpp::embed();

                itemEmbed.set_color(0xFF4500);
                std::string champUrl = "https://ddragon.leagueoflegends.com/cdn/14.1.1/img/champion/" +
                                       CleanChampName(task.champion_name) + ".png";
                itemEmbed.set_thumbnail(champUrl);

                std::stringstream title;
                title << task.reps << " " << task.exercise_name << " (Deaths: " << task.original_deaths << ")";
                itemEmbed.set_title(title.str());

                std::stringstream desc;
                desc << "**" << task.champion_name << "**"
                     << "\n💀 **KDA:** " << task.kills << "/" << task.deaths << "/" << task.assists << "\n📊 **KP:** "
                     << std::fixed << std::setprecision(0) << task.kp_percent << "%"
                     << " • **CS:** " << task.cs << " (" << std::fixed << std::setprecision(1) << task.cs_min << "/m)"
                     << "\n⏱️ " << FormatDuration(task.cs, task.cs_min) << " • <t:" << (task.game_timestamp / 1000) << ":R>";

                itemEmbed.set_description(desc.str());
                msg.add_embed(itemEmbed);

                // Add to Select Menus
                std::string labelComplete = "✅ " + std::to_string(task.reps) + " " + task.exercise_name + " (" + task.champion_name + ")";
                selectMenuComplete.add_select_option(dpp::select_option(labelComplete, "complete_" + task.match_id, "Mark as Done"));

                std::string labelReroll = "🎲 " + task.champion_name + " (" + task.exercise_name + ")";
                selectMenuReroll.add_select_option(dpp::select_option(labelReroll, "reroll_" + task.match_id, "Reroll Exercise"));
            }

            // 3. Components
            if (hasItems)
            {
                 dpp::component actionRow1;
                 actionRow1.add_component(selectMenuComplete);
                 msg.add_component(actionRow1);

                 dpp::component actionRow2;
                 actionRow2.add_component(selectMenuReroll);
                 msg.add_component(actionRow2);
            }

            dpp::component buttonRow;
            buttonRow.add_component(dpp::component()
                                  .set_type(dpp::cot_button)
                                  .set_label("Previous")
                                  .set_style(dpp::cos_secondary)
                                  .set_id("penance_prev_" + std::to_string(page))
                                  .set_disabled(page == 0));

            buttonRow.add_component(dpp::component()
                                  .set_type(dpp::cot_button)
                                  .set_label("Next")
                                  .set_style(dpp::cos_secondary)
                                  .set_id("penance_next_" + std::to_string(page))
                                  .set_disabled(page >= totalPages - 1));

            msg.add_component(buttonRow);
            return msg;
        }

        void Execute(const dpp::interaction_create_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) override
        {
            auto user = event.command.get_issuing_user();
            auto tasks = ctx->db->GetPendingPenanceDetailed(user.id);

            // Initial view is Page 0
            dpp::message msg = BuildMessage(tasks, 0);
            event.edit_original_response(msg);
        }

        void OnButton(const dpp::button_click_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) override
        {
            // ID Format: penance_prev_0 or penance_next_1
            std::string id = event.custom_id;

            int currentPage = 0;
            try
            {
                size_t lastUnderscore = id.find_last_of('_');
                if (lastUnderscore != std::string::npos)
                {
                    currentPage = std::stoi(id.substr(lastUnderscore + 1));
                }
            }
            catch (...)
            {
            }

            int newPage = currentPage;
            if (id.find("prev") != std::string::npos)
                newPage--;
            if (id.find("next") != std::string::npos)
                newPage++;

            auto user = event.command.get_issuing_user();
            auto tasks = ctx->db->GetPendingPenanceDetailed(user.id);

            // Validate Page
            int totalPages = (tasks.empty()) ? 1 : (tasks.size() + 5 - 1) / 5;
            if (newPage < 0)
                newPage = 0;
            if (newPage >= totalPages)
                newPage = totalPages - 1;

            dpp::message msg = BuildMessage(tasks, newPage);

            // Interaction update (replaces the message that spawned the button click)
            event.reply(dpp::ir_update_message, msg);
        }

        void OnSelect(const dpp::select_click_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) override
        {
            // Always acknowledge interaction
            if (event.values.empty()) 
            {
               event.reply(dpp::ir_update_message, dpp::message("❌ Invalid selection")); 
               return;
            }

            std::string value = event.values[0]; 
            std::string gameId = value.substr(value.find('_') + 1);
            auto user = event.command.get_issuing_user();

            bool actionTaken = false;

            if (value.find("complete_") == 0)
            {
                auto task = ctx->db->GetPenanceByGameID(user.id, gameId);
                if (task)
                {
                    ctx->db->CompletePenance(user.id, gameId);
                    actionTaken = true;
                }
            }
            else if (value.find("reroll_") == 0)
            {
                auto task = ctx->db->GetPenanceByGameID(user.id, gameId);
                if (task)
                {
                    auto newExOpt = ctx->db->GetRandomExercise();
                    if (newExOpt)
                    {
                        auto newEx = *newExOpt;
                        double multiplier = ctx->db->GetUserMultiplier(user.id, newEx.type);
                        int totalReps = static_cast<int>(task->original_deaths * newEx.set_count * multiplier);
                        ctx->db->UpdatePenance(task->id, newEx.name, totalReps);
                        actionTaken = true;
                    }
                }
            }

            // Regardless of whether we found the task or not (maybe it was already done),
            // we REFRESH the list.
            auto tasks = ctx->db->GetPendingPenanceDetailed(user.id);
            dpp::message msg = BuildMessage(tasks, 0); 
            
            // If we successfully did something, we update. 
            // If the task was missing, we still update (it disappears from list).
            event.reply(dpp::ir_update_message, msg);
        }
    };
} // namespace Core::Commands::Impl
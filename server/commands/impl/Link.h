#pragma once
#include "server/commands/CommandSystem.h"

namespace Core::Commands::Impl
{
    class CmdLink : public ICommand
    {
    public:
        std::string GetName() const override { return "link"; }
        std::string GetDescription() const override { return "Link your LoL account (Can link multiple)"; }

        void RegisterParams(dpp::slashcommand &command) const override
        {
            command.add_option(dpp::command_option(dpp::co_string, "name", "Riot Game Name", true));
            command.add_option(dpp::command_option(dpp::co_string, "tag", "Riot Tag Line", true));

            dpp::command_option regionOpt(dpp::co_string, "region", "Region", true);
            regionOpt.add_choice(dpp::command_option_choice("North America", "na1"));
            regionOpt.add_choice(dpp::command_option_choice("Europe West", "euw1"));
            regionOpt.add_choice(dpp::command_option_choice("Europe Nordic & East", "eun1"));
            regionOpt.add_choice(dpp::command_option_choice("Korea", "kr"));
            regionOpt.add_choice(dpp::command_option_choice("Brazil", "br1"));
            regionOpt.add_choice(dpp::command_option_choice("Oceania", "oc1"));
            regionOpt.add_choice(dpp::command_option_choice("Russia", "ru"));
            regionOpt.add_choice(dpp::command_option_choice("Turkey", "tr1"));
            regionOpt.add_choice(dpp::command_option_choice("Japan", "jp1"));
            command.add_option(regionOpt);
        }

        void Execute(const dpp::interaction_create_t &event, std::shared_ptr<Core::Utils::AppContext> ctx) override
        {
            std::string name = std::get<std::string>(event.get_parameter("name"));
            std::string tag = std::get<std::string>(event.get_parameter("tag"));
            std::string region = std::get<std::string>(event.get_parameter("region"));
            auto user = event.command.get_issuing_user();

            auto account = ctx->riot->GetAccount(name, tag, region);
            if (!std::get<0>(account).empty())
            {
                Server::DB::User u;
                u.discord_id = user.id;
                u.riot_puuid = std::get<0>(account);
                u.riot_name = std::get<1>(account);
                u.riot_tag = std::get<2>(account);
                u.region = region;
                u.mult_lower = ctx->db->GetUserMultiplier(user.id, "lower");
                u.mult_upper = ctx->db->GetUserMultiplier(user.id, "upper");
                u.mult_core = ctx->db->GetUserMultiplier(user.id, "core");

                ctx->db->AddUser(u);
                event.edit_original_response(
                    dpp::message("✅ Linked **" + u.riot_name + "#" + u.riot_tag + "** to your Discord ID."));
            }
            else
            {
                event.edit_original_response(dpp::message("❌ Summoner not found. Check spelling and region code."));
            }
        }
    };
} // namespace Core::Commands::Impl
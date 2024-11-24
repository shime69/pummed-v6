#include "globals.hpp"
#include "event_logs.hpp"
#include "imgui/imgui.h"
#include "engine_prediction.hpp"
#include "structs.hpp"

/*std::pair<std::string, color> get_message_prefix_type(int type)
{
    auto clr = g_cfg.misc.ui_color.base();
    switch (type)
    {
    case event_hit:
        return std::make_pair(xor_str("HIT"), clr);
    case event_miss:
        return std::make_pair(xor_str("MISS"), color(204, 60, 60));
    case event_plant:
        return std::make_pair(xor_str("BOMB"), color(235, 66, 66));
    case event_server:
        return std::make_pair(xor_str("SERVER"), clr.decrease(20));
    case event_buy:
        return std::make_pair(xor_str("BUY"), clr.decrease(15));
    default:
        return {};
    }
}*/

/*void c_event_logs::DetectUnregisteredShots(c_game_event* event) {
	if (!HACKS->local || !HACKS->local->is_alive())
		return;

	auto* pred_vars = ENGINE_PREDICTION->get_networked_vars(HACKS->client_state->last_command_ack);
	if (!pred_vars)
		return;

	if (pred_vars->command_number != HACKS->client_state->last_command_ack)
		return;

	auto weapon = reinterpret_cast<c_base_combat_weapon*>(HACKS->entity_list->get_client_entity_handle(HACKS->local->active_weapon()));

	if (!weapon)
		return;

	float fLastShotTime = weapon->last_shot_time();

	if (std::abs(HACKS->predicted_time - fLastShotTime) > 0.2f)
		return;

	auto weapon_info = HACKS->weapon_info;

	if (!weapon_info)
		return;

	float cycle_time = weapon_info->cycletime;
	HACKS->weapon->next_primary_attack() = fLastShotTime + cycle_time;
	
#if _DEBUG
	printf("Detected unregistered shot, and fixed it!");
#endif;
}*/


void c_event_logs::on_item_purchase(c_game_event* event)
{
	if (std::strcmp(event->get_name(), CXOR("item_purchase")) || !HACKS->local)
		return;

	if (!(g_cfg.visuals.eventlog.logs & 8))
		return;

	auto userid = event->get_int(CXOR("userid"));
	if (!userid)
		return;

	auto user_id = HACKS->engine->get_player_for_user_id(userid);
	auto player = (c_cs_player*)HACKS->entity_list->get_client_entity(user_id);
	if (!player)
		return;

	if (player->is_teammate())
		return;

	push_message(tfm::format(CXOR("%s bought %s"), player->get_name(), event->get_string(CXOR("weapon"))));
}

void c_event_logs::on_bomb_plant(c_game_event* event)
{
	const char* event_name = event->get_name();
	if (!(g_cfg.visuals.eventlog.logs & 16))
		return;

	auto userid = event->get_int(CXOR("userid"));
	if (!userid)
		return;

	auto user_id = HACKS->engine->get_player_for_user_id(userid);
	auto player = (c_cs_player*)HACKS->entity_list->get_client_entity(user_id);
	if (!player)
		return;

	if (!std::strcmp(event_name, CXOR("bomb_planted")))
		push_message(tfm::format(CXOR("%s planted the bomb"), player->get_name()));

	if (!std::strcmp(event_name, CXOR("bomb_begindefuse")))
		push_message(tfm::format(CXOR("%s is defusing the bomb"), player->get_name()));
}

void c_event_logs::on_player_hurt(c_game_event* event)
{
	if (!(g_cfg.visuals.eventlog.logs & 1))
		return;

	if (std::strcmp(event->get_name(), CXOR("player_hurt")) || !HACKS->local)
		return;

	auto attacker = HACKS->engine->get_player_for_user_id(event->get_int(CXOR("attacker")));
	if (HACKS->local->index() != attacker)
		return;

	auto user_id = HACKS->engine->get_player_for_user_id(event->get_int(CXOR("userid")));
	auto player = (c_cs_player*)HACKS->entity_list->get_client_entity(user_id);

	if (!player)
		return;

	if (player->is_teammate())
		return;

	auto group = event->get_int(CXOR("hitgroup"));
	auto dmg_health = event->get_int(CXOR("dmg_health"));
	auto health = event->get_int(CXOR("health"));

	auto string_group = main_utils::hitgroup_to_string(group);

	if (group == HITGROUP_GENERIC || group == HITGROUP_GEAR)
		push_message(tfm::format(CXOR("Registered shot at %s for %d (%d remaining)"), 
			player->get_name().c_str(),
			dmg_health, 
			health));
	else
		push_message(tfm::format(CXOR("Registered shot at %s`%s for %d (%d remaining)"), 
			player->get_name().c_str(),
			string_group.c_str(), 
			dmg_health, 
			health));
}

void c_event_logs::on_game_events(c_game_event* event)
{
	on_player_hurt(event);
	on_bomb_plant(event);
	on_item_purchase(event);
	//DetectUnregisteredShots(event);
}

void c_event_logs::filter_console()
{
	HACKS->convars.con_filter_text->fn_change_callbacks.remove_count();
	HACKS->convars.con_filter_enable->fn_change_callbacks.remove_count();

	if (set_console)
	{
		set_console = false;

		HACKS->cvar->find_convar(CXOR("developer"))->set_value(0);
		HACKS->convars.con_filter_enable->set_value(1);
		HACKS->convars.con_filter_text->set_value(CXOR(""));
	}

	auto filter = g_cfg.visuals.eventlog.enable && g_cfg.visuals.eventlog.filter_console;
	if (log_value != filter)
	{
		log_value = filter;

		if (!log_value)
			HACKS->convars.con_filter_text->set_value(CXOR(""));
		else
			HACKS->convars.con_filter_text->set_value(CXOR("IrWL5106TZZKNFPz4P4Gl3pSN?J370f5hi373ZjPg%VOVh6lN"));
	}
}

// rebuilt in-game CConPanel::DrawNotify
void c_event_logs::render_logs()
{
	if (!g_cfg.visuals.eventlog.enable)
		return;

	constexpr auto font_size = 15.f;
	constexpr auto padding = 8.f;
	auto render_font = RENDER->fonts.eventlog;
	auto time = HACKS->system_time();

	float x = 10.f, y = 8.f;
	for (int i = 0; i < event_logs.size(); ++i)
	{
		auto event_log = &event_logs[i];

		auto time_left = 1.f - std::clamp((time - event_log->life_time) / 5.f, 0.f, 1.f);
		if (time_left <= 0.5f)
		{
			float f = std::clamp(time_left, 0.0f, .5f) / .5f;
			event_log->clr.a() = ((std::uint8_t)(f * 255.0f));

			if (i == 0 && f < 0.2f)
				y -= font_size * (1.0f - f / 0.2f);

			if (time_left <= 0.f)
			{
				event_logs.erase(event_logs.begin() + i);
				continue;
			}
		}

		// Get text size for box
		ImVec2 text_size_im = RENDER->get_text_size(&render_font, event_log->message.c_str());
		vec2_t text_size(text_size_im.x, text_size_im.y); // Convert ImVec2 to vec2_t
		
		// Draw box background
		RENDER->filled_rect(
			x - padding, 
			y - padding, 
			text_size.x + (padding * 2), 
			text_size.y + (padding * 2), 
			c_color(20, 20, 20, event_log->clr.a())
		);

		// Draw accent line on left side
		RENDER->filled_rect(
			x - padding,
			y - padding,
			2,
			text_size.y + (padding * 2),
			c_color(g_cfg.misc.ui_color[0], g_cfg.misc.ui_color[1], g_cfg.misc.ui_color[2], event_log->clr.a())
		);

		// Draw text
		RENDER->text(x, y, event_log->clr, FONT_DROPSHADOW, &render_font, event_log->message);

		y += text_size.y + (padding * 2) + 2; // Add gap between logs
	}
}

#include "globals.hpp"
#include "resolver.hpp"
#include "animations.hpp"
#include "server_bones.hpp"
#include "ragebot.hpp"
#include "penetration.hpp"

namespace resolver
{
	inline void prepare_jitter(c_cs_player* player, resolver_info_t& resolver_info, anim_record_t* current)
	{
		auto& jitter = resolver_info.jitter;
		jitter.yaw_cache[jitter.yaw_cache_offset % YAW_CACHE_SIZE] = current->eye_angles.y;

		if (jitter.yaw_cache_offset >= YAW_CACHE_SIZE + 1)
			jitter.yaw_cache_offset = 0;
		else
			jitter.yaw_cache_offset++;

		for (int i = 0; i < YAW_CACHE_SIZE - 1; ++i)
		{
			float diff = std::fabsf(jitter.yaw_cache[i] - jitter.yaw_cache[i + 1]);
			if (diff <= 0.f)
			{
				if (jitter.static_ticks < 3)
					jitter.static_ticks++;
				else
					jitter.jitter_ticks = 0;
			}
			else if (diff >= 20.f)
			{
				if (jitter.jitter_ticks < 3)
					jitter.jitter_ticks++;
				else
					jitter.static_ticks = 0;
			}
		}

		jitter.is_jitter = jitter.jitter_ticks > jitter.static_ticks;
	}

	inline void prepare_side(c_cs_player* player, anim_record_t* current, anim_record_t* previous)
	{
		auto& info = resolver_info[player->index()];
		if (!HACKS->weapon_info || !HACKS->local || !HACKS->local->is_alive() || player->is_bot() || !g_cfg.rage.resolver)
		{
			if (info.resolved)
				info.reset();

			return;
		}

		auto state = player->animstate();
		if (!state)
		{
			if (info.resolved)
				info.reset();

			return;
		}

		auto hdr = player->get_studio_hdr();
		if (!hdr)
			return;

		if (current->choke < 2)
			info.add_legit_ticks();
		else
			info.add_fake_ticks();

		if (info.is_legit())
		{
			info.resolved = false;
			info.mode = XOR("no fake");
			return;
		}

		prepare_jitter(player, info, current);
		auto& jitter = info.jitter;
		if (jitter.is_jitter)
		{
			auto& misses = RAGEBOT->missed_shots[player->index()];
			if (misses > 0)
				info.side = side_original;
			else
			{

				float first_angle = math::normalize_yaw(jitter.yaw_cache[YAW_CACHE_SIZE - 1]);
				float second_angle = math::normalize_yaw(jitter.yaw_cache[YAW_CACHE_SIZE - 2]);

				float _first_angle = std::sin(DEG2RAD(first_angle));
				float _second_angle = std::sin(DEG2RAD(second_angle));

				float __first_angle = std::cos(DEG2RAD(first_angle));
				float __second_angle = std::cos(DEG2RAD(second_angle));


				float avg_yaw = math::normalize_yaw(RAD2DEG(std::atan2f((_first_angle + _second_angle), (__first_angle + __second_angle))));
				float diff = math::normalize_yaw(current->eye_angles.y - avg_yaw);

				info.side = diff > 0.f ? side_left : side_right;

			}

			info.resolved = true;
			info.mode = XOR("jitter");
		}

		if (previous)
		{
			if (static_cast<int>(current->layers[6].weight * 1500.f) == static_cast<int>(previous->layers[6].weight * 1000.f))
			{
				const auto& cur_layer6 = current->layers[6];

				const auto zero_delta = std::abs(cur_layer6.playback_rate - current->matrix_zero.layers[6].playback_rate);
				const auto left_delta = std::abs(cur_layer6.playback_rate - current->matrix_left.layers[6].playback_rate);
				const auto right_delta = std::abs(cur_layer6.playback_rate - current->matrix_right.layers[6].playback_rate);

				const auto min_delta = std::min(zero_delta, std::min(left_delta, right_delta));

				if (static_cast<int>(min_delta * 1000.f) == static_cast<int>(left_delta * 1000.f))
				{
					info.anim_resolve_ticks = HACKS->global_vars->tickcount;

					info.resolved = true;
					info.mode = XOR("layers");
					info.side = side_left;
				}
				else if (static_cast<int>(min_delta * 1000.f) == static_cast<int>(right_delta * 1000.f))
				{
					info.anim_resolve_ticks = HACKS->global_vars->tickcount;

					info.resolved = true;
					info.mode = XOR("layers");
					info.side = side_right;
				}
			}
		}

		else
		{
			auto& misses = RAGEBOT->missed_shots[player->index()];
			if (misses > 0)
			{
				info.side = math::random_float(side_left, side_right);

				info.resolved = true;
				info.mode = XOR("brute");
			}
			else
			{
				info.side = side_zero;
				info.mode = XOR("static");

				info.resolved = true;
			}
		}

	}

	inline void apply_side(c_cs_player* player, anim_record_t* current, int choke)
	{
		auto& info = resolver_info[player->index()];
		if (!HACKS->weapon_info || !HACKS->local || !HACKS->local->is_alive() || !info.resolved || info.side == side_original || player->is_teammate(false))
			return;

		auto state = player->animstate();
		if (!state)
			return;

		state->abs_yaw = math::normalize_yaw(player->eye_angles().y + state->get_max_rotation() * info.side);
	}
}
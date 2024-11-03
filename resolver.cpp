#include "globals.hpp"
#include "resolver.hpp"
#include "animations.hpp"
#include "server_bones.hpp"
#include "ragebot.hpp"

namespace resolver
{

	int trace_resolve(c_cs_player* player, resolver_info_t& resolver_info, anim_record_t* record)         //ill do it (normal ver) 
	{
		constexpr float step{ 4.f };
		constexpr float range{ 20.f };
		int side{ 0 };

		vec3_t eye_pos = HACKS->local->get_eye_position();
		vec3_t target_pos = player->origin();

		std::array<float, 3> damages = { 0.f, 0.f, 0.f };

		for (float yaw = record->eye_angles.y - range; yaw <= record->eye_angles.y + range; yaw += step)
		{
			vec3_t head_pos;
			math::angle_vectors(vec3_t(0.f, yaw, 0.f), head_pos);
			head_pos = target_pos + (head_pos * 25.f);

			c_trace_filter filter;
			filter.skip = player;

			c_game_trace trace;
			HACKS->engine_trace->trace_ray(ray_t(eye_pos, head_pos), MASK_SHOT | CONTENTS_GRATE, &filter, &trace);

			if (trace.fraction < 0.97f)
				continue;

			vec3_t angles = math::calc_angle(target_pos, eye_pos);
			float delta = math::normalize_yaw(yaw - angles.y);

			if (delta < -10.f)
				damages[0] += trace.fraction;
			else if (delta > 10.f)
				damages[2] += trace.fraction;
			else
				damages[1] += trace.fraction;
		}

		int best_side = std::distance(damages.begin(), std::max_element(damages.begin(), damages.end()));

		switch (best_side)
		{
		case 0:
			side = 1;
			resolver_info.mode = "traced";
			break;
		case 2:
			side = -1;
			resolver_info.mode = "traced";
			break;
		default:
			side = 0;
			resolver_info.mode = "traced";
			break;
		}

		return side;
	}

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
			else if (diff >= 10.f)
			{
				if (jitter.jitter_ticks < 3)
					jitter.jitter_ticks++;
				else
					jitter.static_ticks = 0;
			}
		}

		jitter.is_jitter = jitter.jitter_ticks > jitter.static_ticks;
	}

	inline void freestand_resolve(c_cs_player* player, resolver_info_t& resolver_info, anim_record_t* current)
	{
		vec3_t eyePos = player->origin() + vec3_t(0, 0, 64 - player->duck_amount() * 16.f);

		vec3_t forward, right, up;
		math::angle_vectors(player->eye_angles(), &forward, &right, &up);

		vec3_t negPos = eyePos - right * 23.f;
		vec3_t posPos = eyePos + right * 23.f;

		c_trace_filter_world_and_props_only filter;
		ray_t rayNeg(negPos, negPos + forward * 128.f);
		ray_t rayPos(posPos, posPos + forward * 128.f);
		c_game_trace negTrace, posTrace;

		HACKS->engine_trace->trace_ray(rayNeg, MASK_SHOT_HULL | CONTENTS_GRATE, &filter, &negTrace);
		HACKS->engine_trace->trace_ray(rayPos, MASK_SHOT_HULL | CONTENTS_GRATE, &filter, &posTrace);

		if (negTrace.start_solid && posTrace.start_solid) {
			resolver_info.side = 0;
			resolver_info.mode = "none";
		}
		else if (negTrace.start_solid) {
			resolver_info.side = -1;
			resolver_info.mode = "fs";
		}
		else if (posTrace.start_solid) {
			resolver_info.side = 1;
			resolver_info.mode = "fs";
		}

		if (negTrace.fraction == 1.f && posTrace.fraction == 1.f) {
			resolver_info.side = 0;
			resolver_info.mode = "none";
		}
		resolver_info.resolved = true;
		resolver_info.side = negTrace.fraction < posTrace.fraction ? -1 : 1;
		resolver_info.mode = "fs";
		resolver_info.freestanding.updated = true;
		resolver_info.freestanding.update_time = HACKS->global_vars->curtime;
	}


	inline void prepare_side(c_cs_player* player, anim_record_t* current, anim_record_t* last)
	{
		bool walking = player->velocity().length_2d() >= 1.2f && player->flags().has(FL_ONGROUND);
		bool running  = player->velocity().length_2d() >= 200.f && player->flags().has(FL_ONGROUND);
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
		if (player->velocity().length_sqr_2d() > 200.f && state->walk_to_run_transition_state > 0.5f && current->layers[ANIMATION_LAYER_MOVEMENT_MOVE].playback_rate > 0.0001f)
		{
			float layer_deltas[3] = {
				abs(current->layers[ANIMATION_LAYER_MOVEMENT_MOVE].playback_rate - current->layers_orig[ANIMATION_LAYER_MOVEMENT_MOVE].playback_rate),
				abs(current->layers[ANIMATION_LAYER_MOVEMENT_MOVE].playback_rate - current->layers_left[ANIMATION_LAYER_MOVEMENT_MOVE].playback_rate),
				abs(current->layers[ANIMATION_LAYER_MOVEMENT_MOVE].playback_rate - current->layers_right[ANIMATION_LAYER_MOVEMENT_MOVE].playback_rate)
			};

			float adjust_layer_delta = abs(current->layers[ANIMATION_LAYER_ADJUST].playback_rate - current->layers_orig[ANIMATION_LAYER_ADJUST].playback_rate);

			int best_side = std::distance(layer_deltas, std::max_element(layer_deltas, layer_deltas + 3));

			switch (best_side) {
			case 1: {
				if (best_side == 1 && layer_deltas[1] > layer_deltas[0] * 1.2f) {
					info.side = -1;
					info.resolved = true;
					info.mode = "anim left";
				}
			}break;
			case 2: {
				if (best_side == 2 && layer_deltas[2] > layer_deltas[0] * 1.2f) {
					info.side = 1;
					info.resolved = true;
					info.mode = "anim right";
				}
			}break;
			case 3: {
				if (adjust_layer_delta > 0.5f) {
					info.resolved = false;
				}
			}break;
			case 0: {
				info.side = 0;
				info.resolved = false;
			}break;
			}
		}
		/*else if (jitter.is_jitter)
		{
			auto& misses = RAGEBOT->missed_shots[player->index()];
			if (misses > 0)
				info.side = 1337;
			else
			{
				float first_angle = math::normalize_yaw(jitter.yaw_cache[YAW_CACHE_SIZE - 1]);
				float second_angle = math::normalize_yaw(jitter.yaw_cache[YAW_CACHE_SIZE - 2]);

				float _first_angle = std::sin(DEG2RAD(first_angle));
				float _second_angle = std::sin(DEG2RAD(second_angle));

				float __first_angle = std::cos(DEG2RAD(first_angle));
				float __second_angle = std::cos(DEG2RAD(second_angle));

				float avg_yaw = math::normalize_yaw(RAD2DEG(std::atan2f((_first_angle + _second_angle) / 2.f, (__first_angle + __second_angle) / 2.f)));
				float diff = math::normalize_yaw(current->eye_angles.y - avg_yaw);

				info.side = diff > 0.f ? -1 : 1;
			}

			info.resolved = true;
			info.mode = XOR("jitter");
		}*/
		else if (walking) {   //unfinished , will finish prob tomorow
			info.side = trace_resolve(player, info, current);
		}
		else {
			freestand_resolve(player, info, current);
		}
	}

	inline void apply_side(c_cs_player* player, anim_record_t* current, int choke)
	{
		auto& info = resolver_info[player->index()];
		if (!HACKS->weapon_info || !HACKS->local || !HACKS->local->is_alive() || !info.resolved || info.side == 1337 || player->is_teammate(false))
			return;

		auto state = player->animstate();
		if (!state)
			return;

		//auto desync_min = GetPlayerFeetYaw(player, MIN);
		//auto desync_max = GetPlayerFeetYaw(player, MAX);

		float desync_angle = choke < 2 ? state->get_max_rotation() : 120.f;
		state->abs_yaw = math::normalize_yaw(player->eye_angles().y + desync_angle * info.side);

		/*
		switch (info.side)
		{
		case 0:
			state->abs_yaw = math::normalize_yaw(player->eye_angles().y);
			break;
		case 1:
			state->abs_yaw = math::normalize_yaw(player->eye_angles().y + desync_max);
			break;
		case -1:
			state->abs_yaw = math::normalize_yaw(player->eye_angles().y + desync_min);
			break;
		}*/
	}
}
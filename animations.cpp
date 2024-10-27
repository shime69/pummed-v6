#include "globals.hpp"
#include "server_bones.hpp"
#include "animations.hpp"
#include "lagcomp.hpp"
#include "entlistener.hpp"
#include "resolver.hpp"
#include "threads.hpp"
#include "engine_prediction.hpp"
#include "ragebot.hpp"

#ifdef _DEBUG
#define DEBUG_SP 0
uint8_t* get_server_edict(c_cs_player* player)
{
	static uintptr_t server_globals = **offsets::server_edict.add(0x2).cast< uintptr_t** >();
	int max_clients = *(int*)((uintptr_t)server_globals + 0x18);
	int index = player->index();
	if (index > 0 && max_clients >= 1)
	{
		if (index <= max_clients)
		{
			int v10 = index * 16;
			uintptr_t v11 = *(uintptr_t*)(server_globals + 96);
			if (v11)
			{
				if (!((*(uintptr_t*)(v11 + v10) >> 1) & 1))
				{
					uintptr_t v12 = *(uintptr_t*)(v10 + v11 + 12);
					if (v12)
					{
						uint8_t* ret = nullptr;
						__asm
						{
							pushad
							mov ecx, v12
							mov eax, dword ptr[ecx]
							call dword ptr[eax + 0x14]
							mov ret, eax
							popad
						}

						return ret;
					}
				}
			}
		}
	}
	return nullptr;
}

void draw_server_hitbox(c_cs_player* player)
{
	auto duration = HACKS->global_vars->interval_per_tick * 2.f;

	auto server_player = get_server_edict(player);
	if (server_player)
	{
		static auto call = offsets::draw_hitbox.cast< uintptr_t* >();

		float current_duration = duration;

		__asm
		{
			pushad
			movss xmm1, current_duration
			push 1
			mov ecx, server_player
			call call
			popad
		}
	}
}

void draw_hitbox(c_cs_player* player, matrix3x4_t* bones, int idx, int idx2, bool dur = false)
{
	auto studio_model = HACKS->model_info->get_studio_model(player->get_model());
	if (!studio_model)
		return;

	auto hitbox_set = studio_model->hitbox_set(0);
	if (!hitbox_set)
		return;

	for (int i = 0; i < hitbox_set->num_hitboxes; i++)
	{
		auto hitbox = hitbox_set->hitbox(i);
		if (!hitbox)
			continue;

		vec3_t vMin, vMax;
		math::vector_transform(hitbox->min, bones[hitbox->bone], vMin);
		math::vector_transform(hitbox->max, bones[hitbox->bone], vMax);

		if (hitbox->radius != -1.f)
			HACKS->debug_overlay->add_capsule_overlay(vMin, vMax, hitbox->radius, 255, 255 * idx, 255 * idx2, 150, dur ? HACKS->global_vars->interval_per_tick * 2 : 5.f, 0, 1);
	}
}
#endif

static INLINE void fix_land(anim_record_t* last_record, anim_record_t* record, c_cs_player* player)
{
	// legacy have different landfix because it anims only 1 tick and you have less data to detect smth
#ifdef LEGACY
	bool on_ground = player->flags().has(FL_ONGROUND);
	record->on_ground = false;
	record->real_on_ground = on_ground;

	if (on_ground && last_record->real_on_ground)
		record->on_ground = true;
	else
	{
		if (record->layers[4].weight != 1.f && record->layers[4].weight == 1.f && record->layers[5].weight != 0.f)
			record->on_ground = true;

		if (on_ground)
		{
			bool ground = record->on_ground;
			if (!last_record->real_on_ground)
				ground = false;
			record->on_ground = ground;
		}
	}
#endif
}

void fix_velocity(anim_record_t* last_record, anim_record_t* record, c_cs_player* player)
{
	auto state = player->animstate();
	if (!state)
		return;

	auto weapon = (c_base_combat_weapon*)(HACKS->entity_list->get_client_entity_handle(player->active_weapon()));
	if (!weapon)
		return;

	auto weapon_info = HACKS->weapon_system->get_weapon_data(weapon->item_definition_index());

	if (player->effects().has(EF_NOINTERP) || player->no_interp_parity() != player->no_interp_parity_old())
	{
		record->velocity.reset();
		return;
	}

	auto prev_record = last_record;
	auto time_delta = TICKS_TO_TIME(record->choke);

	auto max_speed = weapon && weapon_info ? std::max<float>((player->is_scoped() ? weapon_info->max_speed_alt : weapon_info->max_speed), 0.001f) : CS_PLAYER_SPEED_RUN;

	if (player->is_walking())
		max_speed *= CS_PLAYER_SPEED_WALK_MODIFIER;

	if (player->duck_amount() >= 1.f)
		max_speed *= CS_PLAYER_SPEED_DUCK_MODIFIER;

	if (prev_record)
	{
		if (time_delta > 0.f)
			record->velocity = (record->origin - prev_record->origin) / time_delta;

		if (record->flags.has(FL_ONGROUND) && prev_record->flags.has(FL_ONGROUND))
		{
			auto& layer_aliveloop = prev_record->layers[ANIMATION_LAYER_ALIVELOOP];
			auto& new_layer_aliveloop = record->layers[ANIMATION_LAYER_ALIVELOOP];
			auto anim_speed = 0.f;

			if (layer_aliveloop.weight > 0.f && layer_aliveloop.weight < 0.1f && new_layer_aliveloop.playback_rate == layer_aliveloop.playback_rate)
			{
				auto anim_modifier = 0.35f * (1.0f - layer_aliveloop.weight);

				if (anim_modifier > 0.f && anim_modifier < 1.f)
					anim_speed = max_speed * (anim_modifier + 0.55f);
			}

			auto length = record->velocity.length_2d();
			if (length > 0.1f && anim_speed > 0.0f)
			{
				anim_speed /= length;
				record->velocity.x *= anim_speed;
				record->velocity.y *= anim_speed;
			}
		}

		if (record->flags.has(FL_ONGROUND))
			record->velocity.z = 0.f;
	}
	else
	{
		if (record->flags.has(FL_ONGROUND))
		{
			auto& layer_movement = record->layers[ANIMATION_LAYER_MOVEMENT_MOVE];
			auto anim_speed = 0.f;

			if (layer_movement.weight)
				anim_speed = layer_movement.weight;

			auto length = record->velocity.length_2d();

			if (length > 0.1f && anim_speed > 0.0f)
			{
				anim_speed /= length;
				record->velocity.x *= anim_speed;
				record->velocity.y *= anim_speed;
			}

			record->velocity.z = 0.f;
		}
	}

	auto move = &record->layers[ANIMATION_LAYER_MOVEMENT_MOVE];
	auto lean = &record->layers[ANIMATION_LAYER_LEAN];

	if (prev_record)
	{
		auto prev_move = &prev_record->layers[ANIMATION_LAYER_MOVEMENT_MOVE];

		if (record->choke >= 12 && record->flags.has(FL_ONGROUND) && record->velocity.length_2d() > 0.1f && move->weight <= 0.1f && prev_move->cycle == move->cycle && move->playback_rate < 0.0001f)
		{
			record->fakewalking = true;
			record->velocity.reset();
		}
	}
}

INLINE matrix_t* get_matrix_side(anim_record_t* new_record, int side)
{
	switch (side)
	{
	case side_left:
		return &new_record->matrix_left;
	case side_right:
		return &new_record->matrix_right;
	case side_zero:
		return &new_record->matrix_zero;
	case side_original:
		return &new_record->matrix_orig;
	}
}

static INLINE void update_sides(c_cs_player* player, anims_t* anim, anim_record_t* record, anim_record_t* previous, int side, c_studio_hdr* hdr, int* flags_base, int parent_count)
{
	auto state = player->animstate();
	if (!state)
		return;

	if (!previous)
	{
		state->primary_cycle = record->layers[ANIMATION_LAYER_MOVEMENT_MOVE].cycle;
		state->move_weight = record->layers[ANIMATION_LAYER_MOVEMENT_MOVE].weight;

		state->last_update_time = (record->sim_time - HACKS->global_vars->interval_per_tick);

		if (player->flags().has(FL_ONGROUND))
		{
			state->on_ground = true;
			state->landing = false;
		}

		auto last_update_time = state->last_update_time;
		auto layer_jump = &record->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];

		if (player->get_sequence_activity(layer_jump->sequence) == ACT_CSGO_JUMP)
		{
			auto duration_in_air = record->sim_time - layer_jump->cycle / layer_jump->playback_rate;

			if (duration_in_air > last_update_time)
			{
				state->on_ground = false;
				player->pose_parameter()[6] = 0.f;
				state->duration_in_air = 0.f;
				state->duration_in_air = duration_in_air;
			}
		}
	}
	else
	{
		state->primary_cycle = previous->layers[ANIMATION_LAYER_MOVEMENT_MOVE].cycle;
		state->move_weight = previous->layers[ANIMATION_LAYER_MOVEMENT_MOVE].weight;
	}

	if (!previous || record->choke < 2)
	{
		if (previous && !player->flags().has(FL_ONGROUND))
		{
			auto layer_jump = &record->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
			auto old_layer_jump = &previous->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];

			if (layer_jump->weight > 0.f && layer_jump->cycle > 0.f)
			{
				if (player->get_sequence_activity(layer_jump->sequence) == ACT_CSGO_JUMP)
				{
					if (layer_jump->cycle != old_layer_jump->cycle || layer_jump->sequence != old_layer_jump->sequence && old_layer_jump->cycle > layer_jump->cycle)
					{
						player->pose_parameter()[6] = 0.f;
						state->duration_in_air = layer_jump->cycle / layer_jump->playback_rate;
					}
				}
			}
		}

		if (previous)
		{
			float diff = math::normalize_yaw(previous->eye_angles.y - record->eye_angles.y);

			record->fix_jitter_angle = std::fabsf(diff) > 30.f;
			record->last_eyeang = previous->eye_angles.y;
			record->last_eyeang_diff = diff;

			if (record->old_diff != record->last_eyeang_diff)
			{
				record->old_diff = record->last_eyeang_diff;
				record->last_eyeang_diff_time = player->sim_time();
			}
		}

		if (side == side_original)
		{
			if (record->fix_jitter_angle)
			{
				if (std::fabsf(record->last_eyeang_diff_time - record->sim_time) < TICKS_TO_TIME(record->choke))
					record->eye_angles.y += math::normalize_yaw(record->last_eyeang_diff);

				record->fix_jitter_angle = false;
			}

			resolver::apply_side(player, record, record->choke);
		}
		else
			state->abs_yaw = math::normalize_yaw(record->eye_angles.y + state->get_max_rotation() * side);

		player->set_abs_origin(player->origin());
		player->abs_velocity() = player->velocity() = record->velocity;
		player->force_update_animations(anim);
	}
	else
	{
		int land_time{}, jump_time{};

		if (previous)
		{
			if (player->flags().has(FL_ONGROUND))
			{
				auto layer_land = &record->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
				auto old_layer_land = &previous->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];

				if (!previous->flags.has(FL_ONGROUND) && layer_land->weight != 0.f && old_layer_land->weight == 0.f)
				{
					auto sequence_activity = player->get_sequence_activity(layer_land->sequence);

					if (sequence_activity == ACT_CSGO_LAND_LIGHT || sequence_activity == ACT_CSGO_LAND_HEAVY)
						land_time = TIME_TO_TICKS(record->sim_time - (layer_land->cycle / layer_land->playback_rate)) + 1;
				}
			}
			else
			{
				auto layer_jump = &record->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
				auto old_layer_jump = &previous->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];

				if (layer_jump->cycle != old_layer_jump->cycle || layer_jump->sequence != old_layer_jump->sequence && layer_jump->cycle < old_layer_jump->cycle)
				{
					auto sequence_activity = player->get_sequence_activity(layer_jump->sequence);

					if (sequence_activity == ACT_CSGO_JUMP)
						jump_time = TIME_TO_TICKS(record->sim_time - (layer_jump->cycle / layer_jump->playback_rate)) + 1;
				}
			}
		}

		player->lower_body_yaw() = previous->lby;
		player->thirdperson_recoil() = previous->thirdperson_recoil;

		for (int simulate_tick = 0; simulate_tick <= record->choke; ++simulate_tick)
		{
			float simulate_time = previous->sim_time + simulate_tick * HACKS->global_vars->interval_per_tick;

			/*HACKS->global_vars->curtime = simulate_time;
			HACKS->global_vars->realtime = simulate_time;
			HACKS->global_vars->frametime = HACKS->global_vars->interval_per_tick;
			HACKS->global_vars->absoluteframetime = HACKS->global_vars->interval_per_tick;
			HACKS->global_vars->framecount = TIME_TO_TICKS(simulate_time);
			HACKS->global_vars->tickcount = TIME_TO_TICKS(simulate_time);
			HACKS->global_vars->interpolation_amount = 0.f;*/

			if (simulate_time == land_time)
			{
				player->flags().force(FL_ONGROUND);

				auto record_layer_land = &record->layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
				auto layer_land = &player->animlayers()[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
				layer_land->cycle = 0.f;
				layer_land->playback_rate = record_layer_land->playback_rate;
			}

			if (simulate_time == jump_time - 1)
				player->flags().force(FL_ONGROUND);

			if (simulate_time == jump_time)
			{
				player->flags().remove(FL_ONGROUND);

				auto record_layer_jump = &record->layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
				auto layer_jump = &player->animlayers()[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
				layer_jump->cycle = 0.f;
				layer_jump->playback_rate = record_layer_jump->playback_rate;
			}

			float lerp_step = simulate_tick / record->choke;

			player->origin() = math::lerp(lerp_step, previous->origin, record->origin);
			player->abs_velocity() = player->velocity() = math::lerp(lerp_step, previous->velocity, record->velocity);
			player->duck_amount() = math::lerp(lerp_step, previous->duck_amt, record->duck_amt);

			if (previous)
			{
				float diff = math::normalize_yaw(previous->eye_angles.y - record->eye_angles.y);

				record->fix_jitter_angle = std::fabsf(diff) > 30.f;
				record->last_eyeang = math::normalize_yaw(previous->eye_angles.y);
				record->last_eyeang_diff = diff;

				if (record->old_diff != record->last_eyeang_diff)
				{
					record->old_diff = record->last_eyeang_diff;
					record->last_eyeang_diff_time = player->sim_time();
				}
			}

			if (record->shooting)
			{
				player->eye_angles() = record->last_reliable_angle;

				if (record->last_shot_time <= simulate_time)
				{
					player->eye_angles() = record->eye_angles.normalized_angle();
					player->lower_body_yaw() = record->lby;
					player->thirdperson_recoil() = record->thirdperson_recoil;
				}
			}

			if (side == side_original)
			{
				if (record->fix_jitter_angle)
				{
					if (std::fabsf(record->last_eyeang_diff_time - record->sim_time) < TICKS_TO_TIME(record->choke))
						record->eye_angles.y += math::normalize_yaw(record->last_eyeang_diff);

					record->fix_jitter_angle = false;
				}

				resolver::apply_side(player, record, record->choke);
			}
			else
				state->abs_yaw = math::normalize_yaw(player->eye_angles().y + 120.f * side);

			player->force_update_animations(anim);
		}
	}

	auto collideable = player->get_collideable();

	if (collideable)
		offsets::set_collision_bounds.cast<void(__thiscall*)(void*, vec3_t*, vec3_t*)>()(collideable, &player->bb_mins(), &player->bb_maxs());

	record->collision_change_origin = player->collision_change_origin();
	record->collision_change_time = player->collision_change_time();

	player->invalidate_bone_cache();
	{
		record->layers[ANIMATION_LAYER_LEAN].weight = player->animlayers()[ANIMATION_LAYER_LEAN].weight = 0.f;
		record->layers[ANIMATION_LAYER_LEAN].cycle = player->animlayers()[ANIMATION_LAYER_LEAN].cycle = 0.f;

		anim->setup_bones = false;

		auto matrix_side = get_matrix_side(record, side);

		// store simulated layers
		player->store_layers(matrix_side->layers);

		// restore layers
		player->set_layers(record->layers);

		matrix_side->bone_builder.store(player, matrix_side->matrix, 0x7FF00, hdr, flags_base, parent_count);
		matrix_side->bone_builder.setup();
	}
}

void thread_collect_info(c_cs_player* player)
{
	auto state = player->animstate();
	if (!state)
		return;

	auto index = player->index();
	auto anim = ANIMFIX->get_anims(index);
	auto backup = ANIMFIX->get_backup_record(index);

	if (anim->ptr != player)
	{
		state->reset();
		anim->reset();
		backup->reset();
		anim->ptr = player;
		anim->update_anims = anim->setup_bones = true;
		return;
	}

	auto hdr = player->get_studio_hdr();
	if (!hdr)
	{
		state->reset();
		anim->reset();
		backup->reset();
		anim->ptr = player;
		anim->update_anims = anim->setup_bones = true;
		return;
	}

	if (!player->is_alive())
	{
		state->reset();
		anim->reset();
		backup->reset();
		return;
	}

	if (player->dormant())
	{
		anim->dormant_ticks = 0;
		anim->records.clear();
		backup->reset();

		anim->update_anims = anim->setup_bones = true;
		return;
	}

	if (anim->dormant_ticks < 1)
		anim->dormant_ticks++;

	if (player->sim_time() == player->old_sim_time())
		return;

	if (anim->old_spawn_time != player->spawn_time())
	{
		state->player = player;
		state->reset();

		anim->old_spawn_time = player->spawn_time();
		return;
	}

	player->store_layers(resolver_info[player->index()].initial_layers);
	player->update_weapon_dispatch_layers();

	anim_record_t* last_record = nullptr;

	if (anim->records.size() > 0)
		last_record = &anim->records.front();

	auto& new_record = anim->records.emplace_front();
	new_record.dormant = anim->dormant_ticks < 2;
	new_record.store(player);
	new_record.shifting = false;

	if (last_record)
	{
		fix_land(last_record, &new_record, player);
		fix_velocity(last_record, &new_record, player);

		auto layer_alive_loop = &new_record.layers[ANIMATION_LAYER_ALIVELOOP];
		auto old_layer_alive_loop = &last_record->layers[ANIMATION_LAYER_ALIVELOOP];

		auto current_playback_rate = layer_alive_loop->playback_rate;
		auto previous_playback_rate = old_layer_alive_loop->playback_rate;

		auto current_cycle = layer_alive_loop->cycle;
		auto previous_cycle = static_cast<int>(old_layer_alive_loop->cycle / (HACKS->global_vars->interval_per_tick * previous_playback_rate) + 0.5f);

		auto cycle = 0;

		if (current_playback_rate == previous_playback_rate)
			cycle = static_cast<int>(current_cycle / (current_playback_rate * HACKS->global_vars->interval_per_tick) + 0.5f);
		else
			cycle = static_cast<int>(previous_cycle + ((current_cycle / current_playback_rate + (1.f - old_layer_alive_loop->cycle) / previous_playback_rate) / HACKS->global_vars->interval_per_tick + 0.5f));

		auto layer_delta = cycle - previous_cycle;
		if (layer_delta <= 18)
			new_record.choke = std::max(layer_delta, 1);

		new_record.shooting = new_record.last_shot_time > last_record->sim_time && new_record.last_shot_time <= last_record->sim_time;
		if (!new_record.shooting)
			new_record.last_reliable_angle = player->eye_angles();

		new_record.choke = std::clamp(new_record.choke, 0, 16);
	}

	auto bone_flags_base = hdr->bone_flags().base();
	auto bone_parent_count = hdr->bone_parent_count();

	for (int i = 0; i < 13; i++)
	{
		auto layer = &player->animlayers()[i];
		layer->owner = player;
		layer->studio_hdr = player->get_studio_hdr();
	}

	backup->store(player);
	{
#ifndef LEGACY
		math::memcpy_sse(&anim->old_animstate, player->animstate(), sizeof(anim->old_animstate));

		auto fresh_tick = HACKS->local->is_alive() ? HACKS->predicted_time : HACKS->global_vars->curtime;

		clamp_bones_info_t clamp_info{};
		clamp_info.store(&new_record);

		auto rec_angles = new_record.eye_angles.normalized_angle();
		auto angles = vec3_t{ new_record.eye_angles.x, player->eye_angles().y, new_record.eye_angles.z }.normalized_angle();

		for (int i = side_left; i <= side_right; ++i)
		{
			// simulate rotation
			update_sides(player, anim, &new_record, last_record, i, hdr, bone_flags_base, bone_parent_count);

			if (i == side_left)
			{
				// update left matrix & left roll matrix
				LAGCOMP->clamp_matrix(player, &new_record.matrix_left, fresh_tick, rec_angles, clamp_info);
				LAGCOMP->build_roll_matrix(player, &new_record.matrix_left, -1, fresh_tick, rec_angles, clamp_info);
			}
			else if (i == side_right)
			{
				// update right matrix & right roll matrix
				LAGCOMP->clamp_matrix(player, &new_record.matrix_right, fresh_tick, rec_angles, clamp_info);
				LAGCOMP->build_roll_matrix(player, &new_record.matrix_right, 1, fresh_tick, rec_angles, clamp_info);
			}
			else
			{
				// update zero matrix
				LAGCOMP->clamp_matrix(player, &new_record.matrix_zero, fresh_tick, rec_angles, clamp_info);
			}

			math::memcpy_sse(player->animstate(), &anim->old_animstate, sizeof(anim->old_animstate));
		}
#endif

		resolver::prepare_side(player, &new_record, last_record);

		// update orig side
		update_sides(player, anim, &new_record, last_record, side_original, hdr, bone_flags_base, bone_parent_count);

		// update orig matrix
		LAGCOMP->clamp_matrix(player, &new_record.matrix_orig, fresh_tick, angles, clamp_info);
	}

	if (!HACKS->cl_lagcomp0)
	{
		if (last_record)
		{
			if (last_record->sim_time > new_record.sim_time)
			{
				anim->last_valid_time = new_record.sim_time + std::abs(last_record->sim_time - new_record.sim_time) + TICKS_TO_TIME(1);
				new_record.shifting = true;
			}
			else
			{
				if (anim->last_valid_time > new_record.sim_time)
					new_record.shifting = true;
			}
		}
	}
	else
		new_record.shifting = false;

	backup->restore(player);

	while (anim->records.size() > 32)
		anim->records.pop_back();

	//if (anim->records.size() > (HACKS->tick_rate - 1))
		//anim->records.erase(anim->records.end() - 1);
}

void c_animation_fix::update_enemies()
{
	LISTENER_ENTITY->for_each_player([&](c_cs_player* player)
		{
			THREAD_POOL->add_task(thread_collect_info, player);
			//thread_collect_info(player);
		});
	THREAD_POOL->wait_all();
}

INLINE void modify_eye_pos(vec3_t& pos, matrix3x4_t* matrix)
{
	auto state = HACKS->local->animstate();
	if (!state)
		return;

	auto ground_entity = HACKS->entity_list->get_client_entity_handle(HACKS->local->ground_entity());
	if (state->landing || state->anim_duck_amount != 0.f || ground_entity == 0)
	{
		auto bone_pos = matrix[8].get_origin();
		const auto bone_z = bone_pos.z + 1.7f;
		if (pos.z > bone_z)
		{
			const auto view_modifier = std::clamp((fabsf(pos.z - bone_z) - 4.f) * 0.16666667f, 0.f, 1.f);
			const auto view_modifier_sqr = view_modifier * view_modifier;
			pos.z += (bone_z - pos.z) * (3.f * view_modifier_sqr - 2.f * view_modifier_sqr * view_modifier);
		}
	}
}

vec3_t c_animation_fix::get_eye_position(float angle)
{
	auto state = HACKS->local->animstate();
	if (!state)
		return {};

	auto pose_param_body_pitch = XORN(12);
	auto anim = &anims[HACKS->local->index()];
	auto local_anim = &local_anims;
	auto initial = ENGINE_PREDICTION->get_initial_vars();

	auto old_abs_origin = HACKS->local->get_abs_origin();
	HACKS->local->set_abs_origin(initial->origin);

	// store eye pos with unpredicted data
	auto eye_pos = local_anim->eye_pos;

	RESTORE(HACKS->local->pose_parameter()[pose_param_body_pitch]);

	float pose = 0.f;
	float normalized_pitch = angle;

	if (normalized_pitch > 180.f)
		normalized_pitch = normalized_pitch - 360.f;

	normalized_pitch = std::clamp(normalized_pitch, -90.f, 90.f);
	normalized_pitch = HACKS->local->studio_set_pose_parameter(pose_param_body_pitch, normalized_pitch, pose);

	HACKS->local->pose_parameter()[pose_param_body_pitch] = pose;

	HACKS->local->set_abs_angles({ 0.f, state->abs_yaw, 0.f });
	auto hdr = HACKS->local->get_studio_hdr();
	if (hdr)
	{
		auto bone_flags_base = hdr->bone_flags().base();
		auto bone_parent_count = hdr->bone_parent_count();
		local_anim->bone_builder.store(HACKS->local, HACKS->local->bone_cache().base(), 0x7FF00, hdr, bone_flags_base, bone_parent_count);
		local_anim->bone_builder.setup();

#ifndef LEGACY
		clamp_bones_info_t info{};
		info.store(HACKS->local);
		local_anim->bone_builder.clamp_bones_in_bbox(HACKS->local, HACKS->local->bone_cache().base(), 0x7FF00, HACKS->tickbase, HACKS->local->eye_angles(), info);
#endif

		modify_eye_pos(eye_pos, HACKS->local->bone_cache().base());
	}

	HACKS->local->set_abs_origin(old_abs_origin);
	//HACKS->debug_overlay->add_text_overlay(eye_pos, HACKS->global_vars->interval_per_tick * 2.f, "POS");
	return eye_pos;
}

// due to valve are retards & play animation events only on server
// for correct animation sync we should play animation events by yourself
void c_animation_fix::handle_jump_animations(c_animation_state* state, c_animation_layers* layers, c_user_cmd* cmd) {
	auto& local = local_anims;

	auto land_or_climb = &layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	auto jump_or_fall = &layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];

	auto networked = ENGINE_PREDICTION->get_networked_vars(cmd->command_number);

	auto ground_entity_handle = networked->ground_entity;
	auto ground_entity = (c_base_entity*)(HACKS->entity_list->get_client_entity_handle(ground_entity_handle));

	auto old_on_ladder = local.old_move_type == MOVETYPE_LADDER;
	auto on_ladder = networked->move_type == MOVETYPE_LADDER;

	auto& flags = networked->flags;

	auto landed = flags.has(FL_ONGROUND) && !local.old_flags.has(FL_ONGROUND);
	auto jumped = !flags.has(FL_ONGROUND) && local.old_flags.has(FL_ONGROUND);

	auto entered_ladder = on_ladder && !old_on_ladder;
	auto left_ladder = !on_ladder && old_on_ladder;

	local.old_flags = flags;
	local.old_move_type = networked->move_type;

	if (entered_ladder)
		state->set_layer_sequence(land_or_climb, ACT_CSGO_CLIMB_LADDER);

	//	HACKS->debug_overlay->add_text_overlay(HACKS->local->origin(), 0.5f, "%d", ground_entity_handle);

	if (cmd->buttons.has(IN_JUMP) && !ground_entity)
		state->set_layer_sequence(jump_or_fall, ACT_CSGO_JUMP);

	if (flags.has(FL_ONGROUND)) {
		if (!local.landing && landed) {
			int activity = state->duration_in_air > 1.f ? ACT_CSGO_LAND_HEAVY : ACT_CSGO_LAND_LIGHT;
			state->set_layer_sequence(land_or_climb, activity);

			local.landing = true;
		}
	}
	else {
		if (!cmd->buttons.has(IN_JUMP) && (jumped || left_ladder))
			state->set_layer_sequence(jump_or_fall, ACT_CSGO_FALL);

		local.landing = false;
	}
}

// i should handle even strafe values...
void c_animation_fix::handle_strafing(c_animation_state* state, c_user_cmd* cmd) {
	auto buttons = cmd->buttons;

	vec3_t forward{};
	vec3_t right{};
	vec3_t up{};

	math::angle_vectors(vec3_t(0, state->abs_yaw, 0), &forward, &right, &up);
	right = right.normalized();

	auto velocity = state->velocity_normalized_non_zero;
	auto speed = state->speed_as_portion_of_walk_top_speed;

	float vel_to_right_dot = velocity.dot(right);
	float vel_to_foward_dot = velocity.dot(forward);

	bool move_right = (buttons.has(IN_MOVERIGHT)) != 0;
	bool move_left = (buttons.has(IN_MOVELEFT)) != 0;
	bool move_forward = (buttons.has(IN_FORWARD)) != 0;
	bool move_backward = (buttons.has(IN_BACK)) != 0;

	bool strafe_right = (speed >= 0.73f && move_right && !move_left && vel_to_right_dot < -0.63f);
	bool strafe_left = (speed >= 0.73f && move_left && !move_right && vel_to_right_dot > 0.63f);
	bool strafe_forward = (speed >= 0.65f && move_forward && !move_backward && vel_to_foward_dot < -0.55f);
	bool strafe_backward = (speed >= 0.65f && move_backward && !move_forward && vel_to_foward_dot > 0.55f);

	HACKS->local->strafing() = (strafe_right || strafe_left || strafe_forward || strafe_backward);
}

void c_animation_fix::update_local()
{
	if (HACKS->client_state->delta_tick == -1)
		return;

	auto state = HACKS->local->animstate();
	if (!state)
		return;

	auto anim = &anims[HACKS->local->index()];
	if (anim->ptr != HACKS->local)
	{
		anim->reset();
		local_anims.reset();
		state->reset();

		anim->ptr = HACKS->local;
		return;
	}

	if (anim->old_spawn_time != HACKS->local->spawn_time())
	{
		state->reset();
		local_anims.reset();

		anim->ptr = HACKS->local;
		anim->old_spawn_time = HACKS->local->spawn_time();
		return;
	}

	anim->update_anims = false;
	anim->setup_bones = false;
	anim->dormant_ticks = 1;

	RESTORE(HACKS->local->render_angles());
	RESTORE(HACKS->local->eye_angles());
	RESTORE(HACKS->global_vars->curtime);
	RESTORE(HACKS->global_vars->frametime);

	HACKS->global_vars->curtime = TICKS_TO_TIME(HACKS->tickbase);
	HACKS->global_vars->frametime = HACKS->global_vars->interval_per_tick;

	auto viewmodel = (c_base_entity*)(HACKS->entity_list->get_client_entity_handle(HACKS->local->viewmodel()));
	if (viewmodel)
		offsets::update_all_viewmodel_addons.cast<int(__fastcall*)(void*)>()(viewmodel);

	HACKS->local->store_layers(local_anims.backup_layers);

	vec3_t eye_angles{};
	if (HACKS->cmd->command_number >= HACKS->shot_cmd && HACKS->shot_cmd >= HACKS->cmd->command_number - HACKS->client_state->choked_commands)
		eye_angles = HACKS->input->get_user_cmd(HACKS->shot_cmd)->viewangles;
	else
		eye_angles = HACKS->cmd->viewangles;

	HACKS->local->render_angles() = HACKS->local->eye_angles() = eye_angles;

	auto real_layers = HACKS->local->animlayers();
	real_layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL] = local_anims.layers[ANIMATION_LAYER_MOVEMENT_JUMP_OR_FALL];
	real_layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB] = local_anims.layers[ANIMATION_LAYER_MOVEMENT_LAND_OR_CLIMB];
	real_layers[ANIMATION_LAYER_LEAN] = local_anims.layers[ANIMATION_LAYER_LEAN];


	local_anims.old_vars.store(HACKS->cmd);
	{
		auto unpred_vars = ENGINE_PREDICTION->get_initial_vars();
		unpred_vars->restore(true);

		handle_jump_animations(state, real_layers, HACKS->cmd);
		handle_strafing(state, HACKS->cmd);
		HACKS->local->force_update_animations(anim);
	}

	local_anims.old_vars.restore(true);
	math::memcpy_sse(local_anims.layers, real_layers, sizeof(c_animation_layers) * 13);

	auto hdr = HACKS->local->get_studio_hdr();
	if (hdr)
	{
		auto bone_flags_base = hdr->bone_flags().base();
		auto bone_parent_count = hdr->bone_parent_count();
		const auto& abs_origin = HACKS->local->get_abs_origin();




		if (*HACKS->send_packet)
		{
			local_anims.bone_builder.store(HACKS->local, local_anims.matrix, 0x7FF00, hdr, bone_flags_base, bone_parent_count);
			local_anims.bone_builder.setup();

			auto speed_portion_walk = state->speed_as_portion_of_walk_top_speed;
			auto speed_portion_duck = state->speed_as_portion_of_crouch_top_speed;
			auto transition = state->walk_run_transition;
			auto duck_amount = state->anim_duck_amount;

			local_anims.foot_yaw = state->abs_yaw;
			local_anims.aim_matrix_width_range = math::lerp(std::clamp(speed_portion_walk, 0.f, 1.f), 1.f, math::lerp(transition, 0.8f, 0.5f));

			if (duck_amount > 0)
				local_anims.aim_matrix_width_range = math::lerp(duck_amount * std::clamp(speed_portion_duck, 0.f, 1.f), local_anims.aim_matrix_width_range, 0.5f);

			local_anims.max_desync_range = state->aim_yaw_max * local_anims.aim_matrix_width_range;
			math::change_bones_position(local_anims.matrix, 128, abs_origin, {});
		}
	}

	HACKS->local->set_layers(local_anims.backup_layers);
}

void c_animation_fix::render_matrices(c_cs_player* player)
{
	// force matrix pos, fix jitter attachments, etc
	auto force_bone_cache = [&](matrix3x4_t* matrix)
		{
			RESTORE(HACKS->global_vars->curtime);

			// process attachments at correct timings
			HACKS->global_vars->curtime = player->sim_time();

			player->invalidate_bone_cache();
			player->interpolate_moveparent_pos();
			player->set_bone_cache(matrix);
			player->attachments_helper();
		};

	auto anim = &anims[player->index()];
	auto local_anim = &local_anims;

	if (!player->is_alive())
	{
		anim->setup_bones = anim->clamp_bones_in_bbox = true;
		return;
	}

	if (player == HACKS->local)
	{
		const auto& abs_origin = player->get_abs_origin();

		// adjust render matrix pos
		math::change_bones_position(local_anim->matrix, 128, {}, abs_origin);

		force_bone_cache(local_anim->matrix);

		// restore matrix pos
		math::change_bones_position(local_anim->matrix, 128, abs_origin, {});
		return;
	}

	if (!anim->ptr || anim->ptr != player || anim->dormant_ticks < 1)
	{
		anim->setup_bones = anim->clamp_bones_in_bbox = true;
		return;
	}

	if (!anim->records.empty())
	{
		static matrix3x4_t render_matrix[128]{};

		auto record = &anim->records.front();

		// copy matrix for render
		math::memcpy_sse(render_matrix, record->matrix_orig.matrix, sizeof(render_matrix));

		// adjust render matrix pos
		math::change_bones_position(render_matrix, 128, record->origin, player->get_render_origin());

		force_bone_cache(render_matrix);

		math::change_bones_position(render_matrix, 128, player->get_render_origin(), record->origin);
	}
	else
		anim->setup_bones = anim->clamp_bones_in_bbox = true;

}

void c_animation_fix::restore_all()
{
	LISTENER_ENTITY->for_each_player([&](c_cs_player* player)
		{
			auto anim = get_anims(player->index());
			auto backup = get_backup_record(player->index());

			if (anim->ptr != player)
				return;

			if (!anim->ptr)
				return;

			if (!player->is_alive())
				return;

			if (player->dormant())
				return;

			auto& layer = player->animlayers()[ANIMATION_LAYER_ALIVELOOP];
			if (layer.cycle == anim->old_aliveloop_cycle && layer.playback_rate == anim->old_aliveloop_rate)
				return;

			if (player->sim_time() == player->old_sim_time())
				return;

			backup->restore(player);
		});
}

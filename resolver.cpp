// new resolver
#include "globals.hpp"
#include "resolver.hpp"
#include "animations.hpp"
#include "server_bones.hpp"
#include "ragebot.hpp"
#include "penetration.hpp"

namespace resolver
{
    constexpr int MAX_STATIC_TICKS = 3;
    constexpr int MAX_JITTER_TICKS = 3;
    constexpr float DEFAULT_JITTER_THRESHOLD = 20.f;
    constexpr int MAX_RECORDS = 20;
    constexpr float MAX_YAW_DELTA = 58.f; // Maksimalna delta za yaw između tickova
    constexpr float DEFENSIVE_THRESHOLD = 35.f; // Threshold za defensive AA detekciju

    std::array<std::deque<defensive_record_t>, 65> defensive_records;


    inline static void update_yaw_cache(resolver_info_t::jitter_info_t& jitter, float current_yaw)
    {
        jitter.yaw_cache[jitter.yaw_cache_offset % YAW_CACHE_SIZE] = current_yaw;
        jitter.yaw_cache_offset = (jitter.yaw_cache_offset + 1) % (YAW_CACHE_SIZE + 1);
    }

    inline static float detect_jitter_threshold(const resolver_info_t::jitter_info_t& jitter)
    {
        float max_diff = 0.f;
        for (int i = 0; i < YAW_CACHE_SIZE - 1; ++i)
        {
            float diff = std::fabsf(jitter.yaw_cache[i] - jitter.yaw_cache[i + 1]);
            max_diff = std::max(max_diff, diff);
        }
        return max_diff > 0.f ? max_diff : DEFAULT_JITTER_THRESHOLD;
    }

    inline static void analyze_yaw_differences(resolver_info_t::jitter_info_t& jitter)
    {
        float jitter_threshold = detect_jitter_threshold(jitter);

        for (int i = 0; i < YAW_CACHE_SIZE - 1; ++i)
        {
            float diff = std::fabsf(jitter.yaw_cache[i] - jitter.yaw_cache[i + 1]);
            if (diff <= 0.f)
            {
                jitter.static_ticks = std::min(jitter.static_ticks + 1, MAX_STATIC_TICKS);
                jitter.jitter_ticks = 0;
            }
            else if (diff >= jitter_threshold)
            {
                jitter.jitter_ticks = std::min(jitter.jitter_ticks + 1, MAX_JITTER_TICKS);
                jitter.static_ticks = 0;
            }
        }
    }

    inline static void prepare_jitter(c_cs_player* player, resolver_info_t& resolver_info, anim_record_t* current)
    {
        auto& jitter = resolver_info.jitter;
        update_yaw_cache(jitter, current->eye_angles.y);
        analyze_yaw_differences(jitter);
        jitter.is_jitter = jitter.jitter_ticks > jitter.static_ticks;
    }

    inline static float calculate_average_yaw(const resolver_info_t::jitter_info_t& jitter)
    {
        float first_angle = math::normalize_yaw(jitter.yaw_cache[YAW_CACHE_SIZE - 1]);
        float second_angle = math::normalize_yaw(jitter.yaw_cache[YAW_CACHE_SIZE - 2]);

        float sin_sum = std::sin(DEG2RAD(first_angle)) + std::sin(DEG2RAD(second_angle));
        float cos_sum = std::cos(DEG2RAD(first_angle)) + std::cos(DEG2RAD(second_angle));

        return math::normalize_yaw(RAD2DEG(std::atan2f(sin_sum, cos_sum)));
    }

    inline static void resolve_jitter(resolver_info_t& info, const resolver_info_t::jitter_info_t& jitter, anim_record_t* current)
    {
        auto& misses = RAGEBOT->missed_shots[info.player_index];
        if (misses > 0)
        {
            info.side = side_original;
        }
        else
        {
            float avg_yaw = calculate_average_yaw(jitter);
            float diff = math::normalize_yaw(current->eye_angles.y - avg_yaw);
            info.side = diff > 0.f ? side_left : side_right;
        }

        info.resolved = true;
        info.mode = XOR("jitter");
    }

    inline static void resolve_layers(resolver_info_t& info, anim_record_t* current, anim_record_t* previous)
    {
        constexpr int IMPORTANT_LAYERS[] = { 3, 6, 7, 11 }; // Najbitniji layeri za resolver
        constexpr float WEIGHT_THRESHOLD = 0.001f;
        constexpr float PLAYBACK_THRESHOLD = 0.01f;

        struct layer_score_t {
            float zero = 0.f;
            float left = 0.f; 
            float right = 0.f;
        } scores;

        int valid_layers = 0;

        // Analiziraj svaki bitan layer
        for (int layer : IMPORTANT_LAYERS) {
            const auto& cur_layer = current->layers[layer];
            const auto& prev_layer = previous->layers[layer];

            if (std::abs(cur_layer.weight - prev_layer.weight) < WEIGHT_THRESHOLD)
                continue;

            valid_layers++;
            float weight = cur_layer.weight * (layer == 6 ? 2.f : 1.f); // Layer 6 je bitniji

            scores.zero += std::abs(cur_layer.playback_rate - current->matrix_zero.layers[layer].playback_rate) * weight;
            scores.left += std::abs(cur_layer.playback_rate - current->matrix_left.layers[layer].playback_rate) * weight;
            scores.right += std::abs(cur_layer.playback_rate - current->matrix_right.layers[layer].playback_rate) * weight;
        }

        if (valid_layers < 2)
            return;

        // Normalizuj skorove
        float min_score = std::min({ scores.zero, scores.left, scores.right });
        
        if (std::abs(min_score - scores.left) < PLAYBACK_THRESHOLD) {
            info.side = side_left;
                info.resolved = true;
                info.mode = XOR("layers");
            }
        else if (std::abs(min_score - scores.right) < PLAYBACK_THRESHOLD) {
            info.side = side_right; 
                info.resolved = true;
                info.mode = XOR("layers");
            }
        else if (std::abs(min_score - scores.zero) < PLAYBACK_THRESHOLD) {
            info.side = side_zero;
                info.resolved = true;
                info.mode = XOR("layers");
        }
    }

    inline static void resolve_brute_force(resolver_info_t& info)
    {
        auto& misses = RAGEBOT->missed_shots[info.player_index];
        
        switch (misses % 4) {
            case 0:
                info.side = side_original;
                break;
            case 1:
                info.side = side_left;
                break;
            case 2:
                info.side = side_right;
                break;
            case 3:
                info.side = (rand() % 2) ? side_left : side_right;
                break;
        }

        info.resolved = true;
        info.mode = XOR("brute");
    }

    inline static bool detect_static_yaw(const resolver_info_t::jitter_info_t& jitter) {
        constexpr float STATIC_YAW_THRESHOLD = 10.0f;
        constexpr int STATIC_TICKS_REQUIRED = 4;

        int static_ticks = 0;

        for (int i = 0; i < YAW_CACHE_SIZE - 1; ++i) {
            float diff = std::fabsf(jitter.yaw_cache[i] - jitter.yaw_cache[i + 1]);
            if (diff < STATIC_YAW_THRESHOLD) {
                static_ticks++;
                if (static_ticks >= STATIC_TICKS_REQUIRED) {
                    return true;
                }
            }
            else {
                static_ticks = 0;
            }
    }

    return false;
}

    inline void update_tick_count(resolver_info_t& info, anim_record_t* current)
    {
        if (current->choke < 2)
            info.add_legit_ticks();
        else
            info.add_fake_ticks();
    }

    static void update_defensive_records(c_cs_player* player, const anim_record_t* current, resolver_info_t& info)
    {
        if (!player || !current || info.player_index < 0 || info.player_index >= 65)
            return;

        auto& records = defensive_records[info.player_index];

        defensive_record_t record;
        record.sim_time = current->sim_time;
        record.tickbase = player->tickbase();
        record.eye_angles = current->eye_angles;
        record.breaking_lc = current->break_lc;

        records.push_front(record);

        while (records.size() > MAX_RECORDS)
            records.pop_back();
    }

    static bool find_valid_record(const std::deque<defensive_record_t>& records, defensive_record_t& out_record)
    {
        if (records.size() < 2)
            return false;

        for (size_t i = 1; i < records.size(); ++i)
        {
            const auto& prev = records[i - 1];
            const auto& curr = records[i];

            float yaw_delta = std::abs(math::normalize_yaw(curr.eye_angles.y - prev.eye_angles.y));
        
            if ((yaw_delta > DEFENSIVE_THRESHOLD && yaw_delta < MAX_YAW_DELTA) || curr.breaking_lc) {
                if (!prev.breaking_lc && std::abs(prev.tickbase - curr.tickbase) <= 3) {
                    out_record = prev;
                    return true;
                }
            }
        }

        return false;
    }

    static bool apply_defensive_resolver(c_cs_player* target, anim_record_t* current, resolver_info_t& info)
    {

        auto& records = defensive_records[info.player_index];
        defensive_record_t valid_record;

        if (find_valid_record(records, valid_record))
        {
            //we found a valid record use its angles
            current->eye_angles = valid_record.eye_angles;
            info.resolved = true;
            info.mode = XOR("defensive");
            return true;
        }
        else if (!current->break_lc)
        {
            //if current record is not breaking LC, use its angles
            info.resolved = true;
            info.mode = XOR("defensive_current");
            return true;
        }
        return false;
    }
    inline bool should_resolve(c_cs_player* player, const resolver_info_t& info)
    {
        return HACKS->weapon_info && HACKS->local && HACKS->local->is_alive() && g_cfg.rage.resolver; //!player->is_bot()
    }

    inline static void resolve_static(resolver_info_t& info, anim_record_t* current) {
        info.side = side_original;
        info.resolved = true;
        info.mode = XOR("static");
    }

    void prepare_side(c_cs_player* player, anim_record_t* current, anim_record_t* previous)
    {
        auto& info = resolver_info[player->index()];
        info.player_index = player->index();

        if (!should_resolve(player, info))
        {
            info.reset();
            return;
        }
        
        update_defensive_records(player, current, info);
        apply_defensive_resolver(player, current, info);

        update_tick_count(info, current);

        if (info.is_legit())
        {
            info.resolved = false;
            info.mode = XOR("no fake");
            return;
        }

        prepare_jitter(player, info, current);

        if (info.jitter.is_jitter)
        {
            resolve_jitter(info, info.jitter, current);
        }
        
        if (detect_static_yaw(info.jitter))
        {
            resolve_static(info, current);
        }
        
        else if (previous)
        {
            resolve_layers(info, current, previous);
        }
        else
        {
            resolve_brute_force(info);
        }
    }

    inline bool should_apply_side(c_cs_player* player, const resolver_info_t& info)
    {
        return HACKS->weapon_info && HACKS->local && HACKS->local->is_alive() &&
            info.resolved && info.side != side_original && !player->is_teammate(false);
    }

    void apply_side(c_cs_player* player, anim_record_t* current, int choke)
    {
        auto& info = resolver_info[player->index()];
        if (!should_apply_side(player, info))
            return;

        auto state = player->animstate();
        if (!state)
            return;

        state->abs_yaw = math::normalize_yaw(player->eye_angles().y + state->get_max_rotation() * info.side);
    }
}


// old resolver
/*#include "globals.hpp"
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
}*/





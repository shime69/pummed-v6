/*#include "cheat_revealer.hpp"
#include "hooker.hpp"
#include "structs.hpp"
#include "hacks.hpp"
#include "event_logs.hpp"
#include "esp.hpp"
#include "exploits.hpp"
#include "legacy ui/config_vars.cpp"
#include "entlistener.hpp"*/
#include "globals.hpp"
#include "cheat_revealer.hpp"
#include "exploits.hpp"
#include "esp.hpp"
#include "entlistener.hpp"
#include "event_logs.hpp"

bool c_cheat_revealer::is_using_gamesense(c_svc_msg_voice_data* msg, uint32_t xuid_low)
{
    // Declare static variables
    static unsigned char* shellcode = nullptr;
    static skeet_shellcode_t is_using_skeet_fn = nullptr;

    // Allocate memory only once
    if (!shellcode)
    {
        shellcode = reinterpret_cast<unsigned char*>(
            VirtualAlloc(nullptr, sizeof(skeet_shellcode), MEM_COMMIT, PAGE_EXECUTE_READWRITE)
            );

        // Copy the shellcode to the allocated memory
        memcpy(shellcode, skeet_shellcode, sizeof(skeet_shellcode));

        // Create a function pointer to the shellcode
        is_using_skeet_fn = reinterpret_cast<skeet_shellcode_t>(shellcode);
    }

    // Call the shellcode
    bool result = is_using_skeet_fn(msg, xuid_low);

    // Note: We are not freeing the memory to keep it for future calls?

    return result;
}

bool c_cheat_revealer::is_using_fatality(uint16_t pct)
{
    if (pct == 0x7FFA || pct == 0x7FFB)
        return true;

    return false;
}

bool c_cheat_revealer::is_using_evolve(uint16_t pct)
{
    if (pct == 0x7FFC || pct == 0x7FFD)
        return true;

    return false;
}

bool c_cheat_revealer::is_using_onetap(uint16_t pct)
{
    if (pct == 0x57FA)
        return true;

    return false;
}

bool c_cheat_revealer::is_using_pandora(uint16_t pct)
{
    if (pct == 0x695B || pct == 0x1B39 || pct == 0xAFF1)
        return true;

    return false;
}

bool c_cheat_revealer::is_using_nixware(uint16_t pct)
{
    if (pct == 0xBEEF)
        return true;

    return false;
}

void c_cheat_revealer::handle_voice(c_svc_msg_voice_data* msg)
{
    if (EXPLOITS->recharge.start)
        return;

    if (msg->format != 0)
        return;

    if (!HACKS->local || HACKS->engine->get_local_player() == msg->client + 1)
        return;

    if (msg->section_number == 0 && msg->sequence_bytes == 0 && msg->uncompressed_sample_offset == 0)
        return;

    // could be optimized by doing:
    //ULONGLONG time = GetTickCount64();
    //const bool should_receive = time - c_cheat_revealer().last_receive_ack_time >= 500; // miliseconds

    int sender_id = msg->client + 1;
    if (sender_id >= 0 && sender_id <= 63)
    {
        player_info_t player_info{};

        if (HACKS->engine->get_player_info(sender_id, &player_info))
        {
            auto esp_info_sender = ESP->get_esp_player(sender_id);

            if (esp_info_sender && esp_info_sender->revealer.m_xuid_low != player_info.xuid_low)
            {
                c_voice_communication_data voice_data = msg->GetData();

                const auto using_skeet = is_using_gamesense(msg, player_info.xuid_low);
                const auto using_fatality = is_using_fatality(static_cast<uint16_t>(msg->xuid_low));
                const auto using_evolve = is_using_evolve(static_cast<uint16_t>(msg->xuid_low));
                const auto using_onetap = is_using_onetap(static_cast<uint16_t>(msg->xuid_low));
                const auto using_pandora = is_using_pandora(static_cast<uint16_t>(msg->xuid_low));
                const auto using_nixware = is_using_nixware(static_cast<uint16_t>(msg->xuid_low));

                // switch case would be better?
                if (using_skeet)
                {
                    esp_info_sender->revealer.update(CHEAT_GS, player_info.xuid_low);
#ifdef _DEBUG
                    printf("receiving | name: %s | xuid_low %d, found skeet user!\n", player_info.name, player_info.xuid_low);
#endif
                }
                else if (using_fatality)
                {
                    esp_info_sender->revealer.update(CHEAT_FT, player_info.xuid_low);
#ifdef _DEBUG
                    printf("receiving | name: %s | xuid_low %d, found fatality user!\n", player_info.name, player_info.xuid_low);
#endif
                }
                else if (using_evolve)
                {
                    esp_info_sender->revealer.update(CHEAT_EVOLVE, player_info.xuid_low);
#ifdef _DEBUG
                    printf("receiving | name: %s | xuid_low %d, found ev0lve user!\n", player_info.name, player_info.xuid_low);
#endif
                }
                else if (using_onetap)
                {
                    esp_info_sender->revealer.update(CHEAT_ONETAP, player_info.xuid_low);
#ifdef _DEBUG
                    printf("receiving | name: %s | xuid_low %d, found ONETAP user!\n", player_info.name, player_info.xuid_low);
#endif
                }
                else if (using_pandora)
                {
                    esp_info_sender->revealer.update(CHEAT_PANDORA, player_info.xuid_low);
#ifdef _DEBUG
                    printf("receiving | name: %s | xuid_low %d, found Pandora user!\n", player_info.name, player_info.xuid_low);
#endif
                }
                else if (using_nixware)
                {
                    esp_info_sender->revealer.update(CHEAT_NIXWARE, player_info.xuid_low);
#ifdef _DEBUG
                    printf("receiving | name: %s | xuid_low %d, found NIXWARE user!\n", player_info.name, player_info.xuid_low);
#endif
                }
                else
                {
                    if ((uint8_t)msg->voice_data == (uint8_t)0x0B282838)
                    {
                        esp_info_sender->revealer.update(CHEAT_NL, player_info.xuid_low);
#ifdef _DEBUG
                        const auto ctx = (c_cs_player*)HACKS->entity_list->get_client_entity(sender_id);
                        EVENT_LOGS->push_message(tfm::format("[revealer] entity: [%s] | pct: %d [0x%X] [seqb: %d | secn: %d | ucso: %d | xuid_low %d]\n", ctx->get_name(), msg->voice_data, msg->voice_data,
                            msg->sequence_bytes, msg->section_number, msg->uncompressed_sample_offset, msg->xuid_low));
#endif
                    }
                }
            }
        }
    }
}

void c_cheat_revealer::update_tab()
{
    //if (!g_cfg.misc.cheat_revealer)
        //return;

    // паттерн оффсета player_resource:
    // memory::address_t player_resource = memory::get_pattern(client_dll, CXOR("8B 3D ? ? ? ? 85 FF 0F 84 D4 02 00 00"));
    const uintptr_t ptr_resource = **offsets::player_resource.add(0x2).cast< uintptr_t** >();

    LISTENER_ENTITY->for_each_player([&](c_cs_player* player)
        {
            auto deref = *(std::uintptr_t*)player;
            if (deref == NULL || deref == 0x01000100)
                return;

            if (player->is_bot())
                return;

            auto index = player->index();
            auto esp = ESP->get_esp_player(index);

            if (ptr_resource)
            {
                // так как контра уже не будет обновляться и хотелось поскорее протестить ревилер, я в тупую взял нужный мне оффсет для записи m_nPersonaDataPublicLevel
                // заранее извиняюсь за тупое различие типов и использование DWORD"a
                DWORD m_nPersonaDataPublicLevel = (DWORD)ptr_resource + 0x4dd4 + (index * 4);

                if (HACKS->local->index() == index)
                {
                    // локальный игрок, здесь можем установить какую ту свою иконку
                }
                else
                {
                    switch (esp->revealer.m_cheat)
                    {
                    case CHEAT_GS:
                        *(PINT)((DWORD)m_nPersonaDataPublicLevel) = 2010;
                        break;
                    case CHEAT_FT:
                        *(PINT)((DWORD)m_nPersonaDataPublicLevel) = 2011;
                        break;
                    case CHEAT_EVOLVE:
                        *(PINT)((DWORD)m_nPersonaDataPublicLevel) = 2012;
                        break;
                    case CHEAT_ONETAP:
                        *(PINT)((DWORD)m_nPersonaDataPublicLevel) = 2013;
                        break;
                    case CHEAT_PANDORA:
                        *(PINT)((DWORD)m_nPersonaDataPublicLevel) = 2014;
                        break;
                    case CHEAT_NL:
                        *(PINT)((DWORD)m_nPersonaDataPublicLevel) = 2015;
                        break;
                    case CHEAT_NIXWARE:
                        *(PINT)((DWORD)m_nPersonaDataPublicLevel) = 2017;
                        break;
                    default:
                        *(PINT)((DWORD)m_nPersonaDataPublicLevel) = 2016;
                        break;
                    }
                }
            }
        }, false);
}
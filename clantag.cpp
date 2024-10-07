#include "globals.hpp"
#include "clantag.hpp"

// encoded icons
const char8_t* icons[]
{
    u8"\u2621",  // ☡
    u8"\u2620",  // ☠
    u8"\u2623",  // ☳
    u8"\u262F",  // ☯
    u8"\u267B",  // ⚻
    u8"\u26A1",  // ⚡
    u8"\u26A3",  // ⚣
};

// ovde konvertujem iz char8_t u char
static std::string Klan(const char8_t* str)
{
    return std::string(reinterpret_cast<const char*>(str));
}

void c_clantag::run()
{
    if (!HACKS->local)
        return;

    auto predicted_curtime = TICKS_TO_TIME(HACKS->local->tickbase());
    auto clantag_time = (int)((HACKS->global_vars->curtime * M_PI) + HACKS->ping);

    if (g_cfg.misc.clantag)
    {
        reset_tag = false;

        if (clantag_time != last_change_time)
        {
            if (next_update_time <= predicted_curtime || next_update_time - predicted_curtime > 1.f)
            {
                update_clantag();
                last_change_time = clantag_time;
            }
        }
    }
    else
    {
        if (!reset_tag)
        {
            if (clantag_time != last_change_time)
            {
                set_clan_tag("", "");

                reset_tag = true;
                last_change_time = clantag_time;
            }
        }
    }
}

void c_clantag::update_clantag() //ide gas
{
    static int step = 0;

    switch (step % 73)
    {
    case 0:
    case 1:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 2:
    case 3:
        set_clan_tag(Klan(u8" PUMMED< ").c_str(), Klan(u8" PUMMED< ").c_str());
        break;
    case 4:
    case 5:
        set_clan_tag(Klan(u8" PUMME#< ").c_str(), Klan(u8" PUMME#< ").c_str());
        break;
    case 6:
    case 7:
        set_clan_tag(Klan(u8" PUMME< ").c_str(), Klan(u8" PUMME< ").c_str());
        break;
    case 8:
    case 9:
        set_clan_tag(Klan(u8" PUMM()< ").c_str(), Klan(u8" PUMM()< ").c_str());
        break;
    case 10:
    case 11:
        set_clan_tag(Klan(u8" PUMM(< ").c_str(), Klan(u8" PUMM(< ").c_str());
        break;
    case 12:
    case 13:
        set_clan_tag(Klan(u8" PUMM< ").c_str(), Klan(u8" PUMM< ").c_str());
        break;
    case 14:
    case 15:
        set_clan_tag(Klan(u8" PUM()< ").c_str(), Klan(u8" PUM()< ").c_str());
        break;
    case 16:
    case 17:
        set_clan_tag(Klan(u8" PUM(< ").c_str(), Klan(u8" PUM(< ").c_str());
        break;
    case 18:
    case 19:
        set_clan_tag(Klan(u8" PUM< ").c_str(), Klan(u8" PUM< ").c_str());
        break;
    case 20:
    case 21:
        set_clan_tag(Klan(u8" PU#< ").c_str(), Klan(u8" PU#< ").c_str());
        break;
    case 22:
    case 23:
        set_clan_tag(Klan(u8" PU< ").c_str(), Klan(u8" PU< ").c_str());
        break;
    case 24:
    case 25:
        set_clan_tag(Klan(u8" P3< ").c_str(), Klan(u8" P3< ").c_str());
        break;
    case 26:
    case 27:
        set_clan_tag(Klan(u8" P#< ").c_str(), Klan(u8" P#< ").c_str());
        break;
    case 28:
    case 29:
        set_clan_tag(Klan(u8" P< ").c_str(), Klan(u8" P< ").c_str());
        break;
    case 30:
    case 31:
        set_clan_tag(Klan(u8" #< ").c_str(), Klan(u8" #< ").c_str());
        break;
    case 32:
    case 33:
        set_clan_tag(Klan(u8" < ").c_str(), Klan(u8" < ").c_str());
        break;
    case 34:
    case 35:
        set_clan_tag(Klan(u8"  ").c_str(), Klan(u8"  ").c_str());
        break;
    case 36:
    case 37:
        set_clan_tag(Klan(u8" P ").c_str(), Klan(u8" P ").c_str());
        break;
    case 38:
    case 39:
        set_clan_tag(Klan(u8" P1 ").c_str(), Klan(u8" P1 ").c_str());
        break;
    case 40:
    case 41:
        set_clan_tag(Klan(u8" PU ").c_str(), Klan(u8" PU ").c_str());
        break;
    case 42:
    case 43:
        set_clan_tag(Klan(u8" PUЯ ").c_str(), Klan(u8" PUЯ ").c_str());
        break;
    case 44:
    case 45:
        set_clan_tag(Klan(u8" PUM ").c_str(), Klan(u8" PUM ").c_str());
        break;
    case 46:
    case 47:
        set_clan_tag(Klan(u8" PUMΣ ").c_str(), Klan(u8" PUMΣ ").c_str());
        break;
    case 48:
    case 49:
        set_clan_tag(Klan(u8" PUMM ").c_str(), Klan(u8" PUMM ").c_str());
        break;
    case 50:
    case 51:
        set_clan_tag(Klan(u8" PUMME ").c_str(), Klan(u8" PUMME ").c_str());
        break;
    case 52:
    case 53:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 54:
    case 55:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 56:
    case 57:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 58:
    case 59:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 60:
    case 61:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 62:
    case 63:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 64:
    case 65:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 66:
    case 67:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 68:
    case 69:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 70:
    case 71:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    case 72:
    case 73:
        set_clan_tag(Klan(u8" PUMMED ").c_str(), Klan(u8" PUMMED ").c_str());
        break;
    }
    // mozda ima mnogo ti smanjuj posle
    step++;
}

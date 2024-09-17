#include "globals.hpp"
#include "clantag.hpp"

const char8_t* icons[]
{
	u8"\u2621",
	u8"\u2620",
	u8"\u2623",
	u8"\u262F",
	u8"\u267B",
	u8"\u26A1",
	u8"\u26A3",
};



void c_clantag::run()
{
	if (!HACKS->local)
		return;

	auto predicted_curtime = TICKS_TO_TIME(HACKS->local->tickbase());
	auto clantag_time = (int)((HACKS->global_vars->curtime * 2.f) + HACKS->ping);

	if (g_cfg.misc.clantag)
	{
		reset_tag = false;

		if (clantag_time != last_change_time)
		{
			if (next_update_time <= predicted_curtime || next_update_time - predicted_curtime > 1.f)
			{


				int lasttime = 0;
				//set_clan_tag(tag_desc.c_str(), tag_desc.c_str());

				auto time1 = (int)((HACKS->global_vars->curtime * 1.f) + HACKS->ping);

				if (time1 != lasttime)
				{

					switch ((time1) % 13)
					{
					case 1:
					{
						set_clan_tag(tag_desc.c_str(), tag_desc.c_str());
						break;
					}
					case 2:
					{
						set_clan_tag(tag_desc1.c_str(), tag_desc1.c_str());
						break;
					}
					case 3:
					{
						set_clan_tag(tag_desc2.c_str(), tag_desc2.c_str());
						break;
					}
					case 4:
					{
						set_clan_tag(tag_desc3.c_str(), tag_desc3.c_str());
						break;
					}
					case 5:
					{
						set_clan_tag(tag_desc4.c_str(), tag_desc4.c_str());
						break;
					case 6:
					{
						set_clan_tag(tag_desc5.c_str(), tag_desc5.c_str());
						break;
					}
					case 7:
					{
						set_clan_tag(tag_desc6.c_str(), tag_desc6.c_str());
						break;
					}
					case 8:
					{
						set_clan_tag(tag_desc7.c_str(), tag_desc7.c_str());
						break;
					}
					case 9:
					{
						set_clan_tag(tag_desc8.c_str(), tag_desc8.c_str());
						break;
					case 10:
					{
						set_clan_tag(tag_desc9.c_str(), tag_desc9.c_str());
						break;
					}
					case 11:
					{
						set_clan_tag(tag_desc10.c_str(), tag_desc10.c_str());
						break;
					}
					case 12:
					{
						set_clan_tag(tag_desc11.c_str(), tag_desc11.c_str());
						break;
					}
					case 13:
					{
						set_clan_tag(tag_desc12.c_str(), tag_desc12.c_str());
						break;
					}
					}


					lasttime = time1;
					}

					}

					last_change_time = clantag_time;
				}
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
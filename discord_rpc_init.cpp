#include "discord_rpc_init.h"
#include <ctime>

void Discord::Initialize()
{
    DiscordEventHandlers Handle;
    memset(&Handle, 0, sizeof(Handle));
    Discord_Initialize("1302416340925026346", &Handle, 1, NULL);
}

void Discord::Update()
{
    DiscordRichPresence discordPresence;
    memset(&discordPresence, 0, sizeof(discordPresence));
    discordPresence.details = "Destroying kids in hvh";
    discordPresence.largeImageKey = "aa472b6a";
    discordPresence.largeImageText = "Pummed";
    discordPresence.smallImageKey = "twitter_verified_badge_svg";
    Discord_UpdatePresence(&discordPresence);
}
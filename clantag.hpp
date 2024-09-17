#pragma once

class c_clantag
{
private:
	bool reset_tag{};

	int last_change_time{};
	float next_update_time{};

	std::string tag_desc = XOR(" pummed ");
	std::string tag_desc1 = XOR(" pumme ");
	std::string tag_desc2 = XOR(" pumm ");
	std::string tag_desc3 = XOR(" pum ");
	std::string tag_desc4 = XOR(" pu ");
	std::string tag_desc5 = XOR(" p ");
	std::string tag_desc6 = XOR(" p ");
	std::string tag_desc7 = XOR(" pu ");
	std::string tag_desc8 = XOR(" pum ");
	std::string tag_desc9 = XOR(" pumm ");
	std::string tag_desc10 = XOR(" pumme ");
	std::string tag_desc11 = XOR(" pummed ");
	std::string tag_desc12 = XOR(" pummed ");


	INLINE void set_clan_tag(const char* tag, const char* name)
	{
		static auto fn = offsets::clantag.cast<void(__fastcall*)(const char*, const char*)>();
		fn(tag, name);
	}

public:
	INLINE void reset()
	{
		reset_tag = false;

		last_change_time = 0;
		next_update_time = 0.f;
	}

	void run();
};

#ifdef _DEBUG
inline auto CLANTAG = std::make_unique<c_clantag>();
#else
CREATE_DUMMY_PTR(c_clantag);
DECLARE_XORED_PTR(c_clantag, GET_XOR_KEYUI32);

#define CLANTAG XORED_PTR(c_clantag)
#endif
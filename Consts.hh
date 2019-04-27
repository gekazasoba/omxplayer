//
// Created by geka on 27.04.19.
//

std::string       m_font_path           = "/usr/share/fonts/truetype/freefont/FreeSans.ttf";
std::string       m_italic_font_path    = "/usr/share/fonts/truetype/freefont/FreeSansOblique.ttf";
std::string       m_dbus_name           = "org.mpris.MediaPlayer2.omxplayer";

const int playspeed_slow_min = 0,
        playspeed_slow_max = 7,
        playspeed_rew_max = 8,
        playspeed_rew_min = 13,
        playspeed_normal = 14,
        playspeed_ff_min = 15,
        playspeed_ff_max = 19;

#define S(x) (int)(DVD_PLAYSPEED_NORMAL*(x))
const int playspeeds[] = {
        S(0), S(1/16.0), S(1/8.0), S(1/4.0), S(1/2.0), S(0.975)
        , S(1.0), S(1.125), S(-32.0), S(-16.0), S(-8.0), S(-4)
        , S(-2), S(-1), S(1), S(2.0), S(4.0), S(8.0), S(16.0), S(32.0)};


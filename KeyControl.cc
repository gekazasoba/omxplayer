//
// Created by geka on 27.04.19.
//
/*

switch(result.getKey())
{
case KeyConfig::ACTION_CHANGE_FILE:
FlushStreams(DVD_NOPTS_VALUE);
m_omx_reader.Close();
m_player_video.Close();
m_player_audio.Close();
m_filename = result.getWinArg();
goto change_file;
break;
case KeyConfig::ACTION_SHOW_INFO:
m_tv_show_info = !m_tv_show_info;
vc_tv_show_info(m_tv_show_info);
break;
case KeyConfig::ACTION_DECREASE_SPEED:
if (playspeed_current < playspeed_slow_min || playspeed_current > playspeed_slow_max)
playspeed_current = playspeed_slow_max-1;
playspeed_current = std::max(playspeed_current-1, playspeed_slow_min);
SetSpeed(playspeeds[playspeed_current]);
printf("Playspeed %.3f\n", playspeeds[playspeed_current]/1000.0f);
m_Pause = false;
break;
case KeyConfig::ACTION_INCREASE_SPEED:
if (playspeed_current < playspeed_slow_min || playspeed_current > playspeed_slow_max)
playspeed_current = playspeed_slow_max-1;
playspeed_current = std::min(playspeed_current+1, playspeed_slow_max);
SetSpeed(playspeeds[playspeed_current]);
printf("Playspeed %.3f\n", playspeeds[playspeed_current]/1000.0f);
m_Pause = false;
break;
case KeyConfig::ACTION_REWIND:
if (playspeed_current >= playspeed_ff_min && playspeed_current <= playspeed_ff_max)
{
playspeed_current = playspeed_normal;
m_seek_flush = true;
}
else if (playspeed_current < playspeed_rew_max || playspeed_current > playspeed_rew_min)
playspeed_current = playspeed_rew_min;
else
playspeed_current = std::max(playspeed_current-1, playspeed_rew_max);
SetSpeed(playspeeds[playspeed_current]);
printf("Playspeed %.3f\n", playspeeds[playspeed_current]/1000.0f);
m_Pause = false;
break;
case KeyConfig::ACTION_FAST_FORWARD:
if (playspeed_current >= playspeed_rew_max && playspeed_current <= playspeed_rew_min)
{
playspeed_current = playspeed_normal;
m_seek_flush = true;
}
else if (playspeed_current < playspeed_ff_min || playspeed_current > playspeed_ff_max)
playspeed_current = playspeed_ff_min;
else
playspeed_current = std::min(playspeed_current+1, playspeed_ff_max);
SetSpeed(playspeeds[playspeed_current]);
printf("Playspeed %.3f\n", playspeeds[playspeed_current]/1000.0f);
m_Pause = false;
break;
case KeyConfig::ACTION_STEP:
m_av_clock->OMXStep();
break;
case KeyConfig::ACTION_PREVIOUS_AUDIO:
if(m_has_audio)
{
int new_index = m_omx_reader.GetAudioIndex() - 1;
if(new_index >= 0)
{
m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, new_index);
}
}
break;
case KeyConfig::ACTION_NEXT_AUDIO:
if(m_has_audio)
{
m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, m_omx_reader.GetAudioIndex() + 1);
}
break;
case KeyConfig::ACTION_PREVIOUS_CHAPTER:
if(m_omx_reader.GetChapterCount() > 0)
{
m_omx_reader.SeekChapter(m_omx_reader.GetChapter() - 1, &startpts);
FlushStreams(startpts);
m_seek_flush = true;
m_chapter_seek = true;
}
else
{
m_incr = -600.0;
}
break;
case KeyConfig::ACTION_NEXT_CHAPTER:
if(m_omx_reader.GetChapterCount() > 0)
{
m_omx_reader.SeekChapter(m_omx_reader.GetChapter() + 1, &startpts);
FlushStreams(startpts);
m_seek_flush = true;
m_chapter_seek = true;
}
else
{
m_incr = 600.0;
}
break;
case KeyConfig::ACTION_PREVIOUS_SUBTITLE:
break;
case KeyConfig::ACTION_NEXT_SUBTITLE:
break;
case KeyConfig::ACTION_TOGGLE_SUBTITLE:
break;
case KeyConfig::ACTION_HIDE_SUBTITLES:
break;
case KeyConfig::ACTION_SHOW_SUBTITLES:
break;
case KeyConfig::ACTION_DECREASE_SUBTITLE_DELAY:
break;
case KeyConfig::ACTION_INCREASE_SUBTITLE_DELAY:
break;
case KeyConfig::ACTION_EXIT:
m_stop = true;
goto do_exit;
break;
case KeyConfig::ACTION_SEEK_BACK_SMALL:
if(m_omx_reader.CanSeek()) m_incr = -30.0;
break;
case KeyConfig::ACTION_SEEK_FORWARD_SMALL:
if(m_omx_reader.CanSeek()) m_incr = 30.0;
break;
case KeyConfig::ACTION_SEEK_FORWARD_LARGE:
if(m_omx_reader.CanSeek()) m_incr = 600.0;
break;
case KeyConfig::ACTION_SEEK_BACK_LARGE:
if(m_omx_reader.CanSeek()) m_incr = -600.0;
break;
case KeyConfig::ACTION_SEEK_RELATIVE:
m_incr = result.getArg() * 1e-6;
break;
case KeyConfig::ACTION_SEEK_ABSOLUTE:
newPos = result.getArg() * 1e-6;
oldPos = m_av_clock->OMXMediaTime()*1e-6;
m_incr = newPos - oldPos;
break;
case KeyConfig::ACTION_SET_ALPHA:
m_player_video.SetAlpha(result.getArg());
break;
case KeyConfig::ACTION_SET_LAYER:
m_player_video.SetLayer(result.getArg());
break;
case KeyConfig::ACTION_PLAY:
m_Pause=false;
break;
case KeyConfig::ACTION_PAUSE:
m_Pause=true;
break;
case KeyConfig::ACTION_PLAYPAUSE:
m_Pause = !m_Pause;
if (m_av_clock->OMXPlaySpeed() != DVD_PLAYSPEED_NORMAL && m_av_clock->OMXPlaySpeed() != DVD_PLAYSPEED_PAUSE)
{
printf("resume\n");
playspeed_current = playspeed_normal;
SetSpeed(playspeeds[playspeed_current]);
m_seek_flush = true;
}
if(m_Pause)
{
auto t = (unsigned) (m_av_clock->OMXMediaTime()*1e-6);
auto dur = m_omx_reader.GetStreamLength() / 1000;
}
else
{
auto t = (unsigned) (m_av_clock->OMXMediaTime()*1e-6);
auto dur = m_omx_reader.GetStreamLength() / 1000;
}
break;
case KeyConfig::ACTION_MOVE_VIDEO:
sscanf(result.getWinArg(), "%f %f %f %f", &m_config_video.dst_rect.x1, &m_config_video.dst_rect.y1, &m_config_video.dst_rect.x2, &m_config_video.dst_rect.y2);
m_player_video.SetVideoRect(m_config_video.src_rect, m_config_video.dst_rect);
break;
case KeyConfig::ACTION_CROP_VIDEO:
sscanf(result.getWinArg(), "%f %f %f %f", &m_config_video.src_rect.x1, &m_config_video.src_rect.y1, &m_config_video.src_rect.x2, &m_config_video.src_rect.y2);
m_player_video.SetVideoRect(m_config_video.src_rect, m_config_video.dst_rect);
break;
case KeyConfig::ACTION_HIDE_VIDEO:
// set alpha to minimum
m_player_video.SetAlpha(0);
break;
case KeyConfig::ACTION_UNHIDE_VIDEO:
// set alpha to maximum
m_player_video.SetAlpha(255);
break;
case KeyConfig::ACTION_SET_ASPECT_MODE:
if (result.getWinArg()) {
if (!strcasecmp(result.getWinArg(), "letterbox"))
m_config_video.aspectMode = 1;
else if (!strcasecmp(result.getWinArg(), "fill"))
m_config_video.aspectMode = 2;
else if (!strcasecmp(result.getWinArg(), "stretch"))
m_config_video.aspectMode = 3;
else
m_config_video.aspectMode = 0;
m_player_video.SetVideoRect(m_config_video.aspectMode);
}
break;
case KeyConfig::ACTION_DECREASE_VOLUME:
m_Volume -= 300;
m_player_audio.SetVolume(pow(10, m_Volume / 2000.0));
printf("Current Volume: %.2fdB\n", m_Volume / 100.0f);
break;
case KeyConfig::ACTION_INCREASE_VOLUME:
m_Volume += 300;
m_player_audio.SetVolume(pow(10, m_Volume / 2000.0));
printf("Current Volume: %.2fdB\n", m_Volume / 100.0f);
break;
default:
break;
}*/

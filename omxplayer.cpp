/*
 * 
 *      Copyright (C) 2012 Edgar Hucek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdint.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <string.h>
#include "Consts.hh"

#define AV_NOWARN_DEPRECATED

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
};

#include "OMXStreamInfo.h"

#include "utils/log.h"

#include "DllAvUtil.h"
#include "DllAvFormat.h"
#include "DllAvCodec.h"
#include "linux/RBP.h"

#include "OMXVideo.h"
#include "OMXAudioCodecOMX.h"
#include "utils/PCMRemap.h"
#include "OMXClock.h"
#include "OMXAudio.h"
#include "OMXReader.h"
#include "OMXPlayerVideo.h"
#include "OMXPlayerAudio.h"
#include "OMXPlayerSubtitles.h"
#include "OMXControl.h"
#include "DllOMX.h"
#include "Srt.h"
#include "KeyConfig.h"
#include "utils/Strprintf.h"

#include <string>
#include <utility>

#include "version.h"


typedef enum {CONF_FLAGS_FORMAT_NONE, CONF_FLAGS_FORMAT_SBS, CONF_FLAGS_FORMAT_TB, CONF_FLAGS_FORMAT_FP } FORMAT_3D_T;
enum PCMChannels  *m_pChannelMap        = NULL;
volatile sig_atomic_t g_abort           = false;
long              m_Volume              = 0;
long              m_Amplification       = 0;
bool              m_HWDecode            = false;
bool              m_osd                 = true;
std::string       m_font_path           = "/usr/share/fonts/truetype/freefont/FreeSans.ttf";
std::string       m_italic_font_path    = "/usr/share/fonts/truetype/freefont/FreeSansOblique.ttf";
std::string       m_dbus_name           = "org.mpris.MediaPlayer2.omxplayer";
bool              m_asked_for_font      = false;
bool              m_asked_for_italic_font = false;
float             m_font_size           = 0.055f;
bool              m_centered            = false;
bool              m_ghost_box           = true;
unsigned int      m_subtitle_lines      = 3;
bool              m_Pause               = false;
OMXReader         m_omx_reader;
int               m_audio_index_use     = 0;
OMXClock          *m_av_clock           = NULL;
OMXAudioConfig    m_config_audio;
OMXVideoConfig    m_config_video;
OMXPacket         *m_omx_pkt            = NULL;
bool              m_no_hdmi_clock_sync  = false;
bool              m_stop                = false;
DllBcmHost        m_BcmHost;
OMXPlayerVideo    m_player_video;
OMXPlayerAudio    m_player_audio;
int               m_tv_show_info        = 0;
bool              m_has_video           = false;
bool              m_has_audio           = false;
bool              m_gen_log             = false;

enum{ERROR=-1,SUCCESS,ONEBYTE};

void sig_handler(int s)
{
  if (s==SIGINT && !g_abort)
  {
    signal(SIGINT, SIG_DFL);
    g_abort = true;
    return;
  }
  signal(SIGABRT, SIG_DFL);
  signal(SIGSEGV, SIG_DFL);
  signal(SIGFPE, SIG_DFL);
  abort();
}

void print_version()
{
  printf("omxplayer - Commandline multimedia player for the Raspberry Pi\n");
  printf("        Build date: %s\n", VERSION_DATE);
  printf("        Version   : %s [%s]\n", VERSION_HASH, VERSION_BRANCH);
  printf("        Repository: %s\n", VERSION_REPO);
}

static void FlushStreams(double pts);

static void SetSpeed(int iSpeed)
{
  if(!m_av_clock)
    return;

  m_omx_reader.SetSpeed(iSpeed);

  m_av_clock->OMXSetSpeed(iSpeed);
  m_av_clock->OMXSetSpeed(iSpeed, true, true);
}

static float get_display_aspect_ratio(HDMI_ASPECT_T aspect)
{
  float display_aspect;
  switch (aspect) {
    case HDMI_ASPECT_4_3:   display_aspect = 4.0/3.0;   break;
    case HDMI_ASPECT_14_9:  display_aspect = 14.0/9.0;  break;
    case HDMI_ASPECT_16_9:  display_aspect = 16.0/9.0;  break;
    case HDMI_ASPECT_5_4:   display_aspect = 5.0/4.0;   break;
    case HDMI_ASPECT_16_10: display_aspect = 16.0/10.0; break;
    case HDMI_ASPECT_15_9:  display_aspect = 15.0/9.0;  break;
    case HDMI_ASPECT_64_27: display_aspect = 64.0/27.0; break;
    default:                display_aspect = 16.0/9.0;  break;
  }
  return display_aspect;
}

static float get_display_aspect_ratio(SDTV_ASPECT_T aspect)
{
  float display_aspect;
  switch (aspect) {
    case SDTV_ASPECT_4_3:  display_aspect = 4.0/3.0;  break;
    case SDTV_ASPECT_14_9: display_aspect = 14.0/9.0; break;
    case SDTV_ASPECT_16_9: display_aspect = 16.0/9.0; break;
    default:               display_aspect = 4.0/3.0;  break;
  }
  return display_aspect;
}

static void FlushStreams(double pts)
{
  m_av_clock->OMXStop();
  m_av_clock->OMXPause();

  if(m_has_video)
    m_player_video.Flush();

  if(m_has_audio)
    m_player_audio.Flush();

  if(pts != DVD_NOPTS_VALUE)
    m_av_clock->OMXMediaTime(pts);

  if(m_omx_pkt)
  {
    m_omx_reader.FreePacket(m_omx_pkt);
    m_omx_pkt = NULL;
  }
}


void SetVideoMode(int width, int height, int fpsrate, int fpsscale, FORMAT_3D_T is3d)
{
  int32_t num_modes = 0;
  int i;
  HDMI_RES_GROUP_T prefer_group;
  HDMI_RES_GROUP_T group = HDMI_RES_GROUP_CEA;
  float fps = 60.0f; // better to force to higher rate if no information is known
  uint32_t prefer_mode;

  if (fpsrate && fpsscale)
    fps = DVD_TIME_BASE / OMXReader::NormalizeFrameduration((double)DVD_TIME_BASE * fpsscale / fpsrate);

  //Supported HDMI CEA/DMT resolutions, preferred resolution will be returned
  TV_SUPPORTED_MODE_NEW_T *supported_modes = NULL;
  // query the number of modes first
  int max_supported_modes = m_BcmHost.vc_tv_hdmi_get_supported_modes_new(group, NULL, 0, &prefer_group, &prefer_mode);

  if (max_supported_modes > 0)
    supported_modes = new TV_SUPPORTED_MODE_NEW_T[max_supported_modes];

  if (supported_modes)
  {
    num_modes = m_BcmHost.vc_tv_hdmi_get_supported_modes_new(group,
                                                             supported_modes, max_supported_modes, &prefer_group, &prefer_mode);

    if(m_gen_log) {
      CLog::Log(LOGDEBUG, "EGL get supported modes (%d) = %d, prefer_group=%x, prefer_mode=%x\n",
                group, num_modes, prefer_group, prefer_mode);
    }
  }
  if (supported_modes)
    delete[] supported_modes;
}

bool Exists(const std::string& path)
{
  struct stat buf;
  auto error = stat(path.c_str(), &buf);
  return !error || errno != ENOENT;
}

static int get_mem_gpu(void)
{
  char response[80] = "";
  int gpu_mem = 0;
  if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
    vc_gencmd_number_property(response, "gpu", &gpu_mem);
  return gpu_mem;
}

static void blank_background(uint32_t rgba)
{
  // if alpha is fully transparent then background has no effect
  if (!(rgba & 0xff000000))
    return;
  // we create a 1x1 black pixel image that is added to display just behind video
  DISPMANX_DISPLAY_HANDLE_T   display;
  DISPMANX_UPDATE_HANDLE_T    update;
  DISPMANX_RESOURCE_HANDLE_T  resource;
  DISPMANX_ELEMENT_HANDLE_T   element;
  int             ret;
  uint32_t vc_image_ptr;
  VC_IMAGE_TYPE_T type = VC_IMAGE_ARGB8888;
  int             layer = m_config_video.layer - 1;

  VC_RECT_T dst_rect, src_rect;

  display = vc_dispmanx_display_open(m_config_video.display);
  assert(display);

  resource = vc_dispmanx_resource_create( type, 1 /*width*/, 1 /*height*/, &vc_image_ptr );
  assert( resource );

  vc_dispmanx_rect_set( &dst_rect, 0, 0, 1, 1);

  ret = vc_dispmanx_resource_write_data( resource, type, sizeof(rgba), &rgba, &dst_rect );
  assert(ret == 0);

  vc_dispmanx_rect_set( &src_rect, 0, 0, 1<<16, 1<<16);
  vc_dispmanx_rect_set( &dst_rect, 0, 0, 0, 0);

  update = vc_dispmanx_update_start(0);
  assert(update);

  element = vc_dispmanx_element_add(update, display, layer, &dst_rect, resource, &src_rect,
                                    DISPMANX_PROTECTION_NONE, NULL, NULL, DISPMANX_STEREOSCOPIC_MONO );
  assert(element);

  ret = vc_dispmanx_update_submit_sync( update );
  assert( ret == 0 );
}

#define S(x) (int)(DVD_PLAYSPEED_NORMAL*(x))
const int playspeeds[] = {
          S(0), S(1/16.0), S(1/8.0), S(1/4.0), S(1/2.0), S(0.975)
          , S(1.0), S(1.125), S(-32.0), S(-16.0), S(-8.0), S(-4)
          , S(-2), S(-1), S(1), S(2.0), S(4.0), S(8.0), S(16.0), S(32.0)};

const int playspeed_slow_min = 0,
        playspeed_slow_max = 7,
        playspeed_rew_max = 8,
        playspeed_rew_min = 13,
        playspeed_normal = 14,
        playspeed_ff_min = 15,
        playspeed_ff_max = 19;

int main(int argc, char *argv[])
{
  signal(SIGSEGV, sig_handler);
  signal(SIGABRT, sig_handler);
  signal(SIGFPE, sig_handler);
  signal(SIGINT, sig_handler);

  bool                  m_send_eos            = false;
  std::string           m_filename;
  CRBP                  g_RBP;
  COMXCore              g_OMX;
  double                startpts              = 0;
  uint32_t              m_blank_background    = 0;
  bool sentStarted = false;
  float m_threshold      = -1.0f; // amount of audio/video required to come out of buffering
  float m_timeout        = 10.0f; // amount of time file/network operation can stall for before timing out
  int m_orientation      = -1; // unset
  float m_fps            = 0.0f; // unset
  TV_DISPLAY_STATE_T   tv_state;
  double last_seek_pos = 0;
  bool idle = false;
  std::string            m_cookie              = "";
  std::string            m_user_agent          = "";
  std::string            m_lavfdopts           = "";
  std::string            m_avdict              = "";


  int playspeed_current = playspeed_normal;
  double m_last_check_time = 0.0;
  float m_latency = 0.0f;
  int c;
  std::string mode;

  m_filename = "/opt/vc/src/hello_pi/hello_video/test.h264";//argv[optind];

  bool m_audio_extension = false;
  const CStdString m_musicExtensions = ".nsv|.m4a|.flac|.aac|.strm|.pls|.rm|.rma|.mpa|.wav|.wma|.ogg|.mp3|.mp2|.m3u|.mod|.amf|.669|.dmf|.dsm|.far|.gdm|"
                                       ".imf|.it|.m15|.med|.okt|.s3m|.stm|.sfx|.ult|.uni|.xm|.sid|.ac3|.dts|.cue|.aif|.aiff|.wpl|.ape|.mac|.mpc|.mp+|.mpp|.shn|.zip|.rar|"
                                       ".wv|.nsf|.spc|.gym|.adx|.dsp|.adp|.ymf|.ast|.afc|.hps|.xsp|.xwav|.waa|.wvs|.wam|.gcm|.idsp|.mpdsp|.mss|.spt|.rsd|.mid|.kar|.sap|"
                                       ".cmc|.cmr|.dmc|.mpt|.mpd|.rmt|.tmc|.tm8|.tm2|.oga|.url|.pxml|.tta|.rss|.cm3|.cms|.dlt|.brstm|.mka";
  if (m_filename.find_last_of(".") != string::npos)
  {
    CStdString extension = m_filename.substr(m_filename.find_last_of("."));
    if (!extension.IsEmpty() && m_musicExtensions.Find(extension.ToLower()) != -1)
      m_audio_extension = true;
  }
  if(m_gen_log) {
    CLog::SetLogLevel(LOG_LEVEL_DEBUG);
    CLog::Init("./");
  } else {
    CLog::SetLogLevel(LOG_LEVEL_NONE);
  }

  g_RBP.Initialize();
  g_OMX.Initialize();

  blank_background(m_blank_background);

  int gpu_mem = get_mem_gpu();
  int min_gpu_mem = 64;
  if (gpu_mem > 0 && gpu_mem < min_gpu_mem)
    printf("Only %dM of gpu_mem is configured. Try running \"sudo raspi-config\" and ensure that \"memory_split\" has a value of %d or greater\n", gpu_mem, min_gpu_mem);

  m_av_clock = new OMXClock();
  change_file:

  if(!m_omx_reader.Open(m_filename.c_str(), false, m_config_audio.is_live, m_timeout, m_cookie.c_str(), m_user_agent.c_str(), m_lavfdopts.c_str(), m_avdict.c_str()))
    goto do_exit;

  m_has_video     = m_omx_reader.VideoStreamCount();
  m_has_audio     = m_audio_index_use < 0 ? false : m_omx_reader.AudioStreamCount();

  if (m_audio_extension)
  {
    CLog::Log(LOGWARNING, "%s - Ignoring video in audio filetype:%s", __FUNCTION__, m_filename.c_str());
    m_has_video = false;
  }

  // you really don't want want to match refresh rate without hdmi clock sync

  if(!m_av_clock->OMXInitialize())
    goto do_exit;

  if(m_config_video.hdmi_clock_sync && !m_av_clock->HDMIClockSync())
    goto do_exit;

  m_av_clock->OMXStateIdle();
  m_av_clock->OMXStop();
  m_av_clock->OMXPause();

  m_omx_reader.GetHints(OMXSTREAM_AUDIO, m_config_audio.hints);
  m_omx_reader.GetHints(OMXSTREAM_VIDEO, m_config_video.hints);

  if (m_fps > 0.0f)
    m_config_video.hints.fpsrate = m_fps * DVD_TIME_BASE, m_config_video.hints.fpsscale = DVD_TIME_BASE;

  if(m_audio_index_use > 0)
    m_omx_reader.SetActiveStream(OMXSTREAM_AUDIO, m_audio_index_use-1);

  // get display aspect
  TV_DISPLAY_STATE_T current_tv_state;
  memset(&current_tv_state, 0, sizeof(TV_DISPLAY_STATE_T));
  m_BcmHost.vc_tv_get_display_state(&current_tv_state);
  if(current_tv_state.state & ( VC_HDMI_HDMI | VC_HDMI_DVI )) {
    //HDMI or DVI on
    m_config_video.display_aspect = get_display_aspect_ratio((HDMI_ASPECT_T)current_tv_state.display.hdmi.aspect_ratio);
  } else {
    //composite on
    m_config_video.display_aspect = get_display_aspect_ratio((SDTV_ASPECT_T)current_tv_state.display.sdtv.display_options.aspect);
  }
  m_config_video.display_aspect *= (float)current_tv_state.display.hdmi.height/(float)current_tv_state.display.hdmi.width;

  if (m_orientation >= 0)
    m_config_video.hints.orientation = m_orientation;
  if(m_has_video && !m_player_video.Open(m_av_clock, m_config_video))
    goto do_exit;


  m_omx_reader.GetHints(OMXSTREAM_AUDIO, m_config_audio.hints);

  if (m_config_audio.device == "")
  {
    if (m_BcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_ePCM, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) == 0)
      m_config_audio.device = "omx:hdmi";
    else
      m_config_audio.device = "omx:local";
  }

  if(m_config_audio.device == "omx:alsa" && m_config_audio.subdevice.empty())
    m_config_audio.subdevice = "default";

  if ((m_config_audio.hints.codec == AV_CODEC_ID_AC3 || m_config_audio.hints.codec == AV_CODEC_ID_EAC3) &&
      m_BcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_eAC3, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) != 0)
    m_config_audio.passthrough = false;
  if (m_config_audio.hints.codec == AV_CODEC_ID_DTS &&
      m_BcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_eDTS, 2, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) != 0)
    m_config_audio.passthrough = false;

  if(m_has_audio && !m_player_audio.Open(m_av_clock, m_config_audio, &m_omx_reader))
    goto do_exit;

  if(m_has_audio)
  {
    m_player_audio.SetVolume(pow(10, m_Volume / 2000.0));
    if (m_Amplification)
      m_player_audio.SetDynamicRangeCompression(m_Amplification);
  }

  if (m_threshold < 0.0f)
    m_threshold = m_config_audio.is_live ? 0.7f : 0.2f;

  m_av_clock->OMXReset(m_has_video, m_has_audio);
  m_av_clock->OMXStateExecute();
  sentStarted = true;

  while(!m_stop)
  {
    if(g_abort)
      goto do_exit;

    double now = m_av_clock->GetAbsoluteClock();
    bool update = false;
    if (m_last_check_time == 0.0 || m_last_check_time + DVD_MSEC_TO_TIME(20) <= now)
    {
      update = true;
      m_last_check_time = now;
    }

    if (update) {
      OMXControlResult result = KeyConfig::ACTION_BLANK;
      double oldPos, newPos;
    }

    if (idle)
    {
      usleep(10000);
      continue;
    }

    /* player got in an error state */
    if(m_player_audio.Error())
    {
      printf("audio player error. emergency exit!!!\n");
      goto do_exit;
    }

    if (update)
    {
      /* when the video/audio fifos are low, we pause clock, when high we resume */
      double stamp = m_av_clock->OMXMediaTime();
      double audio_pts = m_player_audio.GetCurrentPTS();
      double video_pts = m_player_video.GetCurrentPTS();

      if (0 && m_av_clock->OMXIsPaused())
      {
        double old_stamp = stamp;
        if (audio_pts != DVD_NOPTS_VALUE && (stamp == 0 || audio_pts < stamp))
          stamp = audio_pts;
        if (video_pts != DVD_NOPTS_VALUE && (stamp == 0 || video_pts < stamp))
          stamp = video_pts;
        if (old_stamp != stamp)
        {
          m_av_clock->OMXMediaTime(stamp);
          stamp = m_av_clock->OMXMediaTime();
        }
      }

      float audio_fifo = audio_pts == DVD_NOPTS_VALUE ? 0.0f : audio_pts / DVD_TIME_BASE - stamp * 1e-6;
      float video_fifo = video_pts == DVD_NOPTS_VALUE ? 0.0f : video_pts / DVD_TIME_BASE - stamp * 1e-6;
      float threshold = std::min(0.1f, (float)m_player_audio.GetCacheTotal() * 0.1f);
      bool audio_fifo_low = false, video_fifo_low = false, audio_fifo_high = false, video_fifo_high = false;

      if(m_tv_show_info)
      {
        static unsigned count;
        if ((count++ & 7) == 0)
        {
          char response[80];
          if (m_player_video.GetDecoderBufferSize() && m_player_audio.GetCacheTotal())
            vc_gencmd(response, sizeof response, "render_bar 4 video_fifo %d %d %d %d",
                      (int)(100.0*m_player_video.GetDecoderBufferSize()-m_player_video.GetDecoderFreeSpace())/m_player_video.GetDecoderBufferSize(),
                      (int)(100.0*video_fifo/m_player_audio.GetCacheTotal()),
                      0, 100);
          if (m_player_audio.GetCacheTotal())
            vc_gencmd(response, sizeof response, "render_bar 5 audio_fifo %d %d %d %d",
                      (int)(100.0*audio_fifo/m_player_audio.GetCacheTotal()),
                      (int)(100.0*m_player_audio.GetDelay()/m_player_audio.GetCacheTotal()),
                      0, 100);
          vc_gencmd(response, sizeof response, "render_bar 6 video_queue %d %d %d %d",
                    m_player_video.GetLevel(), 0, 0, 100);
          vc_gencmd(response, sizeof response, "render_bar 7 audio_queue %d %d %d %d",
                    m_player_audio.GetLevel(), 0, 0, 100);
        }
      }

      if (audio_pts != DVD_NOPTS_VALUE)
      {
        audio_fifo_low = m_has_audio && audio_fifo < threshold;
        audio_fifo_high = !m_has_audio || (audio_pts != DVD_NOPTS_VALUE && audio_fifo > m_threshold);
      }
      if (video_pts != DVD_NOPTS_VALUE)
      {
        video_fifo_low = m_has_video && video_fifo < threshold;
        video_fifo_high = !m_has_video || (video_pts != DVD_NOPTS_VALUE && video_fifo > m_threshold);
      }
      CLog::Log(LOGDEBUG, "Normal M:%.0f (A:%.0f V:%.0f) P:%d A:%.2f V:%.2f/T:%.2f (%d,%d,%d,%d) A:%d%% V:%d%% (%.2f,%.2f)\n", stamp, audio_pts, video_pts, m_av_clock->OMXIsPaused(),
                audio_pts == DVD_NOPTS_VALUE ? 0.0:audio_fifo, video_pts == DVD_NOPTS_VALUE ? 0.0:video_fifo, m_threshold, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high,
                m_player_audio.GetLevel(), m_player_video.GetLevel(), m_player_audio.GetDelay(), (float)m_player_audio.GetCacheTotal());

      // keep latency under control by adjusting clock (and so resampling audio)
      if (m_config_audio.is_live)
      {
        float latency = DVD_NOPTS_VALUE;
        if (m_has_audio && audio_pts != DVD_NOPTS_VALUE)
          latency = audio_fifo;
        else if (!m_has_audio && m_has_video && video_pts != DVD_NOPTS_VALUE)
          latency = video_fifo;
        if (!m_Pause && latency != DVD_NOPTS_VALUE)
        {
          if (m_av_clock->OMXIsPaused())
          {
            if (latency > m_threshold)
            {
              CLog::Log(LOGDEBUG, "Resume %.2f,%.2f (%d,%d,%d,%d) EOF:%d PKT:%p\n", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_omx_reader.IsEof(), m_omx_pkt);
              m_av_clock->OMXResume();
              m_latency = latency;
            }
          }
          else
          {
            m_latency = m_latency*0.99f + latency*0.01f;
            float speed = 1.0f;
            if (m_latency < 0.5f*m_threshold)
              speed = 0.990f;
            else if (m_latency < 0.9f*m_threshold)
              speed = 0.999f;
            else if (m_latency > 2.0f*m_threshold)
              speed = 1.010f;
            else if (m_latency > 1.1f*m_threshold)
              speed = 1.001f;

            m_av_clock->OMXSetSpeed(S(speed));
            m_av_clock->OMXSetSpeed(S(speed), true, true);
            CLog::Log(LOGDEBUG, "Live: %.2f (%.2f) S:%.3f T:%.2f\n", m_latency, latency, speed, m_threshold);
          }
        }
      }
      else if(!m_Pause && (m_omx_reader.IsEof() || m_omx_pkt || (audio_fifo_high && video_fifo_high)))
      {
        if (m_av_clock->OMXIsPaused())
        {
          CLog::Log(LOGDEBUG, "Resume %.2f,%.2f (%d,%d,%d,%d) EOF:%d PKT:%p\n", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_omx_reader.IsEof(), m_omx_pkt);
          m_av_clock->OMXResume();
        }
      }
      else if (m_Pause || audio_fifo_low || video_fifo_low)
      {
        if (!m_av_clock->OMXIsPaused())
        {
          if (!m_Pause)
            m_threshold = std::min(2.0f*m_threshold, 16.0f);
          CLog::Log(LOGDEBUG, "Pause %.2f,%.2f (%d,%d,%d,%d) %.2f\n", audio_fifo, video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_threshold);
          m_av_clock->OMXPause();
        }
      }
    }
    if (!sentStarted)
    {
      CLog::Log(LOGDEBUG, "COMXPlayer::HandleMessages - player started RESET");
      m_av_clock->OMXReset(m_has_video, m_has_audio);
      sentStarted = true;
    }

    if(!m_omx_pkt)
      m_omx_pkt = m_omx_reader.Read();

    if(m_omx_pkt)
      m_send_eos = false;

    if(m_omx_reader.IsEof() && !m_omx_pkt)
    {
      if (!m_send_eos && m_has_video)
        m_player_video.SubmitEOS();
      if (!m_send_eos && m_has_audio)
        m_player_audio.SubmitEOS();
      m_send_eos = true;
      if ( (m_has_video && !m_player_video.IsEOS()) ||
           (m_has_audio && !m_player_audio.IsEOS()) )
      {
        OMXClock::OMXSleep(10);
        continue;
      }
      break;
    }

    if(m_has_video && m_omx_pkt && m_omx_reader.IsActive(OMXSTREAM_VIDEO, m_omx_pkt->stream_index))
    {
      if(m_player_video.AddPacket(m_omx_pkt))
        m_omx_pkt = NULL;
      else
        OMXClock::OMXSleep(10);
    }
    else if(m_has_audio && m_omx_pkt && m_omx_pkt->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      if(m_player_audio.AddPacket(m_omx_pkt))
        m_omx_pkt = NULL;
      else
        OMXClock::OMXSleep(10);
    }
    else if(m_omx_pkt) &&
            m_omx_pkt->codec_type == AVMEDIA_TYPE_SUBTITLE)
    {
      OMXClock::OMXSleep(10);
    }
    else
    {
      if(m_omx_pkt)
      {
        m_omx_reader.FreePacket(m_omx_pkt);
        m_omx_pkt = NULL;
      }
      else
        OMXClock::OMXSleep(10);
    }
  }

do_exit:
  if (m_stop)
  {
    unsigned t = (unsigned)(m_av_clock->OMXMediaTime()*1e-6);
    printf("Stopped at: %02d:%02d:%02d\n", (t/3600), (t/60)%60, t%60);
  }

  m_av_clock->OMXStop();
  m_av_clock->OMXStateIdle();

  m_player_video.Close();
  m_player_audio.Close();


  if(m_omx_pkt)
  {
    m_omx_reader.FreePacket(m_omx_pkt);
    m_omx_pkt = NULL;
  }

  m_omx_reader.Close();

  m_av_clock->OMXDeinitialize();
  if (m_av_clock)
    delete m_av_clock;

  vc_tv_show_info(0);

  g_OMX.Deinitialize();
  g_RBP.Deinitialize();

  printf("have a nice day ;)\n");

  // exit status success on playback end
  if (m_send_eos)
    return EXIT_SUCCESS;
  // exit status OMXPlayer defined value on user quit
  if (m_stop)
    return 3;
  // exit status failure on other cases
  return EXIT_FAILURE;
}

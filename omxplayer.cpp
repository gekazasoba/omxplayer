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
#include <stdlib.h>
#include <stdint.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <string.h>

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
#include "Consts.hh"
#include "Tools.hh"

#include <string>
#include <utility>
#include <thread>
#include <memory>
#include <chrono>

#include "version.h"

class Player {
    long m_Volume = 0;
    long m_Amplification = 0;
    bool m_Pause = false;
    OMXReader m_omx_reader;
    OMXClock *m_av_clock = NULL;
    OMXAudioConfig m_config_audio;
    OMXVideoConfig m_config_video;
    OMXPacket *m_omx_pkt = NULL;
    bool m_stop = false;
    DllBcmHost m_BcmHost;
    OMXPlayerVideo m_player_video;
    OMXPlayerAudio m_player_audio;
    bool m_has_video = false;
    bool m_has_audio = false;

    bool m_send_eos = false;
    std::string m_filename;
    CRBP g_RBP;
    COMXCore g_OMX;
    uint32_t m_blank_background = 0xff000000;
    float m_threshold = -1.0f; // amount of audio/video required to come out of buffering
    float m_timeout = 10.0f; // amount of time file/network operation can stall for before timing out


    double m_last_check_time = 0.0;
    float m_latency = 0.0f;

    bool m_audio_extension = false;


    float get_display_aspect_ratio(HDMI_ASPECT_T aspect) {
        float display_aspect;
        switch (aspect) {
            case HDMI_ASPECT_4_3:
                display_aspect = 4.0 / 3.0;
                break;
            case HDMI_ASPECT_14_9:
                display_aspect = 14.0 / 9.0;
                break;
            case HDMI_ASPECT_16_9:
                display_aspect = 16.0 / 9.0;
                break;
            case HDMI_ASPECT_5_4:
                display_aspect = 5.0 / 4.0;
                break;
            case HDMI_ASPECT_16_10:
                display_aspect = 16.0 / 10.0;
                break;
            case HDMI_ASPECT_15_9:
                display_aspect = 15.0 / 9.0;
                break;
            case HDMI_ASPECT_64_27:
                display_aspect = 64.0 / 27.0;
                break;
            default:
                display_aspect = 16.0 / 9.0;
                break;
        }
        return display_aspect;
    }

    float get_display_aspect_ratio(SDTV_ASPECT_T aspect) {
        float display_aspect;
        switch (aspect) {
            case SDTV_ASPECT_4_3:
                display_aspect = 4.0 / 3.0;
                break;
            case SDTV_ASPECT_14_9:
                display_aspect = 14.0 / 9.0;
                break;
            case SDTV_ASPECT_16_9:
                display_aspect = 16.0 / 9.0;
                break;
            default:
                display_aspect = 4.0 / 3.0;
                break;
        }
        return display_aspect;
    }

    void FlushStreams(double pts) {
        m_av_clock->OMXStop();
        m_av_clock->OMXPause();

        if (m_has_video)
            m_player_video.Flush();

        if (m_has_audio)
            m_player_audio.Flush();

        if (pts != DVD_NOPTS_VALUE)
            m_av_clock->OMXMediaTime(pts);

        if (m_omx_pkt) {
            m_omx_reader.FreePacket(m_omx_pkt);
            m_omx_pkt = NULL;
        }
    }


    void blank_background(uint32_t rgba) {
        // if alpha is fully transparent then background has no effect
        if (!(rgba & 0xff000000))
            return;
        // we create a 1x1 black pixel image that is added to display just behind video
        DISPMANX_DISPLAY_HANDLE_T display;
        DISPMANX_UPDATE_HANDLE_T update;
        DISPMANX_RESOURCE_HANDLE_T resource;
        DISPMANX_ELEMENT_HANDLE_T element;
        int ret;
        uint32_t vc_image_ptr;
        VC_IMAGE_TYPE_T type = VC_IMAGE_ARGB8888;
        int layer = m_config_video.layer - 1;

        VC_RECT_T dst_rect, src_rect;

        display = vc_dispmanx_display_open(m_config_video.display);
        assert(display);

        resource = vc_dispmanx_resource_create(type, 1 /*width*/, 1 /*height*/, &vc_image_ptr);
        assert(resource);

        vc_dispmanx_rect_set(&dst_rect, 0, 0, 1, 1);

        ret = vc_dispmanx_resource_write_data(resource, type, sizeof(rgba), &rgba, &dst_rect);
        assert(ret == 0);

        vc_dispmanx_rect_set(&src_rect, 0, 0, 1 << 16, 1 << 16);
        vc_dispmanx_rect_set(&dst_rect, 0, 0, 0, 0);

        update = vc_dispmanx_update_start(0);
        assert(update);

        element = vc_dispmanx_element_add(update, display, layer, &dst_rect, resource, &src_rect,
                                          DISPMANX_PROTECTION_NONE, NULL, NULL, DISPMANX_STEREOSCOPIC_MONO);
        assert(element);

        ret = vc_dispmanx_update_submit_sync(update);
        assert(ret == 0);
    }

public:
    Player(std::string fileName) : m_filename(fileName) {

    }

    void Pause(bool pause) {
        m_Pause = pause;
    }

    bool Pause(){
        return m_Pause;
    }

    void Stop(){
        m_stop = true;
        thread->join();
    }
public:
    void Cleanup() {
        m_av_clock->OMXStop();
        m_av_clock->OMXStateIdle();

        m_player_video.Close();
        m_player_audio.Close();


        if (m_omx_pkt) {
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
    }

    bool Init() {

        m_config_video.dst_rect.x1 = 100;
        m_config_video.dst_rect.y1 = 100;
        m_config_video.dst_rect.x2 = 300;
        m_config_video.dst_rect.y2 = 300;

        if (m_filename.find_last_of(".") != string::npos) {
            CStdString extension = m_filename.substr(m_filename.find_last_of("."));
            if (!extension.IsEmpty() && m_musicExtensions.Find(extension.ToLower()) != -1)
                m_audio_extension = true;
        }
        CLog::SetLogLevel(LOG_LEVEL_NONE);

        g_RBP.Initialize();
        g_OMX.Initialize();

        blank_background(m_blank_background);

        int gpu_mem = get_mem_gpu();
        int min_gpu_mem = 64;
        if (gpu_mem > 0 && gpu_mem < min_gpu_mem)
            printf("Only %dM of gpu_mem is configured. Try running \"sudo raspi-config\" and ensure that \"memory_split\" has a value of %d or greater\n",
                   gpu_mem, min_gpu_mem);

        m_av_clock = new OMXClock();
        if (!m_omx_reader.Open(m_filename.c_str(), false, m_config_audio.is_live, m_timeout))
            return false;

        m_has_video = m_omx_reader.VideoStreamCount();
        m_has_audio = m_omx_reader.AudioStreamCount();

        if (m_audio_extension) {
            CLog::Log(LOGWARNING, "%s - Ignoring video in audio filetype:%s", __FUNCTION__, m_filename.c_str());
            m_has_video = false;
        }

        // you really don't want want to match refresh rate without hdmi clock sync

        if (!m_av_clock->OMXInitialize())
            return false;

        if (m_config_video.hdmi_clock_sync && !m_av_clock->HDMIClockSync())
            return false;

        m_av_clock->OMXStateIdle();
        m_av_clock->OMXStop();
        m_av_clock->OMXPause();

        m_omx_reader.GetHints(OMXSTREAM_AUDIO, m_config_audio.hints);
        m_omx_reader.GetHints(OMXSTREAM_VIDEO, m_config_video.hints);

        // get display aspect
        TV_DISPLAY_STATE_T current_tv_state;
        memset(&current_tv_state, 0, sizeof(TV_DISPLAY_STATE_T));
        m_BcmHost.vc_tv_get_display_state(&current_tv_state);
        if (current_tv_state.state & (VC_HDMI_HDMI | VC_HDMI_DVI)) {
            //HDMI or DVI on
            m_config_video.display_aspect = get_display_aspect_ratio(
                    (HDMI_ASPECT_T) current_tv_state.display.hdmi.aspect_ratio);
        } else {
            //composite on
            m_config_video.display_aspect = get_display_aspect_ratio(
                    (SDTV_ASPECT_T) current_tv_state.display.sdtv.display_options.aspect);
        }
        m_config_video.display_aspect *=
                (float) current_tv_state.display.hdmi.height / (float) current_tv_state.display.hdmi.width;

        if (m_has_video && !m_player_video.Open(m_av_clock, m_config_video))
            return false;


        m_omx_reader.GetHints(OMXSTREAM_AUDIO, m_config_audio.hints);
        m_config_audio.device = "omx:both";

        if ((m_config_audio.hints.codec == AV_CODEC_ID_AC3 || m_config_audio.hints.codec == AV_CODEC_ID_EAC3) &&
            m_BcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_eAC3, 2, EDID_AudioSampleRate_e44KHz,
                                                 EDID_AudioSampleSize_16bit) != 0)
            m_config_audio.passthrough = false;
        if (m_config_audio.hints.codec == AV_CODEC_ID_DTS &&
            m_BcmHost.vc_tv_hdmi_audio_supported(EDID_AudioFormat_eDTS, 2, EDID_AudioSampleRate_e44KHz,
                                                 EDID_AudioSampleSize_16bit) != 0)
            m_config_audio.passthrough = false;

        if (m_has_audio && !m_player_audio.Open(m_av_clock, m_config_audio, &m_omx_reader))
            return false;

        if (m_has_audio) {
            m_player_audio.SetVolume(pow(10, m_Volume / 2000.0));
            if (m_Amplification)
                m_player_audio.SetDynamicRangeCompression(m_Amplification);
        }

        if (m_threshold < 0.0f)
            m_threshold = m_config_audio.is_live ? 0.7f : 0.2f;

        m_av_clock->OMXReset(m_has_video, m_has_audio);
        m_av_clock->OMXStateExecute();
        return true;
    }

    void SetSize(){
        m_config_video.dst_rect.x1 = 100;
        m_config_video.dst_rect.y1 = 100;
        m_config_video.dst_rect.x2 = 300;
        m_config_video.dst_rect.y2 = 300;
        m_player_video.SetVideoRect(m_config_video.src_rect, m_config_video.dst_rect);
    }

    bool Spin() {
        while (!m_stop) {
            double now = m_av_clock->GetAbsoluteClock();
            bool update = false;
            if (m_last_check_time == 0.0 || m_last_check_time + DVD_MSEC_TO_TIME(20) <= now) {
                update = true;
                m_last_check_time = now;
            }

            /* player got in an error state */
            if (m_player_audio.Error()) {
                printf("audio player error. emergency exit!!!\n");
                return false;
            }

            if (update) {
                /* when the video/audio fifos are low, we pause clock, when high we resume */
                double stamp = m_av_clock->OMXMediaTime();
                double audio_pts = m_player_audio.GetCurrentPTS();
                double video_pts = m_player_video.GetCurrentPTS();

                float audio_fifo = audio_pts == DVD_NOPTS_VALUE ? 0.0f : audio_pts / DVD_TIME_BASE - stamp * 1e-6;
                float video_fifo = video_pts == DVD_NOPTS_VALUE ? 0.0f : video_pts / DVD_TIME_BASE - stamp * 1e-6;
                float threshold = std::min(0.1f, (float) m_player_audio.GetCacheTotal() * 0.1f);
                bool audio_fifo_low = false, video_fifo_low = false, audio_fifo_high = false, video_fifo_high = false;

                if (audio_pts != DVD_NOPTS_VALUE) {
                    audio_fifo_low = m_has_audio && audio_fifo < threshold;
                    audio_fifo_high = !m_has_audio || (audio_pts != DVD_NOPTS_VALUE && audio_fifo > m_threshold);
                }
                if (video_pts != DVD_NOPTS_VALUE) {
                    video_fifo_low = m_has_video && video_fifo < threshold;
                    video_fifo_high = !m_has_video || (video_pts != DVD_NOPTS_VALUE && video_fifo > m_threshold);
                }
                CLog::Log(LOGDEBUG,
                          "Normal M:%.0f (A:%.0f V:%.0f) P:%d A:%.2f V:%.2f/T:%.2f (%d,%d,%d,%d) A:%d%% V:%d%% (%.2f,%.2f)\n",
                          stamp, audio_pts, video_pts, m_av_clock->OMXIsPaused(),
                          audio_pts == DVD_NOPTS_VALUE ? 0.0 : audio_fifo,
                          video_pts == DVD_NOPTS_VALUE ? 0.0 : video_fifo, m_threshold, audio_fifo_low, video_fifo_low,
                          audio_fifo_high, video_fifo_high,
                          m_player_audio.GetLevel(), m_player_video.GetLevel(), m_player_audio.GetDelay(),
                          (float) m_player_audio.GetCacheTotal());

                // keep latency under control by adjusting clock (and so resampling audio)
                if (m_config_audio.is_live) {
                    float latency = DVD_NOPTS_VALUE;
                    if (m_has_audio && audio_pts != DVD_NOPTS_VALUE)
                        latency = audio_fifo;
                    else if (!m_has_audio && m_has_video && video_pts != DVD_NOPTS_VALUE)
                        latency = video_fifo;
                    if (!m_Pause && latency != DVD_NOPTS_VALUE) {
                        if (m_av_clock->OMXIsPaused()) {
                            if (latency > m_threshold) {
                                CLog::Log(LOGDEBUG, "Resume %.2f,%.2f (%d,%d,%d,%d) EOF:%d PKT:%p\n", audio_fifo,
                                          video_fifo, audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high,
                                          m_omx_reader.IsEof(), m_omx_pkt);
                                m_av_clock->OMXResume();
                                m_latency = latency;
                            }
                        } else {
                            m_latency = m_latency * 0.99f + latency * 0.01f;
                            float speed = 1.0f;
                            if (m_latency < 0.5f * m_threshold)
                                speed = 0.990f;
                            else if (m_latency < 0.9f * m_threshold)
                                speed = 0.999f;
                            else if (m_latency > 2.0f * m_threshold)
                                speed = 1.010f;
                            else if (m_latency > 1.1f * m_threshold)
                                speed = 1.001f;

                            m_av_clock->OMXSetSpeed(S(speed));
                            m_av_clock->OMXSetSpeed(S(speed), true, true);
                            CLog::Log(LOGDEBUG, "Live: %.2f (%.2f) S:%.3f T:%.2f\n", m_latency, latency, speed,
                                      m_threshold);
                        }
                    }
                } else if (!m_Pause && (m_omx_reader.IsEof() || m_omx_pkt || (audio_fifo_high && video_fifo_high))) {
                    if (m_av_clock->OMXIsPaused()) {
                        CLog::Log(LOGDEBUG, "Resume %.2f,%.2f (%d,%d,%d,%d) EOF:%d PKT:%p\n", audio_fifo, video_fifo,
                                  audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high,
                                  m_omx_reader.IsEof(), m_omx_pkt);
                        m_av_clock->OMXResume();
                    }
                } else if (m_Pause || audio_fifo_low || video_fifo_low) {
                    if (!m_av_clock->OMXIsPaused()) {
                        if (!m_Pause)
                            m_threshold = std::min(2.0f * m_threshold, 16.0f);
                        CLog::Log(LOGDEBUG, "Pause %.2f,%.2f (%d,%d,%d,%d) %.2f\n", audio_fifo, video_fifo,
                                  audio_fifo_low, video_fifo_low, audio_fifo_high, video_fifo_high, m_threshold);
                        m_av_clock->OMXPause();
                    }
                }
            }

            if (!m_omx_pkt)
                m_omx_pkt = m_omx_reader.Read();

            if (m_omx_pkt)
                m_send_eos = false;

            if (m_omx_reader.IsEof() && !m_omx_pkt) {
                if (!m_send_eos && m_has_video)
                    m_player_video.SubmitEOS();
                if (!m_send_eos && m_has_audio)
                    m_player_audio.SubmitEOS();
                m_send_eos = true;
                if ((m_has_video && !m_player_video.IsEOS()) ||
                    (m_has_audio && !m_player_audio.IsEOS())) {
                    OMXClock::OMXSleep(10);
                    continue;
                }
                break;
            }

            if (m_has_video && m_omx_pkt && m_omx_reader.IsActive(OMXSTREAM_VIDEO, m_omx_pkt->stream_index)) {
                if (m_player_video.AddPacket(m_omx_pkt))
                    m_omx_pkt = NULL;
                else
                    OMXClock::OMXSleep(10);
            } else if (m_has_audio && m_omx_pkt && m_omx_pkt->codec_type == AVMEDIA_TYPE_AUDIO) {
                if (m_player_audio.AddPacket(m_omx_pkt))
                    m_omx_pkt = NULL;
                else
                    OMXClock::OMXSleep(10);
            } else if (m_omx_pkt && m_omx_pkt->codec_type == AVMEDIA_TYPE_SUBTITLE) {
                OMXClock::OMXSleep(10);
            } else {
                if (m_omx_pkt) {
                    m_omx_reader.FreePacket(m_omx_pkt);
                    m_omx_pkt = NULL;
                } else
                    OMXClock::OMXSleep(10);
            }
        }

        return true;
    }

    std::shared_ptr<std::thread> thread;
    void SpinAsync(){
        thread = std::shared_ptr<std::thread>(new std::thread(&Player::Spin, this));
    }
};


int main(int argc, char *argv[]) {
    Player player1("/home/pi/omi.mp4");
    //Player player2("/home/pi/shakira.mp4");

    if (!player1.Init()) {
        printf("player 1 init failed\n");
        return EXIT_FAILURE;
    }

    /*if (!player2.Init()) {
        printf("player 2 init failed\n");
        return EXIT_FAILURE;
    }*/

    player1.Pause(true);
    printf("player 1 playing async (paused)\n");
    player1.SpinAsync();

/*    player2.Pause(true);
    printf("player 2 playing async (paused)\n");
    player2.SpinAsync();*/



    printf("p1 unpausing\n");
    player1.Pause(false);
    printf("p1 for 10 sec\n");
    std::this_thread::sleep_for(std::chrono::seconds(10));


    printf("p1 stopping\n");
    player1.Stop();

    /*printf("p2 unpausing\n");
    player2.Pause(false);*/

    printf("player 1 clean up\n");
    player1.Cleanup();

    /*printf("p2 for 10 sec\n");
    std::this_thread::sleep_for(std::chrono::seconds(10));

    printf("p2 stopping\n");
    player2.Stop();

    player2.Cleanup();*/

    printf("have a nice day ;)\n");

    return EXIT_SUCCESS;
}

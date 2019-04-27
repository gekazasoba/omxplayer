//
// Created by geka on 27.04.19.
//



/*

struct option longopts[] = {
        { "info",         no_argument,        NULL,          'i' },
        { "with-info",    no_argument,        NULL,          'I' },
        { "help",         no_argument,        NULL,          'h' },
        { "version",      no_argument,        NULL,          'v' },
        { "keys",         no_argument,        NULL,          'k' },
        { "aidx",         required_argument,  NULL,          'n' },
        { "adev",         required_argument,  NULL,          'o' },
        { "stats",        no_argument,        NULL,          's' },
        { "passthrough",  no_argument,        NULL,          'p' },
        { "vol",          required_argument,  NULL,          vol_opt },
        { "amp",          required_argument,  NULL,          amp_opt },
        { "deinterlace",  no_argument,        NULL,          'd' },
        { "nodeinterlace",no_argument,        NULL,          no_deinterlace_opt },
        { "nativedeinterlace",no_argument,    NULL,          native_deinterlace_opt },
        { "anaglyph",     required_argument,  NULL,          anaglyph_opt },
        { "advanced",     optional_argument,  NULL,          advanced_opt },
        { "hw",           no_argument,        NULL,          'w' },
        { "3d",           required_argument,  NULL,          '3' },
        { "allow-mvc",    no_argument,        NULL,          'M' },
        { "hdmiclocksync", no_argument,       NULL,          'y' },
        { "nohdmiclocksync", no_argument,     NULL,          'z' },
        { "refresh",      no_argument,        NULL,          'r' },
        { "genlog",       no_argument,        NULL,          'g' },
        { "sid",          required_argument,  NULL,          't' },
        { "pos",          required_argument,  NULL,          'l' },
        { "blank",        optional_argument,  NULL,          'b' },
        { "font",         required_argument,  NULL,          font_opt },
        { "italic-font",  required_argument,  NULL,          italic_font_opt },
        { "font-size",    required_argument,  NULL,          font_size_opt },
        { "align",        required_argument,  NULL,          align_opt },
        { "no-ghost-box", no_argument,        NULL,          no_ghost_box_opt },
        { "subtitles",    required_argument,  NULL,          subtitles_opt },
        { "lines",        required_argument,  NULL,          lines_opt },
        { "win",          required_argument,  NULL,          pos_opt },
        { "crop",         required_argument,  NULL,          crop_opt },
        { "aspect-mode",  required_argument,  NULL,          aspect_mode_opt },
        { "audio_fifo",   required_argument,  NULL,          audio_fifo_opt },
        { "video_fifo",   required_argument,  NULL,          video_fifo_opt },
        { "audio_queue",  required_argument,  NULL,          audio_queue_opt },
        { "video_queue",  required_argument,  NULL,          video_queue_opt },
        { "threshold",    required_argument,  NULL,          threshold_opt },
        { "timeout",      required_argument,  NULL,          timeout_opt },
        { "boost-on-downmix", no_argument,    NULL,          boost_on_downmix_opt },
        { "no-boost-on-downmix", no_argument, NULL,          no_boost_on_downmix_opt },
        { "key-config",   required_argument,  NULL,          key_config_opt },
        { "no-osd",       no_argument,        NULL,          no_osd_opt },
        { "no-keys",      no_argument,        NULL,          no_keys_opt },
        { "orientation",  required_argument,  NULL,          orientation_opt },
        { "fps",          required_argument,  NULL,          fps_opt },
        { "live",         no_argument,        NULL,          live_opt },
        { "layout",       required_argument,  NULL,          layout_opt },
        { "dbus_name",    required_argument,  NULL,          dbus_name_opt },
        { "loop",         no_argument,        NULL,          loop_opt },
        { "layer",        required_argument,  NULL,          layer_opt },
        { "alpha",        required_argument,  NULL,          alpha_opt },
        { "display",      required_argument,  NULL,          display_opt },
        { "cookie",       required_argument,  NULL,          http_cookie_opt },
        { "user-agent",   required_argument,  NULL,          http_user_agent_opt },
        { "lavfdopts",    required_argument,  NULL,          lavfdopts_opt },
        { "avdict",       required_argument,  NULL,          avdict_opt },
        { 0, 0, 0, 0 }
};

*/

/*while ((c = getopt_long(argc, argv, "wiIhvkn:l:o:cslb::pd3:Myzt:rg", longopts, NULL)) != -1)
  {
    switch (c)
    {
      case 'r':
        m_refresh = true;
            break;
      case 'g':
        m_gen_log = true;
            break;
      case 'y':
        m_config_video.hdmi_clock_sync = true;
            break;
      case 'z':
        m_no_hdmi_clock_sync = true;
            break;
      case '3':
        mode = optarg;
            if(mode != "SBS" && mode != "TB" && mode != "FP")
            {
              print_usage();
              return 0;
            }
            if(mode == "TB")
              m_3d = CONF_FLAGS_FORMAT_TB;
            else if(mode == "FP")
              m_3d = CONF_FLAGS_FORMAT_FP;
            else
              m_3d = CONF_FLAGS_FORMAT_SBS;
            m_config_video.allow_mvc = true;
            break;
      case 'M':
        m_config_video.allow_mvc = true;
            break;
      case 'd':
        m_config_video.deinterlace = VS_DEINTERLACEMODE_FORCE;
            break;
      case no_deinterlace_opt:
        m_config_video.deinterlace = VS_DEINTERLACEMODE_OFF;
            break;
      case native_deinterlace_opt:
        m_config_video.deinterlace = VS_DEINTERLACEMODE_OFF;
            m_NativeDeinterlace = true;
            break;
      case anaglyph_opt:
        m_config_video.anaglyph = (OMX_IMAGEFILTERANAGLYPHTYPE)atoi(optarg);
            break;
      case advanced_opt:
        m_config_video.advanced_hd_deinterlace = optarg ? (atoi(optarg) ? true : false): true;
            break;
      case 'w':
        m_config_audio.hwdecode = true;
            break;
      case 'p':
        m_config_audio.passthrough = true;
            break;
      case 's':
        m_stats = true;
            break;
      case 'o':
      {
        CStdString str = optarg;
        int colon = str.Find(':');
        if(colon >= 0)
        {
          m_config_audio.device = str.Mid(0, colon);
          m_config_audio.subdevice = str.Mid(colon + 1, str.GetLength() - colon);
        }
        else
        {
          m_config_audio.device = str;
          m_config_audio.subdevice = "";
        }
      }
            if(m_config_audio.device != "local" && m_config_audio.device != "hdmi" && m_config_audio.device != "both" &&
               m_config_audio.device != "alsa")
            {
              printf("Bad argument for -%c: Output device must be `local', `hdmi', `both' or `alsa'\n", c);
              return EXIT_FAILURE;
            }
            m_config_audio.device = "omx:" + m_config_audio.device;
            break;
      case 'i':
        m_dump_format      = true;
            m_dump_format_exit = true;
            break;
      case 'I':
        m_dump_format = true;
            break;
      case 't':
        m_subtitle_index = atoi(optarg) - 1;
            if(m_subtitle_index < 0)
              m_subtitle_index = 0;
            break;
      case 'n':
        m_audio_index_use = atoi(optarg);
            break;
      case 'l':
      {
        if(strchr(optarg, ':'))
        {
          unsigned int h, m, s;
          if(sscanf(optarg, "%u:%u:%u", &h, &m, &s) == 3)
            m_incr = h*3600 + m*60 + s;
        }
        else
        {
          m_incr = atof(optarg);
        }
        if(m_loop)
          m_loop_from = m_incr;
      }
            break;
      case no_osd_opt:
        m_osd = false;
            break;
      case no_keys_opt:
        m_no_keys = true;
            break;
      case font_opt:
        m_font_path = optarg;
            m_asked_for_font = true;
            break;
      case italic_font_opt:
        m_italic_font_path = optarg;
            m_asked_for_italic_font = true;
            break;
      case font_size_opt:
      {
        const int thousands = atoi(optarg);
        if (thousands > 0)
          m_font_size = thousands*0.001f;
      }
            break;
      case align_opt:
        m_centered = !strcmp(optarg, "center");
            break;
      case no_ghost_box_opt:
        m_ghost_box = false;
            break;
      case subtitles_opt:
        m_external_subtitles_path = optarg;
            m_has_external_subtitles = true;
            break;
      case lines_opt:
        m_subtitle_lines = std::max(atoi(optarg), 1);
            break;
      case pos_opt:
        sscanf(optarg, "%f %f %f %f", &m_config_video.dst_rect.x1, &m_config_video.dst_rect.y1, &m_config_video.dst_rect.x2, &m_config_video.dst_rect.y2) == 4 ||
        sscanf(optarg, "%f,%f,%f,%f", &m_config_video.dst_rect.x1, &m_config_video.dst_rect.y1, &m_config_video.dst_rect.x2, &m_config_video.dst_rect.y2);
            break;
      case crop_opt:
        sscanf(optarg, "%f %f %f %f", &m_config_video.src_rect.x1, &m_config_video.src_rect.y1, &m_config_video.src_rect.x2, &m_config_video.src_rect.y2) == 4 ||
        sscanf(optarg, "%f,%f,%f,%f", &m_config_video.src_rect.x1, &m_config_video.src_rect.y1, &m_config_video.src_rect.x2, &m_config_video.src_rect.y2);
            break;
      case aspect_mode_opt:
        if (optarg) {
          if (!strcasecmp(optarg, "letterbox"))
            m_config_video.aspectMode = 1;
          else if (!strcasecmp(optarg, "fill"))
            m_config_video.aspectMode = 2;
          else if (!strcasecmp(optarg, "stretch"))
            m_config_video.aspectMode = 3;
          else
            m_config_video.aspectMode = 0;
        }
            break;
      case vol_opt:
        m_Volume = atoi(optarg);
            break;
      case amp_opt:
        m_Amplification = atoi(optarg);
            break;
      case boost_on_downmix_opt:
        m_config_audio.boostOnDownmix = true;
            break;
      case no_boost_on_downmix_opt:
        m_config_audio.boostOnDownmix = false;
            break;
      case audio_fifo_opt:
        m_config_audio.fifo_size = atof(optarg);
            break;
      case video_fifo_opt:
        m_config_video.fifo_size = atof(optarg);
            break;
      case audio_queue_opt:
        m_config_audio.queue_size = atof(optarg);
            break;
      case video_queue_opt:
        m_config_video.queue_size = atof(optarg);
            break;
      case threshold_opt:
        m_threshold = atof(optarg);
            break;
      case timeout_opt:
        m_timeout = atof(optarg);
            break;
      case orientation_opt:
        m_orientation = atoi(optarg);
            break;
      case fps_opt:
        m_fps = atof(optarg);
            break;
      case live_opt:
        m_config_audio.is_live = true;
            break;
      case layout_opt:
      {
        const char *layouts[] = {"2.0", "2.1", "3.0", "3.1", "4.0", "4.1", "5.0", "5.1", "7.0", "7.1"};
        unsigned i;
        for (i=0; i<sizeof layouts/sizeof *layouts; i++)
          if (strcmp(optarg, layouts[i]) == 0)
          {
            m_config_audio.layout = (enum PCMLayout)i;
            break;
          }
        if (i == sizeof layouts/sizeof *layouts)
        {
          printf("Wrong layout specified: %s\n", optarg);
          print_usage();
          return EXIT_FAILURE;
        }
        break;
      }
      case dbus_name_opt:
        m_dbus_name = optarg;
            break;
      case loop_opt:
        if(m_incr != 0)
          m_loop_from = m_incr;
            m_loop = true;
            break;
      case 'b':
        m_blank_background = optarg ? strtoul(optarg, NULL, 0) : 0xff000000;
            break;
      case key_config_opt:
        keymap = KeyConfig::parseConfigFile(optarg);
            break;
      case layer_opt:
        m_config_video.layer = atoi(optarg);
            break;
      case alpha_opt:
        m_config_video.alpha = atoi(optarg);
            break;
      case display_opt:
        m_config_video.display = atoi(optarg);
            break;
      case http_cookie_opt:
        m_cookie = optarg;
            break;
      case http_user_agent_opt:
        m_user_agent = optarg;
            break;
      case lavfdopts_opt:
        m_lavfdopts = optarg;
            break;
      case avdict_opt:
        m_avdict = optarg;
            break;
      case 0:
        break;
      case 'h':
        print_usage();
            return EXIT_SUCCESS;
            break;
      case 'v':
        print_version();
            return EXIT_SUCCESS;
            break;
      case 'k':
        print_keybindings();
            return EXIT_SUCCESS;
            break;
      case ':':
        return EXIT_FAILURE;
            break;
      default:
        return EXIT_FAILURE;
            break;
    }
  }

  if (optind >= argc) {
    print_usage();
    return 0;
  }*/
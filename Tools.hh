//
// Created by geka on 27.04.19.
//

#ifndef OMXPLAYER_TOOLS_HH
#define OMXPLAYER_TOOLS_HH


static int get_mem_gpu(void)
{
    char response[80] = "";
    int gpu_mem = 0;
    if (vc_gencmd(response, sizeof response, "get_mem gpu") == 0)
        vc_gencmd_number_property(response, "gpu", &gpu_mem);
    return gpu_mem;
}

#endif //OMXPLAYER_TOOLS_HH

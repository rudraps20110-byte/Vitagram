#include "api_client.hpp"
#include "net_client.hpp"
#include "texture_cache.hpp"
#include "ui.hpp"

#include <cstring>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/touch.h>
#include <vita2d.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    vita2d_init();
    vita2d_set_clear_color(RGBA8(14, 15, 21, 255));
    vita2d_pgf* font = vita2d_load_default_pgf();

    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    vitagram::NetClient net;
    net.init();

    vitagram::ApiClient api(net);
    vitagram::TextureCache textures(24);
    vitagram::AppUI ui(api, textures, font);
    ui.init();

    SceCtrlData pad;
    std::memset(&pad, 0, sizeof(pad));
    uint32_t prev_buttons = 0;

    SceTouchData touch;
    std::memset(&touch, 0, sizeof(touch));
    bool prev_touching = false;

    while (true) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        uint32_t pressed = pad.buttons & ~prev_buttons;
        prev_buttons = pad.buttons;

        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
        bool touching = touch.reportNum > 0;
        bool touch_just_pressed = touching && !prev_touching;
        prev_touching = touching;

        if (!ui.update(pressed, touch, touch_just_pressed)) {
            break;
        }
        ui.draw();
    }

    textures.clear();
    net.term();
    if (font) {
        vita2d_free_pgf(font);
    }
    vita2d_fini();
    sceKernelExitProcess(0);
    return 0;
}


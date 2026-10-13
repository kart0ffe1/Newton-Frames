#include "FirmwareApi.h"

extern "C" {
float* web_out_l(void);
float* web_out_r(void);
void web_render(uint32_t n);
uint8_t* web_nor(void);
uint32_t web_nor_size(void);
void web_nor_erase(void);
uint32_t web_flash_writes_count(void);
void web_seed(uint32_t s);
void web_boot(void);
void web_buttons(uint32_t labels);
void web_keys(uint32_t mask);
void web_enc(uint32_t role, int32_t n);
void web_master(uint32_t v);
uint32_t web_midi(uint32_t status, uint32_t d1, uint32_t d2);
uint16_t* web_screen(void);
uint32_t web_lit_keys(void);
uint32_t web_mid_keys(void);
uint32_t web_sample_rate(void);
}

static const FM1BackendApiV1 api = {
    FM1_BACKEND_ABI_VERSION,
    "jiant",
    "JIANT",
    web_out_l,
    web_out_r,
    web_render,
    web_nor,
    web_nor_size,
    web_nor_erase,
    web_flash_writes_count,
    web_seed,
    web_boot,
    web_buttons,
    web_keys,
    web_enc,
    web_master,
    web_midi,
    web_screen,
    web_lit_keys,
    web_mid_keys,
    web_sample_rate
};

#if defined(_WIN32)
#define FM1_EXPORT extern "C" __declspec(dllexport)
#else
#define FM1_EXPORT extern "C" __attribute__((visibility("default")))
#endif

FM1_EXPORT const FM1BackendApiV1* fm1_backend_get_api(void)
{
    return &api;
}

#pragma once
#include <stdint.h>

#define FM1_BACKEND_ABI_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FM1BackendApiV1
{
    uint32_t abi_version;
    const char* firmware_id;
    const char* firmware_name;

    float* (*out_l)(void);
    float* (*out_r)(void);
    void (*render)(uint32_t n);

    uint8_t* (*nor)(void);
    uint32_t (*nor_size)(void);
    void (*nor_erase)(void);
    uint32_t (*flash_writes_count)(void);

    void (*seed)(uint32_t s);
    void (*boot)(void);

    void (*buttons)(uint32_t labels);
    void (*keys)(uint32_t mask);
    void (*enc)(uint32_t role, int32_t n);
    void (*master)(uint32_t v);
    uint32_t (*midi)(uint32_t status, uint32_t d1, uint32_t d2);

    uint16_t* (*screen)(void);
    uint32_t (*lit_keys)(void);
    uint32_t (*mid_keys)(void);
    uint32_t (*sample_rate)(void);
} FM1BackendApiV1;

typedef const FM1BackendApiV1* (*FM1BackendGetApiFn)(void);

#ifdef __cplusplus
}
#endif

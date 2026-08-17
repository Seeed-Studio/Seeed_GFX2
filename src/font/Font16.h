#ifndef SEEED_GFX_FONT16_H
#define SEEED_GFX_FONT16_H

#define nr_chrs_f16 96
#define chr_hgt_f16 16
#define baseline_f16 13
#define data_size_f16 8
#define firstchr_f16 32

#ifdef __cplusplus
extern "C" {
#endif
extern const unsigned char widtbl_f16[96];
extern const unsigned char* const chrtbl_f16[96];
#ifdef __cplusplus
}
#endif

#endif

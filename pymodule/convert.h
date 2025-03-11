/***************************************************************************//**
 * @note
 * Copyright (c) 2019-2025 MemryX Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************/

#include <stdint.h>
#include <string.h>

// arch optimization checks
#ifdef __linux__
  #if defined(__x86_64__) || defined(_M_X64) || defined(__amd64__)
    #if defined(__BMI__) && defined(__BMI2__) && defined(__LZCNT__) && defined(__AVX2__)
      #ifndef USE_X86_OPT
        #define USE_X86_OPT
      #endif
    #endif
  #elif defined(__aarch64__) || defined(_M_ARM64)
    #ifndef USE_ARM64_OPT
      #define USE_ARM64_OPT
    #endif
  #endif
#endif

typedef struct _MemxGbfGbf80Map
{
    unsigned int man_0 : 8;
    unsigned int sign_0 : 1;
    unsigned int man_1 : 8;
    unsigned int sign_1 : 1;
    unsigned int man_2 : 8;
    unsigned int sign_2 : 1;
    unsigned int man_3_0 : 5; // because of C compiler will align to 4 bytes
    unsigned int man_3_1 : 3; // if we declare 9 bits here, it will be placed to bit[32:32+9]
    unsigned int sign_3 : 1;
    unsigned int man_4 : 8;
    unsigned int sign_4 : 1;
    unsigned int man_5 : 8;
    unsigned int sign_5 : 1;
    unsigned int man_6 : 8;
    unsigned int sign_6 : 1;
    unsigned int man_7_0 : 1;
    unsigned int man_7_1 : 7;
    unsigned int sign_7 : 1;
    unsigned int exp : 8;
    // to be noticed, this structure will actually be aligned to 12 bytes
    // but do not append dummy padding manually or will cause memory violation
} MemxGbfGbf80Map;

typedef struct _MemxGbfFloat32Map
{
    unsigned int zero : 16; // always zeros field
    unsigned int man : 7;
    unsigned int exp : 8;
    unsigned int sign : 1;
} MemxGbfFloat32Map;


void gbf_encode(float *flt32_buffer, uint8_t *gbf80_buffer, int length)
{
    MemxGbfGbf80Map *gbf80_map;
    MemxGbfFloat32Map *flt32_map;
    uint8_t *gbf80;
    float *flt32;
    int gbf80_offset = 0;
    int flt32_offset = 0;

    unsigned char exp;
    unsigned char man;
    #define _MX_MAX(x, y) (((x) > (y)) ? (x) : (y))
    #define _SET_MANTISSA_SHIFT_WITH_ROUNDING(_exp_shift_)                                                                       \
        do                                                                                                                       \
        {                                                                                                                        \
            if ((_exp_shift_) == 0)                                                                                              \
            {                                                                                                                    \
                man = (flt32_map->man == 0x7f) ? (unsigned char)(0x80 | flt32_map->man)                                          \
                                               : (unsigned char)(0x80 | flt32_map->man) + ((flt32_map->zero >> 15) & 0x1);       \
            }                                                                                                                    \
            else                                                                                                                 \
            {                                                                                                                    \
                man = (unsigned char)((0x80 | flt32_map->man) >> (_exp_shift_)) + ((flt32_map->man >> ((_exp_shift_)-1)) & 0x1); \
            }                                                                                                                    \
        } while (0)

    while ((flt32_offset < length))
    {
        gbf80 = gbf80_buffer + gbf80_offset;
        flt32 = flt32_buffer + flt32_offset;

        // performs float32 to float16 rounding, based on IEEE floating point design
        // no need to handle exponent and mantissa separately
        for (int i = 0; i < 8; ++i)
        {
            if (flt32_offset + i < length)
            {
                *(uint32_t *)(flt32 + i) += 0x00008000;
                *(uint32_t *)(flt32 + i) &= 0xffff0000;
            }
        }

        // gets maximum exponent among 8 floating points
        exp = 0;
        for (int i = 0; i < 8; ++i)
        {
            if (flt32_offset + i < length)
            {
                flt32_map = (MemxGbfFloat32Map *)(flt32 + i);
                exp = _MX_MAX(exp, (unsigned char)flt32_map->exp);
            }
        }

        // combines 8 floating points to gbf80
        gbf80_map = (MemxGbfGbf80Map *)gbf80;
        gbf80_map->exp = exp;
        if (flt32_offset + 0 < length)
        {
            flt32_map = (MemxGbfFloat32Map *)(flt32 + 0);
            _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp - flt32_map->exp);
            gbf80_map->man_0 = man & 0xff;
            gbf80_map->sign_0 = (man & 0xff) ? flt32_map->sign : 0;
        }
        else
        {
            gbf80_map->man_0 = 0;
            gbf80_map->sign_0 = 0;
        }
        if (flt32_offset + 1 < length)
        {
            flt32_map = (MemxGbfFloat32Map *)(flt32 + 1);
            _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp - flt32_map->exp);
            gbf80_map->man_1 = man & 0xff;
            gbf80_map->sign_1 = (man & 0xff) ? flt32_map->sign : 0;
        }
        else
        {
            gbf80_map->man_1 = 0;
            gbf80_map->sign_1 = 0;
        }
        if (flt32_offset + 2 < length)
        {
            flt32_map = (MemxGbfFloat32Map *)(flt32 + 2);
            _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp - flt32_map->exp);
            gbf80_map->man_2 = man & 0xff;
            gbf80_map->sign_2 = (man & 0xff) ? flt32_map->sign : 0;
        }
        else
        {
            gbf80_map->man_2 = 0;
            gbf80_map->sign_2 = 0;
        }
        if (flt32_offset + 3 < length)
        {
            flt32_map = (MemxGbfFloat32Map *)(flt32 + 3);
            _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp - flt32_map->exp);
            gbf80_map->man_3_0 = man & 0x1f;
            gbf80_map->man_3_1 = (man >> 5) & 0x7;
            gbf80_map->sign_3 = (man & 0xff) ? flt32_map->sign : 0;
        }
        else
        {
            gbf80_map->man_3_0 = 0;
            gbf80_map->man_3_1 = 0;
            gbf80_map->sign_3 = 0;
        }
        if (flt32_offset + 4 < length)
        {
            flt32_map = (MemxGbfFloat32Map *)(flt32 + 4);
            _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp - flt32_map->exp);
            gbf80_map->man_4 = man & 0xff;
            gbf80_map->sign_4 = (man & 0xff) ? flt32_map->sign : 0;
        }
        else
        {
            gbf80_map->man_4 = 0;
            gbf80_map->sign_4 = 0;
        }
        if (flt32_offset + 5 < length)
        {
            flt32_map = (MemxGbfFloat32Map *)(flt32 + 5);
            _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp - flt32_map->exp);
            gbf80_map->man_5 = man & 0xff;
            gbf80_map->sign_5 = (man & 0xff) ? flt32_map->sign : 0;
        }
        else
        {
            gbf80_map->man_5 = 0;
            gbf80_map->sign_5 = 0;
        }
        if (flt32_offset + 6 < length)
        {
            flt32_map = (MemxGbfFloat32Map *)(flt32 + 6);
            _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp - flt32_map->exp);
            gbf80_map->man_6 = man & 0xff;
            gbf80_map->sign_6 = (man & 0xff) ? flt32_map->sign : 0;
        }
        else
        {
            gbf80_map->man_6 = 0;
            gbf80_map->sign_6 = 0;
        }
        if (flt32_offset + 7 < length)
        {
            flt32_map = (MemxGbfFloat32Map *)(flt32 + 7);
            _SET_MANTISSA_SHIFT_WITH_ROUNDING(exp - flt32_map->exp);
            gbf80_map->man_7_0 = man & 0x1;
            gbf80_map->man_7_1 = (man >> 1) & 0x7f;
            gbf80_map->sign_7 = (man & 0xff) ? flt32_map->sign : 0;
        }
        else
        {
            gbf80_map->man_7_0 = 0;
            gbf80_map->man_7_1 = 0;
            gbf80_map->sign_7 = 0;
        }

        gbf80_offset += 10;
        flt32_offset += 8;
    }

} // gbf_encode

#ifdef USE_X86_OPT
#include <x86intrin.h>

// EXTRACTs
inline uint32_t getbits32(const void* val, unsigned int highbit, unsigned int lowbit){
    return _bextr_u32(*((const uint32_t*) val), lowbit, highbit-lowbit+1);
}
inline uint64_t getbits64(const void* val, unsigned int highbit, unsigned int lowbit){
    return _bextr_u64(*((const uint64_t*) val), lowbit, highbit-lowbit+1);
}

// LZCNT
inline int lzcount(uint32_t a){
    return _lzcnt_u32(a);
}

#else

// EXTRACTs (arch-independent)
inline uint32_t getbits32(const void* val, unsigned int highbit, unsigned int lowbit){
    return ( (*((const uint32_t*) val)) & (((uint32_t)((1<<(highbit-lowbit+1))-1))<<lowbit) ) >> lowbit;
}
inline uint64_t getbits64(const void* val, unsigned int highbit, unsigned int lowbit){
    return ( (*((const uint64_t*) val)) & (((uint64_t)((1L<<(highbit-lowbit+1))-1L))<<lowbit) ) >> lowbit;
}

// LZCNT
#ifdef __GNUC__
// GCC builin for optimized CLZ
inline int lzcount(uint32_t a){
    return (a==0) ? 32 : __builtin_clz(a);
}
#else
// compiler & architecture independent lzcnt algorithm
const unsigned int clz_table_4[] = {
    0,
    4,
    3, 3,
    2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1
};

inline int lzcount(uint32_t a){
    unsigned int n;

    if (a == 0) {
        return 32;
    }

    n = clz_table_4[a >> (sizeof(a)*8 - 4)];
    if (n == 0) {
        if ((a & 0xFFFF0000) == 0) { n  = 16; a <<= 16; }
        if ((a & 0xFF000000) == 0) { n += 8;  a <<= 8;  }
        if ((a & 0xF0000000) == 0) { n += 4;  a <<= 4;  }
        n += clz_table_4[a >> (sizeof(a)*8 - 4)];
    }

    return n - 1;
}
#endif // lzcount GCC check

#endif // if not USE_X86_OPT


// SATURATED SUBTRACT
inline uint8_t saturated_subtract8(uint8_t a, uint8_t b){
    uint8_t res = a - b;
    res &= -(res <= a);
    return res;
}


void gbf_decode(uint8_t *gbf80_buffer, float *flt32_buffer, unsigned int length)
{
    size_t gbf80_offset = 0;
    size_t flt32_offset = 0;

    uint32_t *flt32 = NULL;

    uint8_t  *gbf80 = NULL;
    uint64_t gbf80_bot64 = 0;
    uint32_t gbf80_top16p32 = 0;

    uint8_t  gbf80_exp = 0;

    // explicitly duplicated so it's obvious to the compiler
    // that these are independent and have no data hazard
    uint32_t t0, t1, t2, t3, t4, t5, t6, t7;
    uint32_t e0, e1, e2, e3, e4, e5, e6, e7;
    uint8_t  d0, d1, d2, d3, d4, d5, d6, d7;

    unsigned int remainder = length % 8;
    unsigned int num_full_x8 = length-remainder;

    // go through the bulk without having to `if` to check
    // for final chunk (when ch%8!=0)
    while( flt32_offset < num_full_x8 ){

        // set base pointers
        flt32 = ((uint32_t*)flt32_buffer) + flt32_offset;
        gbf80 = gbf80_buffer + gbf80_offset;
        memcpy(&gbf80_bot64, gbf80_buffer + gbf80_offset, 8);
        memcpy(&gbf80_top16p32, gbf80_buffer + gbf80_offset + 8, 2);

        // clear & set the signs
        *(flt32+0) = (getbits64(&gbf80_bot64,  8,  8))<<31;
        *(flt32+1) = (getbits64(&gbf80_bot64, 17, 17))<<31;
        *(flt32+2) = (getbits64(&gbf80_bot64, 26, 26))<<31;
        *(flt32+3) = (getbits64(&gbf80_bot64, 35, 35))<<31;
        *(flt32+4) = (getbits64(&gbf80_bot64, 44, 44))<<31;
        *(flt32+5) = (getbits64(&gbf80_bot64, 53, 53))<<31;
        *(flt32+6) = (getbits64(&gbf80_bot64, 62, 62))<<31;
        *(flt32+7) = (getbits32(&gbf80_top16p32,  7,  7))<<31;

        // shared exp
        gbf80_exp = getbits32(&gbf80_top16p32, 15, 8) & 0xFF;

        // ch[0]
        t0 = getbits64(&gbf80_bot64, 7, 0); // get 1.mantissa
        d0 = (uint8_t) (lzcount(t0)-24); // count leading 1s
        e0 = ((uint32_t) saturated_subtract8(gbf80_exp, d0)); // saturated subtract exps
        e0 &= ((uint32_t) ((int32_t) -(d0 < 8)) ); // force exp=0 if t0=0
        t0 <<= d0; // rsh up by diff
        t0 &= 0x7F; // clear the hidden 1.
        *(flt32+0) |= (e0<<23) | (t0<<16); // set data

        // ch[1]
        t1 = getbits64(&gbf80_bot64, 16, 9);
        d1 = (uint8_t) (lzcount(t1)-24);
        e1 = ((uint32_t) saturated_subtract8(gbf80_exp, d1));
        e1 &= ((uint32_t) ((int32_t) -(d1 < 8)) );
        t1 <<= d1;
        t1 &= 0x7F;
        *(flt32+1) |= (e1<<23) | (t1<<16);

        // ch[2]
        t2 = getbits64(&gbf80_bot64, 25, 18);
        d2 = (uint8_t) (lzcount(t2)-24);
        e2 = ((uint32_t) saturated_subtract8(gbf80_exp, d2));
        e2 &= ((uint32_t) ((int32_t) -(d2 < 8)) );
        t2 <<= d2;
        t2 &= 0x7F;
        *(flt32+2) |= (e2<<23) | (t2<<16);

        // ch[3]
        t3 = getbits64(&gbf80_bot64, 34, 27);
        d3 = (uint8_t) (lzcount(t3)-24);
        e3 = ((uint32_t) saturated_subtract8(gbf80_exp, d3));
        e3 &= ((uint32_t) ((int32_t) -(d3 < 8)) );
        t3 <<= d3;
        t3 &= 0x7F;
        *(flt32+3) |= (e3<<23) | (t3<<16);

        // ch[4]
        t4 = getbits64(&gbf80_bot64, 43, 36);
        d4 = (uint8_t) (lzcount(t4)-24);
        e4 = ((uint32_t) saturated_subtract8(gbf80_exp, d4));
        e4 &= ((uint32_t) ((int32_t) -(d4 < 8)) );
        t4 <<= d4;
        t4 &= 0x7F;
        *(flt32+4) |= (e4<<23) | (t4<<16);

        // ch[5]
        t5 = getbits64(&gbf80_bot64, 52, 45);
        d5 = (uint8_t) (lzcount(t5)-24);
        e5 = ((uint32_t) saturated_subtract8(gbf80_exp, d5));
        e5 &= ((uint32_t) ((int32_t) -(d5 < 8)) );
        t5 <<= d5;
        t5 &= 0x7F;
        *(flt32+5) |= (e5<<23) | (t5<<16);

        // ch[6]
        t6 = getbits64(&gbf80_bot64, 61, 54);
        d6 = (uint8_t) (lzcount(t6)-24);
        e6 = ((uint32_t) saturated_subtract8(gbf80_exp, d6));
        e6 &= ((uint32_t) ((int32_t) -(d6 < 8)) );
        t6 <<= d6;
        t6 &= 0x7F;
        *(flt32+6) |= (e6<<23) | (t6<<16);

        // ch[7]
        t7 = ((*(gbf80+7) & 0x80)>>7) | ((*(gbf80+8) & 0x7F)<<1);
        d7 = (uint8_t) (lzcount(t7)-24);
        e7 = ((uint32_t) saturated_subtract8(gbf80_exp, d7));
        e7 &= ((uint32_t) ((int32_t) -(d7 < 8)) );
        t7 <<= d7;
        t7 &= 0x7F;
        *(flt32+7) |= (e7<<23) | (t7<<16);


        // increment counters and stuff
        flt32_offset += 8;
        gbf80_offset += 10;
    }

    // now the final bunch (if not %8)
    // this is slow due to the branches!
    if(remainder > 0){
        // set base pointers
        flt32 = ((uint32_t*)flt32_buffer) + flt32_offset;
        gbf80 = gbf80_buffer + gbf80_offset;
        memcpy(&gbf80_bot64, gbf80_buffer + gbf80_offset, 8);
        memcpy(&gbf80_top16p32, gbf80_buffer + gbf80_offset + 8, 2);

        // shared exp
        gbf80_exp = getbits32(&gbf80_top16p32, 15, 8) & 0xFF;

        // ch[0]  (remainder >= 1)
        *(flt32+0) = (getbits64(&gbf80_bot64,  8,  8))<<31;
        t0 = getbits64(&gbf80_bot64, 7, 0); // get 1.mantissa
        d0 = (uint8_t) (lzcount(t0)-24); // count leading 1s
        e0 = ((uint32_t) saturated_subtract8(gbf80_exp, d0)); // saturated subtract exps
        e0 &= ((uint32_t) ((int32_t) -(d0 < 8)) ); // force exp=0 if t0=0
        t0 <<= d0; // rsh up by diff
        t0 &= 0x7F; // clear the hidden 1.
        *(flt32+0) |= (e0<<23) | (t0<<16); // set data

        if(remainder >= 2){
            // ch[1]
            *(flt32+1) = (getbits64(&gbf80_bot64, 17, 17))<<31;
            t1 = getbits64(&gbf80_bot64, 16, 9);
            d1 = (uint8_t) (lzcount(t1)-24);
            e1 = ((uint32_t) saturated_subtract8(gbf80_exp, d1));
            e1 &= ((uint32_t) ((int32_t) -(d1 < 8)) );
            t1 <<= d1;
            t1 &= 0x7F;
            *(flt32+1) |= (e1<<23) | (t1<<16);

            if(remainder >= 3){
                // ch[2]
                *(flt32+2) = (getbits64(&gbf80_bot64, 26, 26))<<31;
                t2 = getbits64(&gbf80_bot64, 25, 18);
                d2 = (uint8_t) (lzcount(t2)-24);
                e2 = ((uint32_t) saturated_subtract8(gbf80_exp, d2));
                e2 &= ((uint32_t) ((int32_t) -(d2 < 8)) );
                t2 <<= d2;
                t2 &= 0x7F;
                *(flt32+2) |= (e2<<23) | (t2<<16);

                if(remainder >= 4){
                    // ch[3]
                    *(flt32+3) = (getbits64(&gbf80_bot64, 35, 35))<<31;
                    t3 = getbits64(&gbf80_bot64, 34, 27);
                    d3 = (uint8_t) (lzcount(t3)-24);
                    e3 = ((uint32_t) saturated_subtract8(gbf80_exp, d3));
                    e3 &= ((uint32_t) ((int32_t) -(d3 < 8)) );
                    t3 <<= d3;
                    t3 &= 0x7F;
                    *(flt32+3) |= (e3<<23) | (t3<<16);

                    if(remainder >= 5){
                        // ch[4]
                        *(flt32+4) = (getbits64(&gbf80_bot64, 44, 44))<<31;
                        t4 = getbits64(&gbf80_bot64, 43, 36);
                        d4 = (uint8_t) (lzcount(t4)-24);
                        e4 = ((uint32_t) saturated_subtract8(gbf80_exp, d4));
                        e4 &= ((uint32_t) ((int32_t) -(d4 < 8)) );
                        t4 <<= d4;
                        t4 &= 0x7F;
                        *(flt32+4) |= (e4<<23) | (t4<<16);

                        if(remainder >= 6){
                            // ch[5]
                            *(flt32+5) = (getbits64(&gbf80_bot64, 53, 53))<<31;
                            t5 = getbits64(&gbf80_bot64, 52, 45);
                            d5 = (uint8_t) (lzcount(t5)-24);
                            e5 = ((uint32_t) saturated_subtract8(gbf80_exp, d5));
                            e5 &= ((uint32_t) ((int32_t) -(d5 < 8)) );
                            t5 <<= d5;
                            t5 &= 0x7F;
                            *(flt32+5) |= (e5<<23) | (t5<<16);

                            if(remainder == 7){

                                // ch[6]
                                *(flt32+6) = (getbits64(&gbf80_bot64, 62, 62))<<31;
                                t6 = getbits64(&gbf80_bot64, 61, 54);
                                d6 = (uint8_t) (lzcount(t6)-24);
                                e6 = ((uint32_t) saturated_subtract8(gbf80_exp, d6));
                                e6 &= ((uint32_t) ((int32_t) -(d6 < 8)) );
                                t6 <<= d6;
                                t6 &= 0x7F;
                                *(flt32+6) |= (e6<<23) | (t6<<16);

                                // IMPOSSIBLE to get remainder >= 8!!!
                            }// 7
                        }// 6
                    }// 5
                }// 4
            }// 3
        }// 2

        // increment counters and stuff
        flt32_offset += remainder;
        gbf80_offset += 10;
    }
}


// actual conversions

void convert_gbf(float *src, uint8_t *dst, int tensor_size, int num_ch){
    int  num_xyz_pixels = (tensor_size / num_ch);
    int  num_gbf_per_pixel = (num_ch / 8) + ( ((num_ch%8)!=0) ? 1 : 0);

    for(int i = 0; i < num_xyz_pixels; i++){
        uint8_t *gbf_base = &(dst[ i * (num_gbf_per_pixel * 10) ]);
        float   *flt_base = (float*) &(src[ i * num_ch ]);

        gbf_encode(flt_base, gbf_base, num_ch);
    }
}

void convert_gbf_row_pad(float *src, uint8_t *dst, int height, int width, int z, int num_ch){
    int num_gbf_per_pixel = (num_ch / 8) + (((num_ch%8)!=0) ? 1 : 0);
    int gbf80_pixel_size = num_gbf_per_pixel * 10;
    int gbf80_row_size = ((width * z * gbf80_pixel_size) + 3) & ~0x3;
    int flt32_row_size = width * z * num_ch;
    int gbf80_row_offset = 0;
    int flt32_row_offset = 0;

    for (int h_idx = 0; h_idx < height; h_idx++) {
        int gbf80_pixel_offset = 0;
        int flt32_pixel_offset = 0;
        for (int w_idx = 0; w_idx < width; w_idx++) {
            for (int z_idx = 0; z_idx < z; z_idx++) {
                float *flt32_buffer = (float *)(src + flt32_row_offset + flt32_pixel_offset);
                uint8_t *gbf80_buffer = (uint8_t *)(dst + gbf80_row_offset + gbf80_pixel_offset);
                gbf_encode(flt32_buffer, gbf80_buffer, num_ch);

                gbf80_pixel_offset += gbf80_pixel_size;
                flt32_pixel_offset += num_ch;
            }
        }
        gbf80_row_offset += gbf80_row_size;
        flt32_row_offset += flt32_row_size;
    }
}

void unconvert_gbf(uint8_t *src, float *dst, int tensor_size, int num_ch){
    int  num_xyz_pixels = (tensor_size / num_ch);
    int  num_gbf_per_pixel = (num_ch / 8) + ( ((num_ch%8)!=0) ? 1 : 0);

    for(int i = 0; i < num_xyz_pixels; i++){
        uint8_t *gbf_base = &(src[ i * (num_gbf_per_pixel * 10) ]);
        float   *flt_base = (float*) &(dst[ i * num_ch ]);

        gbf_decode(gbf_base, flt_base, num_ch);
    }
}

void unconvert_gbf_hpoc(uint8_t *src, float *dst, int height, int width, int z, int num_ch, int hpoc_size, int *hpoc_indexes, int row_pad) {
    int num_gbf_ch = num_ch + hpoc_size;
    int num_gbf_per_pixel = (num_gbf_ch / 8) + (((num_gbf_ch%8)!=0) ? 1 : 0);
    int gbf80_pixel_size = num_gbf_per_pixel * 10;
    int gbf80_row_size = width * z * gbf80_pixel_size;
    int flt32_row_size = width * z * num_ch;
    int gbf80_row_offset = 0;
    int flt32_row_offset = 0;

    if (row_pad) {
       gbf80_row_size = (gbf80_row_size + 3) & ~0x3;
    }

    // loop each row
    for (int h_idx = 0; h_idx < height; h_idx++) {
        int gbf80_pixel_offset = 0;
        int flt32_pixel_offset = 0;
        // visist all GBF pixel in each row
        for (int w_idx = 0; w_idx < width; w_idx++) {
            for (int z_idx = 0; z_idx < z; z_idx++) {
                int check_dummy_ch_idx = 0;
                // decode for each buf of GBF pixel
                for (int gbf_ch_idx = 0, gbf_buf_offset = 0, flt32_buf_offset = 0; gbf_ch_idx < num_gbf_ch; gbf_ch_idx += 8, gbf_buf_offset += 10) {
                    float decode_float_buf[8] = {0};
                    uint8_t *gbf80_buffer = (uint8_t *)(src + gbf80_row_offset + gbf80_pixel_offset + gbf_buf_offset);
                    gbf_decode(gbf80_buffer, decode_float_buf, 8);

                    for (int ch_offset = 0; ch_offset < 8; ++ch_offset) {
                        int curr_ch_idx = gbf_ch_idx + ch_offset;
                        // skip dummy channel
                        if ((check_dummy_ch_idx < num_gbf_ch) &&
                            (curr_ch_idx == hpoc_indexes[check_dummy_ch_idx])) {
                            check_dummy_ch_idx++;
                            continue;
                        } else {
                            // update target channel data of FP32 pixel
                            if (gbf_ch_idx + ch_offset < num_gbf_ch) {
                                float *flt32_buffer = (float *)(dst + flt32_row_offset + flt32_pixel_offset + flt32_buf_offset);
                                *flt32_buffer = decode_float_buf[ch_offset];
                                flt32_buf_offset++;
                            }
                        }
                    }/* ch_offset */
                }/* gbf_ch_idx */
                gbf80_pixel_offset += gbf80_pixel_size;
                flt32_pixel_offset += num_ch;
            }/* z */
        }/* w */
        gbf80_row_offset += gbf80_row_size;
        flt32_row_offset += flt32_row_size;
    }/* h */
}

void unconvert_gbf_row_pad(uint8_t *src, float *dst, int height, int width, int z, int num_ch) {
    int num_gbf_per_pixel = (num_ch / 8) + (((num_ch%8)!=0) ? 1 : 0);
    int gbf80_pixel_size = num_gbf_per_pixel * 10;
    int gbf80_row_size = ((width * z * gbf80_pixel_size) + 3) & ~0x3;
    int flt32_row_size = width * z * num_ch;
    int gbf80_row_offset = 0;
    int flt32_row_offset = 0;

    for (int h_idx = 0; h_idx < height; h_idx++) {
        int gbf80_pixel_offset = 0;
        int flt32_pixel_offset = 0;
        for (int w_idx = 0; w_idx < width; w_idx++) {
            for (int z_idx = 0; z_idx < z; z_idx++) {
                uint8_t *gbf80_buffer = (uint8_t *)(src + gbf80_row_offset + gbf80_pixel_offset);
                float *flt32_buffer = (float *)(dst + flt32_row_offset + flt32_pixel_offset);
                gbf_decode(gbf80_buffer, flt32_buffer, num_ch);

                gbf80_pixel_offset += gbf80_pixel_size;
                flt32_pixel_offset += num_ch;
            }
        }
        gbf80_row_offset += gbf80_row_size;
        flt32_row_offset += flt32_row_size;
    }
}

void convert_bf16(float *src, uint8_t *dst, int tensor_size){
    uint32_t *fp_uint32 = (uint32_t*) src;
    for(int i = 0; i < tensor_size; i++){
        uint32_t v = fp_uint32[i] + 0x00008000;
        memcpy(&(dst[i*2]), ((uint8_t*) &v)+2, 2);
    }
}

void unconvert_bf16(uint8_t *src, float *dst, int tensor_size){
    memset(dst, 0, tensor_size*sizeof(float)); // wipe data

    uint16_t *bf_dat = (uint16_t*) src;
    for(int i = 0; i < tensor_size; i++){
        memcpy(((uint8_t*) (dst + i))+2, (uint8_t*) (bf_dat + i), 2);
    }
}

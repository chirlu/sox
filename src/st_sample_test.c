#define DEBUG
#include <assert.h>
#include "st.h"

#define TEST_UINT(bits) \
  uint##bits = 0; \
  sample = ST_UNSIGNED_TO_SAMPLE(bits,uint##bits); \
  assert(sample == ST_SAMPLE_MIN); \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == 0 && clips == 0); \
 \
  uint##bits = 1; \
  sample = ST_UNSIGNED_TO_SAMPLE(bits,uint##bits); \
  assert(sample > ST_SAMPLE_MIN && sample < 0); \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == 1 && clips == 0); \
 \
  uint##bits = ST_INT_MAX(bits); \
  sample = ST_UNSIGNED_TO_SAMPLE(bits,uint##bits); \
  assert(sample * ST_INT_MAX(bits) == ST_UNSIGNED_TO_SAMPLE(bits,1)); \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == ST_INT_MAX(bits) && clips == 0); \
 \
  sample =ST_UNSIGNED_TO_SAMPLE(bits,1)+ST_UNSIGNED_TO_SAMPLE(bits,ST_INT_MAX(bits))/2; \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == 1 && clips == 0); \
 \
  sample = ST_UNSIGNED_TO_SAMPLE(bits,1)+ST_UNSIGNED_TO_SAMPLE(bits,ST_INT_MAX(bits))/2-1; \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == 0 && clips == 0); \
 \
  uint##bits = (0^ST_INT_MIN(bits)); \
  sample = ST_UNSIGNED_TO_SAMPLE(bits,uint##bits); \
  assert(sample == 0); \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == (0^ST_INT_MIN(bits)) && clips == 0); \
 \
  uint##bits = ((0^ST_INT_MIN(bits))+1); \
  sample = ST_UNSIGNED_TO_SAMPLE(bits,uint##bits); \
  assert(sample > 0 && sample < ST_SAMPLE_MAX); \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == ((0^ST_INT_MIN(bits))+1) && clips == 0); \
 \
  uint##bits = ST_UINT_MAX(bits); \
  sample = ST_UNSIGNED_TO_SAMPLE(bits,uint##bits); \
  assert(sample == ST_INT_MAX(bits) * ST_UNSIGNED_TO_SAMPLE(bits,((0^ST_INT_MIN(bits))+1))); \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == ST_UINT_MAX(bits) && clips == 0); \
 \
  sample =ST_UNSIGNED_TO_SAMPLE(bits,ST_UINT_MAX(bits))+ST_UNSIGNED_TO_SAMPLE(bits,((0^ST_INT_MIN(bits))+1))/2-1; \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == ST_UINT_MAX(bits) && clips == 0); \
 \
  sample = ST_UNSIGNED_TO_SAMPLE(bits,ST_UINT_MAX(bits))+ST_UNSIGNED_TO_SAMPLE(bits,((0^ST_INT_MIN(bits))+1))/2; \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == ST_UINT_MAX(bits) && --clips == 0); \
 \
  sample = ST_SAMPLE_MAX; \
  uint##bits = ST_SAMPLE_TO_UNSIGNED(bits,sample, clips); \
  assert(uint##bits == ST_UINT_MAX(bits) && --clips == 0); \

#define TEST_SINT(bits) \
  int##bits = ST_INT_MIN(bits); \
  sample = ST_SIGNED_TO_SAMPLE(bits,int##bits); \
  assert(sample == ST_SAMPLE_MIN); \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && clips == 0); \
 \
  int##bits = ST_INT_MIN(bits)+1; \
  sample = ST_SIGNED_TO_SAMPLE(bits,int##bits); \
  assert(sample > ST_SAMPLE_MIN && sample < 0); \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && clips == 0); \
 \
  int##bits = ST_UINT_MAX(bits) /* i.e. -1 */; \
  sample = ST_SIGNED_TO_SAMPLE(bits,int##bits); \
  assert(sample * ST_INT_MAX(bits) == ST_SIGNED_TO_SAMPLE(bits,ST_INT_MIN(bits)+1)); \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && clips == 0); \
 \
  int##bits = ST_INT_MIN(bits)+1; \
  sample =ST_UNSIGNED_TO_SAMPLE(bits,1)+ST_UNSIGNED_TO_SAMPLE(bits,ST_INT_MAX(bits))/2; \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && clips == 0); \
 \
  int##bits = ST_INT_MIN(bits); \
  sample = ST_UNSIGNED_TO_SAMPLE(bits,1)+ST_UNSIGNED_TO_SAMPLE(bits,ST_INT_MAX(bits))/2-1; \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && clips == 0); \
 \
  int##bits = 0; \
  sample = ST_SIGNED_TO_SAMPLE(bits,int##bits); \
  assert(sample == 0); \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && clips == 0); \
 \
  int##bits = 1; \
  sample = ST_SIGNED_TO_SAMPLE(bits,int##bits); \
  assert(sample > 0 && sample < ST_SAMPLE_MAX); \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && clips == 0); \
 \
  int##bits = ST_INT_MAX(bits); \
  sample = ST_SIGNED_TO_SAMPLE(bits,int##bits); \
  assert(sample == ST_INT_MAX(bits) * ST_SIGNED_TO_SAMPLE(bits,1)); \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && clips == 0); \
 \
  sample =ST_UNSIGNED_TO_SAMPLE(bits,ST_UINT_MAX(bits))+ST_UNSIGNED_TO_SAMPLE(bits,((0^ST_INT_MIN(bits))+1))/2-1; \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && clips == 0); \
 \
  sample =ST_UNSIGNED_TO_SAMPLE(bits,ST_UINT_MAX(bits))+ST_UNSIGNED_TO_SAMPLE(bits,((0^ST_INT_MIN(bits))+1))/2; \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && --clips == 0); \
 \
  sample = ST_SAMPLE_MAX; \
  int##bits##_2 = ST_SAMPLE_TO_SIGNED(bits,sample, clips); \
  assert(int##bits##_2 == int##bits && --clips == 0);

int main()
{
  int8_t int8;
  int16_t int16;
  int24_t int24;

  uint8_t uint8;
  uint16_t uint16;
  uint24_t uint24;

  int8_t int8_2;
  int16_t int16_2;
  int24_t int24_2;

  st_sample_t sample;
  st_size_t clips = 0;

  double d;

  TEST_UINT(8)
  TEST_UINT(16)
  TEST_UINT(24)

  TEST_SINT(8)
  TEST_SINT(16)
  TEST_SINT(24)

  d = -1.0000000001;
  sample = ST_FLOAT_DDWORD_TO_SAMPLE(d, clips);
  assert(sample == -ST_SAMPLE_MAX && --clips == 0);

  d = -1;
  sample = ST_FLOAT_DDWORD_TO_SAMPLE(d, clips);
  assert(sample == -ST_SAMPLE_MAX && clips == 0);
  d = ST_SAMPLE_TO_FLOAT_DDWORD(sample,clips);
  assert(d == -1 && clips == 0);

  --sample;
  d = ST_SAMPLE_TO_FLOAT_DDWORD(sample,clips);
  assert(d == -1 && --clips == 0);

  d = 1;
  sample = ST_FLOAT_DDWORD_TO_SAMPLE(d, clips);
  assert(sample == ST_SAMPLE_MAX && clips == 0);
  d = ST_SAMPLE_TO_FLOAT_DDWORD(sample,clips);
  assert(d == 1 && clips == 0);

  d = 1.0000000001;
  sample = ST_FLOAT_DDWORD_TO_SAMPLE(d, clips);
  assert(sample == ST_SAMPLE_MAX && --clips == 0);

  {
    enum {MEANT_TO_FAIL};
    assert(MEANT_TO_FAIL);
  }
  return 0;
}

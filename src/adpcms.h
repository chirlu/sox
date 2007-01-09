typedef struct adpcm_struct
{
  int last_output;
  int step_index;
  int max_step_index;
  int const * steps;
  int mask;
} * adpcm_t;

int adpcm_decode(int code, adpcm_t state);   /* 4-bit -> 16-bit */
int adpcm_encode(int sample, adpcm_t state); /* 16-bit -> 4-bit */
void adpcm_init(adpcm_t state, int type /* 0:IMA, 1:OKI */);

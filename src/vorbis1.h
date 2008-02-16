#if defined __GNUC__ 
  #pragma GCC system_header 
#elif defined __SUNPRO_CC 
  #pragma disable_warn 
#elif defined _MSC_VER 
  #pragma warning(push, 1) 
#endif 

  vorbis_encode_init_vbr(
      &ve->vi, ft->signal.channels, ft->signal.rate + .5, quality / 10);

#if defined __SUNPRO_CC 
  #pragma enable_warn 
#elif defined _MSC_VER 
  #pragma warning(pop) 
#endif 

#if defined __GNUC__ 
  #pragma GCC system_header 
#elif defined __SUNPRO_CC 
  #pragma disable_warn 
#elif defined _MSC_VER 
  #pragma warning(push, 1) 
#endif 

  int n = E_IF_encode(amr->state, amr->mode, amr->pcm, coded, 1);

#if defined __SUNPRO_CC 
  #pragma enable_warn 
#elif defined _MSC_VER 
  #pragma warning(pop) 
#endif 

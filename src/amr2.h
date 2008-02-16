#if defined __GNUC__ 
  #pragma GCC system_header 
#elif defined __SUNPRO_CC 
  #pragma disable_warn 
#elif defined _MSC_VER 
  #pragma warning(push, 1) 
#endif 

  amr->state = E_IF_init();

#if defined __SUNPRO_CC 
  #pragma enable_warn 
#elif defined _MSC_VER 
  #pragma warning(pop) 
#endif 

#pragma once

#ifdef QOSRTP
  #ifdef _MSC_VER
    #define QOSRTP_API __declspec(dllexport)
  #else
    #define QOSRTP_API 
  #endif
#else
  #ifdef _MSC_VER
    #define QOSRTP_API __declspec(dllimport)
  #else
    #define QOSRTP_API 
  #endif 
#endif
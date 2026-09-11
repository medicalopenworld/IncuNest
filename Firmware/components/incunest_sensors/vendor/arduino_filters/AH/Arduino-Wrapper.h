#pragma once

#include <AH/Math/MinMaxFix.hpp>
#include <AH/STL/type_traits>

AH_DIAGNOSTIC_EXTERNAL_HEADER()
// PARCHE INCUNEST: Arduino -> capa de plataforma propia
#include "platform/plat_num.h"
#include "platform/plat_time.h"
#include "platform/plat_types.h" // min max
AH_DIAGNOSTIC_POP()

AH_DIAGNOSTIC_WERROR() // Enable errors on warnings

#ifdef PSTR
#pragma push_macro("PSTR")
#undef PSTR
#define PSTR(s) (s)
#endif

using FlashString_t = std::remove_reference<decltype(*F(""))>::type *;

#ifdef PSTR 
#pragma pop_macro("PSTR")
#endif

AH_DIAGNOSTIC_POP()

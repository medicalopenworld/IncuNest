#pragma once

#include <AH/Settings/Warnings.hpp>
AH_DIAGNOSTIC_WERROR() // Enable errors on warnings

AH_DIAGNOSTIC_EXTERNAL_HEADER()
// PARCHE INCUNEST: Arduino -> capa de plataforma propia
#include "platform/plat_num.h"
#include "platform/plat_time.h"
#include "platform/plat_types.h" // min max
AH_DIAGNOSTIC_POP()

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef abs
#undef abs
#endif

#ifdef round
#undef round
#endif

AH_DIAGNOSTIC_POP()

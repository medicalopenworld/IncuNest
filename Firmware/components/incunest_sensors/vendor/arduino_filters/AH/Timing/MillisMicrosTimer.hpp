#pragma once

#include <AH/Settings/Warnings.hpp>
AH_DIAGNOSTIC_WERROR() // Enable errors on warnings

AH_DIAGNOSTIC_EXTERNAL_HEADER()
#include <AH/Arduino-Wrapper.h> // millis, micros
AH_DIAGNOSTIC_POP()

#include <AH/Settings/NamespaceSettings.hpp>

BEGIN_AH_NAMESPACE

/// @addtogroup    AH_Timing
/// @{

/// A function type that returns a time value.
using timefunction = unsigned long (*)();

/**
 * @brief   A class for easily managing timed events. A wrapper for "Blink 
 *          Without Delay".
 * 
 * @tparam  time
 *          The time function to use.
 */
template <timefunction time = micros>
class Timer {
  public:
    /**
     * @brief   Constructor.
     * @param   interval
     *          The interval between two events.
     */
    Timer(unsigned long interval) : interval(interval) {
        // PARCHE INCUNEST: begin() estaba bajo #ifdef ARDUINO, y sin el
        // `previous` se quedaba en 0. Con un reloj que ya lleva segundos
        // andando, `now - 0 >= interval` es cierto muchisimas veces seguidas:
        // el temporizador dispararia EN RAFAGA hasta ponerse al dia, metiendo
        // cientos de muestras de golpe en el filtro Butterworth de la
        // temperatura. begin() deja previous = time() - interval, que dispara
        // UNA vez y sigue el periodo. Se llama siempre, que es lo que pasaba
        // antes en la placa.
        begin();
    }
    /// Initialize or reset the timer. The timer will fire immediately.
    void begin() { previous = time() - interval; }
    /// Initialize or reset the timer. The timer will fire after one period.
    void beginNextPeriod() { previous = time(); }
    /// Update the timer and return true if the event should fire.
    explicit operator bool() {
        auto now = time();
        if (now - previous >= interval) {
            previous += interval;
            return true;
        }
        return false;
    }

    /// Get the interval of the timer.
    unsigned long getInterval() const { return interval; }
    /// Set the interval of the timer.
    void setInterval(unsigned long interval) { this->interval = interval; }

  private:
    unsigned long interval;
    unsigned long previous = 0;
};

/// @}

END_AH_NAMESPACE

AH_DIAGNOSTIC_POP()

#pragma once

// Los ayudantes numericos que Arduino ponia como macros: min, max, abs,
// constrain y map. Nada de Arduino aqui; son unas pocas lineas propias.
//
// POR QUE PLANTILLAS Y NO MACROS:
// el primer intento fueron macros, calcadas de Arduino, para que admitiesen
// tipos mezclados —max(unsigned, int), min(double, long)—, que es justo lo
// que std::min y std::max NO hacen sin castear. Ese intento fallo en 20
// ficheros a la vez: una macro llamada `min` tambien pisa
// `std::numeric_limits<T>::min()`, que la libreria estandar usa por dentro.
// Es el clasico problema de las macros min/max, y Arduino lo tenia tambien;
// aqui simplemente sale a la luz porque ahora se compila contra la libreria
// estandar de verdad.
//
// Con plantillas se consigue lo mismo —tipos mezclados, sin castear en el
// punto de llamada— sin romper nada, y ademas los argumentos se evaluan UNA
// sola vez, al contrario que con las macros de Arduino. Ese es el unico
// cambio de comportamiento, y va a mejor: si alguna llamada dependia de la
// doble evaluacion de un argumento con efectos secundarios, era un fallo
// latente.

#ifdef __cplusplus

#include <type_traits>

template <class A, class B>
constexpr typename std::common_type<A, B>::type min(A a, B b) {
  using C = typename std::common_type<A, B>::type;
  return static_cast<C>(a) < static_cast<C>(b) ? static_cast<C>(a)
                                               : static_cast<C>(b);
}

template <class A, class B>
constexpr typename std::common_type<A, B>::type max(A a, B b) {
  using C = typename std::common_type<A, B>::type;
  return static_cast<C>(a) > static_cast<C>(b) ? static_cast<C>(a)
                                               : static_cast<C>(b);
}

template <class T> constexpr T abs(T v) { return v < 0 ? -v : v; }

template <class T, class L, class H>
constexpr typename std::common_type<T, L, H>::type constrain(T amt, L low,
                                                             H high) {
  using C = typename std::common_type<T, L, H>::type;
  const C a = static_cast<C>(amt);
  const C l = static_cast<C>(low);
  const C h = static_cast<C>(high);
  return a < l ? l : (a > h ? h : a);
}

extern "C" {
#endif // __cplusplus

// map() ya era una funcion en Arduino, no una macro, y opera en long. Se
// mantiene igual: la division entera y su truncado forman parte del resultado
// que espera el codigo que la llama.
long map(long x, long in_min, long in_max, long out_min, long out_max);

#ifdef __cplusplus
}
#endif

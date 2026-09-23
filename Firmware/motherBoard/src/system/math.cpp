/*
  MIT License

  Copyright (c) 2022 Medical Open World, Pablo Sánchez Bergasa

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/

#include <Arduino.h>

long minsToMillis(long timeInMillis) { return (timeInMillis * 60 * 1000); }

long secsToMillis(long timeInMillis) { return (timeInMillis * 1000); }

float millisToHours(long timeInMillis) {
  return (timeInMillis / 1000.0 / 60.0 / 60.0);
}

double roundSignificantDigits(double value, int numberOfDecimals) {
  double exponent;
  double provisionalValue = abs(value);
  while (provisionalValue < 1 && provisionalValue > 0) {
    provisionalValue *= 10;
    exponent++;
  }
  exponent = pow(10, numberOfDecimals);
  return (int)(value * exponent + 0.5) / exponent;
}
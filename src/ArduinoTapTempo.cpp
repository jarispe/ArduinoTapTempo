/*
 * ArduinoTapTempo.cpp
 * An Arduino library that times consecutive button presses to calculate Beats Per Minute. Corrects for missed beats and resets phase with single taps. 
 *
 * Copyright (c) 2016 Damien Clarke
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.  
 */

/*
 MIT License still applies
 This fork is an update to calculate BPM using micros() instead of millis() to provide a more accurate BPM
 Please note that not all Arduino platforms have an accurate micro resolution. https://www.arduino.cc/reference/en/language/functions/time/micros/
 Double check your platform has the proper resolution or calculations may be inaccurate
*/

#include <Arduino.h>
#include "ArduinoTapTempoMicros.h"

void ArduinoTapTempo::setSkippedTapThresholdLow(float threshold)
{
  if(threshold >= 1.0 && threshold <= 2.0){
    skippedTapThresholdLow = threshold;
  } else if (threshold < 1.0) {
    skippedTapThresholdLow = 1.0;
  } else {
    skippedTapThresholdLow = 2.0;
  }
}

void ArduinoTapTempo::setSkippedTapThresholdHigh(float threshold)
{
  if(threshold >= 2.0 && threshold <= 4.0){
    skippedTapThresholdHigh = threshold;
  }else if (threshold < 2.0) {
    skippedTapThresholdHigh =  2.0;
  } else {
    skippedTapThresholdHigh = 4.0;
  }
  
}

float ArduinoTapTempo::getBPM()
{
  return bpm;
}

unsigned long ArduinoTapTempo::getBeatLength()
{
  return beatLengthMS;
}


//This can be used to calculate how many beats until your timing is ahead by x micros for even more precision
//ex: In your program, every beat add this value and check to see if a whole number (x) has been reached. 
//Once it has, you're now x micros ahead and need to add as many to stay on beat.
double ArdunioTapTempo::getBeatFract()
{
  return beatFract
}


void ArduinoTapTempo::setSigFigs(unsigned short setSigFigs)
{
  if setSigFigs < 4 {
    sigFigs = setSigFigs;
  } else {
    sigFigs = 3;
  }
}


void ArduinoTapTempo::setBPM(float setBpm)
{
    bpm = setBpm;
    double tempMS = 0.0;
    beatFract = modf(double(sixtySeconds / setBpm),&tempMS );
    beatLengthMS = unsigned long(tempMS);


}


void ArduinoTapTempo::setBPM(unsigned long ms)
{
  switch(sigFigs){
    case 0: 
      bpm = floor((double)(sixtySeconds/ms + 0.5));
      break;
    case 1:
      bpm = floor((double)(sixtySeconds/ms *10 + 0.5)) / 10;
      break;
    case 2:
      bpm = floor((double)(sixtySeconds/ms *100 + 0.5)) / 100;
      break:
    case 3:
      bpm = floor((double)(sixtySeconds/ms *1000 + 0.5)) / 1000;
      break;
    default:
      bpm = floor((double)(sixtySeconds/ms + 0.5));
      break;
  }
}


bool ArduinoTapTempo::onBeat()
{
  return fmod((double)microsSinceReset, (double)beatLengthMS) < fmod((double)microsSinceResetOld, (double)beatLengthMS);
}

bool ArduinoTapTempo::isChainActive()
{
  return isChainActive(micros());
}

bool ArduinoTapTempo::isChainActive(unsigned long ms)
{
  return lastTapMS + maxBeatLengthMS > ms && lastTapMS + (beatLengthMS * beatsUntilChainReset) > ms;
}

double ArduinoTapTempo::beatProgress()
{
  return fmod((double)microsSinceReset / (double)beatLengthMS, 1.0);
}

void ArduinoTapTempo::update(bool buttonDown)
{
  unsigned long ms = micros();

  // if a tap has occured...
  if(buttonDown && !buttonDownOld)
    tap(ms);

  buttonDownOld = buttonDown;
  microsSinceResetOld = microsSinceReset;
  microsSinceReset = ms - lastResetMS;
}

void ArduinoTapTempo::tap(unsigned long ms)
{
  // start a new tap chain if last tap was over an amount of beats ago
  if(!isChainActive(ms))
    resetTapChain(ms);

  addTapToChain(ms);
}

void ArduinoTapTempo::addTapToChain(unsigned long ms)
{

  unsigned long duration = 0;
  // get time since last tap
  //rollover check
  if (ms > lastTapMS){
    duration = ms - lastTapMS;
  } else {
    duration = microsRollover - lastResetMS + 1 + ms;
  }

  // reset beat to occur right now
  lastTapMS = ms;

  tapsInChain++;
  if(tapsInChain == 1)
    return;
  
  // detect if last duration was approximately twice the length of the current beat length
  // and if so then we've simply missed a beat and can halve the duration to get the real beat length
  if(skippedTapDetection
     && tapsInChain > 2
     && !lastTapSkipped
     && duration > beatLengthMS * skippedTapThresholdLow
     && duration < beatLengthMS * skippedTapThresholdHigh)
  {
    duration = duration >> 1;
    lastTapSkipped = true;
  }
  else
  {
    lastTapSkipped = false;
  }
  
  tapDurations[tapDurationIndex] = duration;
  tapDurationIndex++;
  if(tapDurationIndex == totalTapValues) {
    tapDurationIndex = 0;
  }
  
  beatLengthMS = getAverageTapDuration();
  setBPM(beatLengthMS);
}

void ArduinoTapTempo::resetTapChain()
{
  resetTapChain(micros());
}

void ArduinoTapTempo::resetTapChain(unsigned long ms)
{
  tapsInChain = 0;
  tapDurationIndex = 0;
  lastResetMS = ms;
  for(int i = 0; i < totalTapValues; i++) {
    tapDurations[i] = 0;
  }
}

unsigned long ArduinoTapTempo::getAverageTapDuration()
{
  int amount = tapsInChain - 1;
  if(amount > totalTapValues)
    amount = totalTapValues;
  
  unsigned long runningTotalMS = 0;
  for(int i = 0; i < amount; i++) {
    runningTotalMS += tapDurations[i];
  }
  unsigned long avgTapDurationMS = runningTotalMS / amount;
  if(avgTapDurationMS < minBeatLengthMS) {
    return minBeatLengthMS;
  }
  return avgTapDurationMS;
}

void ArduinoTapTempo::setBeatsUntilChainReset(int beats)
{
  if(beats < 2)
  {
    beats = 2;
  }
  beatsUntilChainReset = beats;
}

void ArduinoTapTempo::setTotalTapValues(int total)
{
  if(total < 2)
  {
    total = 2;
  }
  else if(total > ArduinoTapTempo::MAX_TAP_VALUES)
  {
    total = ArduinoTapTempo::MAX_TAP_VALUES;
  }
  totalTapValues = total;
}

// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "UnrealVision.h"
#include <chrono>
/**
 *
 */
class UNREALVISION_API StopTime
{
protected:
  const std::chrono::high_resolution_clock::time_point StartTime;

public:
  StopTime() : StartTime(std::chrono::high_resolution_clock::now())
  {
  }

  virtual ~StopTime()
  {
  }

  inline double GetTimePassed() const
  {
    return std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - StartTime).count() * 1000.0;
  }
};

class UNREALVISION_API ScopeTime : private StopTime
{
private:
  const char *Function;
  const char *Message;
  const int Line;

public:
  inline ScopeTime(const char *_Function, const int _Line, const char *_Message) :
    StopTime(), Function(_Function), Message(_Message), Line(_Line)
  {
  }

  virtual inline ~ScopeTime()
  {
    OUT_AUX(FG_GREEN, NO_COLOR, "%s: " FG_CYAN "%f ms.", Function, Line, Message, GetTimePassed());
  }
};

#ifndef MEASURE_TIME
#define MEASURE_TIME(MSG) ScopeTime scopeTime(__PRETTY_FUNCTION__, __LINE__, MSG)
#endif

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "Engine.h"

class FUnrealVisionModule : public IModuleInterface
{
public:

  /** IModuleInterface implementation */
  virtual void StartupModule() override;
  virtual void ShutdownModule() override;
};

#include <string>

DECLARE_LOG_CATEGORY_EXTERN(UnrealVisionLog, Log, All);

#define OUT_AUX(LEVEL, MSG, ...) UE_LOG(UnrealVisionLog, LEVEL, TEXT("[%s][%d] " MSG), ##__VA_ARGS__)

#define OUT_INFO(MSG, ...)  OUT_AUX(Display, MSG, ANSI_TO_TCHAR(__FUNCTION__), __LINE__, ##__VA_ARGS__)
#define OUT_WARN(MSG, ...)  OUT_AUX(Warning, MSG, ANSI_TO_TCHAR(__FUNCTION__), __LINE__, ##__VA_ARGS__)
#define OUT_ERROR(MSG, ...) OUT_AUX(Error, MSG, ANSI_TO_TCHAR(__FUNCTION__), __LINE__, ##__VA_ARGS__)


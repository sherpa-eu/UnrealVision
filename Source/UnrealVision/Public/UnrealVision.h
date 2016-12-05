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

#define NO_COLOR   "\033[0m"
#define FG_BLACK   "\033[30m"
#define FG_RED     "\033[31m"
#define FG_GREEN   "\033[32m"
#define FG_YELLOW  "\033[33m"
#define FG_BLUE    "\033[34m"
#define FG_MAGENTA "\033[35m"
#define FG_CYAN    "\033[36m"

//#define OUT_AUX(FUNC_COLOR, MSG_COLOR, LEVEL, MSG, ...) UE_LOG(UnrealVisionLog, LEVEL, TEXT(FUNC_COLOR "[%s]"  FG_CYAN "[%d] " MSG_COLOR MSG NO_COLOR), ANSI_TO_TCHAR(__PRETTY_FUNCTION__), __LINE__, ##__VA_ARGS__)
//#define OUT_DEBUG(MSG, ...) OUT_AUX(FG_BLUE, NO_COLOR, Display, MSG, ##__VA_ARGS__)
//#define OUT_INFO(MSG, ...)  OUT_AUX(FG_GREEN, NO_COLOR, Display, MSG, ##__VA_ARGS__)
//#define OUT_WARN(MSG, ...)  OUT_AUX(FG_YELLOW, FG_YELLOW, Warning, MSG, ##__VA_ARGS__)
//#define OUT_ERROR(MSG, ...) OUT_AUX(FG_RED, FG_RED, Error, MSG, ##__VA_ARGS__)

#define OUT_AUX(FUNC_COLOR, MSG_COLOR, MSG, ...) printf(FUNC_COLOR "[%s]"  FG_CYAN "[%d] " MSG_COLOR MSG NO_COLOR "\n", ##__VA_ARGS__)

#define OUT_DEBUG(MSG, ...) OUT_AUX(FG_BLUE, NO_COLOR, MSG, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OUT_INFO(MSG, ...)  OUT_AUX(FG_GREEN, NO_COLOR, MSG, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OUT_WARN(MSG, ...)  OUT_AUX(FG_YELLOW, FG_YELLOW, MSG, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OUT_ERROR(MSG, ...) OUT_AUX(FG_RED, FG_RED, MSG, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)


// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealVision.h"
#include "UnrealVisionPrivatePCH.h"

#define LOCTEXT_NAMESPACE "FUnrealVisionModule"

void FUnrealVisionModule::StartupModule()
{
  // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FUnrealVisionModule::ShutdownModule()
{
  // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
  // we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

DEFINE_LOG_CATEGORY(UnrealVisionLog);

IMPLEMENT_MODULE(FUnrealVisionModule, UnrealVision)

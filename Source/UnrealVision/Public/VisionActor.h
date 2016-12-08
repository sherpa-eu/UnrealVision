// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GameFramework/Actor.h"
#include "Camera/CameraActor.h"
#include "StaticMeshResources.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "VisionActor.generated.h"

UCLASS()
class UNREALVISION_API AVisionActor : public ACameraActor
{
  GENERATED_BODY()

public:
  // Sets default values for this actor's properties
  AVisionActor();

  // Sets default values for this actor's properties
  virtual ~AVisionActor();

  // Called when the game starts or when spawned
  virtual void BeginPlay() override;
  virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

  // Called every frame
  virtual void Tick(float DeltaSeconds) override;

  void SetFramerate(const float _Framerate);
  void Pause(const bool _Pause = true);
  bool IsPaused() const;

  UPROPERTY(EditAnywhere, Category = "RGB-D Settings")
  uint32 Width;
  UPROPERTY(EditAnywhere, Category = "RGB-D Settings")
  uint32 Height;
  UPROPERTY(EditAnywhere, Category = "RGB-D Settings")
  float Framerate;
  UPROPERTY(EditAnywhere, Category = "RGB-D Settings")
  float FieldOfView;
  UPROPERTY(EditAnywhere, Category = "RGB-D Settings")
  int32 ServerPort;

private:
  class PrivateData;
  PrivateData *Priv;

  USceneCaptureComponent2D *Color;
  USceneCaptureComponent2D *Depth;
  USceneCaptureComponent2D *Object;
  UMaterialInstanceDynamic *MaterialDepthInstance;

  float FrameTime, TimePassed;
  TArray<FFloat16Color> ImageColor, ImageDepth, ImageObject;
  TArray<uint8> DataColor, DataDepth, DataObject;
  TArray<FColor> ObjectColors;
  TMap<FString, uint32> ObjectToColor;
  uint32 ColorsUsed;
  bool Running, Paused;

  void ShowFlagsBasicSetting(FEngineShowFlags &ShowFlags) const;
  void ShowFlagsLit(FEngineShowFlags &ShowFlags) const;
  void ShowFlagsPostProcess(FEngineShowFlags &ShowFlags) const;
  void ShowFlagsVertexColor(FEngineShowFlags &ShowFlags) const;
  void ReadImage(UTextureRenderTarget2D *RenderTarget, TArray<FFloat16Color> &ImageData) const;
  void ToColorImage(const TArray<FFloat16Color> &ImageData, uint8 *Bytes) const;
  void ToDepthImage(const TArray<FFloat16Color> &ImageData, uint8 *Bytes) const;
  void StoreImage(const uint8 *ImageData, const uint32 Size, const char *Name) const;
  void GenerateColors(const uint32_t NumberOfColors);
  bool ColorObject(AActor *Actor, const FString &name);
  bool ColorAllObjects();
  void ProcessColor();
  void ProcessDepth();
  void ProcessObject();
};

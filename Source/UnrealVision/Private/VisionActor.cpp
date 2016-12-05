// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealVision.h"
#include "VisionActor.h"
#include "StopTime.h"
#include "Server.h"
#include "PacketBuffer.h"
#include <fstream>
#include <sstream>

class UNREALVISION_API AVisionActor::PrivateData
{
public:
  TSharedPtr<PacketBuffer> Buffer;
  TCPServer Server;
  std::mutex WaitColor, WaitDepth, WaitObject;
  std::mutex DoneColor, DoneObject;
  std::thread ThreadColor, ThreadDepth, ThreadObject;
};

// Sets default values
AVisionActor::AVisionActor() : ACameraActor(), Width(960), Height(540), Framerate(1), FieldOfView(90.0), MaxDepth(10000.0f), ServerPort(10000), FrameTime(1.0f / Framerate), TimePassed(0), ColorsUsed(0)
{
  Priv = new PrivateData();
  // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.

  PrimaryActorTick.bCanEverTick = true;
  ImageColor.AddUninitialized(Width * Height);
  ImageDepth.AddUninitialized(Width * Height);
  ImageObject.AddUninitialized(Width * Height);

  OUT_INFO("Creating color camera.");
  Color = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ColorCapture"));
  Color->SetupAttachment(RootComponent);
  Color->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
  Color->TextureTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("ColorTarget"));
  Color->TextureTarget->InitAutoFormat(Width, Height);
  Color->FOVAngle = FieldOfView;
  //Color->TextureTarget->TargetGamma = GEngine->GetDisplayGamma();

  OUT_INFO("Creating depth camera.");
  Depth = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("DepthCapture"));
  Depth->SetupAttachment(RootComponent);
  Depth->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
  Depth->TextureTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("DepthTarget"));
  Depth->TextureTarget->InitAutoFormat(Width, Height);
  Depth->FOVAngle = FieldOfView;

  OUT_INFO("Creating object camera.");
  Object = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ObjectCapture"));
  Object->SetupAttachment(RootComponent);
  Object->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
  Object->TextureTarget = CreateDefaultSubobject<UTextureRenderTarget2D>(TEXT("ObjectTarget"));
  Object->TextureTarget->InitAutoFormat(Width, Height);
  Object->FOVAngle = FieldOfView;

  GetCameraComponent()->FieldOfView = FieldOfView;
  GetCameraComponent()->AspectRatio = Width / (float)Height;

  ShowFlagsLit(Color->ShowFlags);
  ShowFlagsPostProcess(Depth->ShowFlags);
  ShowFlagsVertexColor(Object->ShowFlags);

  OUT_INFO("Loading materials.");
  ConstructorHelpers::FObjectFinder<UMaterial> MaterialDepthFinder(TEXT("Material'/UnrealVision/SceneDepth.SceneDepth'"));
  if(MaterialDepthFinder.Object != nullptr)
  {
    MaterialDepthInstance = UMaterialInstanceDynamic::Create(MaterialDepthFinder.Object, Depth);
    if(MaterialDepthInstance != nullptr)
    {
      MaterialDepthInstance->SetScalarParameterValue(FName(TEXT("MaxDepth")), MaxDepth);
      Depth->PostProcessSettings.AddBlendable(MaterialDepthInstance, 1);
    }
  }
  else
    OUT_ERROR("Could not load material for depth.");

  Priv->Buffer = TSharedPtr<PacketBuffer>(new PacketBuffer(Width, Height, FieldOfView));
  Priv->Server.Buffer = Priv->Buffer;
}

AVisionActor::~AVisionActor()
{
  delete Priv;
  OUT_INFO("VisionActor got destroyed!");
}

// Called when the game starts or when spawned
void AVisionActor::BeginPlay()
{
  Super::BeginPlay();
  OUT_INFO("Begin play!");

  Priv->Server.Start(ServerPort);

  ColorAllObjects();

  Running = true;

  Priv->DoneColor.lock();
  Priv->DoneObject.lock();

  Priv->WaitColor.lock();
  Priv->WaitDepth.lock();
  Priv->WaitObject.lock();

  Priv->ThreadColor = std::thread(&AVisionActor::ProcessColor, this);
  Priv->ThreadDepth = std::thread(&AVisionActor::ProcessDepth, this);
  Priv->ThreadObject = std::thread(&AVisionActor::ProcessObject, this);
}

// Called when the game starts or when spawned
void AVisionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
  Super::EndPlay(EndPlayReason);
  OUT_INFO("End play!");

  Running = false;

  Priv->WaitColor.unlock();
  Priv->WaitDepth.unlock();
  Priv->WaitObject.unlock();

  Priv->ThreadColor.join();
  Priv->ThreadDepth.join();
  Priv->ThreadObject.join();

  Priv->DoneColor.unlock();
  Priv->DoneObject.unlock();

  Priv->Server.Stop();
}

// Called every frame
void AVisionActor::Tick(float DeltaTime)
{
  Super::Tick(DeltaTime);

  TimePassed += DeltaTime;
  if(TimePassed < FrameTime)
  {
    return;
  }
  TimePassed -= FrameTime;
  MEASURE_TIME("Tick");

  UpdateComponentTransforms();

  if(!Priv->Server.HasClient())
  {
    return;
  }

  FDateTime Now = FDateTime::UtcNow();
  Priv->Buffer->HeaderWrite->TimestampCapture = Now.ToUnixTimestamp() * 1000000000 + Now.GetMillisecond() * 1000000;

  FVector Translation = GetActorLocation();
  FQuat Rotation = GetActorQuat();
  // Convert to meters
  Priv->Buffer->HeaderWrite->Translation.X = Translation.X / 100.0f;
  Priv->Buffer->HeaderWrite->Translation.Y = -Translation.Y / 100.0f;
  Priv->Buffer->HeaderWrite->Translation.Z = Translation.Z / 100.0f;
  Priv->Buffer->HeaderWrite->Rotation.X = -Rotation.X;
  Priv->Buffer->HeaderWrite->Rotation.Y = Rotation.Y;
  Priv->Buffer->HeaderWrite->Rotation.Z = -Rotation.Z;
  Priv->Buffer->HeaderWrite->Rotation.W = Rotation.W;

  Priv->Buffer->StartWriting(ObjectToColor, ObjectColors);

  StopTime Timer;

  double t1 = Timer.GetTimePassed();
  ReadImage(Color->TextureTarget, ImageColor);
  double t2 = Timer.GetTimePassed();
  Priv->WaitColor.unlock();
  double t3 = Timer.GetTimePassed();
  ReadImage(Object->TextureTarget, ImageObject);
  double t4 = Timer.GetTimePassed();
  Priv->WaitObject.unlock();
  double t5 = Timer.GetTimePassed();
  ReadImage(Depth->TextureTarget, ImageDepth);
  double t6 = Timer.GetTimePassed();
  Priv->WaitDepth.unlock();
  double t7 = Timer.GetTimePassed();

  OUT_INFO("ReadImage(Color->TextureTarget, ImageColor): %f", t2 - t1);
  OUT_INFO("WaitColor.unlock(): %f", t3 - t2);
  OUT_INFO("ReadImage(Object->TextureTarget, ImageObject): %f", t4 - t3);
  OUT_INFO("WaitObject.unlock(): %f", t5 - t4);
  OUT_INFO("ReadImage(Depth->TextureTarget, ImageDepth): %f", t6 - t5);
  OUT_INFO("WaitDepth.unlock(): %f", t7 - t6);
}

void AVisionActor::ShowFlagsBasicSetting(FEngineShowFlags &ShowFlags) const
{
  ShowFlags = FEngineShowFlags(EShowFlagInitMode::ESFIM_All0);
  ShowFlags.SetRendering(true);
  ShowFlags.SetStaticMeshes(true);
  ShowFlags.SetLandscape(true);
  ShowFlags.SetInstancedFoliage(true);
  ShowFlags.SetInstancedGrass(true);
  ShowFlags.SetInstancedStaticMeshes(true);
}

void AVisionActor::ShowFlagsLit(FEngineShowFlags &ShowFlags) const
{
  ShowFlagsBasicSetting(ShowFlags);
  ShowFlags = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
  ApplyViewMode(VMI_Lit, true, ShowFlags);
  ShowFlags.SetMaterials(true);
  ShowFlags.SetLighting(true);
  ShowFlags.SetPostProcessing(true);
  // ToneMapper needs to be enabled, otherwise the screen will be very dark
  ShowFlags.SetTonemapper(true);
  // TemporalAA needs to be disabled, otherwise the previous frame might contaminate current frame.
  // Check: https://answers.unrealengine.com/questions/436060/low-quality-screenshot-after-setting-the-actor-pos.html for detail
  ShowFlags.SetTemporalAA(false);
  ShowFlags.SetAntiAliasing(true);
  ShowFlags.SetEyeAdaptation(false); // Eye adaption is a slow temporal procedure, not useful for image capture
}

void AVisionActor::ShowFlagsPostProcess(FEngineShowFlags &ShowFlags) const
{
  ShowFlagsBasicSetting(ShowFlags);
  ShowFlags.SetPostProcessing(true);
  ShowFlags.SetPostProcessMaterial(true);

  GVertexColorViewMode = EVertexColorViewMode::Color;
}

void AVisionActor::ShowFlagsVertexColor(FEngineShowFlags &ShowFlags) const
{
  ShowFlagsLit(ShowFlags);
  ApplyViewMode(VMI_Lit, true, ShowFlags);

  // From MeshPaintEdMode.cpp:2942
  ShowFlags.SetMaterials(false);
  ShowFlags.SetLighting(false);
  ShowFlags.SetBSPTriangles(true);
  ShowFlags.SetVertexColors(true);
  ShowFlags.SetPostProcessing(false);
  ShowFlags.SetHMDDistortion(false);
  ShowFlags.SetTonemapper(false); // This won't take effect here

  GVertexColorViewMode = EVertexColorViewMode::Color;
}

void AVisionActor::ReadImage(UTextureRenderTarget2D *RenderTarget, TArray<FFloat16Color> &ImageData) const
{
  //MEASURE_TIME("Reading float image from buffer");
  FTextureRenderTargetResource *RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
  RenderTargetResource->ReadFloat16Pixels(ImageData);
}

void AVisionActor::ToColorImage(const TArray<FFloat16Color> &ImageData, uint8 *Bytes) const
{
  //MEASURE_TIME("Convert FColor to color");
  const FFloat16Color *itI = ImageData.GetData();
  uint8 *itO = Bytes;

  for(size_t i = 0; i < ImageData.Num(); ++i, ++itI, ++itO)
  {
    const FFloat16Color &color = *itI;
    *itO = (float)color.B * 255.f;
    *++itO = (float)color.G * 255.f;
    *++itO = (float)color.R * 255.f;
  }
  return;
}

void AVisionActor::ToDepthImage(const TArray<FFloat16Color> &ImageData, uint8 *Bytes) const
{
  //MEASURE_TIME("Convert FFloat16Color to depth");
  const FFloat16Color *itI = ImageData.GetData();
  float *itO = reinterpret_cast<float *>(Bytes);
  const float ToMeters = MaxDepth * 0.01f;

  for(size_t i = 0; i < ImageData.Num(); ++i, ++itI, ++itO)
  {
    const FFloat16Color &color = *itI;
    *itO = (float)color.R * ToMeters;
  }
  return;
}

void AVisionActor::StoreImage(const uint8 *ImageData, const uint32 Size, const char *Name) const
{
  //MEASURE_TIME("Storing image to disk");
  std::ofstream File(Name, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
  File.write(reinterpret_cast<const char *>(ImageData), Size);
  File.close();
  return;
}

void AVisionActor::GenerateColors(const uint32_t NumberOfColors)
{
  const int32_t MaxHue = 50;
  const int32_t ShiftHue = 21;
  const float MinSat = 0.65;
  const float MinVal = 0.65;

  uint32_t HueCount = MaxHue;
  uint32_t SatCount = 1;
  uint32_t ValCount = 1;

  int32_t left = std::max<int32_t>(0, NumberOfColors - HueCount);
  while(left > 0)
  {
    if(left > 0)
    {
      ++ValCount;
      left = NumberOfColors - SatCount * ValCount * HueCount;
    }
    if(left > 0)
    {
      ++SatCount;
      left = NumberOfColors - SatCount * ValCount * HueCount;
    }
  }

  const float StepHue = 360.0f / HueCount;
  const float StepSat = (1.0f - MinSat) / std::max(1.0f, SatCount - 1.0f);
  const float StepVal = (1.0f - MinVal) / std::max(1.0f, ValCount - 1.0f);

  ObjectColors.Reserve(SatCount * ValCount * HueCount);
  OUT_INFO("Generating %d colors.", SatCount * ValCount * HueCount);

  FLinearColor HSVColor;
  for(uint32_t s = 0; s < SatCount; ++s)
  {
    HSVColor.G = 1.0f - s * StepSat;
    for(uint32_t v = 0; v < ValCount; ++v)
    {
      HSVColor.B = 1.0f - v * StepVal;
      for(uint32_t h = 0; h < HueCount; ++h)
      {
        HSVColor.R = ((h * ShiftHue) % MaxHue) * StepHue;
        ObjectColors.Add(HSVColor.HSVToLinearRGB().ToFColor(false));
        OUT_INFO("Added color %d: %d %d %d", ObjectColors.Num(), ObjectColors.Last().R, ObjectColors.Last().G, ObjectColors.Last().B);
      }
    }
  }
}

bool AVisionActor::ColorObject(AActor *Actor, const FString &name)
{
  const FColor &ObjectColor = ObjectColors[ObjectToColor[name]];
  TArray<UMeshComponent *> PaintableComponents;
  Actor->GetComponents<UMeshComponent>(PaintableComponents);

  for(auto MeshComponent : PaintableComponents)
  {
    if(MeshComponent == nullptr)
      continue;

    if(UStaticMeshComponent *StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
    {
      if(UStaticMesh *StaticMesh = StaticMeshComponent->GetStaticMesh())
      {
        uint32 PaintingMeshLODIndex = 0;
        uint32 NumLODLevel = StaticMesh->RenderData->LODResources.Num();
        check(NumLODLevel == 1);
        FStaticMeshLODResources &LODModel = StaticMesh->RenderData->LODResources[PaintingMeshLODIndex];
        FStaticMeshComponentLODInfo *InstanceMeshLODInfo = NULL;

        // PaintingMeshLODIndex + 1 is the minimum requirement, enlarge if not satisfied
        StaticMeshComponent->SetLODDataCount(PaintingMeshLODIndex + 1, StaticMeshComponent->LODData.Num());
        InstanceMeshLODInfo = &StaticMeshComponent->LODData[PaintingMeshLODIndex];

        {
          InstanceMeshLODInfo->OverrideVertexColors = new FColorVertexBuffer;

          FColor FillColor = FColor(255, 255, 255, 255);
          InstanceMeshLODInfo->OverrideVertexColors->InitFromSingleColor(FColor::White, LODModel.GetNumVertices());
        }

        uint32 NumVertices = LODModel.GetNumVertices();
        check(InstanceMeshLODInfo->OverrideVertexColors);
        check(NumVertices <= InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices());

        for(uint32 ColorIndex = 0; ColorIndex < NumVertices; ++ColorIndex)
        {
          uint32 NumOverrideVertexColors = InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices();
          uint32 NumPaintedVertices = InstanceMeshLODInfo->PaintedVertices.Num();
          InstanceMeshLODInfo->OverrideVertexColors->VertexColor(ColorIndex) = ObjectColor;
        }
        BeginInitResource(InstanceMeshLODInfo->OverrideVertexColors);
        StaticMeshComponent->MarkRenderStateDirty();
      }
    }
  }
  return true;
}

bool AVisionActor::ColorAllObjects()
{
  ULevel *Level = GWorld->GetFirstPlayerController()->GetPawn()->GetLevel();
  if(Level == nullptr)
  {
    OUT_ERROR("Could not get level.");
    return false;
  }

  uint32_t NumberOfActors = 0;
  for(auto Actor : Level->Actors)
  {
    if(Actor && Actor->IsA(AStaticMeshActor::StaticClass()))
    {
      ++NumberOfActors;
    }
  }

  OUT_INFO("Found %d Actors.", NumberOfActors);
  GenerateColors(NumberOfActors * 2);

  for(auto Actor : Level->Actors)
  {
    if(Actor && Actor->IsA(AStaticMeshActor::StaticClass()))
    {
      FString ActorLabel = Actor->GetHumanReadableName();
      if(!ObjectToColor.Contains(ActorLabel))
      {
        check(ColorsUsed < ObjectColors.Num());
        ObjectToColor.Add(ActorLabel, ColorsUsed);
        OUT_INFO("Adding color %d for object %s.", ColorsUsed, TCHAR_TO_ANSI(*ActorLabel));

        ++ColorsUsed;
      }

      OUT_INFO("Coloring object %s.", TCHAR_TO_ANSI(*ActorLabel));
      ColorObject(Actor, ActorLabel);
    }
  }
  return true;
}

void AVisionActor::ProcessColor()
{
  while(true)
  {
    Priv->WaitColor.lock();
    if(!this->Running) break;
    MEASURE_TIME("Color processing");
    ToColorImage(ImageColor, Priv->Buffer->Color);
    Priv->DoneColor.unlock();
  }
}

void AVisionActor::ProcessDepth()
{
  while(true)
  {
    Priv->WaitDepth.lock();
    if(!this->Running) break;
    {
      MEASURE_TIME("Depth processing");
      ToDepthImage(ImageDepth, Priv->Buffer->Depth);
    }
    Priv->DoneColor.lock();
    Priv->DoneObject.lock();

    Priv->Buffer->DoneWriting();
  }
}

void AVisionActor::ProcessObject()
{
  while(true)
  {
    Priv->WaitObject.lock();
    if(!this->Running) break;
    MEASURE_TIME("Object processing");
    ToColorImage(ImageObject, Priv->Buffer->Object);
    Priv->DoneObject.unlock();
  }
}

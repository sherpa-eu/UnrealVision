// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <mutex>
#include <vector>

/**
 *
 */
class UNREALVISION_API PacketBuffer
{
public:
  struct Vector
  {
    float X;
    float Y;
    float Z;
  };

  struct Quaternion
  {
    float X;
    float Y;
    float Z;
    float W;
  };

  struct PacketHeader
  {
    uint32_t Size;
    uint32_t SizeHeader;
    uint32_t MapEntries;
    uint32_t Width;
    uint32_t Height;
    uint64_t TimestampCapture;
    uint64_t TimestampSent;
    float FieldOfViewX;
    float FieldOfViewY;
    Vector Translation;
    Quaternion Rotation;
  };

  struct MapEntry
  {
    uint32_t Size;
    uint8_t R;
    uint8_t G;
    uint8_t B;
    char FirstChar;
  };


private:
  std::vector<uint8> ReadBuffer, WriteBuffer;
  bool IsDataReadable;
  std::mutex LockBuffer, LockRead;

public:
  const uint32 SizeHeader, SizeRGB, SizeFloat;
  const uint32 OffsetColor, OffsetDepth, OffsetObject, OffsetMap;
  const uint32 Size;
  uint8 *Color, *Depth, *Object, *Map, *Read;
  PacketHeader *HeaderWrite, *HeaderRead;

  PacketBuffer(const uint32 Width, const uint32 Height, const float FieldOfView);

  void StartWriting(const TMap<FString, uint32> &ObjectToColor, const TArray<FColor> &ObjectColors);

  void DoneWriting();

  void StartReading();

  void DoneReading();

  void Release();
};

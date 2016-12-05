// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealVision.h"
#include "Server.h"
#include "StopTime.h"

TCPServer::TCPServer() : Running(false)
{
}

TCPServer::~TCPServer()
{
  if(Running)
  {
    Stop();
  }
}

void TCPServer::Start(const int32 ServerPort)
{
  OUT_INFO(TEXT("Starting server."));

  if(!Buffer.IsValid())
  {
    OUT_ERROR(TEXT("No package buffer set."));
    return;
  }

  bool bCanBind = false;
  TSharedRef<FInternetAddr> LocalIP = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBind);
  LocalIP->SetPort(ServerPort);
  OUT_INFO(TEXT("Server address: %s"), *LocalIP->ToString(true));

  ListenSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("Server Listening Socket"), false);
  ListenSocket->SetReuseAddr(true);
  ListenSocket->Bind(*LocalIP);
  ListenSocket->Listen(8);

  if(!ListenSocket)
  {
    OUT_ERROR(TEXT("Could not create socket."));
  }
  else
  {
    OUT_INFO(TEXT("Socket created"));
  }

  Running = true;
  Thread = std::thread(&TCPServer::ServerLoop, this);
}

void TCPServer::Stop()
{
  if(Running)
  {
    Running = false;
    Buffer->Release();
    Thread.join();
    Buffer->DoneReading();
  }

  if(ClientSocket)
  {
    ClientSocket->Close();
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
    ClientSocket = nullptr;
  }

  if(ListenSocket)
  {
    ListenSocket->Close();
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
    ListenSocket = nullptr;
  }

  OUT_INFO(TEXT("Server stopped."));
}

// Called every frame
void TCPServer::ServerLoop()
{
  while(Running)
  {
    if(!ClientSocket && !ListenConnections())
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }

    if(ClientSocket->GetConnectionState() != ESocketConnectionState::SCS_Connected)
    {
      OUT_WARN(TEXT("Client disconnected"));
      ClientSocket->Close();
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
      ClientSocket = nullptr;
      continue;
    }

    Buffer->StartReading();
    if(!Running)
    {
		Buffer->DoneReading();
      break;
    }

    MEASURE_TIME("Transmitting data");
    int32 BytesSent = 0;
    OUT_INFO(TEXT("sending images."));

    FDateTime Now = FDateTime::UtcNow();
    Buffer->HeaderRead->TimestampSent = Now.ToUnixTimestamp() * 1000000000 + Now.GetMillisecond() * 1000000;
    if(!ClientSocket->Send(Buffer->Read, Buffer->HeaderRead->Size, BytesSent) || BytesSent != Buffer->HeaderRead->Size)
    {
      OUT_WARN(TEXT("Not all bytes sent. Client disconnected."));
      ClientSocket->Close();
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
      ClientSocket = nullptr;
    }
    Buffer->DoneReading();
  }
}

bool TCPServer::ListenConnections()
{
  if(!ListenSocket)
  {
    OUT_ERROR(TEXT("No socket for listening."));
    return false;
  }

  //Remote address
  TSharedRef<FInternetAddr> RemoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
  bool Pending = false;

  // handle incoming connections
  if(ListenSocket->HasPendingConnection(Pending) && Pending)
  {
    OUT_INFO(TEXT("New connection."));

    // Destroy previous connection if available
    if(ClientSocket)
    {
      ClientSocket->Close();
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
      ClientSocket = nullptr;
    }

    // Set new connection
    ClientSocket = ListenSocket->Accept(*RemoteAddress, TEXT("Received socket connection"));

    if(ClientSocket)
    {
      OUT_INFO(TEXT("Client connected: %s"), *RemoteAddress->ToString(true));
      if(Buffer.IsValid())
      {
        int32 NewSize = 0;
        ClientSocket->SetSendBufferSize(Buffer->Size, NewSize);
        if(NewSize < (int32)Buffer->Size)
        {
          OUT_WARN(TEXT("Could not set socket buffer size. New size: %d"), NewSize);
        }
      }
      return true;
    }
  }
  return false;
}

bool TCPServer::HasClient() const
{
  return ClientSocket != nullptr;
}

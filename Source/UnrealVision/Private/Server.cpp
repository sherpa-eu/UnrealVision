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
  OUT_INFO("Starting server.");

  if(!Buffer.IsValid())
  {
    OUT_ERROR("No package buffer set.");
    return;
  }

  bool bCanBind = false;
  TSharedRef<FInternetAddr> LocalIP = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBind);
  LocalIP->SetPort(ServerPort);
  OUT_INFO("Server address: %s", TCHAR_TO_ANSI(*LocalIP->ToString(true)));

  ListenSocket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("Server Listening Socket"), false);
  ListenSocket->SetReuseAddr(true);
  ListenSocket->Bind(*LocalIP);
  ListenSocket->Listen(8);

  if(!ListenSocket)
  {
    OUT_ERROR("Could not create socket.");
  }
  else
  {
    OUT_INFO("Socket created");
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

  OUT_INFO("Server stopped.");
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
      OUT_WARN("Client disconnected");
      ClientSocket->Close();
      ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
      ClientSocket = nullptr;
      continue;
    }

    Buffer->StartReading();
    if(!Running)
    {
      break;
    }

    //MEASURE_TIME("Transmitting data");
    int32 BytesSent = 0;
    //OUT_INFO("sending images.");

    FDateTime Now = FDateTime::UtcNow();
    Buffer->HeaderRead->TimestampSent = Now.ToUnixTimestamp() * 1000000000 + Now.GetMillisecond() * 1000000;
    if(!ClientSocket->Send(Buffer->Read, Buffer->HeaderRead->Size, BytesSent) || BytesSent != Buffer->HeaderRead->Size)
    {
      OUT_WARN("Not all bytes sent. Client disconnected.");
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
    OUT_ERROR("No socket for listening.");
    return false;
  }

  //Remote address
  TSharedRef<FInternetAddr> RemoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
  bool Pending = false;

  // handle incoming connections
  if(ListenSocket->HasPendingConnection(Pending) && Pending)
  {
    OUT_INFO("New connection.");

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
      OUT_INFO("Client connected: %s", TCHAR_TO_ANSI(*RemoteAddress->ToString(true)));
      if(Buffer.IsValid())
      {
        int32 NewSize = 0;
        ClientSocket->SetSendBufferSize(Buffer->Size, NewSize);
        if(NewSize < Buffer->Size)
        {
          OUT_WARN("Could not set socket buffer size. New size: %d", NewSize);
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

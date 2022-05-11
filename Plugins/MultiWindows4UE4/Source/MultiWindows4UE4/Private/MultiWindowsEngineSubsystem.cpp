/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#include "MultiWindowsEngineSubsystem.h"

DEFINE_LOG_CATEGORY(LogMultiWindowsEngineSubsystem);

UMultiWindowsEngineSubsystem::UMultiWindowsEngineSubsystem()
	: UEngineSubsystem()
{

}

void UMultiWindowsEngineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogMultiWindowsEngineSubsystem, Display, TEXT("UMultiWindowsEngineSubsystem::Initialize()"));
}

void UMultiWindowsEngineSubsystem::Deinitialize()
{
	UE_LOG(LogMultiWindowsEngineSubsystem, Display, TEXT("UMultiWindowsEngineSubsystem::Deinitialize()"));
}

void UMultiWindowsEngineSubsystem::RedrawViewports(bool bShouldPresent)
{
	UE_LOG(LogMultiWindowsEngineSubsystem, Display, TEXT("UMultiWindowsEngineSubsystem::RedrawViewports()"));
}
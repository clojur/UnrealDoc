/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#include "MultiWindows4UE4.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericApplication.h"
#include "MultiWindowsManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "FMultiWindows4UE4Module"

void FMultiWindows4UE4Module::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FMultiWindows4UE4Module::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FMultiWindows4UE4Module::GetMultiWindowsManager(UMultiWindowsManager*& MultiWindowsManager)
{
	MultiWindowsManager = nullptr;
	if (!GEngine)
	{
		return;
	}
	FUObjectItem* ObjectItem = GUObjectArray.IndexToObject(MultiWindowsManagerObjectIndex);

	if (ObjectItem && ObjectItem->Object)
	{
		UObject* Object = static_cast<UObject*>(ObjectItem->Object);
		MultiWindowsManager = Cast<UMultiWindowsManager>(Object);
	}

	if (!MultiWindowsManager || UMultiWindowsManager::StaticClass() != MultiWindowsManager->StaticClass())
	{
		MultiWindowsManager = NewObject<UMultiWindowsManager>(UMultiWindowsManager::StaticClass());
		MultiWindowsManager->AddToRoot();
		MultiWindowsManagerObjectIndex = GUObjectArray.ObjectToIndex(MultiWindowsManager);
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FMultiWindows4UE4Module, MultiWindows4UE4)
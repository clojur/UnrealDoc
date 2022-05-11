/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#include "MultiWindowsUnrealEdEngine.h"
#include "MultiWindows4UE4.h"
#include "MultiWindowsManager.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogMultiWindowsUnrealEdEngine, Log, All);

UMultiWindowsUnrealEdEngine::~UMultiWindowsUnrealEdEngine()
{

}

void UMultiWindowsUnrealEdEngine::Init(IEngineLoop* InEngineLoop)
{
	Super::Init(InEngineLoop);
}

void UMultiWindowsUnrealEdEngine::PreExit()
{
	Super::PreExit();
}

void UMultiWindowsUnrealEdEngine::Tick(float DeltaSeconds, bool bIdleMode)
{
	if (!MultiWindowsManager)
	{
		FMultiWindows4UE4Module& MultiWindows4UE4Module = FModuleManager::LoadModuleChecked<FMultiWindows4UE4Module>("MultiWindows4UE4");
		MultiWindows4UE4Module.GetMultiWindowsManager(MultiWindowsManager);
	}

	// Close Window when EndPlayMap
	if (MultiWindowsManager && bRequestEndPlayMapQueued)
	{
		TArray<UWindow*> AncillaryWindows = MultiWindowsManager->AncillaryWindows;
		for (auto Window : AncillaryWindows)
		{
			if (!Window)
			{
				continue;
			}
			
			// The window may have already been destroyed in the case that the PIE window close box was pressed 
			if (Window->GameViewportClientWindow.IsValid())
			{
				// Destroy the SWindow
				FSlateApplication::Get().DestroyWindowImmediately(Window->GameViewportClientWindow.Pin().ToSharedRef());
			}
		}
	}

	if (MultiWindowsManager)
	{
		MultiWindowsManager->UpdateWorldContentBeforeTick(WorldList);
	}

	Super::Tick(DeltaSeconds, bIdleMode);

	if (MultiWindowsManager)
	{
		MultiWindowsManager->UpdateWorldContentAfterTick(WorldList);
	}

	if (MultiWindowsManager)
	{
		TArray<UWindow*> AncillaryWindows = MultiWindowsManager->AncillaryWindows;
		for (auto Window : AncillaryWindows)
		{
			if (!Window)
			{
				continue;
			}

			//bool IsValid = false;
			//for (int32 WorldIdx = WorldList.Num() - 1; WorldIdx >= 0; --WorldIdx)
			//{
			//	FWorldContext& ThisContext = WorldList[WorldIdx];
			//	if (ThisContext.GameViewport)
			//	{
			//		if (Window->GameViewportClient == ThisContext.GameViewport)
			//		{
			//			IsValid = true;
			//			break;
			//		}
			//	}
			//}
			//if (!IsValid)
			//{
			//	MultiWindowsManager->AncillaryWindows.Remove(Window);
			//  MultiWindowsManager->OnAncillaryWindowsArrayChanged.Broadcast(MultiWindowsManager);
			//}

			if (MultiWindowsManager->bAddedNewWindow && Window && Window->GameViewportClient)
			{
				// Do something After AddedNewWindow.
			}
		}
		MultiWindowsManager->bAddedNewWindow = false;
		
		return;
		for (auto Window : MultiWindowsManager->AncillaryWindows)
		{
			if (!Window)
			{
				continue;
			}
			if (Window->GameViewportClient)
			{
				// Render everything.
				Window->GameViewportClient->LayoutPlayers();
				if (Window->GameViewportClient->Viewport)
				{
					Window->GameViewportClient->Viewport->Draw();
				}
				// UE_LOG(LogMultiWindowsUnrealEdEngine, Log, TEXT("UMultiWindowsUnrealEdEngine->Tick()"));
			}
		}
	}
}


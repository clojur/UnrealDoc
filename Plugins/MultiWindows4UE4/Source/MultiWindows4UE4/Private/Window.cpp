/*
*  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
*  All rights reserved.
*  @ Date : 2020/01/26
*
*/

#include "Window.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SGameLayerManager.h"
#include "MultiWindows4UE4BPLibrary.h"

void UWindow::ClearViews()
{
	ViewManager.EnableMultiViews = true;
	ViewManager.Views.Empty();
}

void UWindow::SetAsCustomViewMode(const FViewManager& InViewManager)
{
	ViewManager = InViewManager;
}

void UWindow::SetAsNormalViewMode()
{
	ViewManager.EnableMultiViews = false;
}

void UWindow::ToggleNormalViewModeAndMultiViewsMode(const bool IsNormalViewMode)
{
	ViewManager.EnableMultiViews = !IsNormalViewMode;
}

void UWindow::IsNormalViewModeOrMultiViewsMode(bool& IsNormalViewMode)
{
	IsNormalViewMode = !ViewManager.EnableMultiViews;
}

void UWindow::GetWindowIndex(int32& WindowIndex, bool& IsValid)
{
	UMultiWindowsManager* MultiWindowsManager;
	UMultiWindows4UE4BPLibrary::GetMultiWindowsManager(MultiWindowsManager);

	IsValid = false;
	WindowIndex = INDEX_NONE;

	if (MultiWindowsManager)
	{
		WindowIndex = MultiWindowsManager->AncillaryWindows.Find(this);
		if (WindowIndex != INDEX_NONE)
		{
			IsValid = true;
		}
	}
}

void UWindow::ResizeWindow(int32 ResX, int32 ResY, EWindowMode::Type WindowMode)
{
	if (!GameViewportClient)
	{
		return;
	}

	// tell anyone listening about the change
	FCoreDelegates::OnSystemResolutionChanged.Broadcast(ResX, ResY);

	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportFrame)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s: Resizing viewport due to setres change, %d x %d\n"), *GetName(), ResX, ResY);
		GameViewportClient->ViewportFrame->ResizeFrame(ResX, ResY, WindowMode);
	}
}

void UWindow::SetWindowTitle(FText WindowTitle)
{
	if (GameViewportClientWindow.Pin().IsValid())
	{
		GameViewportClientWindow.Pin()->SetTitle(WindowTitle);
	}
}

void UWindow::SetWindowPosition(FVector2D NewPosition)
{
	if (GameViewportClientWindow.Pin().IsValid())
	{
		GameViewportClientWindow.Pin()->MoveWindowTo(NewPosition);
	}
}

void UWindow::GetWindowPosition(FVector2D& WindowPosition)
{
	if (GameViewportClientWindow.Pin().IsValid())
	{
		WindowPosition = GameViewportClientWindow.Pin()->GetPositionInScreen();
	}
}

void UWindow::OnGameWindowClosed(const TSharedRef<SWindow>& WindowBeingClosed)
{
	// FSlateApplication::Get().UnregisterGameViewport();
	// This will shutdown the game
	if (SceneViewport->GetViewport() && GameViewportClient->Viewport)
	{
		GameViewportClient->CloseRequested(SceneViewport->GetViewport());
	}
	else
	{
		// broadcast close request to anyone that registered an interest
		GameViewportClient->OnCloseRequested().Broadcast(SceneViewport->GetViewport());
		GameViewportClient->SetViewportFrame(NULL);
		TSharedPtr< IGameLayerManager > GameLayerManager(GameViewportClient->GetGameLayerManager());
		if (GameLayerManager.IsValid())
		{
			GameLayerManager->SetSceneViewport(nullptr);
		}
	}
	SceneViewport.Reset();

	TIndirectArray<FWorldContext>& WorldList = const_cast<TIndirectArray<FWorldContext>&>(GEngine->GetWorldContexts());
	for (int32 Index= 0; Index < WorldList.Num(); Index++)
	{
		FWorldContext& WorldContext = WorldList[Index];
		// For now, kill PIE session if any of the viewports are closed
		if (WorldContext.GameViewport != NULL && WorldContext.GameViewport == GameViewportClient)
		{
			WorldList.RemoveAt(Index);
		}
	}

	// Remove all UserWidgets in the viewport of this Window.
	TArray<UUserWidget*> UserWidgetsInViewportTemp = UserWidgetsInViewport;
	for (auto UserWidgetInViewport : UserWidgetsInViewportTemp)
	{
		if (UserWidgetInViewport)
		{
			TSharedRef<SWidget> UserSlateWidget = UserWidgetInViewport->TakeWidget();
			TSharedPtr<SWidget> WidgetHost = UserSlateWidget->GetParentWidget();
			if (WidgetHost.IsValid())
			{
				// If this is a game world add the widget to the current worlds viewport.
				UWorld* World = UserWidgetInViewport->GetWorld();
				if (World && World->IsGameWorld())
				{
					if (UGameViewportClient* ViewportClient = GameViewportClient)
					{
						TSharedRef<SWidget> WidgetHostRef = WidgetHost.ToSharedRef();

						ViewportClient->RemoveViewportWidgetContent(WidgetHostRef);

						if (ULocalPlayer* LocalPlayer = UserWidgetInViewport->GetOwningLocalPlayer())
						{
							ViewportClient->RemoveViewportWidgetForPlayer(LocalPlayer, WidgetHostRef);
						}
					}
				}
			}
			else
			{
				UserWidgetInViewport->RemoveFromParent();
			}

			UserWidgetsInViewport.Remove(UserWidgetInViewport);
		}
	}
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);

	UMultiWindowsManager* MultiWindowsManager;
	UMultiWindows4UE4BPLibrary::GetMultiWindowsManager(MultiWindowsManager);
	if (MultiWindowsManager)
	{
		MultiWindowsManager->AncillaryWindows.Remove(this);
		MultiWindowsManager->OnAncillaryWindowsArrayChanged.Broadcast(MultiWindowsManager);
	}
}

void UWindow::OnGameWindowMoved(const TSharedRef<SWindow>& WindowBeingMoved)
{
	const FSlateRect WindowRect = WindowBeingMoved->GetRectInScreen();
	if (!GEngine)
	{
		return;
	}
	/*GEngine->GetGameUserSettings()->SetWindowPosition(WindowRect.Left, WindowRect.Top);
	GEngine->GetGameUserSettings()->SaveConfig();*/
}

void UWindow::OnViewportResized(FViewport* Viewport, uint32 Unused)
{
	if (Viewport && Viewport == SceneViewport.Get() && GameViewportClientWindow.IsValid() && GameViewportClientWindow.Pin()->GetWindowMode() == EWindowMode::Windowed)
	{
		const FIntPoint ViewportSize = Viewport->GetSizeXY();
		if (ViewportSize.X > 0 && ViewportSize.Y > 0)
		{
			//GSystemResolution.ResX = ViewportSize.X;
			//GSystemResolution.ResY = ViewportSize.Y;
			//FSystemResolution::RequestResolutionChange(GSystemResolution.ResX, GSystemResolution.ResY, EWindowMode::Windowed);

			//UGameUserSettings* Settings = GetGameUserSettings();
			//Settings->SetScreenResolution(ViewportSize);
			//Settings->ConfirmVideoMode();
			//Settings->RequestUIUpdate();
		}
	}
}

void UWindow::OnLevelRemovedFromWorldAndRemoveWidgetsInViewport(ULevel* InLevel, UWorld* InWorld)
{
	// If the InLevel is null, it's a signal that the entire world is about to disappear, so
	// go ahead and remove this widget from the viewport, it could be holding onto too many
	// dangerous actor references that won't carry over into the next world.
	if (InLevel == nullptr)
	{
		TArray<UUserWidget*> UserWidgetsInViewportTemp = UserWidgetsInViewport;
		for (auto UserWidgetInViewport : UserWidgetsInViewportTemp)
		{
			if (UserWidgetInViewport && InWorld == UserWidgetInViewport->GetWorld())
			{
				TSharedRef<SWidget> UserSlateWidget = UserWidgetInViewport->TakeWidget();
				TSharedPtr<SWidget> WidgetHost = UserSlateWidget->GetParentWidget();

				// If this is a game world add the widget to the current worlds viewport.
				UWorld* World = UserWidgetInViewport->GetWorld();
				if (World && World->IsGameWorld())
				{
					if (GameViewportClient)
					{
						TSharedRef<SWidget> WidgetHostRef = WidgetHost.ToSharedRef();

						GameViewportClient->RemoveViewportWidgetContent(WidgetHostRef);

						if (ULocalPlayer* LocalPlayer = UserWidgetInViewport->GetOwningLocalPlayer())
						{
							GameViewportClient->RemoveViewportWidgetForPlayer(LocalPlayer, WidgetHostRef);
						}
					}
				}
				UserWidgetInViewport->RemoveFromParent();
				UserWidgetsInViewport.Remove(UserWidgetInViewport);
			}
		}
		FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);
	}
}
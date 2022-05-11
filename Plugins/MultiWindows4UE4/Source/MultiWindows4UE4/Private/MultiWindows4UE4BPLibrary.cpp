/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#include "MultiWindows4UE4BPLibrary.h"
#include "MultiWindows4UE4.h"
#include "MultiWindowsGameViewportClient.h"
#include "MultiWindowsLocalPlayer.h"
#include "Engine/Engine.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Internationalization/Internationalization.h"
#include "Components/SlateWrapperTypes.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "MultiWindows4UE4BPLibrary"

UMultiWindows4UE4BPLibrary::UMultiWindows4UE4BPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

void UMultiWindows4UE4BPLibrary::GetMultiWindowsManager(UMultiWindowsManager*& MultiWindowsManager)
{
	FMultiWindows4UE4Module& MultiWindows4UE4Module = FModuleManager::LoadModuleChecked<FMultiWindows4UE4Module>("MultiWindows4UE4");
	MultiWindows4UE4Module.GetMultiWindowsManager(MultiWindowsManager);
}

void UMultiWindows4UE4BPLibrary::AddWidgetToWindow(UUserWidget* UserWidget, UWindow* Window, int32 ZOrder)
{
	if (!UserWidget || !Window || !Window->GameViewportClient)
	{
		return;
	}

	{
		TSharedRef<SWidget> UserSlateWidget = UserWidget->TakeWidget();
		UPanelWidget * ParentPanel = UserWidget->GetParent();
		if (ParentPanel != nullptr || UserSlateWidget->GetParentWidget().IsValid())
		{
			FMessageLog("PIE").Error(FText::Format(LOCTEXT("WidgetAlreadyHasParent", "The widget '{0}' already has a parent widget.  It can't also be added to the viewport!"),
				FText::FromString(UserWidget->GetClass()->GetName())));
			return;
		}

		// First create and initialize the variable so that users calling this function twice don't
		// attempt to add the widget to the viewport again.
		TSharedRef<SConstraintCanvas> FullScreenCanvas = SNew(SConstraintCanvas);

		FullScreenCanvas->AddSlot()
			.Offset(TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateUObject(UserWidget, &UUserWidget::GetFullScreenOffset)))
			.Anchors(TAttribute<FAnchors>::Create(TAttribute<FAnchors>::FGetter::CreateUObject(UserWidget, &UUserWidget::GetAnchorsInViewport)))
			.Alignment(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateUObject(UserWidget, &UUserWidget::GetAlignmentInViewport)))
			[
				UserSlateWidget
			];

		// We add 10 to the zorder when adding to the viewport to avoid 
		// displaying below any built-in controls, like the virtual joysticks on mobile builds.
		Window->GameViewportClient->AddViewportWidgetContent(FullScreenCanvas, ZOrder + 10);
		Window->UserWidgetsInViewport.Add(UserWidget);
		// Just in case we already hooked this delegate, remove the handler.
		FWorldDelegates::LevelRemovedFromWorld.RemoveAll(Window);

		// Widgets added to the viewport are automatically removed if the persistent level is unloaded.
		FWorldDelegates::LevelRemovedFromWorld.AddUObject(Window, &UWindow::OnLevelRemovedFromWorldAndRemoveWidgetsInViewport);
	}
}

void UMultiWindows4UE4BPLibrary::RemoveWidgetFromWindow(UUserWidget* UserWidget)
{
	if (!UserWidget)
	{
		return;
	}

	TSharedRef<SWidget> UserSlateWidget = UserWidget->TakeWidget();
	TSharedPtr<SWidget> WidgetHost = UserSlateWidget->GetParentWidget();

	UMultiWindowsManager* MultiWindowsManager;
	UMultiWindows4UE4BPLibrary::GetMultiWindowsManager(MultiWindowsManager);

	UWindow* ParentWindow = nullptr;

	if (MultiWindowsManager)
	{
		for (auto Window : MultiWindowsManager->AncillaryWindows)
		{
			if (!Window)
			{
				continue;
			}
			for (auto UserWidgetInViewport : Window->UserWidgetsInViewport)
			{
				if (UserWidgetInViewport && UserWidget == UserWidgetInViewport)
				{
					ParentWindow = Window;
					break;
				}
			}
			if (ParentWindow)
			{
				break;
			}
		}

		if (!ParentWindow)
		{
			UWindow* MainWindow;
			MultiWindowsManager->GetMainWindow(MainWindow);
			if (MainWindow)
			{
				for (auto UserWidgetInViewport : MainWindow->UserWidgetsInViewport)
				{
					if (UserWidgetInViewport && UserWidget == UserWidgetInViewport)
					{
						ParentWindow = MainWindow;
						break;
					}
				}
			}
		}
	}

	if (!ParentWindow)
	{
		UserWidget->RemoveFromParent();
		return;
	}

	if (!UserWidget->HasAnyFlags(RF_BeginDestroyed))
	{
		if (WidgetHost.IsValid())
		{
			// If this is a game world add the widget to the current worlds viewport.
			UWorld* World = UserWidget->GetWorld();
			if (World && World->IsGameWorld())
			{
				if (UGameViewportClient* ViewportClient = ParentWindow->GameViewportClient)
				{
					TSharedRef<SWidget> WidgetHostRef = WidgetHost.ToSharedRef();

					ViewportClient->RemoveViewportWidgetContent(WidgetHostRef);

					if (ULocalPlayer* LocalPlayer = UserWidget->GetOwningLocalPlayer())
					{
						ViewportClient->RemoveViewportWidgetForPlayer(LocalPlayer, WidgetHostRef);
					}
				}
			}
		}
		else
		{
			UserWidget->RemoveFromParent();
		}
	}

	ParentWindow->UserWidgetsInViewport.Remove(UserWidget);
}

void UMultiWindows4UE4BPLibrary::BindViewportClientToPlayer(APlayerController* PlayerCont, UWindow* Window)
{
	if(PlayerCont && Window)
	{
		ULocalPlayer* localPlayer = Cast<ULocalPlayer>(PlayerCont->Player);
		if(localPlayer)
		{
			localPlayer->ViewportClient = Window->GameViewportClient;
		}
	}
}

void UMultiWindows4UE4BPLibrary::AttachWidgetToPlayer(APlayerController* PlayerCont, UUserWidget* Widget)
{
	if(PlayerCont && Widget)
	{
		// get window
		auto player = PlayerCont->GetLocalPlayer();
		if(player)
		{
			auto viewportClient = Cast<UMultiWindowsGameViewportClient>(player->ViewportClient);
			if(viewportClient)
			{
				auto window = viewportClient->Window;
				if(window)
				{
					AddWidgetToWindow(Widget, window, 0);
				}
				else
				{
					Widget->AddToViewport(0);
				}
			}
		}
	}
}

bool UMultiWindows4UE4BPLibrary::ProjectWorldToScreen(APlayerController const* Player, const FVector& WorldPosition,
	FVector2D& ScreenPosition, bool bPlayerViewportRelative)
{
	UMultiWindowsLocalPlayer* const player = (UMultiWindowsLocalPlayer*)(Player ? Player->GetLocalPlayer() : nullptr);
	UMultiWindowsGameViewportClient* viewportClient = Cast<UMultiWindowsGameViewportClient>(player->ViewportClient);
	if (player && viewportClient)
	{
		if(viewportClient->Window)
		{
			player->Size = FVector2D(1, 1);
			player->EnableMultiViews = true;
			player->ViewManager = viewportClient->Window->ViewManager;
		}
		
		// get the projection data
		FSceneViewProjectionData ProjectionData;
		if (player->GetProjectionData(player->ViewportClient->Viewport, /*out*/ ProjectionData))
		{
			FMatrix const ViewProjectionMatrix = ProjectionData.ComputeViewProjectionMatrix();
			bool bResult = FSceneView::ProjectWorldToScreen(WorldPosition, ProjectionData.GetConstrainedViewRect(), ViewProjectionMatrix, ScreenPosition);

			if (bPlayerViewportRelative)
			{
				ScreenPosition -= FVector2D(ProjectionData.GetConstrainedViewRect().Min);
			}

			bResult = bResult && Player->PostProcessWorldToScreen(WorldPosition, ScreenPosition, bPlayerViewportRelative);

			if(viewportClient->Window)
			{
				player->Size = FVector2D(0, 0);
				player->EnableMultiViews = false;
				player->ViewManager = FViewManager();
			}
			
			return bResult;
		}
		
		if(viewportClient->Window)
		{
			player->Size = FVector2D(0, 0);
			player->EnableMultiViews = false;
			player->ViewManager = FViewManager();
		}
	}

	ScreenPosition = FVector2D::ZeroVector;
	return false;
}

#undef LOCTEXT_NAMESPACE

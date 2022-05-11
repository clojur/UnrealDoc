/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ViewManager.h"
#include "Blueprint/UserWidget.h"
#include "Window.generated.h"



/**
 * 
 */
UCLASS(BlueprintType)
class MULTIWINDOWS4UE4_API UWindow : public UObject
{
	GENERATED_BODY()
	
public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ClearViews", Keywords = "ClearViews"), Category = "YeHaike|MultiWindows4UE4|Window")
	void ClearViews();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetAsCustomViewMode", Keywords = "SetAsCustomViewMode"), Category = "YeHaike|MultiWindows4UE4|Window")
	void SetAsCustomViewMode(const FViewManager& InViewManager);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetAsNormalViewMode", Keywords = "SetAsNormalViewMode"), Category = "YeHaike|MultiWindows4UE4|Window")
	void SetAsNormalViewMode();

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Toggle(NormalViewMode|MultiViewsMode)", Keywords = "ToggleNormalViewModeAndMultiViewsMode"), Category = "YeHaike|MultiWindows4UE4|Window")
	void ToggleNormalViewModeAndMultiViewsMode(const bool IsNormalViewMode);

	UFUNCTION(BlueprintPure, meta = (DisplayName = "IsNormalViewModeOrMultiViewsMode", Keywords = "IsNormalViewModeOrMultiViewsMode"), Category = "YeHaike|MultiWindows4UE4|Window")
	void IsNormalViewModeOrMultiViewsMode(bool& IsNormalViewMode);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetWindowIndex", Keywords = "GetWindowIndex"), Category = "YeHaike|MultiWindows4UE4|Window")
	void GetWindowIndex(int32& WindowIndex, bool& IsValid);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "ResizeWindow", Keywords = "Resize Window"), Category = "YeHaike|MultiWindows4UE4|Window")
	void ResizeWindow(int32 ResX = 1280, int32 ResY = 720, EWindowMode::Type WindowMode = EWindowMode::Type::Windowed);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetWindowTitle", Keywords = "Set Window Title"), Category = "YeHaike|MultiWindows4UE4|Window")
	void SetWindowTitle(FText WindowTitle = FText());

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "SetWindowPosition", Keywords = "Set Window Position"), Category = "YeHaike|MultiWindows4UE4|Window")
	void SetWindowPosition(FVector2D NewPosition = FVector2D(100.0f, 100.0f));
	
	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetWindowPosition", Keywords = "Get Window Position"), Category = "YeHaike|MultiWindows4UE4|Window")
	void GetWindowPosition(FVector2D& WindowPosition);

	/**
	 * Called when the game window closes (ends the game)
	 */
	void OnGameWindowClosed(const TSharedRef<SWindow>& WindowBeingClosed);

	/**
	 * Called when the game window is moved
	 */
	void OnGameWindowMoved(const TSharedRef<SWindow>& WindowBeingMoved);

	void OnViewportResized(FViewport* Viewport, uint32 Unused);
	
	void OnLevelRemovedFromWorldAndRemoveWidgetsInViewport(ULevel* InLevel, UWorld* InWorld);

public:

	/** GameViewportClient Array. The view port representing the current game instance. Can be 0 so don't use without checking. */
	UPROPERTY()
	UGameViewportClient* GameViewportClient;

public:

	/** The game viewport window */
	TWeakPtr<class SWindow> GameViewportClientWindow;
	/** The primary scene viewport */
	TSharedPtr<class FSceneViewport> SceneViewport;
	/** The game viewport widget */
	TSharedPtr<class SViewport> ViewportWidget;

	UPROPERTY()
	TArray<UUserWidget*> UserWidgetsInViewport;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|Window")
	FViewManager ViewManager;

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|ViewManager")
	//bool DoNotShowAnyView = false;
};

/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Window.h"
#include "MultiWindowsManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMultiWindowsManager, Log, All);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAncillaryWindowsArrayChanged, const UMultiWindowsManager*, MultiWindowsManager);

/**
 * 
 */
UCLASS(BlueprintType)
class MULTIWINDOWS4UE4_API UMultiWindowsManager : public UObject
{
	GENERATED_BODY()
	
public:

	//UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateWindow", Keywords = "Create Window"), Category = "YeHaike|MultiWindows4UE4|MultiWindowsManager|Window")
	//void CreateWindow(FText WindowTitle, FVector2D WindowPosition = FVector2D(500.0f, 500.0f), FVector2D WindowSize = FVector2D(1280.0f, 720.0f));

	/** Create a new Window. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "CreateGameViewportClientWindow", Keywords = "Create GameViewportClient Window"), Category = "YeHaike|MultiWindows4UE4|MultiWindowsManager|Window")
	UWindow* CreateGameViewportClientWindow();

	UFUNCTION(BlueprintPure, meta = (DisplayName = "GetMainWindow", Keywords = "Get Main Window"), Category = "YeHaike|MultiWindows4UE4|MultiWindowsManager|Window")
	void GetMainWindow(UWindow*& OutMainWindow);

	void UpdateWorldContentBeforeTick(TIndirectArray<FWorldContext>& WorldList);
	void UpdateWorldContentAfterTick(TIndirectArray<FWorldContext>& WorldList);

private:

	TSharedPtr<SWindow> CreateWindow_Internal(FText WindowTitle, FVector2D WindowPosition = FVector2D(500.0f, 500.0f), FVector2D WindowSize = FVector2D(1280.0f, 720.0f));

	/**
	 * Creates the game viewport
	 */
	void CreateSceneViewport(UWindow* Window);

	/**
	 * Creates the viewport widget where the games Slate UI is added to.
	 */
	void CreateViewportWidget(UWindow* Window);

	FSceneViewport* GetSceneViewport(UGameViewportClient* ViewportClient) const;

public:

	UPROPERTY()
	int32 NumOfNewViewportWindow = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "YeHaike|MultiWindows4UE4|MultiWindowsManager")
	TArray<UWindow*> AncillaryWindows;

	UPROPERTY()
	bool bAddedNewWindow = false;

	UPROPERTY(BlueprintAssignable, Category = "YeHaike|MultiWindows4UE4|MultiWindowsManager|Delegate")
	FOnAncillaryWindowsArrayChanged OnAncillaryWindowsArrayChanged;

private:

	UPROPERTY()
	UWindow* MainWindow;

	UPROPERTY()
	TArray<UGameViewportClient*> TempGameViewportClients;
	TIndirectArray<FWorldContext> TempWorldList;
};

/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MultiWindowsManager.h"
#include "GenericPlatform/GenericWindow.h"
#include "Blueprint/UserWidget.h"
#include "MultiWindows4UE4BPLibrary.generated.h"

/* 
*	
*/
UCLASS()
class MULTIWINDOWS4UE4_API UMultiWindows4UE4BPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetMultiWindowsManager", Keywords = "GetMultiWindowsManager"), Category = "YeHaike|MultiWindows4UE4|BPLibrary|Managers")
	static void GetMultiWindowsManager(UMultiWindowsManager*& MultiWindowsManager);

	/**
	 * Adds a UserWidget to the one window and fills the entire screen, unless SetDesiredSizeInViewport is called
	 * to explicitly set the size. 
	 * Note: Pair with the "RemoveWidgetFromWindow()". Use "RemoveWidgetFromWindow()" to remove the UserWidget from the Window.
	 *
	 * @param ZOrder The higher the number, the more on top this widget will be.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (DisplayName = "AddWidgetToWindow", Keywords = "Add Widget To Window" ,AdvancedDisplay = "ZOrder"), Category = "YeHaike|MultiWindows4UE4|BPLibrary|Widget")
	static void AddWidgetToWindow(UUserWidget* UserWidget, UWindow* Window, int32 ZOrder);
	
	/**
	 * Pair with the "AddWidgetToWindow()".
	 * Removes the widget from the viewport of one window.
	 * 
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "YeHaike|MultiWindows4UE4|BPLibrary|Widget")
	static void RemoveWidgetFromWindow(UUserWidget* UserWidget);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (DisplayName = "BindViewportClientToPlayer", Keywords = "Bind ViewportClient To Player") , Category = "Magic|MultiWindows4UE4|BPLibrary|Player")
	static void BindViewportClientToPlayer(APlayerController* PlayerCont, UWindow* Window);

	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (DisplayName = "AttachWidgetToPlayer", Keywords = "Attach Widget To Player") , Category = "Magic|MultiWindows4UE4|BPLibrary|Widget")
	static void AttachWidgetToPlayer(APlayerController* PlayerCont, UUserWidget* Widget);

	UFUNCTION(BlueprintPure, Category = "Magic|MultiWindows4UE4|BPLibrary|Camera")
	static bool ProjectWorldToScreen(APlayerController const* Player, const FVector& WorldPosition, FVector2D& ScreenPosition, bool bPlayerViewportRelative = false);
};

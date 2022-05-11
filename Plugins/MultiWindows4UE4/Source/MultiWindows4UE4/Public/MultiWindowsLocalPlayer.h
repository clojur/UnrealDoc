/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "SceneView.h"
#include "ViewManager.h"
#include "MultiWindowsLocalPlayer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(Log_MultiWindowsLocalPlayer, Log, All);

/**
*
*/
UCLASS(ClassGroup = "YeHaike|MultiWindows4UE4|MultiWindowsLocalPlayer", BlueprintType, Blueprintable, meta = (ShortTooltip = "MultiWindowsLocalPlayer is use to show multi views."))
class UMultiWindowsLocalPlayer : public ULocalPlayer
{
	GENERATED_UCLASS_BODY()
public:
	

public:
	virtual void PostInitProperties() override;
	/**
	* Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
	* asynchronous cleanup process.
	*/
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;

	/**
	 * Retrieve the viewpoint of this player.
	 * @param OutViewInfo - Upon return contains the view information for the player.
	 * @param StereoPass - Which stereoscopic pass, if any, to get the viewport for.  This will include eye offsetting
	 */
	virtual void GetViewPoint(FMinimalViewInfo& OutViewInfo) const override;


	/**
	* Calculate the view settings for drawing from this view actor
	*
	* @param	View - output view struct
	* @param	OutViewLocation - output actor location
	* @param	OutViewRotation - output actor rotation
	* @param	Viewport - current client viewport
	* @param	ViewDrawer - optional drawing in the view
	* @param	StereoPass - whether we are drawing the full viewport, or a stereo left / right pass
	*/
	virtual FSceneView* CalcSceneView(class FSceneViewFamily* ViewFamily,
		FVector& OutViewLocation,
		FRotator& OutViewRotation,
		FViewport* Viewport,
		class FViewElementDrawer* ViewDrawer = NULL,
		int32 StereoViewIndex = INDEX_NONE) override;
	/**
	* Helper function for deriving various bits of data needed for projection
	*
	* @param	Viewport				The ViewClient's viewport
	* @param	StereoPass			    Whether this is a full viewport pass, or a left/right eye pass
	* @param	ProjectionData			The structure to be filled with projection data
	* @return  False if there is no viewport, or if the Actor is null
	*/
	virtual bool GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData, int32 StereoViewIndex = INDEX_NONE) const override;
	
	virtual void OffsetViewLocationAndRotation(FMinimalViewInfo& InOutViewInfo) const;
	
	/** FExec interface
	*/
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	/**
	* Exec command handlers
	*/

	/**
	* Calculate the view init settings for drawing from this view actor
	*
	* @param	OutInitOptions - output view struct. Not every field is initialized, some of them are only filled in by CalcSceneView
	* @param	Viewport - current client viewport
	* @param	ViewDrawer - optional drawing in the view
	* @param	StereoPass - whether we are drawing the full viewport, or a stereo left / right pass
	* @return	true if the view options were filled in. false in various fail conditions.
	*/
	bool CalcSceneViewInitOptions_Custom(
		struct FSceneViewInitOptions& OutInitOptions,
		FViewport* Viewport,
		const int32 IndexOfView,
		class FViewElementDrawer* ViewDrawer = NULL,
		int32 StereoViewIndex = INDEX_NONE);

	virtual FSceneView* CalcMultiViews(class FSceneViewFamily* ViewFamily,
		FVector& OutViewLocation,
		FRotator& OutViewRotation,
		FViewport* Viewport,
		class FViewElementDrawer* ViewDrawer = NULL,
		int32 StereoViewIndex = INDEX_NONE);

	virtual FSceneView* CalcView_Custom(class FSceneViewFamily* ViewFamily,
		FVector& OutViewLocation,
		FRotator& OutViewRotation,
		FViewport* Viewport,
		const FView ViewSetting,
		const int32 IndexOfView,
		class FViewElementDrawer* ViewDrawer = NULL,
		int32 StereoViewIndex = INDEX_NONE);

public:
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "GetMaxNumOfViews"), Category = "YeHaike|MultiWindows4UE4|MultiWindowsLocalPlayer")
	int32 GetMaxNumOfViews()
	{
		return MaxNumOfViews;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiWindowsLocalPlayer")
	bool EnableMultiViews = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiWindowsLocalPlayer")
	FViewManager ViewManager;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiWindowsLocalPlayer")
	EViewModeType ViewMode;

	static const int32 MaxNumOfViews;

	TArray<FSceneViewStateReference> ViewStates;

private:
	
	int32 CurrentViewIndex = 0;
};

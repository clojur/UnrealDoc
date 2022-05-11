/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/GameViewportClient.h"
#include "Window.h"
#include "SceneTypes.h"

#include "MultiWindowsGameViewportClient.generated.h"

UCLASS()
class MULTIWINDOWS4UE4_API UMultiWindowsGameViewportClient : public UGameViewportClient
{
	GENERATED_UCLASS_BODY()

public:
	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	UMultiWindowsGameViewportClient(FVTableHelper& Helper);

	virtual ~UMultiWindowsGameViewportClient();

public:

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	/**
	 * Called to finish destroying the object.  After UObject::FinishDestroy is called, the object's memory should no longer be accessed.
	 *
	 * @warning Because properties are destroyed here, Super::FinishDestroy() should always be called at the end of your child class's FinishDestroy() method, rather than at the beginning.
	 */
	virtual void FinishDestroy() override;
	//~ End UObject Interface

public:

	//~ Begin FViewportClient Interface.
	virtual void Draw(FViewport* Viewport, FCanvas* SceneCanvas) override;

	//~ End FViewportClient Interface.

public:
	/** Updates CSVProfiler camera stats */
	virtual void UpdateCsvCameraStats(const FSceneView* View);

	/**
	 * Retrieve the viewpoint of this player.
	 * @param OutViewInfo - Upon return contains the view information for the player.
	 * @param StereoPass - Which stereoscopic pass, if any, to get the viewport for.  This will include eye offsetting
	 */
	virtual void GetViewPoint(class ULocalPlayer* LocalPlayer, const int32 IndexOfView, FMinimalViewInfo& OutViewInfo, int32 StereoViewIndex = INDEX_NONE) const;

	virtual void OffsetViewLocationAndRotation(FMinimalViewInfo& InOutViewInfo, const int32 IndexOfView) const;

	/**
	 * Calculate the view init settings for drawing from this view actor
	 *
	 * @param	OutInitOptions - output view struct. Not every field is initialized, some of them are only filled in by CalcSceneView
	 * @param	Viewport - current client viewport
	 * @param	ViewDrawer - optional drawing in the view
	 * @param	StereoPass - whether we are drawing the full viewport, or a stereo left / right pass
	 * @return	true if the view options were filled in. false in various fail conditions.
	 */
	virtual bool CalcSceneViewInitOptions(class ULocalPlayer* LocalPlayer,
		struct FSceneViewInitOptions& OutInitOptions,
		FViewport* InViewport,
		const int32 IndexOfView,
		class FViewElementDrawer* ViewDrawer = NULL,
		int32 StereoViewIndex = INDEX_NONE);

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
	virtual FSceneView* CalcSceneView(class ULocalPlayer* LocalPlayer,
		class FSceneViewFamily* ViewFamily,
		FVector& OutViewLocation,
		FRotator& OutViewRotation,
		FViewport* InViewport,
		int32 IndexOfView,
		class FViewElementDrawer* ViewDrawer = NULL,
		int32 StereoViewIndex = INDEX_NONE);

public:

	UPROPERTY()
	UWindow* Window;

public:
	static const int32 MaxNumOfViews;

private:
	TArray<FSceneViewStateReference> ViewStates;
};

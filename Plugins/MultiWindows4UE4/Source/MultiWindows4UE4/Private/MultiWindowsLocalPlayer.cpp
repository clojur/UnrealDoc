/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#include "MultiWindowsLocalPlayer.h"

#include "Engine/LocalPlayer.h"
#include "Misc/FileHelper.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "Engine/Scene.h"
#include "Camera/CameraTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "SceneView.h"
#include "UObject/UObjectAnnotation.h"
#include "Logging/LogScopedCategoryAndVerbosityOverride.h"
#include "UObject/UObjectIterator.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerController.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"

#include "Matinee/MatineeActor.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpGroupInst.h"
#include "Net/OnlineEngineInterface.h"
#include "SceneManagement.h"
#include "PhysicsPublic.h"
#include "SkeletalMeshTypes.h"
#include "HAL/PlatformApplicationMisc.h"

#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "IXRCamera.h"
#include "SceneViewExtension.h"
#include "Net/DataChannel.h"
#include "GameFramework/PlayerState.h"

#define LOCTEXT_NAMESPACE "MultiWindowsLocalPlayer"
DEFINE_LOG_CATEGORY(Log_MultiWindowsLocalPlayer);

DECLARE_CYCLE_STAT(TEXT("CalcView_Custom"), STAT_CalcView_Custom, STATGROUP_Engine);

const int32 UMultiWindowsLocalPlayer::MaxNumOfViews =20;

UMultiWindowsLocalPlayer::UMultiWindowsLocalPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UMultiWindowsLocalPlayer::PostInitProperties()
{
	Super::PostInitProperties();

	//FView view;
	//view.ViewpointType = EViewPointType::BindToPlayerController;
	//view.LocationAndSizeOnScreen = FVector4(0.0f, 0.0f, 1.0f, 1.0f);
	//ViewManager.Views.Add(view);

	ViewMode = EViewModeType::VMI_Lit;

	ViewStates.SetNum(MaxNumOfViews);
	for (auto& State : ViewStates)
	{
		State.Allocate();
	}
}

void UMultiWindowsLocalPlayer::BeginDestroy()
{
	Super::BeginDestroy();
}

void UMultiWindowsLocalPlayer::FinishDestroy()
{
	if (true)
	{
		for (FSceneViewStateReference& ViewStateTemp : ViewStates)
		{
			ViewStateTemp.Destroy();
		}
	}
	Super::FinishDestroy();
}

void UMultiWindowsLocalPlayer::GetViewPoint(FMinimalViewInfo& OutViewInfo) const
{
	Super::GetViewPoint(OutViewInfo);

	if (EnableMultiViews)
	{
		if (ViewManager.Views.Num() > CurrentViewIndex)
		{
			FView CurrentView = ViewManager.Views[CurrentViewIndex];
			if (CurrentView.ViewpointType == EViewPointType::CustomViewPoint)
			{
				CurrentView.CustomViewPoint.CustomPOV.CopyToViewInfo(OutViewInfo);
			}
			else if (CurrentView.ViewpointType == EViewPointType::BindToViewTarget)
			{
				CurrentView.BindToViewTarget.ApplyToViewInfo(OutViewInfo);
			}
		}

		OffsetViewLocationAndRotation(OutViewInfo);
	}
}

void UMultiWindowsLocalPlayer::OffsetViewLocationAndRotation(FMinimalViewInfo& InOutViewInfo) const
{
	if (EnableMultiViews)
	{
		if (ViewManager.Views.Num() > CurrentViewIndex)
		{
			FView CurrentView = ViewManager.Views[CurrentViewIndex];
			
			FVector ViewLocation = InOutViewInfo.Location;
			FRotator ViewRotation = InOutViewInfo.Rotation;

			const FTransform ViewRelativeTransform(CurrentView.RotationOffsetOfViewpoint, CurrentView.LocationOffsetOfViewpoint);
			const FTransform ViewWorldTransform(ViewRotation, ViewLocation);
			FTransform NewViewWorldTransform = ViewRelativeTransform*ViewWorldTransform;

			ViewLocation = NewViewWorldTransform.GetLocation();
			ViewRotation = NewViewWorldTransform.GetRotation().Rotator();

			InOutViewInfo.Location = ViewLocation;
			InOutViewInfo.Rotation = ViewRotation;
		}
	}
}

bool UMultiWindowsLocalPlayer::CalcSceneViewInitOptions_Custom(
	struct FSceneViewInitOptions& ViewInitOptions,
	FViewport* Viewport,
	const int32 IndexOfView,
	class FViewElementDrawer* ViewDrawer,
	int32 StereoViewIndex)
{
	// CalcSceneViewInitOptions(ViewInitOptions, Viewport, ViewDrawer, StereoPass);

	if ((PlayerController == NULL) || (Size.X <= 0.f) || (Size.Y <= 0.f) || (Viewport == NULL))
	{
		return false;
	}
	// get the projection data
	if (GetProjectionData(Viewport, /*inout*/ ViewInitOptions, StereoViewIndex) == false)
	{
		// Return NULL if this we didn't get back the info we needed
		return false;
	}

	// return if we have an invalid view rect
	if (!ViewInitOptions.IsValidViewRectangle())
	{
		return false;
	}

	if (PlayerController->PlayerCameraManager != NULL)
	{
		// Apply screen fade effect to screen.
		if (PlayerController->PlayerCameraManager->bEnableFading)
		{
			ViewInitOptions.OverlayColor = PlayerController->PlayerCameraManager->FadeColor;
			ViewInitOptions.OverlayColor.A = FMath::Clamp(PlayerController->PlayerCameraManager->FadeAmount, 0.0f, 1.0f);
		}

		// Do color scaling if desired.
		if (PlayerController->PlayerCameraManager->bEnableColorScaling)
		{
			ViewInitOptions.ColorScale = FLinearColor(
				PlayerController->PlayerCameraManager->ColorScale.X,
				PlayerController->PlayerCameraManager->ColorScale.Y,
				PlayerController->PlayerCameraManager->ColorScale.Z
			);
		}

		// Was there a camera cut this frame?
		ViewInitOptions.bInCameraCut = PlayerController->PlayerCameraManager->bGameCameraCutThisFrame;
	}

	check(PlayerController && PlayerController->GetWorld());

	//switch (StereoPass)
	//{
	//case eSSP_FULL:
	//case eSSP_LEFT_EYE:
		ViewInitOptions.SceneViewStateInterface = ViewStates[IndexOfView].GetReference();
	//	break;

	//case eSSP_RIGHT_EYE:
	//	ViewInitOptions.SceneViewStateInterface = ViewStates[IndexOfView].GetReference();
	//	break;
	//}

	ViewInitOptions.ViewActor = PlayerController->GetViewTarget();
	ViewInitOptions.PlayerIndex = GetControllerId();
	ViewInitOptions.ViewElementDrawer = ViewDrawer;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.LODDistanceFactor = PlayerController->LocalPlayerCachedLODDistanceFactor;
	ViewInitOptions.StereoPass = EStereoscopicPass::eSSP_FULL;
	ViewInitOptions.WorldToMetersScale = PlayerController->GetWorldSettings()->WorldToMeters;
	ViewInitOptions.CursorPos = Viewport->HasMouseCapture() ? FIntPoint(-1, -1) : FIntPoint(Viewport->GetMouseX(), Viewport->GetMouseY());
	ViewInitOptions.OriginOffsetThisFrame = PlayerController->GetWorld()->OriginOffsetThisFrame;

	return true;
}

FSceneView* UMultiWindowsLocalPlayer::CalcSceneView(class FSceneViewFamily* ViewFamily,
	FVector& OutViewLocation,
	FRotator& OutViewRotation,
	FViewport* Viewport,
	class FViewElementDrawer* ViewDrawer,
	int32 StereoViewIndex)
{
	if (!EnableMultiViews)
	{
		return 	Super::CalcSceneView(ViewFamily,
			OutViewLocation,
			OutViewRotation,
			Viewport,
			ViewDrawer,
			StereoViewIndex);
	}
	else
	{
		return 	CalcMultiViews(ViewFamily, OutViewLocation, OutViewRotation, Viewport, ViewDrawer, StereoViewIndex);
	}
}

FSceneView* UMultiWindowsLocalPlayer::CalcMultiViews(class FSceneViewFamily* ViewFamily,
	FVector& OutViewLocation,
	FRotator& OutViewRotation,
	FViewport* Viewport,
	class FViewElementDrawer* ViewDrawer,
	int32 StereoViewIndex)
{
	FSceneView* View = nullptr;
	for (int32 IndexOfView = 0; IndexOfView < ViewManager.Views.Num()
		&& IndexOfView < MaxNumOfViews; IndexOfView++)
	{
		CurrentViewIndex = IndexOfView;
		FView CurrentView = ViewManager.Views[IndexOfView];
		
		if(CurrentView.ViewpointType == EViewPointType::BindToPlayerController)
		{
			if(CurrentView.BindToPlayerController.PlayerIndex >= 0 && CurrentView.BindToPlayerController.PlayerIndex != GetLocalPlayerIndex())
			{
				continue;
			}
		}
		
		FSceneView* ViewTemp = CalcView_Custom(ViewFamily, OutViewLocation, OutViewRotation, Viewport, CurrentView, IndexOfView, ViewDrawer, StereoViewIndex);
		if (View == nullptr)
		{
			View = ViewTemp;
		}
	}

	return 	View;
}

//static void SetupMonoParameters(FSceneViewFamily& ViewFamily, const FSceneView& MonoView)
//{
//	// Compute the NDC depths for the far field clip plane. This assumes symmetric projection.
//	const FMatrix& LeftEyeProjection = ViewFamily.Views[0]->ViewMatrices.GetProjectionMatrix();
//
//	// Start with a point on the far field clip plane in eye space. The mono view uses a point slightly biased towards the camera to ensure there's overlap.
//	const FVector4 StereoDepthCullingPointEyeSpace(0.0f, 0.0f, ViewFamily.MonoParameters.CullingDistance, 1.0f);
//	const FVector4 FarFieldDepthCullingPointEyeSpace(0.0f, 0.0f, ViewFamily.MonoParameters.CullingDistance - ViewFamily.MonoParameters.OverlapDistance, 1.0f);
//
//	// Project into clip space
//	const FVector4 ProjectedStereoDepthCullingPointClipSpace = LeftEyeProjection.TransformFVector4(StereoDepthCullingPointEyeSpace);
//	const FVector4 ProjectedFarFieldDepthCullingPointClipSpace = LeftEyeProjection.TransformFVector4(FarFieldDepthCullingPointEyeSpace);
//
//	// Perspective divide for NDC space
//	ViewFamily.MonoParameters.StereoDepthClip = ProjectedStereoDepthCullingPointClipSpace.Z / ProjectedStereoDepthCullingPointClipSpace.W;
//	ViewFamily.MonoParameters.MonoDepthClip = ProjectedFarFieldDepthCullingPointClipSpace.Z / ProjectedFarFieldDepthCullingPointClipSpace.W;
//
//	// We need to determine the stereo disparity difference between the center mono view and an offset stereo view so we can account for it when compositing.
//	// We take a point on a stereo view far field clip plane, unproject it, then reproject it using the mono view. The stereo disparity offset is then
//	// the difference between the original test point and the reprojected point.
//	const FVector4 ProjectedPointAtLimit(0.0f, 0.0f, ViewFamily.MonoParameters.MonoDepthClip, 1.0f);
//	const FVector4 WorldProjectedPoint = ViewFamily.Views[0]->ViewMatrices.GetInvViewProjectionMatrix().TransformFVector4(ProjectedPointAtLimit);
//	FVector4 MonoProjectedPoint = MonoView.ViewMatrices.GetViewProjectionMatrix().TransformFVector4(WorldProjectedPoint / WorldProjectedPoint.W);
//	MonoProjectedPoint = MonoProjectedPoint / MonoProjectedPoint.W;
//	ViewFamily.MonoParameters.LateralOffset = (MonoProjectedPoint.X - ProjectedPointAtLimit.X) / 2.0f;
//}


FSceneView* UMultiWindowsLocalPlayer::CalcView_Custom(class FSceneViewFamily* ViewFamily,
	FVector& OutViewLocation,
	FRotator& OutViewRotation,
	FViewport* Viewport,
	const FView ViewSetting,
	const int32 IndexOfView,
	class FViewElementDrawer* ViewDrawer,
	int32 StereoViewIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_CalcView_Custom);

	FVector2D OriginCache = Origin;
	FVector2D SizeCache = Size;
	Origin = FVector2D(ViewSetting.LocationAndSizeOnScreen.X, ViewSetting.LocationAndSizeOnScreen.Y);
	Size = FVector2D(ViewSetting.LocationAndSizeOnScreen.Z, ViewSetting.LocationAndSizeOnScreen.W);

	FSceneViewInitOptions ViewInitOptions;

	if (!CalcSceneViewInitOptions_Custom(ViewInitOptions, Viewport, IndexOfView, ViewDrawer, StereoViewIndex))
	{
		return nullptr;
	}

	// Get the viewpoint...technically doing this twice
	// but it makes GetProjectionData better
	FMinimalViewInfo ViewInfo;
	GetViewPoint(ViewInfo);
	OutViewLocation = ViewInfo.Location;
	OutViewRotation = ViewInfo.Rotation;
	ViewInitOptions.bUseFieldOfViewForLOD = ViewInfo.bUseFieldOfViewForLOD;
	ViewInitOptions.FOV = ViewInfo.FOV;
	ViewInitOptions.DesiredFOV = ViewInfo.DesiredFOV;

	// Fill out the rest of the view init options
	ViewInitOptions.ViewFamily = ViewFamily;

	if (!PlayerController->bRenderPrimitiveComponents)
	{
		// Emplaces an empty show only primitive list.
		ViewInitOptions.ShowOnlyPrimitives.Emplace();
	}
	else
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_BuildHiddenComponentList);
		PlayerController->BuildHiddenComponentList(OutViewLocation, /*out*/ ViewInitOptions.HiddenPrimitives);
	}

	//@TODO: SPLITSCREEN: This call will have an issue with splitscreen, as the show flags are shared across the view family
	EngineShowFlagOrthographicOverride(ViewInitOptions.IsPerspectiveProjection(), ViewFamily->EngineShowFlags);

	FSceneView* const View = new FSceneView(ViewInitOptions);

	View->ViewLocation = OutViewLocation;
	View->ViewRotation = OutViewRotation;

	ViewFamily->Views.Add(View);

	{
		// Clear MIDPool of Views for MultiWindow. Otherwise the "Resource" property of MID maybe invalid and crash at "check(NewMID->GetRenderProxy());" in "ScenePrivate.h"
		if (View->State)
		{
			View->State->ClearMIDPool();
		}

		View->StartFinalPostprocessSettings(OutViewLocation);

		// CameraAnim override
		if (PlayerController != NULL && PlayerController->PlayerCameraManager)
		{
			TArray<FPostProcessSettings> const* CameraAnimPPSettings;
			TArray<float> const* CameraAnimPPBlendWeights;
			PlayerController->PlayerCameraManager->GetCachedPostProcessBlends(CameraAnimPPSettings, CameraAnimPPBlendWeights);

			for (int32 PPIdx = 0; PPIdx < CameraAnimPPBlendWeights->Num(); ++PPIdx)
			{
				View->OverridePostProcessSettings((*CameraAnimPPSettings)[PPIdx], (*CameraAnimPPBlendWeights)[PPIdx]);
			}
		}

		//	CAMERA OVERRIDE
		//	NOTE: Matinee works through this channel
		View->OverridePostProcessSettings(ViewInfo.PostProcessSettings, ViewInfo.PostProcessBlendWeight);

		View->EndFinalPostprocessSettings(ViewInitOptions);
	}

	for (int ViewExt = 0; ViewExt < ViewFamily->ViewExtensions.Num(); ViewExt++)
	{
		ViewFamily->ViewExtensions[ViewExt]->SetupView(*ViewFamily, *View);
	}

	//// Monoscopic far field setup
	//if (ViewFamily->IsMonoscopicFarFieldEnabled() && StereoPass == eSSP_MONOSCOPIC_EYE)
	//{
	//	SetupMonoParameters(*ViewFamily, *View);
	//}

	Origin = OriginCache;
	Size = SizeCache;

	return View;
}

bool UMultiWindowsLocalPlayer::GetProjectionData(FViewport* Viewport, FSceneViewProjectionData& ProjectionData, int32 StereoViewIndex) const
{
	if (Super::GetProjectionData(Viewport, ProjectionData, StereoViewIndex))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool UMultiWindowsLocalPlayer::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	return Super::Exec(InWorld, Cmd, Ar);
}

#undef LOCTEXT_NAMESPACE

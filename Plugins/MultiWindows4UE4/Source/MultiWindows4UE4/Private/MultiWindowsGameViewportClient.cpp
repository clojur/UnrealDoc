/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#include "MultiWindowsGameViewportClient.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "GameMapsSettings.h"
#include "EngineStats.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "LegacyScreenPercentageDriver.h"
#include "AI/NavigationSystemBase.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "GameFramework/Volume.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UObjectIterator.h"
#include "UObject/Package.h"
#include "SceneManagement.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/NetDriver.h"
#include "Engine/LocalPlayer.h"
#include "ContentStreaming.h"
#include "UnrealEngine.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Engine/Console.h"
#include "GameFramework/HUD.h"
#include "FXSystem.h"
#include "SubtitleManager.h"
#include "ImageUtils.h"
#include "SceneViewExtension.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "EngineModule.h"
#include "AudioDeviceManager.h"
#include "AudioDevice.h"
#include "Audio/AudioDebug.h"
#include "Sound/SoundWave.h"
#include "HighResScreenshot.h"
#include "BufferVisualizationData.h"
#include "GameFramework/InputSettings.h"
#include "Components/LineBatchComponent.h"
#include "Debug/DebugDrawService.h"
#include "Components/BrushComponent.h"
#include "Engine/GameEngine.h"
#include "Logging/MessageLog.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/UserInterfaceSettings.h"
#include "Slate/SceneViewport.h"
#include "Slate/SGameLayerManager.h"
#include "ActorEditorUtils.h"
#include "ComponentRecreateRenderStateContext.h"
#include "DynamicResolutionState.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "HAL/PlatformApplicationMisc.h"
#include "MultiWindowsLocalPlayer.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#include "Engine/DebugCameraController.h"
#endif

CSV_DEFINE_CATEGORY(View_MultiWindowsGameViewportClient, true);

#define LOCTEXT_NAMESPACE "MultiWindowsGameViewportClient"


static TAutoConsoleVariable<int32> CVarSetBlackBordersEnabled(
	TEXT("r.BlackBorders"),
	0,
	TEXT("To draw black borders around the rendered image\n")
	TEXT("(prevents artifacts from post processing passes that read outside of the image e.g. PostProcessAA)\n")
	TEXT("in pixels, 0:off"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarScreenshotDelegate(
	TEXT("r.ScreenshotDelegate"),
	1,
	TEXT("ScreenshotDelegates prevent processing of incoming screenshot request and break some features. This allows to disable them.\n")
	TEXT("Ideally we rework the delegate code to not make that needed.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: delegates are on (default)"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarSecondaryScreenPercentage( // TODO: make it a user settings instead?
	TEXT("r.SecondaryScreenPercentage.GameViewport"),
	0,
	TEXT("Override secondary screen percentage for game viewport.\n")
	TEXT(" 0: Compute secondary screen percentage = 100 / DPIScalefactor automaticaly (default);\n")
	TEXT(" 1: override secondary screen percentage."),
	ECVF_Default);

/**
 * UI Stats
 */
DECLARE_CYCLE_STAT(TEXT("UI Drawing Time"), STAT_UIDrawingTime_MultiWindowsGameViewportClient, STATGROUP_UI);
/** HUD stat */
DECLARE_CYCLE_STAT(TEXT("HUD Time"), STAT_HudTime_MultiWindowsGameViewportClient, STATGROUP_Engine);

DECLARE_CYCLE_STAT(TEXT("CalcSceneView"), STAT_CalcSceneView_MultiWindowsGameViewportClient, STATGROUP_Engine);

/** Util to find named canvas in transient package, and create if not found */
static UCanvas* GetCanvasByName(FName CanvasName)
{
	// Cache to avoid FString/FName conversions/compares
	static TMap<FName, UCanvas*> CanvasMap;
	UCanvas** FoundCanvas = CanvasMap.Find(CanvasName);
	if (!FoundCanvas)
	{
		UCanvas* CanvasObject = FindObject<UCanvas>(GetTransientPackage(), *CanvasName.ToString());
		if (!CanvasObject)
		{
			CanvasObject = NewObject<UCanvas>(GetTransientPackage(), CanvasName);
			CanvasObject->AddToRoot();
		}

		CanvasMap.Add(CanvasName, CanvasObject);
		return CanvasObject;
	}

	return *FoundCanvas;
}

const int32 UMultiWindowsGameViewportClient::MaxNumOfViews = 20;

UMultiWindowsGameViewportClient::UMultiWindowsGameViewportClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

UMultiWindowsGameViewportClient::UMultiWindowsGameViewportClient(FVTableHelper& Helper)
	: Super(Helper)
{

}

UMultiWindowsGameViewportClient::~UMultiWindowsGameViewportClient()
{
}

void UMultiWindowsGameViewportClient::PostInitProperties()
{
	Super::PostInitProperties();
	ViewStates.SetNum(MaxNumOfViews);
	for (auto& State : ViewStates)
	{
		State.Allocate();
	}
}

void UMultiWindowsGameViewportClient::FinishDestroy()
{
	for (FSceneViewStateReference& ViewStateTemp : ViewStates)
	{
		ViewStateTemp.Destroy();
	}

	Super::FinishDestroy();
}

void UMultiWindowsGameViewportClient::UpdateCsvCameraStats(const FSceneView* View)
{
#if CSV_PROFILER
	if (!View)
	{
		return;
	}
	static uint32 PrevFrameNumber = GFrameNumber;
	static double PrevTime = 0.0;
	static FVector PrevViewOrigin = FVector(ForceInitToZero);

	// TODO: support multiple views/view families, e.g for splitscreen. For now, we just output stats for the first one.
	if (GFrameNumber != PrevFrameNumber)
	{
		FVector ViewOrigin = View->ViewMatrices.GetViewOrigin();
		FVector ForwardVec = View->ViewMatrices.GetOverriddenTranslatedViewMatrix().GetColumn(2);
		FVector UpVec = View->ViewMatrices.GetOverriddenTranslatedViewMatrix().GetColumn(1);
		FVector Diff = ViewOrigin - PrevViewOrigin;
		double CurrentTime = FPlatformTime::Seconds();
		double DeltaT = CurrentTime - PrevTime;
		FVector Velocity = Diff / float(DeltaT);
		float CameraSpeed = Velocity.Size();
		PrevViewOrigin = ViewOrigin;
		PrevTime = CurrentTime;
		PrevFrameNumber = GFrameNumber;

		CSV_CUSTOM_STAT(View_MultiWindowsGameViewportClient, PosX, View->ViewMatrices.GetViewOrigin().X, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(View_MultiWindowsGameViewportClient, PosY, View->ViewMatrices.GetViewOrigin().Y, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(View_MultiWindowsGameViewportClient, PosZ, View->ViewMatrices.GetViewOrigin().Z, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(View_MultiWindowsGameViewportClient, ForwardX, ForwardVec.X, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(View_MultiWindowsGameViewportClient, ForwardY, ForwardVec.Y, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(View_MultiWindowsGameViewportClient, ForwardZ, ForwardVec.Z, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(View_MultiWindowsGameViewportClient, UpX, UpVec.X, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(View_MultiWindowsGameViewportClient, UpY, UpVec.Y, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(View_MultiWindowsGameViewportClient, UpZ, UpVec.Z, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(View_MultiWindowsGameViewportClient, Speed, CameraSpeed, ECsvCustomStatOp::Set);
	}
#endif
}

void UMultiWindowsGameViewportClient::GetViewPoint(class ULocalPlayer* LocalPlayer, const int32 IndexOfView, FMinimalViewInfo& OutViewInfo, int32 StereoViewIndex) const
{
	// if (FLockedViewState::Get().GetViewPoint(LocalPlayer, OutViewInfo.Location, OutViewInfo.Rotation, OutViewInfo.FOV) == false
	//  	&& PlayerController != NULL)
	
	if (!LocalPlayer)
	{
		return;
	}

	APlayerController* PlayerController = LocalPlayer->PlayerController;
	if (PlayerController != NULL)
	{
		if (PlayerController->PlayerCameraManager != NULL)
		{
			OutViewInfo = PlayerController->PlayerCameraManager->GetCameraCachePOV();
			OutViewInfo.FOV = PlayerController->PlayerCameraManager->GetFOVAngle();
			PlayerController->GetPlayerViewPoint(/*out*/ OutViewInfo.Location, /*out*/ OutViewInfo.Rotation);
		}
		else
		{
			PlayerController->GetPlayerViewPoint(/*out*/ OutViewInfo.Location, /*out*/ OutViewInfo.Rotation);
		}
	}

	// We store the originally desired FOV as other classes may adjust to account for ultra-wide aspect ratios
	OutViewInfo.DesiredFOV = OutViewInfo.FOV;

	FView CurrentView = Window->ViewManager.Views[IndexOfView];
	if (CurrentView.ViewpointType == EViewPointType::CustomViewPoint)
	{
		CurrentView.CustomViewPoint.CustomPOV.CopyToViewInfo(OutViewInfo);
	}
	else if (CurrentView.ViewpointType == EViewPointType::BindToViewTarget)
	{
		CurrentView.BindToViewTarget.ApplyToViewInfo(OutViewInfo);
	}

	OffsetViewLocationAndRotation(OutViewInfo, IndexOfView);

	for (auto& ViewExt : GEngine->ViewExtensions->GatherActiveExtensions())
	{
		ViewExt->SetupViewPoint(PlayerController, OutViewInfo);
	};
}

void UMultiWindowsGameViewportClient::OffsetViewLocationAndRotation(FMinimalViewInfo& InOutViewInfo, const int32 IndexOfView) const
{
	FView CurrentView = Window->ViewManager.Views[IndexOfView];

	FVector ViewLocation = InOutViewInfo.Location;
	FRotator ViewRotation = InOutViewInfo.Rotation;

	const FTransform ViewRelativeTransform(CurrentView.RotationOffsetOfViewpoint, CurrentView.LocationOffsetOfViewpoint);
	const FTransform ViewWorldTransform(ViewRotation, ViewLocation);
	FTransform NewViewWorldTransform = ViewRelativeTransform * ViewWorldTransform;

	ViewLocation = NewViewWorldTransform.GetLocation();
	ViewRotation = NewViewWorldTransform.GetRotation().Rotator();

	InOutViewInfo.Location = ViewLocation;
	InOutViewInfo.Rotation = ViewRotation;
}

bool UMultiWindowsGameViewportClient::CalcSceneViewInitOptions(class ULocalPlayer* LocalPlayer,
	struct FSceneViewInitOptions& ViewInitOptions,
	FViewport* InViewport,
	const int32 IndexOfView,
	class FViewElementDrawer* ViewDrawer,
	int32 StereoViewIndex)
{
	// CalcSceneViewInitOptions(ViewInitOptions, InViewport, ViewDrawer, StereoPass);

	if (!LocalPlayer)
	{
		return false;
	}

	FView& View = Window->ViewManager.Views[IndexOfView];

	APlayerController* PlayerController = LocalPlayer->PlayerController;
	FVector2D Size = LocalPlayer->Size;

	if ((PlayerController == NULL) || (Size.X <= 0.f) || (Size.Y <= 0.f) || (InViewport == NULL))
	{
		return false;
	}
	// get the projection data
	if (LocalPlayer->GetProjectionData(InViewport, /*inout*/ ViewInitOptions, StereoViewIndex) == false)
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
	ViewInitOptions.PlayerIndex = LocalPlayer->GetControllerId();
	ViewInitOptions.ViewElementDrawer = ViewDrawer;
	ViewInitOptions.BackgroundColor = FLinearColor::Black;
	ViewInitOptions.LODDistanceFactor = PlayerController->LocalPlayerCachedLODDistanceFactor;
	ViewInitOptions.StereoPass = EStereoscopicPass::eSSP_FULL;
	ViewInitOptions.WorldToMetersScale = PlayerController->GetWorldSettings()->WorldToMeters;
	ViewInitOptions.CursorPos = InViewport->HasMouseCapture() ? FIntPoint(-1, -1) : FIntPoint(InViewport->GetMouseX(), InViewport->GetMouseY());
	ViewInitOptions.OriginOffsetThisFrame = PlayerController->GetWorld()->OriginOffsetThisFrame;

	return true;
}

FSceneView* UMultiWindowsGameViewportClient::CalcSceneView(class ULocalPlayer* LocalPlayer,
	class FSceneViewFamily* ViewFamily,
	FVector& OutViewLocation,
	FRotator& OutViewRotation,
	FViewport* InViewport,
	int32 IndexOfView,
	class FViewElementDrawer* ViewDrawer,
	int32 StereoViewIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_CalcSceneView_MultiWindowsGameViewportClient);

	if (!LocalPlayer)
	{
		return nullptr;
	}

	FView CurrentView = Window->ViewManager.Views[IndexOfView];

	FVector2D OriginCache = LocalPlayer->Origin;
	FVector2D SizeCache = LocalPlayer->Size;
	LocalPlayer->Origin = FVector2D(CurrentView.LocationAndSizeOnScreen.X, CurrentView.LocationAndSizeOnScreen.Y);
	LocalPlayer->Size = FVector2D(CurrentView.LocationAndSizeOnScreen.Z, CurrentView.LocationAndSizeOnScreen.W);

	FSceneViewInitOptions ViewInitOptions;

	if (!CalcSceneViewInitOptions(LocalPlayer, ViewInitOptions, InViewport, IndexOfView, ViewDrawer, StereoViewIndex))
	{
		return nullptr;
	}

	// Get the viewpoint...technically doing this twice
	// but it makes GetProjectionData better
	FMinimalViewInfo ViewInfo;
	GetViewPoint(LocalPlayer, IndexOfView, ViewInfo, StereoViewIndex);
	OutViewLocation = ViewInfo.Location;
	OutViewRotation = ViewInfo.Rotation;
	ViewInitOptions.bUseFieldOfViewForLOD = ViewInfo.bUseFieldOfViewForLOD;
	ViewInitOptions.FOV = ViewInfo.FOV;
	ViewInitOptions.DesiredFOV = ViewInfo.DesiredFOV;

	// Fill out the rest of the view init options
	ViewInitOptions.ViewFamily = ViewFamily;

	APlayerController* PlayerController = LocalPlayer->PlayerController;

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
	// Pass on the previous view transform from the view info (probably provided by the camera if set)
	View->PreviousViewTransform = ViewInfo.PreviousViewTransform;

	ViewFamily->Views.Add(View);

	{
		View->StartFinalPostprocessSettings(OutViewLocation);

		// CameraAnim override
		if (PlayerController->PlayerCameraManager)
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

		if (PlayerController->PlayerCameraManager)
		{
			PlayerController->PlayerCameraManager->UpdatePhotographyPostProcessing(View->FinalPostProcessSettings);
		}

		if (GEngine->StereoRenderingDevice.IsValid())
		{
			FPostProcessSettings StereoDeviceOverridePostProcessinSettings;
			float BlendWeight = 1.0f;
			bool StereoSettingsAvailable = GEngine->StereoRenderingDevice->OverrideFinalPostprocessSettings(&StereoDeviceOverridePostProcessinSettings, EStereoscopicPass::eSSP_FULL, StereoViewIndex, BlendWeight);
			if (StereoSettingsAvailable)
			{
				View->OverridePostProcessSettings(StereoDeviceOverridePostProcessinSettings, BlendWeight);
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		ADebugCameraController* DebugCameraController = Cast<ADebugCameraController>(PlayerController);
		if (DebugCameraController != nullptr)
		{
			DebugCameraController->UpdateVisualizeBufferPostProcessing(View->FinalPostProcessSettings);
		}
#endif

		View->EndFinalPostprocessSettings(ViewInitOptions);
	}

	for (int ViewExt = 0; ViewExt < ViewFamily->ViewExtensions.Num(); ViewExt++)
	{
		ViewFamily->ViewExtensions[ViewExt]->SetupView(*ViewFamily, *View);
	}

	LocalPlayer->Origin = OriginCache;
	LocalPlayer->Size = SizeCache;

	return View;
}

void UMultiWindowsGameViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{

	//Valid SceneCanvas is required.  Make this explicit.
	check(SceneCanvas);

	UWorld* MyWorld = GetWorld();
	for (FLocalPlayerIterator Iterator(GEngine, MyWorld); Iterator; ++Iterator)
	{
		ULocalPlayer* LocalPlayer = *Iterator;
		if (LocalPlayer)
		{
			UMultiWindowsLocalPlayer* MultiWindowsLocalPlayer = Cast<UMultiWindowsLocalPlayer>(LocalPlayer);
			if (MultiWindowsLocalPlayer)
			{
				if (Window)
				{
					MultiWindowsLocalPlayer->ViewManager = Window->ViewManager;
					MultiWindowsLocalPlayer->EnableMultiViews = Window->ViewManager.EnableMultiViews;
				}
				else
				{
					MultiWindowsLocalPlayer->EnableMultiViews = false;
				}
			}
		}
	}

	Super::Draw(InViewport, SceneCanvas);

	// Restore MultiWindowsLocalPlayer state after Draw().
	for (FLocalPlayerIterator Iterator(GEngine, MyWorld); Iterator; ++Iterator)
	{
		ULocalPlayer* LocalPlayer = *Iterator;
		if (LocalPlayer)
		{
			UMultiWindowsLocalPlayer* MultiWindowsLocalPlayer = Cast<UMultiWindowsLocalPlayer>(LocalPlayer);
			if (MultiWindowsLocalPlayer)
			{
				MultiWindowsLocalPlayer->ViewManager = FViewManager();
				MultiWindowsLocalPlayer->EnableMultiViews = false;
			}
		}
	}

	return;

//	OnBeginDraw().Broadcast();
//
//	bool HasValidViewsInWindow = Window && ((Window->ViewManager.Views.Num() > 0 && Window->ViewManager.EnableMultiViews) || Window->DoNotShowAnyView);
//	// Disable StereoRendering for MultiViews Mode. Because user can config StereoRendering effect using MultiViews Mode.
//	const bool bStereoRendering = HasValidViewsInWindow ? false : GEngine->IsStereoscopic3D(InViewport);
//
//	FCanvas* DebugCanvas = InViewport->GetDebugCanvas();
//
//	// Create a temporary canvas if there isn't already one.
//	static FName CanvasObjectName(TEXT("CanvasObject"));
//	UCanvas* CanvasObject = GetCanvasByName(CanvasObjectName);
//	CanvasObject->Canvas = SceneCanvas;
//
//	// Create temp debug canvas object
//	FIntPoint DebugCanvasSize = InViewport->GetSizeXY();
//	if (bStereoRendering && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
//	{
//		DebugCanvasSize = GEngine->XRSystem->GetHMDDevice()->GetIdealDebugCanvasRenderTargetSize();
//	}
//
//	static FName DebugCanvasObjectName(TEXT("DebugCanvasObject"));
//	UCanvas* DebugCanvasObject = GetCanvasByName(DebugCanvasObjectName);
//	DebugCanvasObject->Init(DebugCanvasSize.X, DebugCanvasSize.Y, NULL, DebugCanvas);
//
//	if (DebugCanvas)
//	{
//		DebugCanvas->SetScaledToRenderTarget(bStereoRendering);
//		DebugCanvas->SetStereoRendering(bStereoRendering);
//	}
//	if (SceneCanvas)
//	{
//		SceneCanvas->SetScaledToRenderTarget(bStereoRendering);
//		SceneCanvas->SetStereoRendering(bStereoRendering);
//	}
//
//	UWorld* MyWorld = GetWorld();
//
//	// Force path tracing view mode, and extern code set path tracer show flags
//	const bool bForcePathTracing = InViewport->GetClient()->GetEngineShowFlags()->PathTracing;
//	if (bForcePathTracing)
//	{
//		EngineShowFlags.SetPathTracing(true);
//		ViewModeIndex = VMI_PathTracing;
//	}
//
//	// create the view family for rendering the world scene to the viewport's render target
//	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
//		InViewport,
//		MyWorld->Scene,
//		EngineShowFlags)
//		.SetRealtimeUpdate(true));
//
//#if WITH_EDITOR
//	if (GIsEditor)
//	{
//		// Force enable view family show flag for HighDPI derived's screen percentage.
//		ViewFamily.EngineShowFlags.ScreenPercentage = true;
//	}
//
//	UpdateDebugViewModeShaders();
//#endif
//
//	ViewFamily.ViewExtensions = GEngine->ViewExtensions->GatherActiveExtensions(InViewport);
//
//	for (auto ViewExt : ViewFamily.ViewExtensions)
//	{
//		ViewExt->SetupViewFamily(ViewFamily);
//	}
//
//	if (bStereoRendering && GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
//	{
//		// Allow HMD to modify screen settings
//		GEngine->XRSystem->GetHMDDevice()->UpdateScreenSettings(Viewport);
//	}
//
//	ESplitScreenType::Type SplitScreenConfig = GetCurrentSplitscreenConfiguration();
//	ViewFamily.ViewMode = EViewModeIndex(ViewModeIndex);
//	EngineShowFlagOverride(ESFIM_Game, ViewFamily.ViewMode, ViewFamily.EngineShowFlags, false);
//
//	if (ViewFamily.EngineShowFlags.VisualizeBuffer && AllowDebugViewmodes())
//	{
//		// Process the buffer visualization console command
//		FName NewBufferVisualizationMode = NAME_None;
//		static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
//		if (ICVar)
//		{
//			static const FName OverviewName = TEXT("Overview");
//			FString ModeNameString = ICVar->GetString();
//			FName ModeName = *ModeNameString;
//			if (ModeNameString.IsEmpty() || ModeName == OverviewName || ModeName == NAME_None)
//			{
//				NewBufferVisualizationMode = NAME_None;
//			}
//			else
//			{
//				if (GetBufferVisualizationData().GetMaterial(ModeName) == NULL)
//				{
//					// Mode is out of range, so display a message to the user, and reset the mode back to the previous valid one
//					UE_LOG(LogConsoleResponse, Warning, TEXT("Buffer visualization mode '%s' does not exist"), *ModeNameString);
//					NewBufferVisualizationMode = GetCurrentBufferVisualizationMode();
//					// todo: cvars are user settings, here the cvar state is used to avoid log spam and to auto correct for the user (likely not what the user wants)
//					ICVar->Set(*NewBufferVisualizationMode.GetPlainNameString(), ECVF_SetByCode);
//				}
//				else
//				{
//					NewBufferVisualizationMode = ModeName;
//				}
//			}
//		}
//
//		if (NewBufferVisualizationMode != GetCurrentBufferVisualizationMode())
//		{
//			SetCurrentBufferVisualizationMode(NewBufferVisualizationMode);
//		}
//	}
//
//	TMap<ULocalPlayer*, FSceneView*> PlayerViewMap;
//
//	FAudioDevice* AudioDevice = MyWorld->GetAudioDevice();
//	TArray<FSceneView*> Views;
//
//	for (FLocalPlayerIterator Iterator(GEngine, MyWorld); Iterator; ++Iterator)
//	{
//		ULocalPlayer* LocalPlayer = *Iterator;
//		if (LocalPlayer)
//		{
//			APlayerController* PlayerController = LocalPlayer->PlayerController;
//
//			int32 NumViewsInWindow = Window ? Window->ViewManager.Views.Num() : 1;
//			if (NumViewsInWindow < 1)
//			{
//				NumViewsInWindow = 1;
//			}
//			else if(NumViewsInWindow > UMultiWindowsGameViewportClient::MaxNumOfViews)
//			{
//				NumViewsInWindow = UMultiWindowsGameViewportClient::MaxNumOfViews;
//			}
//
//			if (Window && Window->DoNotShowAnyView)
//			{
//				NumViewsInWindow = 0;
//			}
//
//			const int32 NumViews = bStereoRendering ? GEngine->StereoRenderingDevice->GetDesiredNumberOfViews(bStereoRendering) : NumViewsInWindow;
//
//			for (int32 i = 0; i < NumViews; ++i)
//			{
//				// Calculate the player's view information.
//				FVector		ViewLocation;
//				FRotator	ViewRotation;
//
//				EStereoscopicPass PassType = bStereoRendering ? GEngine->StereoRenderingDevice->GetViewPassForIndex(bStereoRendering, i) : eSSP_FULL;
//
//				FSceneView* View = nullptr;
//				if (HasValidViewsInWindow)
//				{
//					View = CalcSceneView(LocalPlayer, &ViewFamily, ViewLocation, ViewRotation, InViewport, i, nullptr, PassType);
//				}
//				else
//				{
//					View = LocalPlayer->CalcSceneView(&ViewFamily, ViewLocation, ViewRotation, InViewport, nullptr, PassType);
//				}
//				
//				if (View)
//				{
//					Views.Add(View);
//
//					if (View->Family->EngineShowFlags.Wireframe)
//					{
//						// Wireframe color is emissive-only, and mesh-modifying materials do not use material substitution, hence...
//						View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
//						View->SpecularOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
//					}
//					else if (View->Family->EngineShowFlags.OverrideDiffuseAndSpecular)
//					{
//						View->DiffuseOverrideParameter = FVector4(GEngine->LightingOnlyBrightness.R, GEngine->LightingOnlyBrightness.G, GEngine->LightingOnlyBrightness.B, 0.0f);
//						View->SpecularOverrideParameter = FVector4(.1f, .1f, .1f, 0.0f);
//					}
//					else if (View->Family->EngineShowFlags.ReflectionOverride)
//					{
//						View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
//						View->SpecularOverrideParameter = FVector4(1, 1, 1, 0.0f);
//						View->NormalOverrideParameter = FVector4(0, 0, 1, 0.0f);
//						View->RoughnessOverrideParameter = FVector2D(0.0f, 0.0f);
//					}
//
//					if (!View->Family->EngineShowFlags.Diffuse)
//					{
//						View->DiffuseOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
//					}
//
//					if (!View->Family->EngineShowFlags.Specular)
//					{
//						View->SpecularOverrideParameter = FVector4(0.f, 0.f, 0.f, 0.f);
//					}
//
//					View->CurrentBufferVisualizationMode = GetCurrentBufferVisualizationMode();
//
//					View->CameraConstrainedViewRect = View->UnscaledViewRect;
//
//					// If this is the primary drawing pass, update things that depend on the view location
//					if (i == 0)
//					{
//						// Save the location of the view.
//						LocalPlayer->LastViewLocation = ViewLocation;
//
//						PlayerViewMap.Add(LocalPlayer, View);
//
//						// Update the listener.
//						if (AudioDevice != NULL && PlayerController != NULL)
//						{
//							bool bUpdateListenerPosition = true;
//
//							// If the main audio device is used for multiple PIE viewport clients, we only
//							// want to update the main audio device listener position if it is in focus
//							if (GEngine)
//							{
//								FAudioDeviceManager* AudioDeviceManager = GEngine->GetAudioDeviceManager();
//
//								// If there is more than one world referencing the main audio device
//								if (AudioDeviceManager->GetNumMainAudioDeviceWorlds() > 1)
//								{
//									uint32 MainAudioDeviceHandle = GEngine->GetAudioDeviceHandle();
//									if (AudioDevice->DeviceHandle == MainAudioDeviceHandle && !HasAudioFocus())
//									{
//										bUpdateListenerPosition = false;
//									}
//								}
//							}
//
//							if (bUpdateListenerPosition)
//							{
//								FVector Location;
//								FVector ProjFront;
//								FVector ProjRight;
//								PlayerController->GetAudioListenerPosition(/*out*/ Location, /*out*/ ProjFront, /*out*/ ProjRight);
//
//								FTransform ListenerTransform(FRotationMatrix::MakeFromXY(ProjFront, ProjRight));
//
//								// Allow the HMD to adjust based on the head position of the player, as opposed to the view location
//								if (GEngine->XRSystem.IsValid() && GEngine->StereoRenderingDevice.IsValid() && GEngine->StereoRenderingDevice->IsStereoEnabled())
//								{
//									const FVector Offset = GEngine->XRSystem->GetAudioListenerOffset();
//									Location += ListenerTransform.TransformPositionNoScale(Offset);
//								}
//
//								ListenerTransform.SetTranslation(Location);
//								ListenerTransform.NormalizeRotation();
//
//								uint32 ViewportIndex = PlayerViewMap.Num() - 1;
//								AudioDevice->SetListener(MyWorld, ViewportIndex, ListenerTransform, (View->bCameraCut ? 0.f : MyWorld->GetDeltaSeconds()));
//
//								FVector OverrideAttenuation;
//								if (PlayerController->GetAudioListenerAttenuationOverridePosition(OverrideAttenuation))
//								{
//									AudioDevice->SetListenerAttenuationOverride(OverrideAttenuation);
//								}
//								else
//								{
//									AudioDevice->ClearListenerAttenuationOverride();
//								}
//							}
//						}
//
//#if RHI_RAYTRACING
//						View->SetupRayTracedRendering();
//#endif
//
//#if CSV_PROFILER
//						UpdateCsvCameraStats(View);
//#endif
//					}
//
//					// Add view information for resource streaming. Allow up to 5X boost for small FOV.
//					const float StreamingScale = 1.f / FMath::Clamp<float>(View->LODDistanceFactor, .2f, 1.f);
//					IStreamingManager::Get().AddViewInformation(View->ViewMatrices.GetViewOrigin(), View->UnscaledViewRect.Width(), View->UnscaledViewRect.Width() * View->ViewMatrices.GetProjectionMatrix().M[0][0], StreamingScale);
//					MyWorld->ViewLocationsRenderedLastFrame.Add(View->ViewMatrices.GetViewOrigin());
//				}
//			}
//		}
//	}
//
//	FinalizeViews(&ViewFamily, PlayerViewMap);
//
//	// Update level streaming.
//	MyWorld->UpdateLevelStreaming();
//
//	// Find largest rectangle bounded by all rendered views.
//	uint32 MinX = InViewport->GetSizeXY().X, MinY = InViewport->GetSizeXY().Y, MaxX = 0, MaxY = 0;
//	uint32 TotalArea = 0;
//	{
//		for (int32 ViewIndex = 0; ViewIndex < ViewFamily.Views.Num(); ++ViewIndex)
//		{
//			const FSceneView* View = ViewFamily.Views[ViewIndex];
//
//			FIntRect UpscaledViewRect = View->UnscaledViewRect;
//
//			MinX = FMath::Min<uint32>(UpscaledViewRect.Min.X, MinX);
//			MinY = FMath::Min<uint32>(UpscaledViewRect.Min.Y, MinY);
//			MaxX = FMath::Max<uint32>(UpscaledViewRect.Max.X, MaxX);
//			MaxY = FMath::Max<uint32>(UpscaledViewRect.Max.Y, MaxY);
//			TotalArea += FMath::TruncToInt(UpscaledViewRect.Width()) * FMath::TruncToInt(UpscaledViewRect.Height());
//		}
//
//		// To draw black borders around the rendered image (prevents artifacts from post processing passes that read outside of the image e.g. PostProcessAA)
//		{
//			int32 BlackBorders = FMath::Clamp(CVarSetBlackBordersEnabled.GetValueOnGameThread(), 0, 10);
//
//			if (ViewFamily.Views.Num() == 1 && BlackBorders)
//			{
//				MinX += BlackBorders;
//				MinY += BlackBorders;
//				MaxX -= BlackBorders;
//				MaxY -= BlackBorders;
//				TotalArea = (MaxX - MinX) * (MaxY - MinY);
//			}
//		}
//	}
//
//	// If the views don't cover the entire bounding rectangle, clear the entire buffer.
//	bool bBufferCleared = false;
//	bool bStereoscopicPass = (ViewFamily.Views.Num() != 0 && ViewFamily.Views[0]->StereoPass != eSSP_FULL);
//	if (ViewFamily.Views.Num() == 0 || TotalArea != (MaxX - MinX) * (MaxY - MinY) || bDisableWorldRendering || bStereoscopicPass)
//	{
//		if (bDisableWorldRendering || !bStereoscopicPass) // TotalArea computation does not work correctly for stereoscopic views
//		{
//			SceneCanvas->Clear(FLinearColor::Transparent);
//		}
//
//		bBufferCleared = true;
//	}
//
//	// Force screen percentage show flag to be turned off if not supported.
//	if (!ViewFamily.SupportsScreenPercentage())
//	{
//		ViewFamily.EngineShowFlags.ScreenPercentage = false;
//	}
//
//	// Set up secondary resolution fraction for the view family.
//	if (!bStereoRendering && ViewFamily.SupportsScreenPercentage())
//	{
//		float CustomSecondaruScreenPercentage = CVarSecondaryScreenPercentage.GetValueOnGameThread();
//
//		if (CustomSecondaruScreenPercentage > 0.0)
//		{
//			// Override secondary resolution fraction with CVar.
//			ViewFamily.SecondaryViewFraction = FMath::Min(CustomSecondaruScreenPercentage / 100.0f, 1.0f);
//		}
//		else
//		{
//			// Automatically compute secondary resolution fraction from DPI.
//			ViewFamily.SecondaryViewFraction = GetDPIDerivedResolutionFraction();
//		}
//
//		check(ViewFamily.SecondaryViewFraction > 0.0f);
//	}
//
//	checkf(ViewFamily.GetScreenPercentageInterface() == nullptr,
//		TEXT("Some code has tried to set up an alien screen percentage driver, that could be wrong if not supported very well by the RHI."));
//
//	// Setup main view family with screen percentage interface by dynamic resolution if screen percentage is enabled.
//#if WITH_DYNAMIC_RESOLUTION
//	if (ViewFamily.EngineShowFlags.ScreenPercentage)
//	{
//		FDynamicResolutionStateInfos DynamicResolutionStateInfos;
//		GEngine->GetDynamicResolutionCurrentStateInfos(/* out */ DynamicResolutionStateInfos);
//
//		// Do not allow dynamic resolution to touch the view family if not supported to ensure there is no possibility to ruin
//		// game play experience on platforms that does not support it, but have it enabled by mistake.
//		if (DynamicResolutionStateInfos.Status == EDynamicResolutionStatus::Enabled)
//		{
//			GEngine->EmitDynamicResolutionEvent(EDynamicResolutionStateEvent::BeginDynamicResolutionRendering);
//			GEngine->GetDynamicResolutionState()->SetupMainViewFamily(ViewFamily);
//		}
//		else if (DynamicResolutionStateInfos.Status == EDynamicResolutionStatus::DebugForceEnabled)
//		{
//			GEngine->EmitDynamicResolutionEvent(EDynamicResolutionStateEvent::BeginDynamicResolutionRendering);
//			ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
//				ViewFamily,
//				DynamicResolutionStateInfos.ResolutionFractionApproximation,
//				/* AllowPostProcessSettingsScreenPercentage = */ false,
//				DynamicResolutionStateInfos.ResolutionFractionUpperBound));
//		}
//
//#if CSV_PROFILER
//		if (DynamicResolutionStateInfos.ResolutionFractionApproximation >= 0.0f)
//		{
//			CSV_CUSTOM_STAT_GLOBAL(DynamicResolutionPercentage, DynamicResolutionStateInfos.ResolutionFractionApproximation * 100.0f, ECsvCustomStatOp::Set);
//		}
//#endif
//	}
//#endif
//
//	// If a screen percentage interface was not set by dynamic resolution, then create one matching legacy behavior.
//	if (ViewFamily.GetScreenPercentageInterface() == nullptr)
//	{
//		bool AllowPostProcessSettingsScreenPercentage = false;
//		float GlobalResolutionFraction = 1.0f;
//
//		if (ViewFamily.EngineShowFlags.ScreenPercentage)
//		{
//			// Allow FPostProcessSettings::ScreenPercentage.
//			AllowPostProcessSettingsScreenPercentage = true;
//
//			// Get global view fraction set by r.ScreenPercentage.
//			GlobalResolutionFraction = FLegacyScreenPercentageDriver::GetCVarResolutionFraction();
//		}
//
//		ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(
//			ViewFamily, GlobalResolutionFraction, AllowPostProcessSettingsScreenPercentage));
//	}
//	else if (bStereoRendering)
//	{
//		// Change screen percentage method to raw output when doing dynamic resolution with VR if not using TAA upsample.
//		for (FSceneView* View : Views)
//		{
//			if (View->PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::SpatialUpscale)
//			{
//				View->PrimaryScreenPercentageMethod = EPrimaryScreenPercentageMethod::RawOutput;
//			}
//		}
//	}
//
//	// Draw the player views.
//	if (!bDisableWorldRendering && PlayerViewMap.Num() > 0 && FSlateApplication::Get().GetPlatformApplication()->IsAllowedToRender()) //-V560
//	{
//		GetRendererModule().BeginRenderingViewFamily(SceneCanvas, &ViewFamily);
//	}
//	else
//	{
//		// Make sure RHI resources get flushed if we're not using a renderer
//		ENQUEUE_RENDER_COMMAND(UGameViewportClient_FlushRHIResources)(
//			[](FRHICommandListImmediate& RHICmdList)
//			{
//				RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
//			});
//	}
//
//	// Beyond this point, only UI rendering independent from dynamc resolution.
//	GEngine->EmitDynamicResolutionEvent(EDynamicResolutionStateEvent::EndDynamicResolutionRendering);
//
//	// Clear areas of the rendertarget (backbuffer) that aren't drawn over by the views.
//	if (!bBufferCleared)
//	{
//		// clear left
//		if (MinX > 0)
//		{
//			SceneCanvas->DrawTile(0, 0, MinX, InViewport->GetSizeXY().Y, 0.0f, 0.0f, 1.0f, 1.f, FLinearColor::Black, NULL, false);
//		}
//		// clear right
//		if (MaxX < (uint32)InViewport->GetSizeXY().X)
//		{
//			SceneCanvas->DrawTile(MaxX, 0, InViewport->GetSizeXY().X, InViewport->GetSizeXY().Y, 0.0f, 0.0f, 1.0f, 1.f, FLinearColor::Black, NULL, false);
//		}
//		// clear top
//		if (MinY > 0)
//		{
//			SceneCanvas->DrawTile(MinX, 0, MaxX, MinY, 0.0f, 0.0f, 1.0f, 1.f, FLinearColor::Black, NULL, false);
//		}
//		// clear bottom
//		if (MaxY < (uint32)InViewport->GetSizeXY().Y)
//		{
//			SceneCanvas->DrawTile(MinX, MaxY, MaxX, InViewport->GetSizeXY().Y, 0.0f, 0.0f, 1.0f, 1.f, FLinearColor::Black, NULL, false);
//		}
//	}
//
//	// Remove temporary debug lines.
//	if (MyWorld->LineBatcher != nullptr)
//	{
//		MyWorld->LineBatcher->Flush();
//	}
//
//	if (MyWorld->ForegroundLineBatcher != nullptr)
//	{
//		MyWorld->ForegroundLineBatcher->Flush();
//	}
//
//	// Draw FX debug information.
//	if (MyWorld->FXSystem)
//	{
//		MyWorld->FXSystem->DrawDebug(SceneCanvas);
//	}
//
//	// Render the UI.
//	if (FSlateApplication::Get().GetPlatformApplication()->IsAllowedToRender())
//	{
//		SCOPE_CYCLE_COUNTER(STAT_UIDrawingTime_MultiWindowsGameViewportClient);
//		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(UI);
//
//		// render HUD
//		bool bDisplayedSubtitles = false;
//		for (FConstPlayerControllerIterator Iterator = MyWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
//		{
//			APlayerController* PlayerController = Iterator->Get();
//			if (PlayerController)
//			{
//				ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(PlayerController->Player);
//				if (LocalPlayer)
//				{
//					FSceneView* View = PlayerViewMap.FindRef(LocalPlayer);
//					if (View != NULL)
//					{
//						// rendering to directly to viewport target
//						FVector CanvasOrigin(FMath::TruncToFloat(View->UnscaledViewRect.Min.X), FMath::TruncToInt(View->UnscaledViewRect.Min.Y), 0.f);
//
//						CanvasObject->Init(View->UnscaledViewRect.Width(), View->UnscaledViewRect.Height(), View, SceneCanvas);
//
//						// Set the canvas transform for the player's view rectangle.
//						check(SceneCanvas);
//						SceneCanvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));
//						CanvasObject->ApplySafeZoneTransform();
//
//						// Render the player's HUD.
//						if (PlayerController->MyHUD)
//						{
//							SCOPE_CYCLE_COUNTER(STAT_HudTime_MultiWindowsGameViewportClient);
//
//							DebugCanvasObject->SceneView = View;
//							PlayerController->MyHUD->SetCanvas(CanvasObject, DebugCanvasObject);
//
//							PlayerController->MyHUD->PostRender();
//
//							// Put these pointers back as if a blueprint breakpoint hits during HUD PostRender they can
//							// have been changed
//							CanvasObject->Canvas = SceneCanvas;
//							DebugCanvasObject->Canvas = DebugCanvas;
//
//							// A side effect of PostRender is that the playercontroller could be destroyed
//							if (!PlayerController->IsPendingKill())
//							{
//								PlayerController->MyHUD->SetCanvas(NULL, NULL);
//							}
//						}
//
//						if (DebugCanvas != NULL)
//						{
//							DebugCanvas->PushAbsoluteTransform(FTranslationMatrix(CanvasOrigin));
//							UDebugDrawService::Draw(ViewFamily.EngineShowFlags, InViewport, View, DebugCanvas, DebugCanvasObject);
//							DebugCanvas->PopTransform();
//						}
//
//						CanvasObject->PopSafeZoneTransform();
//						SceneCanvas->PopTransform();
//
//						// draw subtitles
//						if (!bDisplayedSubtitles)
//						{
//							FVector2D MinPos(0.f, 0.f);
//							FVector2D MaxPos(1.f, 1.f);
//							GetSubtitleRegion(MinPos, MaxPos);
//
//							const uint32 SizeX = SceneCanvas->GetRenderTarget()->GetSizeXY().X;
//							const uint32 SizeY = SceneCanvas->GetRenderTarget()->GetSizeXY().Y;
//							FIntRect SubtitleRegion(FMath::TruncToInt(SizeX * MinPos.X), FMath::TruncToInt(SizeY * MinPos.Y), FMath::TruncToInt(SizeX * MaxPos.X), FMath::TruncToInt(SizeY * MaxPos.Y));
//							FSubtitleManager::GetSubtitleManager()->DisplaySubtitles(SceneCanvas, SubtitleRegion, MyWorld->GetAudioTimeSeconds());
//							bDisplayedSubtitles = true;
//						}
//					}
//				}
//			}
//		}
//
//		//ensure canvas has been flushed before rendering UI
//		SceneCanvas->Flush_GameThread();
//
//		OnDrawn().Broadcast();
//
//		// Allow the viewport to render additional stuff
//		PostRender(DebugCanvasObject);
//	}
//
//
//	// Grab the player camera location and orientation so we can pass that along to the stats drawing code.
//	FVector PlayerCameraLocation = FVector::ZeroVector;
//	FRotator PlayerCameraRotation = FRotator::ZeroRotator;
//	{
//		for (FConstPlayerControllerIterator Iterator = MyWorld->GetPlayerControllerIterator(); Iterator; ++Iterator)
//		{
//			if (APlayerController* PC = Iterator->Get())
//			{
//				PC->GetPlayerViewPoint(PlayerCameraLocation, PlayerCameraRotation);
//			}
//		}
//	}
//
//	if (DebugCanvas)
//	{
//		// Reset the debug canvas to be full-screen before drawing the console
//		// (the debug draw service above has messed with the viewport size to fit it to a single player's subregion)
//		DebugCanvasObject->Init(DebugCanvasSize.X, DebugCanvasSize.Y, NULL, DebugCanvas);
//
//		DrawStatsHUD(MyWorld, InViewport, DebugCanvas, DebugCanvasObject, DebugProperties, PlayerCameraLocation, PlayerCameraRotation);
//
//		// if (GEngine->IsStereoscopic3D(InViewport))
//		if(bStereoRendering)
//		{
//#if 0 //!UE_BUILD_SHIPPING
//			// TODO: replace implementation in OculusHMD with a debug renderer
//			if (GEngine->XRSystem.IsValid())
//			{
//				GEngine->XRSystem->DrawDebug(DebugCanvasObject);
//			}
//#endif
//		}
//
//		// Render the console absolutely last because developer input is was matter the most.
//		if (ViewportConsole)
//		{
//			ViewportConsole->PostRender_Console(DebugCanvasObject);
//		}
//	}
//
//	OnEndDraw().Broadcast();
}


#undef LOCTEXT_NAMESPACE

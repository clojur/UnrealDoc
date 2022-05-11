/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraTypes.h"
#include "View.generated.h"

/**
 * Define view modes to get specific show flag settings (some on, some off and some are not altered)
 * Don't change the order, the ID is serialized with the editor
 */
UENUM(BlueprintType)
enum class EViewModeType : uint8
{
	/** Wireframe w/ brushes. */
	VMI_BrushWireframe = 0,
	/** Wireframe w/ BSP. */
	VMI_Wireframe = 1,
	/** Unlit. */
	VMI_Unlit = 2,
	/** Lit. */
	VMI_Lit = 3,
	VMI_Lit_DetailLighting = 4,
	/** Lit wo/ materials. */
	VMI_LightingOnly = 5,
	/** Colored according to light count. */
	VMI_LightComplexity = 6,
	/** Colored according to shader complexity. */
	VMI_ShaderComplexity = 8,
	/** Colored according to world-space LightMap texture density. */
	VMI_LightmapDensity = 9,
	/** Colored according to light count - showing lightmap texel density on texture mapped objects. */
	VMI_LitLightmapDensity = 10,
	VMI_ReflectionOverride = 11,
	VMI_VisualizeBuffer = 12,
	//	VMI_VoxelLighting = 13,

	/** Colored according to stationary light overlap. */
	VMI_StationaryLightOverlap = 14,

	VMI_CollisionPawn = 15,
	VMI_CollisionVisibility = 16,
	//VMI_UNUSED = 17,
	/** Colored according to the current LOD index. */
	VMI_LODColoration = 18,
	/** Colored according to the quad coverage. */
	VMI_QuadOverdraw = 19,
	/** Visualize the accuracy of the primitive distance computed for texture streaming. */
	VMI_PrimitiveDistanceAccuracy = 20,
	/** Visualize the accuracy of the mesh UV densities computed for texture streaming. */
	VMI_MeshUVDensityAccuracy = 21,
	/** Colored according to shader complexity, including quad overdraw. */
	VMI_ShaderComplexityWithQuadOverdraw = 22,
	/** Colored according to the current HLOD index. */
	VMI_HLODColoration = 23,
	/** Group item for LOD and HLOD coloration*/
	VMI_GroupLODColoration = 24,
	/** Visualize the accuracy of the material texture scales used for texture streaming. */
	VMI_MaterialTextureScaleAccuracy = 25,
	/** Compare the required texture resolution to the actual resolution. */
	VMI_RequiredTextureResolution = 26,

	VMI_Max UMETA(Hidden),

	VMI_Unknown = 255 UMETA(Hidden),
};

/**
 *
 */
UENUM(BlueprintType)
enum class EViewPointType : uint8
{
	/*  */
	CustomViewPoint,
	/**
	 * 
	 */
	BindToPlayerController,
	/**
	 * 
	 */
	BindToViewTarget
};

USTRUCT(BlueprintType, Blueprintable, meta = (ShortTooltip = ""))
struct FCustomViewInfo
{
public:
	GENERATED_USTRUCT_BODY()

	FCustomViewInfo()
		: Location(ForceInit)
		, Rotation(ForceInit)
		, FOV(90.0f)
		, OrthoWidth(512.0f)
		, OrthoNearClipPlane(0.0f)
		, OrthoFarClipPlane(WORLD_MAX)
		, AspectRatio(1.33333333f)
		, bConstrainAspectRatio(false)
		, bUseFieldOfViewForLOD(true)
		, ProjectionMode(ECameraProjectionMode::Perspective)
		, PostProcessBlendWeight(0.0f)
	{
	}

public:

	/** Location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	FVector Location;

	/** (x:Roll y; Pitch; z: Yaw). Units: deg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	FRotator Rotation;

	/** The field of view (in degrees) in perspective mode (ignored in Orthographic mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera, meta = (UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0", Units = deg))
	float FOV;

	/** The desired width (in world units) of the orthographic view (ignored in Perspective mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	float OrthoWidth;

	/** The near plane distance of the orthographic view (in world units) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = Camera)
	float OrthoNearClipPlane;

	/** The far plane distance of the orthographic view (in world units) */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = Camera)
	float OrthoFarClipPlane;

	// Aspect Ratio (Width/Height); ignored unless bConstrainAspectRatio is true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera, meta = (ClampMin = "0.1", ClampMax = "100.0", EditCondition = "bConstrainAspectRatio"))
	float AspectRatio;

	// If bConstrainAspectRatio is true, black bars will be added if the destination view has a different aspect ratio than this camera requested.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	uint32 bConstrainAspectRatio : 1;

	// If true, account for the field of view angle when computing which level of detail to use for meshes.
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = CameraSettings)
	uint32 bUseFieldOfViewForLOD : 1;

	// The type of camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera)
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionMode;

	/** Indicates if PostProcessSettings should be applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Camera, meta = (UIMin = "0.0", UIMax = "1.0"))
	float PostProcessBlendWeight;

	/** Post-process settings to use if PostProcessBlendWeight is non-zero. */
	UPROPERTY(BlueprintReadWrite, Category = Camera)
	struct FPostProcessSettings PostProcessSettings;

	void CopyToViewInfo(FMinimalViewInfo& InOutInfo) const;
};

USTRUCT(BlueprintType, Blueprintable, meta = (ShortTooltip = ""))
struct MULTIWINDOWS4UE4_API FCustomViewPoint
{
public:
	GENERATED_BODY()

	/** Camera POV. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View")
	FCustomViewInfo CustomPOV;
};

USTRUCT(BlueprintType, Blueprintable, meta = (ShortTooltip = ""))
struct MULTIWINDOWS4UE4_API FBindToPlayerController
{
public:
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View")
	int32 PlayerIndex;
};



USTRUCT(BlueprintType, Blueprintable, meta = (ShortTooltip = ""))
struct MULTIWINDOWS4UE4_API FBindToViewTarget
{
public:
	GENERATED_USTRUCT_BODY()

	FBindToViewTarget()
		:ViewTarget(nullptr), bUseCustomPOV(true)
	{

	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View")
	AActor* ViewTarget;

	/** If bUseCustomPOV is false, and ViewTarget is a camera actor, will use the POV of the camera actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View")
	uint32 bUseCustomPOV : 1;

	/** If bUseCustomPOV is true, will use this POV. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View", meta = (EditCondition = "bUseCustomPOV"))
	FCustomViewInfo CustomPOV;

	void ApplyToViewInfo(FMinimalViewInfo& InOutInfo) const;
};

/**
 * 
 */
USTRUCT(BlueprintType, Blueprintable, meta = (ShortTooltip = ""))
struct MULTIWINDOWS4UE4_API FView
{	
public:
	GENERATED_USTRUCT_BODY()

	FView() :
		LocationAndSizeOnScreen(0.0f, 0.0f, 0.5f, 0.5f), LocationOffsetOfViewpoint(ForceInit),
		RotationOffsetOfViewpoint(ForceInit), ViewpointType(EViewPointType::BindToPlayerController)
	{
	}

	~FView()
	{
	}

public:
	/** The Name of this view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View")
	FName Name;

	/** (LocationX, LocationY, Width, Height); LocationX: [0.0~1.0];  LocationY: [0.0~1.0]; Width: [0.0~1.0]; Height: [0.0~1.0]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View")
	FVector4 LocationAndSizeOnScreen;

	/** Add a location offset to viewpoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View")
	FVector LocationOffsetOfViewpoint;

	/** Add a rotation offset to viewpoint. (x:Roll y; Pitch; z: Yaw). Units: deg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View")
	FRotator RotationOffsetOfViewpoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View")
	EViewPointType ViewpointType;

	/** Used when ViewpointType == EViewPointType::CustomViewPoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View", meta = (EditCondition = "ViewpointType==EViewPointType::CustomViewPoint"))
	FCustomViewPoint CustomViewPoint;

	/** Used when ViewpointType == EViewPointType::BindToPlayerController. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View", meta = (EditCondition = "ViewpointType==EViewPointType::BindToPlayerController"))
	FBindToPlayerController BindToPlayerController;

	/** Used when ViewpointType == EViewPointType::BindToViewTarget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|View", meta = (EditCondition = "ViewpointType==EViewPointType::BindToViewTarget"))
	FBindToViewTarget BindToViewTarget;
};

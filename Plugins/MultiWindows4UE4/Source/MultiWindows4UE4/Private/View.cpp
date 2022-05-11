/*
*  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
*  All rights reserved.
*  @ Date : 2020/01/26
*
*/

#include "View.h"
#include "Camera/CameraComponent.h"

void FCustomViewInfo::CopyToViewInfo(FMinimalViewInfo& InOutInfo) const
{
	InOutInfo.Location = Location;
	InOutInfo.Rotation = Rotation;
	InOutInfo.FOV = FOV;
	InOutInfo.OrthoWidth = OrthoWidth;
	InOutInfo.OrthoNearClipPlane = OrthoNearClipPlane;
	InOutInfo.OrthoFarClipPlane = OrthoFarClipPlane;
	InOutInfo.AspectRatio = AspectRatio;
	InOutInfo.bConstrainAspectRatio = bConstrainAspectRatio;
	InOutInfo.bUseFieldOfViewForLOD = bUseFieldOfViewForLOD;
	InOutInfo.ProjectionMode = ProjectionMode;
	InOutInfo.PostProcessBlendWeight = PostProcessBlendWeight;
	InOutInfo.PostProcessSettings = PostProcessSettings;
}

void FBindToViewTarget::ApplyToViewInfo(FMinimalViewInfo& InOutInfo) const
{
	if (!bUseCustomPOV)
	{
		if (ViewTarget->IsA(ACameraActor::StaticClass()))
		{
			ACameraActor* CameraActor = dynamic_cast<ACameraActor*>(ViewTarget);
			if (CameraActor != nullptr)
			{
				UCameraComponent* CameraComponent = CameraActor->GetCameraComponent();
				InOutInfo.Location = CameraComponent->GetComponentLocation();
				InOutInfo.Rotation = CameraComponent->GetComponentRotation();
				InOutInfo.FOV = CameraComponent->FieldOfView;
				InOutInfo.OrthoWidth = CameraComponent->OrthoWidth;
				InOutInfo.OrthoNearClipPlane = CameraComponent->OrthoNearClipPlane;
				InOutInfo.OrthoFarClipPlane = CameraComponent->OrthoFarClipPlane;
				InOutInfo.AspectRatio = CameraComponent->AspectRatio;
				InOutInfo.bConstrainAspectRatio = CameraComponent->bConstrainAspectRatio;
				InOutInfo.bUseFieldOfViewForLOD = CameraComponent->bUseFieldOfViewForLOD;
				InOutInfo.ProjectionMode = CameraComponent->ProjectionMode;
				InOutInfo.PostProcessBlendWeight = CameraComponent->PostProcessBlendWeight;
				InOutInfo.PostProcessSettings = CameraComponent->PostProcessSettings;
				return;
			}
		}
	}

	CustomPOV.CopyToViewInfo(InOutInfo);
	if (ViewTarget != nullptr)
	{
		InOutInfo.Location = ViewTarget->GetActorLocation();
		InOutInfo.Rotation = ViewTarget->GetActorRotation();
	}
}
/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "MultiWindowsEngineSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMultiWindowsEngineSubsystem, Display, All);

UCLASS()
class MULTIWINDOWS4UE4_API UMultiWindowsEngineSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	UMultiWindowsEngineSubsystem();

public:

	/** Implement this for initialization of instances of the system */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Implement this for deinitialization of instances of the system */
	virtual void Deinitialize() override;

public:

	/**
	 * Redraws all viewports.
	 * @param	bShouldPresent	Whether we want this frame to be presented
	 */
	virtual void RedrawViewports(bool bShouldPresent = true);

};

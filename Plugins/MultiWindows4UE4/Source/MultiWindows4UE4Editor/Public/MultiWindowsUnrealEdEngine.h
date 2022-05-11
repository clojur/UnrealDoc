/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Editor/UnrealEdEngine.h"
#include "MultiWindowsUnrealEdEngine.generated.h"

UCLASS(config=Engine, transient)
class UMultiWindowsUnrealEdEngine : public UUnrealEdEngine
{
public:
	GENERATED_BODY()

public:

	//~ Begin UObject Interface.
	~UMultiWindowsUnrealEdEngine();

public:
	//~ Begin UEngine Interface.
	virtual void Init(IEngineLoop* InEngineLoop) override;
	virtual void PreExit() override;
	virtual void Tick(float DeltaSeconds, bool bIdleMode) override;
	//~ End UEngine Interface.

private:
	UPROPERTY()
	class UMultiWindowsManager* MultiWindowsManager;
};

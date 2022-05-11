/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "View.h"
#include "ViewManager.generated.h"


/**
 * 
 */
USTRUCT(BlueprintType, Blueprintable, meta = (ShortTooltip = ""))
struct MULTIWINDOWS4UE4_API FViewManager
{
public:
	GENERATED_USTRUCT_BODY()
	
	FViewManager()
	{

	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|ViewManager")
	TArray<FView> Views;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "YeHaike|MultiWindows4UE4|MultiViews|ViewManager")
	bool EnableMultiViews = false;
};

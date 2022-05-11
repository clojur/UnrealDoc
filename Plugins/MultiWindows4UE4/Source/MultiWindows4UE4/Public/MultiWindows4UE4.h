/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#pragma once

#include "Modules/ModuleManager.h"

class MULTIWINDOWS4UE4_API FMultiWindows4UE4Module : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:

	void GetMultiWindowsManager(class UMultiWindowsManager*& MultiWindowsManager);

private:

	int32 MultiWindowsManagerObjectIndex = 0;
};

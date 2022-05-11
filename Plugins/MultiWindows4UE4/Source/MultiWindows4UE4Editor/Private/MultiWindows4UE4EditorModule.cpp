/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

#include "MultiWindows4UE4EditorModule.h"

class FMultiWindows4UE4EditorModule : public IMultiWindows4UE4EditorModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

private:

};

IMPLEMENT_MODULE(FMultiWindows4UE4EditorModule, MultiWindows4UE4Editor)

void FMultiWindows4UE4EditorModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("FMultiWindows4UE4EditorModule->StartupModule()"));
}

void FMultiWindows4UE4EditorModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("FMultiWindows4UE4EditorModule->ShutdownModule()"));
}

/*
 *  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
 *  All rights reserved.
 *  @ Date : 2020/01/26
 *
 */

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MultiWindows4UE4Editor : ModuleRules
	{
		public MultiWindows4UE4Editor(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivatePCHHeaderFile = "Public/MultiWindows4UE4EditorModule.h";

            var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

            // These nodes are not public so are hard to subclass

			PrivateIncludePaths.AddRange(
				new string[] {
					// Path.Combine(EngineDir, @"Source/Editor/GraphEditor/Private"),
				}
                );

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "MultiWindows4UE4"
                    // ... add other public dependencies that you statically link with here ...
                }
                );

            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"CoreUObject",
					"Engine",
					"UnrealEd",
                    "Slate"
                }
                );
		}
	}
}

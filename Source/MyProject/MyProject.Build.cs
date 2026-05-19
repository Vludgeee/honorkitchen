// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class MyProject : ModuleRules
{
	public MyProject(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Perception/*.h лежат в AIModule/Classes; при сбое IWYU подхватываем явно.
		PublicIncludePaths.Add(Path.Combine(EngineDirectory, "Source", "Runtime", "AIModule", "Classes"));

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"AIModule", "NavigationSystem", "GameplayTasks",
			"MediaAssets", "MediaUtils"
		});
	}
}

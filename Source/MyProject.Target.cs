// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MyProjectTarget : TargetRules
{
	public MyProjectTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		// Unique + установленный движок (Epic) → ошибка UBT при Package: "cannot be built with an installed engine".
		BuildEnvironment = TargetBuildEnvironment.Shared;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		bOverrideBuildEnvironment = true;
		AdditionalCompilerArguments = "/Zm2000";
		ExtraModuleNames.Add("MyProject");
	}
}

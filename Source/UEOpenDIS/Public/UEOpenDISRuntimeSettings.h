// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEOpenDISGameState.h"
#include "DISEntity_Base.h"
#include "UEOpenDISRuntimeSettings.generated.h"

USTRUCT()
struct FOpenDISEnumerationMappings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "OpenDISMappings")
		TAssetSubclassOf<ADISEntity_Base> DISEntity;

	UPROPERTY(EditAnywhere, Category = "OpenDISMappings")
		TArray<FEntityType> AssociatedDISEnumerations;
};

/**
 * 
 */
UCLASS(ClassGroup = "OpenDISRuntimeSettings", config = EditorPerProjectUserSettings)
class UEOPENDIS_API UUEOpenDISRuntimeSettings : public UObject
{
public:
	GENERATED_BODY()
		
	UUEOpenDISRuntimeSettings(const FObjectInitializer& ObjectInitializer);

	static UUEOpenDISRuntimeSettings* Get();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenDIS Mappings",	
		Meta = (DisplayName = "DIS Enumeration Mappings",	Tooltip = "Mappings between DIS enumerations and associated actors."))
		TArray<FOpenDISEnumerationMappings> DISClassMappings;
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenDIS Networking|Entity",
		Meta = (DisplayName = "Exercise ID", Tooltip = "The Exercise ID of the DIS sim. Valid Exercise IDs range from 0 to 255."))
		int32 ExerciseID;
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenDIS Networking|Entity",
		Meta = (DisplayName = "Site ID", Tooltip = "The Site ID of this application instance. Valid Site IDs range from 0 to 65535."))
		int32 SiteID;
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenDIS Networking|Entity",
		Meta = (DisplayName = "Application ID", Tooltip = "The Application ID of this application instance. Valid Application IDs range from 0 to 65535."))
		int32 ApplicationID;
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenDIS Networking|Connection",
		Meta = (DisplayName = "Auto Connect Send", Tooltip = "Whether or not sending UDP packets should be auto connected."))
		bool AutoConnectSend = false;
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenDIS Networking|Connection",
		Meta = (DisplayName = "Send IP Address", Tooltip = "The local IP address of this machine."))
		FString SendIPAddress = "127.0.0.1";
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenDIS Networking|Connection",
		Meta = (DisplayName = "Send Port", Tooltip = "The Port used by both Multicast and Broadcast connection types. Valid Port ranges are from 1024 to 65535."))
		int32 SendPort = 3001;
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenDIS Networking|Connection",
		Meta = (DisplayName = "Auto Connect Receive", Tooltip = "Whether or not receiving UDP packets should be auto connected."))
		bool AutoConnectReceive = false;
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenDIS Networking|Connection",
		Meta = (DisplayName = "Receive IP Address", Tooltip = "The IP address to receive UDP packets from. Set the IP to '0.0.0.0' if all connections are desired."))
		FString ReceiveIPAddress = "0.0.0.0";
	UPROPERTY(GlobalConfig, EditAnywhere, Category = "OpenDIS Networking|Connection",
		Meta = (DisplayName = "Receive Port", Tooltip = "The Port to receive UDP packets on. Valid Port ranges are from 1024 to 65535."))
		int32 ReceivePort = 3000;

private:
	static class UUEOpenDISRuntimeSettings* OpenDISSimSettingsSingleton;
};

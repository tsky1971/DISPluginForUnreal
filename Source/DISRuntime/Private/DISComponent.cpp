// Fill out your copyright notice in the Description page of Project Settings.

#include "DISComponent.h"

#include "DIS_BPFL.h"
#include "Kismet/KismetMathLibrary.h"

DEFINE_LOG_CATEGORY(LogDISComponent);

// Sets default values for this component's properties
UDISComponent::UDISComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UDISComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UDISComponent::GetLocalEulerAngles(TArray<uint8> OtherDeadReckoningParameters, FRotator& LocalRotator)
{
	// Ensure the DR Parameter type is set to 1
	if (OtherDeadReckoningParameters[0] != 1) return false;

	// Ensure the Array is at least 15 bytes long
	if (OtherDeadReckoningParameters.Num() < 15) return false;

	// The next 2 bytes are padding and not necessary so skip indices 1 and 2
	// Concatenate the next three groups of four bytes
	float LocalYaw = (OtherDeadReckoningParameters[3] << 24) + (OtherDeadReckoningParameters[4] << 16) + (OtherDeadReckoningParameters[5] << 8) + (OtherDeadReckoningParameters[6]);
	float LocalPitch = (OtherDeadReckoningParameters[7] << 24) + (OtherDeadReckoningParameters[8] << 16) + (OtherDeadReckoningParameters[9] << 8) + (OtherDeadReckoningParameters[10]);
	float LocalRoll = (OtherDeadReckoningParameters[11] << 24) + (OtherDeadReckoningParameters[12] << 16) + (OtherDeadReckoningParameters[13] << 8) + (OtherDeadReckoningParameters[14]);

	// Convert each angle from radians to degrees for FRotator
	LocalYaw = FMath::RadiansToDegrees(LocalYaw);
	LocalPitch = FMath::RadiansToDegrees(LocalPitch);
	LocalRoll = FMath::RadiansToDegrees(LocalRoll);

	// Set the values and return
	LocalRotator = FRotator(LocalPitch, LocalYaw, LocalRoll);

	return true;
}

bool UDISComponent::GetLocalQuaternionAngles(TArray<uint8> OtherDeadReckoningParameters, FQuat& EntityOrientation)
{
	// Ensure the DR Parameter type is set to 2
	if (OtherDeadReckoningParameters[0] != 2) return false;

	// Ensure the array is at least 15 bytes long
	if (OtherDeadReckoningParameters.Num() < 15) return false;

	//The next two bytes represent the 16 bit unsigned int approximation of q_0 (q_w in UE4 terminology)
	uint16 Qu0 = (OtherDeadReckoningParameters[1] << 8) + OtherDeadReckoningParameters[2];

	// The x, y, and z components of the quaternion are the next three groups of four bytes
	float QuX = (OtherDeadReckoningParameters[3] << 24) + (OtherDeadReckoningParameters[4] << 16) + (OtherDeadReckoningParameters[5] << 8) + (OtherDeadReckoningParameters[6]);
	float QuY = (OtherDeadReckoningParameters[7] << 24) + (OtherDeadReckoningParameters[8] << 16) + (OtherDeadReckoningParameters[9] << 8) + (OtherDeadReckoningParameters[10]);
	float QuZ = (OtherDeadReckoningParameters[11] << 24) + (OtherDeadReckoningParameters[12] << 16) + (OtherDeadReckoningParameters[13] << 8) + (OtherDeadReckoningParameters[14]);

	// Calculate the appropriate Qu0
	Qu0 = FMath::Sqrt(1 - (FMath::Square(QuX) + FMath::Square(QuY) + FMath::Square(QuZ)));

	// Set the values and return
	EntityOrientation = FQuat(QuX, QuY, QuZ, Qu0);

	return true;
}

glm::dvec3 UDISComponent::CalculateDeadReckonedPosition(const glm::dvec3 PositionVector, const glm::dvec3 VelocityVector,
	const glm::dvec3 AccelerationVector, const double DeltaTime)
{
	return PositionVector + (VelocityVector * DeltaTime) + ((1. / 2.) * AccelerationVector * FMath::Square(DeltaTime));
}

glm::dmat3 UDISComponent::CreateDeadReckoningMatrix(glm::dvec3 AngularVelocityVector, double DeltaTime)
{
	const double AngularVelocityMagnitude = glm::length(AngularVelocityVector);

	const auto AngularVelocityMatrix = glm::dmat3(AngularVelocityVector, glm::dvec3(0), glm::dvec3(0));
	const auto AngularVelocity = AngularVelocityMatrix * glm::transpose(AngularVelocityMatrix);

	const auto CosOmega = glm::cos(AngularVelocityMagnitude * DeltaTime);
	const auto SinOmega = glm::sin(AngularVelocityMagnitude * DeltaTime);

	glm::dmat3 DeadReckoningMatrix = (((1 - CosOmega) / glm::pow(AngularVelocityMagnitude, 2)) * AngularVelocity) +
									(CosOmega * glm::dmat3(1)) -
									(SinOmega / AngularVelocityMagnitude * UDIS_BPFL::CreateNCrossXMatrix(AngularVelocityVector));

	return DeadReckoningMatrix;
}

glm::dmat3 UDISComponent::GetEntityOrientationMatrix(const double PsiRadians, const double ThetaRadians, const double PhiRadians)
{
	//Trig orientations
	const auto CosPsi = glm::cos(PsiRadians);
	const auto SinPsi = glm::sin(PsiRadians);
	const auto CosTheta = glm::cos(ThetaRadians);
	const auto SinTheta = glm::sin(ThetaRadians);
	const auto CosPhi = glm::cos(PhiRadians);
	const auto SinPhi = glm::sin(PhiRadians);

	const auto HeadingRotationMatrix = glm::dmat3(CosPsi, -SinPsi, 0, SinPsi, CosPsi, 0, 0, 0, 1);
	const auto PitchRotationMatrix = glm::dmat3(CosTheta, 0, SinTheta, 0, 1, 0, -SinTheta, 0, CosTheta);
	const auto RollRotationMatrix = glm::dmat3(1, 0, 0, 0, CosPhi, -SinPhi, 0, SinPhi, CosPhi);

	return RollRotationMatrix * PitchRotationMatrix * HeadingRotationMatrix;

}

void UDISComponent::CalculateDeadReckonedOrientation(const double PsiRadians, const double ThetaRadians, const double PhiRadians,
	const glm::dvec3 AngularVelocityVector, const float DeltaTime, double &OutPsiRadians, double &OutThetaRadians, double &OutPhiRadians)
{
	// Get the entity's current orientation matrix
	auto OrientationMatrix = GetEntityOrientationMatrix(PsiRadians, ThetaRadians, PhiRadians);

	// Get the change in rotation for this time step
	const auto DeadReckoningMatrix = CreateDeadReckoningMatrix(AngularVelocityVector, DeltaTime);

	// Calculate the new orientation matrix
	OrientationMatrix = DeadReckoningMatrix * OrientationMatrix;

	// Extract Euler angles from orientation matrix
	OutThetaRadians = glm::asin(-OrientationMatrix[2][0]);

	// Special case for |Theta| = pi/2
	double CosTheta = 1e-5;
	if (abs(OutThetaRadians) != glm::pi<double>()/2)
	{
		CosTheta = glm::cos(OutThetaRadians);
	}

	OutPsiRadians = glm::acos(OrientationMatrix[0][0] / CosTheta) * (abs(OrientationMatrix[1][0]) / OrientationMatrix[1][0]);
	OutPhiRadians = glm::acos(OrientationMatrix[2][2] / CosTheta) * (abs(OrientationMatrix[2][1]) / OrientationMatrix[2][1]);
}

glm::dvec3 UDISComponent::GetEntityBodyDeadReckonedPosition(const glm::dvec3 InitialPositionVector, const glm::dvec3 BodyVelocityVector, const glm::dvec3 BodyLinearAccelerationVector, const glm::dvec3 BodyAngularAccelerationVector, const glm::dvec3 EntityOrientation, const double DeltaTime)
{
	const auto OmegaMatrix = UDIS_BPFL::CreateNCrossXMatrix(BodyAngularAccelerationVector);
	const auto BodyAccelerationVector = BodyLinearAccelerationVector - (OmegaMatrix * BodyVelocityVector);

	// Get the entity's current orientation matrix
	const auto OrientationMatrix = GetEntityOrientationMatrix(EntityOrientation.x, EntityOrientation.y, EntityOrientation.z);
	const auto InverseInitialOrientationMatrix = inverse(OrientationMatrix);

	// Calculate R1
	const auto AccelerationMagnitude = OmegaMatrix.length();
	const auto R1 = (((AccelerationMagnitude * DeltaTime - glm::sin(AccelerationMagnitude * DeltaTime)) / glm::pow(AccelerationMagnitude, 3)) * OmegaMatrix * glm::transpose(OmegaMatrix)) +
		(static_cast<double>(glm::sin(AccelerationMagnitude * DeltaTime) / AccelerationMagnitude) * glm::dmat3(1)) +
		(((1 - glm::cos(AccelerationMagnitude * DeltaTime)) / glm::pow(AccelerationMagnitude, 2)) * OmegaMatrix);

	const auto R2 = ((((0.5 * glm::pow(AccelerationMagnitude, 2) * glm::pow(DeltaTime, 2)) - (glm::cos(AccelerationMagnitude * DeltaTime)) - (AccelerationMagnitude * DeltaTime * glm::sin(AccelerationMagnitude * DeltaTime)) + (1)) / glm::pow(AccelerationMagnitude, 4)) * OmegaMatrix * glm::transpose(OmegaMatrix)) +
		(static_cast<double>(((glm::cos(AccelerationMagnitude * DeltaTime)) + (AccelerationMagnitude * DeltaTime * glm::sin(AccelerationMagnitude * DeltaTime)) - (1)) / (glm::pow(AccelerationMagnitude, 2))) * glm::dmat3(1)) +
		((((glm::sin(AccelerationMagnitude * DeltaTime)) - (AccelerationMagnitude * DeltaTime * glm::cos(AccelerationMagnitude * DeltaTime))) / (glm::pow(AccelerationMagnitude, 3))) * OmegaMatrix);

	return InitialPositionVector + (InverseInitialOrientationMatrix * ((R1 * BodyVelocityVector) + (R2 * BodyAccelerationVector)));
}

// Called every frame
void UDISComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//Check if dead reckoning is supported/enabled. Broadcast dead reckoning update if it is
	if (DeadReckoning(DeadReckoningEntityStatePDU, DeltaTime, TempDeadReckonedPDU))
	{
		DeadReckoningEntityStatePDU = TempDeadReckonedPDU;
		OnDeadReckoningUpdate.Broadcast(DeadReckoningEntityStatePDU);

		GroundClamping_Implementation();
	}
}

void UDISComponent::HandleEntityStatePDU(UGRILL_EntityStatePDU* NewEntityStatePDU)
{
	//Check if the entity has been deactivated -- Entity is deactivated if the 23rd bit of the Entity Appearance value is set
	if (NewEntityStatePDU->EntityStatePduStruct.EntityAppearance & (1 << 23))
	{
		UE_LOG(LogDISComponent, Log, TEXT("%s Entity Appearance is set to deactivated, deleting entity..."), *NewEntityStatePDU->EntityStatePduStruct.Marking);
		GetOwner()->Destroy();
	}

	LatestEntityStatePDUTimestamp = FDateTime::Now();
	MostRecentEntityStatePDU = NewEntityStatePDU;
	DeadReckoningEntityStatePDU = MostRecentEntityStatePDU;

	EntityType = NewEntityStatePDU->EntityStatePduStruct.EntityType;
	EntityID = NewEntityStatePDU->EntityStatePduStruct.EntityID;

	GetOwner()->SetLifeSpan(DISTimeoutSeconds);

	OnReceivedEntityStatePDU.Broadcast(NewEntityStatePDU);

	GroundClamping_Implementation();
}

void UDISComponent::HandleEntityStateUpdatePDU(UGRILL_EntityStateUpdatePDU* NewEntityStateUpdatePDU)
{
	//Check if the entity has been deactivated -- Entity is deactivated if the 23rd bit of the Entity Appearance value is set
	if (NewEntityStateUpdatePDU->EntityStateUpdatePduStruct.EntityAppearance & (1 << 23))
	{
		UE_LOG(LogDISComponent, Log, TEXT("%s Entity Appearance is set to deactivated, deleting entity..."), *NewEntityStateUpdatePDU->EntityStateUpdatePduStruct.EntityID.ToString());
		GetOwner()->Destroy();
	}

	LatestEntityStatePDUTimestamp = FDateTime::Now();
	MostRecentEntityStatePDU->SetupFromEntityStateUpdatePDU(NewEntityStateUpdatePDU);
	DeadReckoningEntityStatePDU = MostRecentEntityStatePDU;

	EntityID = NewEntityStateUpdatePDU->EntityStateUpdatePduStruct.EntityID;

	GetOwner()->SetLifeSpan(DISTimeoutSeconds);

	OnReceivedEntityStateUpdatePDU.Broadcast(NewEntityStateUpdatePDU);

	GroundClamping_Implementation();
}

void UDISComponent::HandleFirePDU(UGRILL_FirePDU* FirePDUIn)
{
	OnReceivedFirePDU.Broadcast(FirePDUIn);
}

void UDISComponent::HandleDetonationPDU(UGRILL_DetonationPDU* DetonationPDUIn)
{
	OnReceivedDetonationPDU.Broadcast(DetonationPDUIn);
}

void UDISComponent::HandleRemoveEntityPDU(UGRILL_RemoveEntityPDU* RemoveEntityPDUIn)
{
	OnReceivedRemoveEntityPDU.Broadcast(RemoveEntityPDUIn);
}

FEntityStatePDU UDISComponent::GetMostRecentEntityStatePDU()
{
	FEntityStatePDU mostRecentEntityStatePdu;

	if (MostRecentEntityStatePDU) 
	{
		mostRecentEntityStatePdu = MostRecentEntityStatePDU->EntityStatePduStruct;
	}

	return mostRecentEntityStatePdu;
}

FEntityStatePDU UDISComponent::GetMostRecentDeadReckoningPDU()
{
	FEntityStatePDU mostRecentDeadReckonedPdu;

	if (DeadReckoningEntityStatePDU)
	{
		mostRecentDeadReckonedPdu = DeadReckoningEntityStatePDU->EntityStatePduStruct;
	}

	return mostRecentDeadReckonedPdu;
}

void UDISComponent::SetMostRecentEntityStatePDU(FEntityStatePDU EntityStatePDUIn)
{
	if (!MostRecentEntityStatePDU)
	{
		MostRecentEntityStatePDU = NewObject<UGRILL_EntityStatePDU>();
	}

	MostRecentEntityStatePDU->EntityStatePduStruct = EntityStatePDUIn;
}

void UDISComponent::SetMostRecentDeadReckoningPDU(FEntityStatePDU DeadReckonedPDUIn)
{
	if (!DeadReckoningEntityStatePDU)
	{
		DeadReckoningEntityStatePDU = NewObject<UGRILL_EntityStatePDU>();
	}

	MostRecentEntityStatePDU->EntityStatePduStruct = DeadReckonedPDUIn;
}

// TODO: Cleanup copy pasted code in switch
bool UDISComponent::DeadReckoning(UGRILL_EntityStatePDU* EntityPDUToDeadReckon, float DeltaTime, UGRILL_EntityStatePDU*& DeadReckonedEntityPDU)
{
	//Check if dead reckoning should be performed and if the entity is owned by another sim on the network
	//If not, then don't do dead reckoning
	if (!(PerformDeadReckoning && SpawnedFromNetwork))
	{
		PerformDeadReckoning = false;
		return false;
	}
	
	DeadReckonedEntityPDU = EntityPDUToDeadReckon;
	bool bSupported = true;

	switch (EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.DeadReckoningAlgorithm) {
	case 1: // Static
		{
			FRotator LocalRotator;
			bool bUseOtherParams = GetLocalEulerAngles(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.OtherParameters, LocalRotator);

			if (bUseOtherParams)
			{
				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = LocalRotator;
			}

			break;
		}

	case 2: // Fixed Position World (FPW)
		{
			// Set entity orientation
			FRotator LocalRotator;
			bool bUseOtherParams = GetLocalEulerAngles(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.OtherParameters, LocalRotator);

			// Calculate and set entity location
			glm::dvec3 PositionVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[0], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[1], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[2]);
			glm::dvec3 VelocityVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.X, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Z);

			auto CalculatedPositionVector = CalculateDeadReckonedPosition(PositionVector, VelocityVector, glm::dvec3(0), DeltaTime);			

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[0] = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[1] = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[2] = CalculatedPositionVector[2];

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.X = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Y = CalculatedPositionVector[1];

			if (bUseOtherParams)
			{
				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = LocalRotator;
			}
			break;
		}
	
	case 3: // Rotation Position World (RPW)
		{
			FQuat EntityRotation;
			bool bUseOtherParams = GetLocalQuaternionAngles(DeadReckonedEntityPDU->EntityStatePduStruct.DeadReckoningParameters.OtherParameters, EntityRotation);

			// Calculate and set entity location
			glm::dvec3 PositionVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[0], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[1], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[2]);
			glm::dvec3 VelocityVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.X, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Z);

			auto CalculatedPositionVector = CalculateDeadReckonedPosition(PositionVector, VelocityVector, glm::dvec3(0), DeltaTime);

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[0] = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[1] = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[2] = CalculatedPositionVector[2];

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.X = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Y = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Z = CalculatedPositionVector[2];

			if (bUseOtherParams)
			{
				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = FRotator(EntityRotation);
			} else
			{
				glm::dvec3 AngularVelocityVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.X, 
					EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.Z);
				//NOTE: Roll=Phi, Pitch=Theta, Yaw=Psi
				double PsiRadians, ThetaRadians, PhiRadians;
				CalculateDeadReckonedOrientation(EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Roll, 
					EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Pitch, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Yaw, AngularVelocityVector, DeltaTime, PsiRadians, ThetaRadians, PhiRadians);

				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = FRotator(ThetaRadians, PsiRadians, PhiRadians);
			}
			break;
		}
	
	case 4: // Rotation Velocity World (RVW)
		{
			FQuat EntityRotation;
			bool bUseOtherParams = GetLocalQuaternionAngles(DeadReckonedEntityPDU->EntityStatePduStruct.DeadReckoningParameters.OtherParameters, EntityRotation);

			// Calculate and set entity location
			glm::dvec3 PositionVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[0], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[1], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[2]);
			glm::dvec3 VelocityVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.X, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Z);
			glm::dvec3 AccelerationVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.X,
				EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Y, EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Z);

			auto CalculatedPositionVector = CalculateDeadReckonedPosition(PositionVector, VelocityVector, AccelerationVector, DeltaTime);

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[0] = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[1] = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[2] = CalculatedPositionVector[2];

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.X = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Y = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Z = CalculatedPositionVector[2];

			if (bUseOtherParams)
			{
				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = FRotator(EntityRotation);
			} else
			{
				glm::dvec3 AngularVelocityVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.X, 
					EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.Z);
				//NOTE: Roll=Phi, Pitch=Theta, Yaw=Psi
				double PsiRadians, ThetaRadians, PhiRadians;
				CalculateDeadReckonedOrientation(EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Yaw, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Pitch, 
					EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Roll, AngularVelocityVector, DeltaTime, PsiRadians, ThetaRadians, PhiRadians);

				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = FRotator(ThetaRadians, PsiRadians, PhiRadians);
			}
			break;
		}
	
	case 5: // Fixed Velocity World (FVW)
		{
			FRotator LocalRotator;
			bool bUseOtherParams = GetLocalEulerAngles(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.OtherParameters, LocalRotator);

			// Calculate and set entity location
			glm::dvec3 PositionVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[0], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[1], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[2]);
			glm::dvec3 VelocityVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.X, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Z);
			glm::dvec3 AccelerationVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.X, 
				EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Y, EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Z);

			auto CalculatedPositionVector = CalculateDeadReckonedPosition(PositionVector, VelocityVector, AccelerationVector, DeltaTime);

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[0] = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[1] = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[2] = CalculatedPositionVector[2];

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.X = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Y = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Z = CalculatedPositionVector[2];

			if (bUseOtherParams)
			{
				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = LocalRotator;
			}
			break;
		}

	case 6: // Fixed Position Body (FPB)
		{
			FRotator LocalRotator;
			bool bUseOtherParams = GetLocalEulerAngles(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.OtherParameters, LocalRotator);

			auto InitialPosition = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[0], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[1], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[2]);
			auto BodyVelocityVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.X, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Z);
			auto BodyLinearAccelerationVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.X,
				EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Y, EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Z);
			auto BodyAngularAccelerationVector = glm::dvec3(0);
			auto EntityOrientation = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Yaw, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Pitch, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Roll);

			glm::dvec3 CalculatedPositionVector = GetEntityBodyDeadReckonedPosition(InitialPosition, BodyVelocityVector, BodyLinearAccelerationVector,
				BodyAngularAccelerationVector, EntityOrientation, DeltaTime);

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[0] = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[1] = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[2] = CalculatedPositionVector[2];

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.X = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Y = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Z = CalculatedPositionVector[2];

			if (bUseOtherParams)
			{
				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = LocalRotator;
			}
			
			break;
		}

	case 7: // Rotation Position Body (RPB)
		{
			FQuat EntityRotation;
			bool bUseOtherParams = GetLocalQuaternionAngles(DeadReckonedEntityPDU->EntityStatePduStruct.DeadReckoningParameters.OtherParameters, EntityRotation);

			auto InitialPosition = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[0], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[1], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[2]);
			auto BodyVelocityVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.X, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Z);
			auto BodyLinearAccelerationVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.X,
				EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Y, EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Z);
			auto BodyAngularAccelerationVector = glm::dvec3(0);
			auto EntityOrientation = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Yaw, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Pitch, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Roll);

			glm::dvec3 CalculatedPositionVector = GetEntityBodyDeadReckonedPosition(InitialPosition, BodyVelocityVector, BodyLinearAccelerationVector,
				BodyAngularAccelerationVector, EntityOrientation, DeltaTime);

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[0] = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[1] = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[2] = CalculatedPositionVector[2];

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.X = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Y = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Z = CalculatedPositionVector[2];

			if (bUseOtherParams)
			{
				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = FRotator(EntityRotation);
			} else
			{
				//NOTE: Roll=Phi, Pitch=Theta, Yaw=Psi
				double PsiRadians, ThetaRadians, PhiRadians;
				CalculateDeadReckonedOrientation(EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Yaw, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Pitch,
					EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Roll, BodyAngularAccelerationVector, DeltaTime, PsiRadians, ThetaRadians, PhiRadians);

				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = FRotator(ThetaRadians, PsiRadians, PhiRadians);
			}
			
			break;
		}

	case 8: // Rotation Velocity Body (RVB)
		{
			FQuat EntityRotation;
			bool bUseOtherParams = GetLocalQuaternionAngles(DeadReckonedEntityPDU->EntityStatePduStruct.DeadReckoningParameters.OtherParameters, EntityRotation);

			auto InitialPosition = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[0], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[1], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[2]);
			auto BodyVelocityVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.X, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Z);
			auto BodyLinearAccelerationVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.X, 
				EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Y, EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Z);
			auto BodyAngularAccelerationVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.X, 
				EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.Z);
			auto EntityOrientation = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Yaw, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Pitch, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Roll);

			glm::dvec3 CalculatedPositionVector = GetEntityBodyDeadReckonedPosition(InitialPosition, BodyVelocityVector, BodyLinearAccelerationVector,
			                                  BodyAngularAccelerationVector, EntityOrientation, DeltaTime);

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[0] = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[1] = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[2] = CalculatedPositionVector[2];

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.X = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Y = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Z = CalculatedPositionVector[2];

			if (bUseOtherParams)
			{
				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = FRotator(EntityRotation);
			} else
			{
				//NOTE: Roll=Phi, Pitch=Theta, Yaw=Psi
				double PsiRadians, ThetaRadians, PhiRadians;
				CalculateDeadReckonedOrientation(EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Yaw, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Pitch,
					EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Roll, BodyAngularAccelerationVector, DeltaTime, PsiRadians, ThetaRadians, PhiRadians);

				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = FRotator(ThetaRadians, PsiRadians, PhiRadians);
			}
			
			break;
		}

	case 9: // Fixed Velocity Body (FVB)
		{
			FRotator LocalRotator;
			bool bUseOtherParams = GetLocalEulerAngles(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.OtherParameters, LocalRotator);

			auto InitialPosition = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[0], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[1], EntityPDUToDeadReckon->EntityStatePduStruct.EntityLocationDouble[2]);
			auto BodyVelocityVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.X, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.EntityLinearVelocity.Z);
			auto BodyLinearAccelerationVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.X,
				EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Y, EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityLinearAcceleration.Z);
			auto BodyAngularAccelerationVector = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.X,
				EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.Y, EntityPDUToDeadReckon->EntityStatePduStruct.DeadReckoningParameters.EntityAngularVelocity.Z);
			auto EntityOrientation = glm::dvec3(EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Yaw, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Pitch, EntityPDUToDeadReckon->EntityStatePduStruct.EntityOrientation.Roll);

			glm::dvec3 CalculatedPositionVector = GetEntityBodyDeadReckonedPosition(InitialPosition, BodyVelocityVector, BodyLinearAccelerationVector,
				BodyAngularAccelerationVector, EntityOrientation, DeltaTime);

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[0] = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[1] = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocationDouble[2] = CalculatedPositionVector[2];

			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.X = CalculatedPositionVector[0];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Y = CalculatedPositionVector[1];
			DeadReckonedEntityPDU->EntityStatePduStruct.EntityLocation.Z = CalculatedPositionVector[2];

			if (bUseOtherParams)
			{
				DeadReckonedEntityPDU->EntityStatePduStruct.EntityOrientation = LocalRotator;
			}
			
			break;
		}

	default: // Unknown
		{
			bSupported = false;
			break;
		}
	}

	return bSupported;
}

void UDISComponent::GroundClamping_Implementation()
{
	//Verify that ground clamping is enabled, the entity is owned by another sim, is of the ground domain, and that it is not a munition
	if (PerformGroundClamping && SpawnedFromNetwork && EntityType.Domain == 1 && EntityType.EntityKind != 2)
	{
		//Get the most recent calculated ECEF location of the entity from the dead reckoned ESPDU
		FEarthCenteredEarthFixedDouble ecefDouble = FEarthCenteredEarthFixedDouble(DeadReckoningEntityStatePDU->EntityStatePduStruct.EntityLocationDouble[0],
			DeadReckoningEntityStatePDU->EntityStatePduStruct.EntityLocationDouble[1], DeadReckoningEntityStatePDU->EntityStatePduStruct.EntityLocationDouble[2]);

		//Get the LLH location of the entity from the ECEF location
		FLatLonHeightDouble llhDouble;
		UDIS_BPFL::CalculateLatLonHeightFromEcefXYZ(ecefDouble, llhDouble);

		//Get the North East Down vectors from the calculated LLH
		FNorthEastDown northEastDownVectors;
		UDIS_BPFL::CalculateNorthEastDownVectorsFromLatLon(llhDouble.Latitude, llhDouble.Longitude, northEastDownVectors);
		//Set clamp direction using the North East Down down vector
		FVector clampDirection = northEastDownVectors.DownVector;

		//Get the location the object is supposed to be at according to the most recent dead reckoning update.
		FVector actorLocation;
		UDIS_BPFL::GetEntityUnrealLocationFromEntityStatePdu(DeadReckoningEntityStatePDU, AGeoReferencingSystem::GetGeoReferencingSystem(GetWorld()), actorLocation);

		FHitResult lineTraceHitResult;
		FVector endLocation = (clampDirection * 100000) + actorLocation;
		FVector aboveActorStartLocation = (clampDirection * -100000) + actorLocation;

		FCollisionQueryParams queryParams = FCollisionQueryParams(FName("Ground Clamping"), false, GetOwner());
		//Find colliding point above/below the actor
		if (GetWorld()->LineTraceSingleByChannel(lineTraceHitResult, aboveActorStartLocation, endLocation, UEngineTypes::ConvertToCollisionChannel(GoundClampingCollisionChannel), queryParams))
		{
			FVector clampLocation = lineTraceHitResult.Location;
			//Calculate what the new forward and right vectors should be based on the impact normal
			FVector newForward = FVector::CrossProduct(GetOwner()->GetActorRightVector(), lineTraceHitResult.ImpactNormal);
			FVector newRight = FVector::CrossProduct(lineTraceHitResult.ImpactNormal, newForward);

			FRotator clampRotation = UKismetMathLibrary::MakeRotationFromAxes(newForward, newRight, lineTraceHitResult.ImpactNormal);

			//Create clamp transform and broadcast
			FTransform clampTransform = FTransform(clampRotation, clampLocation);
			TArray<FTransform> allClampTransforms;
			allClampTransforms.Add(clampTransform);

			OnGroundClampingUpdate.Broadcast(allClampTransforms);
		}
	}
}
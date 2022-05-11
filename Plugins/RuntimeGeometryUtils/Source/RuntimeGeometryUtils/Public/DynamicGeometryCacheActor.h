#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"
#include "GeometryCacheComponent.h" 
#include "GeometryCache.h"
#include "DynamicMeshBaseActor.h"
#include "DynamicGeometryCacheActor.generated.h"

UCLASS()
class RUNTIMEGEOMETRYUTILS_API ADynamicGeometryCacheActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADynamicGeometryCacheActor();

public:
	UPROPERTY(VisibleAnywhere,Category="Custom")
	UGeometryCacheComponent* Ciji = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Custom")
	UGeometryCacheComponent* BuDaiCiji = nullptr;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void PostLoad() override;
	virtual void PostActorCreated() override;


public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

protected:
	/**
	 * ADynamicBaseActor API
	 */

protected:
	virtual void UpdateGeometryCacheMesh();

	class ADynamicSDMCActor* CijiSDM;
	FDynamicMesh3 cjMesh;
	bool bFirst = true;
};

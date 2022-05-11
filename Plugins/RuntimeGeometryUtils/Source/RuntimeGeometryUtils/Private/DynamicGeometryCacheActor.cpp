#include "DynamicGeometryCacheActor.h"

#include "DynamicSDMCActor.h"
#include "GeometryCacheMeshData.h"
#include "GeometryCacheSceneProxy.h"
#include "MeshComponentRuntimeUtils.h"
#include "MeshNormals.h"

// Sets default values
ADynamicGeometryCacheActor::ADynamicGeometryCacheActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Ciji = CreateDefaultSubobject<UGeometryCacheComponent>(TEXT("MeshCiji"), false);
	BuDaiCiji = CreateDefaultSubobject<UGeometryCacheComponent>(TEXT("MeshBuDaiCiji"), false);
	SetRootComponent(Ciji);
	BuDaiCiji->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
	//SourceType = EDynamicMeshActorSourceType::GeometryCache;
	
	
}

// Called when the game starts or when spawned
void ADynamicGeometryCacheActor::BeginPlay()
{
	Super::BeginPlay();
	FRotator ActorRotator = GetActorQuat().Rotator();
	CijiSDM = GWorld->SpawnActor<ADynamicSDMCActor>(GetActorLocation(), ActorRotator);
	
	ADynamicSDMCActor* BudaiCijiSDM = GWorld->SpawnActor<ADynamicSDMCActor>(GetActorLocation(), ActorRotator);

	UGeometryCache* CijiGC = Ciji->GetGeometryCache();
	UGeometryCache* BudaiCijiGC = BuDaiCiji->GetGeometryCache();

}


void ADynamicGeometryCacheActor::PostLoad()
{
	Super::PostLoad();
}

void ADynamicGeometryCacheActor::PostActorCreated()
{
	Super::PostActorCreated();
}

// Called every frame
void ADynamicGeometryCacheActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(bFirst)
	{
		bFirst = false;
		FGeometryCacheMeshData* CijiGCMD = Ciji->CacheRenderDatas[0]->MeshData;
			

		for (int32 i = 0; i < CijiGCMD->Indices.Num(); ++i)
		{
			int32 index = CijiGCMD->Indices[i];
			cjMesh.AppendVertex(CijiGCMD->Positions[index]);
		}
		cjMesh.EnableVertexUVs(FVector2f(1, 1));
		for (int32 i = 0; i < CijiGCMD->Indices.Num(); ++i)
		{
			int32 index = CijiGCMD->Indices[i];
			cjMesh.SetVertexUV(index, CijiGCMD->TextureCoordinates[index]);
		}


		for(int32 i = 0; i < CijiGCMD->Indices.Num(); i+=3)
		{
			int32 index1 = CijiGCMD->Indices[i];
			int32 index2 = CijiGCMD->Indices[i+1];
			int32 index3 = CijiGCMD->Indices[i+2];

			cjMesh.AppendTriangle(FIndex3i(index1, index2, index3));
		}

		cjMesh.EnableAttributes();
		FMeshNormals::InitializeOverlayToPerTriangleNormals(cjMesh.Attributes()->PrimaryNormals());
		*(CijiSDM->MeshComponent->GetMesh()) = cjMesh;
		CijiSDM->MeshComponent->NotifyMeshUpdated();

		Ciji->SetVisibility(false);
		BuDaiCiji->SetVisibility(false);
	}
}

void ADynamicGeometryCacheActor::UpdateGeometryCacheMesh()
{

}
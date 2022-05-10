// Fill out your copyright notice in the Description page of Project Settings.


#include "ComputerSecondAnimActor.h"
#include "Kismet/GameplayStatics.h"

#include "GeometryCacheActor.h"
#include "GeometryCacheComponent.h"
#include "GeometryCacheSceneProxy.h"
#include "GeometryCacheTrackStreamable.h"
#include "VectorVM.h"


class FGeomCacheTrackProxy;
// Sets default values
AComputerSecondAnimActor::AComputerSecondAnimActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AComputerSecondAnimActor::BeginPlay()
{
	Super::BeginPlay();



	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), Actors);

	for (AActor* Actor : Actors)
	{
		if (Cast<AGeometryCacheActor>(Actor))
		{
			GEngine->AddOnScreenDebugMessage(-1, 4.0f, FColor(255, 0, 0), Actor->GetActorLabel());

			if (Actor->GetActorLabel() == TEXT("budaiciji"))
			{
				budaiciji = Cast<AGeometryCacheActor>(Actor)->GetGeometryCacheComponent();
			}
			else
			{
				daiciji = Cast<AGeometryCacheActor>(Actor)->GetGeometryCacheComponent();
			}
		}

	}

	if (budaiciji.IsValid() && daiciji.IsValid())
	{
		TArray<FGeometryCacheMeshData> budaicijiMeshs;
		FGeomCacheTrackProxy* budaicijiTrack=budaiciji->CacheRenderDatas[0];

		TArray<FGeometryCacheMeshData> daicijiMeshs;
		FGeomCacheTrackProxy* daicijiTrack= daiciji->CacheRenderDatas[0];;

		for (FGeomCacheTrackProxy* CTP0 : budaiciji->CacheRenderDatas)
		{
			if (UGeometryCacheTrackStreamable* StreamableTrack = Cast<UGeometryCacheTrackStreamable>(CTP0->Track))
			{
				for (int i = 0; i < StreamableTrack->GetChunkNum(); ++i)
				{
					FGeometryCacheMeshData GMD;
					CTP0->GetMeshData(i, GMD);
					budaicijiMeshs.Add(GMD);
				}
			}
		}

		for (FGeomCacheTrackProxy* CTP1 : daiciji->CacheRenderDatas)
		{
			if (UGeometryCacheTrackStreamable* StreamableTrack = Cast<UGeometryCacheTrackStreamable>(CTP1->Track))
			{
				for (int i = 0; i < StreamableTrack->GetChunkNum(); ++i)
				{
					FGeometryCacheMeshData GMD;
					CTP1->GetMeshData(i, GMD);
					daicijiMeshs.Add(GMD);
				}
			}
		}

		if (budaicijiTrack->MeshData->Positions.Num() != daicijiTrack->MeshData->Positions.Num())
		{
			FString Msg = FString::Printf(TEXT("MeshData.Positions diff"));
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor(0, 255, 183), Msg);
		}

		TArray<FGeometryCacheMeshBatchInfo> budaiBatch = budaicijiTrack->MeshData->BatchesInfo;
		TArray<FGeometryCacheMeshBatchInfo> daiBatch = daicijiTrack->MeshData->BatchesInfo;

		if(budaiBatch.Num()== daiBatch.Num())
		{
			int32 num = budaiBatch.Num();
			for(int32 i=0;i<num;++i)
			{
				if(budaiBatch[i].NumTriangles!=daiBatch[i].NumTriangles)
				{
					FString Msg=FString::Printf(TEXT("Batch.NumTriangles diff: %d"),i);
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor(0, 255, 183), Msg);
				}

				if(budaiBatch[i].StartIndex != daiBatch[i].StartIndex)
				{
					FString Msg = FString::Printf(TEXT("Batch.StartIndex diff: %d"), i);
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor(0, 255, 183), Msg);
				}

				if (budaiBatch[i].MaterialIndex != daiBatch[i].MaterialIndex)
				{
					FString Msg = FString::Printf(TEXT("Batch.MaterialIndex diff: %d"), i);
					GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor(0, 255, 183), Msg);
				}
			}
		}

		//int32 MeshNum = daicijiMeshs.Num();
		//if (budaicijiMeshs.Num() == daicijiMeshs.Num())
		//{
		//	ParallelFor(MeshNum, [budaicijiMeshs, daicijiMeshs](int32 MeshIndex)
		//		{
		//			if (budaicijiMeshs[MeshIndex].Positions.Num() == daicijiMeshs[MeshIndex].Positions.Num())
		//			{
		//				for (int j = 0; j < daicijiMeshs[MeshIndex].Positions.Num(); ++j)
		//				{
		//					float dis = FVector3f::Distance(budaicijiMeshs[MeshIndex].Positions[j], daicijiMeshs[MeshIndex].Positions[j]);
		//					FVector3f sub = budaicijiMeshs[MeshIndex].Positions[j] - daicijiMeshs[MeshIndex].Positions[j];
		//					UE_LOG(LogTemp,Warning, TEXT("%.3f,%.3f,%.3f"), sub.X, sub.Y, sub.Z);
		//					if (dis > 0.002f)
		//					{
		//						GEngine->AddOnScreenDebugMessage(-1, 2, FColor(0, 255, 0), TEXT("xxx"));
		//					}
		//				}
		//			}
		//		});
		//}

		//if (budaicijiMeshs.Num() == daicijiMeshs.Num())
		//{
		//	for (int32 i = 0; i < daicijiMeshs.Num(); ++i)
		//	{

		//		for (int j = 0; j < budaicijiMeshs[i].Positions.Num(); ++j)
		//		{

		//			FVector3f sub = budaicijiMeshs[i].Positions[j] - daicijiMeshs[i].Positions[j];
		//			UE_LOG(LogTemp, Warning, TEXT("%.3f,%.3f,%.3f"), sub.X, sub.Y, sub.Z);
		//			//float dis = FVector3f::Distance(budaicijiMeshs[i].Positions[j], daicijiMeshs[i].Positions[j]);
		//			//if (dis > 0.02f)
		//			//{
		//			//	GEngine->AddOnScreenDebugMessage(-1, 2, FColor(0, 255, 0), TEXT("xxx"));
		//			//}
		//		}
		//	}
		//}

		//int32 dacijiNum = daicijiMeshs[0].Positions.Num();
		//int32 budaicijiNum = budaicijiMeshs[0].Positions.Num();
		//int32 subnum= dacijiNum - budaicijiNum;
		//for(int32 i=dacijiNum-1;i>=dacijiNum- subnum-5;--i)
		//{
		//	FVector3f position = daicijiMeshs[0].Positions[i];
		//	UE_LOG(LogTemp, Warning, TEXT("daiciji_End:%.3f,%.3f,%.3f"), position.X, position.Y, position.Z);
		//}

		//for(int32 i=0;i<10;++i)
		//{
		//	FVector3f position = budaicijiMeshs[0].Positions[budaicijiNum - 1 - i];
		//	UE_LOG(LogTemp, Warning, TEXT("budaiciji_End_10:%.3f,%.3f,%.3f"), position.X, position.Y, position.Z);
		//}
		
	}




}

// Called every frame
void AComputerSecondAnimActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


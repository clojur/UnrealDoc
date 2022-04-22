#pragma once
#include "WeightMapTypes.h"
#include "Components/MeshComponent.h"
#include "Curves/RichCurve.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "GeometryFramework/Public/Components/DynamicMeshComponent.h"
#include "Spatial/SampledScalarField2.h"
#include "Util/ProgressCancel.h"


using namespace  UE::Geometry;
class FDynamicMeshOperator;


class FAbortableBackgroundTask : public FNonAbandonableTask
{
private:
	/** pointer to a bool owned somewhere else. If that bool becomes true, this task should cancel further computation */
	bool* bExternalAbortFlag = nullptr;
	/** Internal ProgressCancel instance that can be passed to expensive compute functions, etc */
	FProgressCancel Progress;

public:

	FAbortableBackgroundTask()
	{
		Progress.CancelF = [this]() { return (bExternalAbortFlag != nullptr) ? *bExternalAbortFlag : false; };
	}

	/** Set the abort source flag. */
	void SetAbortSource(bool* bAbortFlagLocation)
	{
		bExternalAbortFlag = bAbortFlagLocation;
	}

	/** @return true if the task should stop computation */
	bool IsAborted()
	{
		return (bExternalAbortFlag != nullptr) ? *bExternalAbortFlag : false;
	}

	/** @return pointer to internal progress object which can be passed to expensive child computations */
	FProgressCancel* GetProgress()
	{
		return &Progress;
	}
};

template<typename DeleteTaskType>
class TDeleterTask : public FNonAbandonableTask
{
	friend class FAsyncTask<TDeleterTask<DeleteTaskType>>;
public:
	TDeleterTask(FAsyncTask<DeleteTaskType>* TaskIn)
		: Task(TaskIn)
	{}

	/** The task we will delete, when it completes */
	FAsyncTask<DeleteTaskType>* Task;

	void DoWork()
	{
		// wait for task to complete
		Task->EnsureCompletion();
		// should always be true if EnsureCompletion() returned
		check(Task->IsDone());
		// delete it
		delete Task;
	}


	// required for task system
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(TDeleterTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};

template<typename OpType>
class TModelingOpTask : public FAbortableBackgroundTask
{
	friend class FAsyncTask<TModelingOpTask<OpType>>;

public:

	TModelingOpTask(TUniquePtr<OpType> OperatorIn) :
		Operator(MoveTemp(OperatorIn))
	{}

	/**
	 * @return the contained computation Operator
	 */
	TUniquePtr<OpType> ExtractOperator()
	{
		return MoveTemp(Operator);
	}

protected:
	TUniquePtr<OpType> Operator;

	// 	FAbortableBackgroundTask API
	void DoWork()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ModelingOpTask_DoWork);
		
		if (Operator)
		{
			Operator->CalculateResult(GetProgress());
		}
	}

	// FAsyncTask framework required function
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(TModelingOpTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};

template<typename TTask>
class FAsyncTaskExecuterWithAbort : public FAsyncTask<TTask>
{
public:
	/** Set to true to abort the child task. CancelAndDelete() should be used in most cases, instead of changing bAbort directly. */
	bool bAbort = false;

	template <typename Arg0Type, typename... ArgTypes>
	FAsyncTaskExecuterWithAbort(Arg0Type&& Arg0, ArgTypes&&... Args)
		: FAsyncTask<TTask>(Forward<Arg0Type>(Arg0), Forward<ArgTypes>(Args)...)

	{
		FAsyncTask<TTask>::GetTask().SetAbortSource(&bAbort);
	}


	/**
	 * Tells the child FAbandonableTask to terminate itself, via the bAbort flag
	 * passed in SetAbortSource, and then starts a TDeleterTask that waits for
	 * completion and 
	 */
	void CancelAndDelete()
	{
		bAbort = true;
		(new FAutoDeleteAsyncTask<TDeleterTask<TTask>>(this))->StartBackgroundTask();
	}
};

class IDynamicMeshOperatorFactory
{
public:
	virtual ~IDynamicMeshOperatorFactory() {}

	virtual TUniquePtr<FDynamicMeshOperator> MakeNewOperator() = 0;
};

namespace  DisplaceMeshToolLocals
{
	class FDisplaceMeshThread;
}


class FDpMeshOptional
{
public:
	FDpMeshOptional(UMeshComponent* OpTarget,UTexture2D* Texture);
	~FDpMeshOptional();
	void StartOptional();
	void UpdateActiveWeightMap();
	void StartComputation();
	void AdvanceComputation();
	void SaveMeshToStaticMesh();
	float WeightMapQuery(const FVector3d& Position, const FIndexedWeightMap& WeightMap) const;

public:
	UMeshComponent* Target=nullptr;
	UTexture2D* DpTexture=nullptr;
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent;
	TObjectPtr<AInternalToolFrameworkActor> PreviewMeshActor = nullptr;
	TWeakObjectPtr<UWorld> TargetWorld;
	UE::Geometry::FDynamicMesh3 OriginalMesh;
	UE::Geometry::FDynamicMeshAABBTree3 OriginalMeshSpatial;
	//TUniquePtr<IDynamicMeshOperatorFactory> Subdivider = nullptr;
	TUniquePtr<IDynamicMeshOperatorFactory> Displacer = nullptr;
	//TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> SubdividedMesh = nullptr;
	TSharedPtr<UE::Geometry::FIndexedWeightMap, ESPMode::ThreadSafe> ActiveWeightMap;
	//FAsyncTaskExecuterWithAbort<TModelingOpTask<FDynamicMeshOperator>>* SubdivideTask = nullptr;
	FAsyncTaskExecuterWithAbort<TModelingOpTask<FDynamicMeshOperator>>* DisplaceTask = nullptr;
	//bool bNeedsSubdivided = true;
	bool bNeedsDisplaced = true;
	DisplaceMeshToolLocals::FDisplaceMeshThread* DisplaceMeshTask=nullptr;
	FDynamicMesh3 ReMesh;
	float TextureAspect=1.0f;
};

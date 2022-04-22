#include "DpMeshOptional.h"

#include "DpPNTriangles.h"
#include "DpUniformTessellate.h"
#include "DynamicMeshToMeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/MeshNormals.h"
#include "Engine/StaticMesh.h"
#include "Image/ImageBuilder.h"
#include "Math/Vector.h"
#include "InteractiveToolsFramework/Public/TargetInterfaces/MeshTargetInterfaceTypes.h"
#include "MeshConversion/Public/MeshDescriptionToDynamicMesh.h"
#include "AssetUtils/Texture2DUtil.h"
#include "GeometryCore/Public/BoxTypes.h"
#include "Misc/AutomationTest.h"

struct FPerlinLayerProperties;

enum class EDisplaceMeshToolDisplaceType : uint8
{
	/** Offset a set distance in the normal direction. */
	Constant,

	DisplacementMap,

	/** Offset vertices randomly. */
	RandomNoise,

	/** Offset in the normal direction weighted by Perlin noise. 
		We use the following formula to compute the weighting for each vertex:
			w = PerlinNoise3D(f * (X + r))
		Where f is a frequency parameter, X is the vertex position, and r is a randomly-generated offset (using the Seed property).
		Note the range of 3D Perlin noise is [-sqrt(3/4), sqrt(3/4)].
	*/
	PerlinNoise,

	/** Move vertices in spatial sine wave pattern */
	SineWave,
};

struct FPerlinLayerProperties
{
	FPerlinLayerProperties() = default;

	FPerlinLayerProperties(float FrequencyIn, float IntensityIn) :
		Frequency(FrequencyIn),
		Intensity(IntensityIn)
	{
	}

	/** Frequency of Perlin noise layer */
	float Frequency = 0.1f;

	/** Intensity/amplitude of Perlin noise layer */
	float Intensity = 1.0f;
};

class FDynamicMeshOperator
{
protected:
	TUniquePtr<FDynamicMesh3> ResultMesh;
	FTransformSRT3d ResultTransform;
	FGeometryResult ResultInfo;

public:
	FDynamicMeshOperator()
	{
		ResultMesh = MakeUnique<FDynamicMesh3>();
		ResultTransform = FTransformSRT3d::Identity();
	}

	virtual ~FDynamicMeshOperator()
	{
	}

	/**
	 * Set the output transform
	 */
	virtual void SetResultTransform(const FTransformSRT3d& Transform)
	{
		ResultTransform = Transform;
	}

	/**
	 * Set the output information
	 */
	virtual void SetResultInfo(const FGeometryResult& Info)
	{
		ResultInfo = Info;
	}

	/**
	 * @return ownership of the internal mesh that CalculateResult() produced
	 */
	TUniquePtr<FDynamicMesh3> ExtractResult()
	{
		return MoveTemp(ResultMesh);
	}

	/**
	 * @return the transform applied to the mesh produced by CalculateResult()
	 */
	const FTransformSRT3d& GetResultTransform() const
	{
		return ResultTransform;
	}

	/**
	 * @return the result information returned by CalculateResult()
	 */
	const FGeometryResult& GetResultInfo() const
	{
		return ResultInfo;
	}

	/**
	 * Calculate the result of the operator. This will populate the internal Mesh and Transform.
	 * @param Progress implementors can use this object to report progress and determine if they should halt and terminate expensive computations
	 */
	virtual void CalculateResult(FProgressCancel* Progress) = 0;
};

namespace DisplaceMeshToolLocals{

	namespace ComputeDisplacement 
	{
		/// Directional Filter: Scale displacement for a given vertex based on how well 
		/// the vertex normal agrees with the specified direction.
		struct FDirectionalFilter
		{
			bool bEnableFilter = false;
			FVector3d FilterDirection = {1,0,0};
			double FilterWidth = 0.1;
			const double RampSlope = 5.0;

			double FilterValue(const FVector3d& EvalNormal) const
			{
				if (!bEnableFilter)	{ return 1.0;}

				double DotWithFilterDirection = EvalNormal.Dot(FilterDirection);
				double Offset = 1.0 / RampSlope;
				double MinX = 1.0 - (2.0 + Offset) * FilterWidth;			// Start increasing here
				double MaxX = FMathd::Min(1.0, MinX + Offset);				// Stop increasing here
				
				if (FMathd::Abs(MaxX - MinX) < FMathd::ZeroTolerance) { return 0.0; }
				
				double Y = (DotWithFilterDirection - MinX) / (MaxX - MinX); // Clamped linear interpolation for the ramp region
				return FMathd::Clamp(Y, 0.0, 1.0);
			}
		};

		template<typename DisplaceFunc>
		void ParallelDisplace(const FDynamicMesh3& Mesh,
			const TArray<FVector3d>& Positions,
			const FMeshNormals& Normals,
			TArray<FVector3d>& DisplacedPositions,
			DisplaceFunc Displace)
		{
			ensure(Positions.Num() == Normals.GetNormals().Num());
			ensure(Positions.Num() == DisplacedPositions.Num());
			ensure(Mesh.VertexCount() == Positions.Num());

			int32 NumVertices = Mesh.MaxVertexID();
			ParallelFor(NumVertices, [&](int32 vid)
			{
				if (Mesh.IsVertex(vid))
				{
					DisplacedPositions[vid] = Displace(vid, Positions[vid], Normals[vid]);
				}
			});
		}


		void Constant(const FDynamicMesh3& Mesh,
			const TArray<FVector3d>& Positions, 
			const FMeshNormals& Normals, 
			TFunctionRef<float(int32, const FVector3d&, const FVector3d&)> IntensityFunc,
			TArray<FVector3d>& DisplacedPositions)
		{
			ParallelDisplace(Mesh, Positions, Normals, DisplacedPositions,
				[&](int32 vid, const FVector3d& Position, const FVector3d& Normal)
			{
				double Intensity = IntensityFunc(vid, Position, Normal);
				return Position + (Intensity * Normal);
			});
		}


		void RandomNoise(const FDynamicMesh3& Mesh,
			const TArray<FVector3d>& Positions, 
			const FMeshNormals& Normals,
			TFunctionRef<float(int32, const FVector3d&, const FVector3d&)> IntensityFunc,
			int RandomSeed, 
			TArray<FVector3d>& DisplacedPositions)
		{
			FMath::SRandInit(RandomSeed);
			for (int vid : Mesh.VertexIndicesItr())
			{
				double RandVal = 2.0 * (FMath::SRand() - 0.5);
				double Intensity = IntensityFunc(vid, Positions[vid], Normals[vid]);
				DisplacedPositions[vid] = Positions[vid] + (Normals[vid] * RandVal * Intensity);
			}
		}

		void PerlinNoise(const FDynamicMesh3& Mesh,
			const TArray<FVector3d>& Positions,
			const FMeshNormals& Normals,
			TFunctionRef<float(int32, const FVector3d&, const FVector3d&)> IntensityFunc,
			const TArray<FPerlinLayerProperties>& PerlinLayerProperties,
			int RandomSeed,
			TArray<FVector3d>& DisplacedPositions)
		{
			FMath::SRandInit(RandomSeed);
			const float RandomOffset = 10000.0f * FMath::SRand();

			ParallelDisplace(Mesh, Positions, Normals, DisplacedPositions,
				[&](int32 vid, const FVector3d& Position, const FVector3d& Normal)
			{
				// Compute the sum of Perlin noise evaluations for this point
				FVector EvalLocation(Position + RandomOffset);
				double TotalNoiseValue = 0.0;
				for (int32 Layer = 0; Layer < PerlinLayerProperties.Num(); ++Layer)
				{
					TotalNoiseValue += PerlinLayerProperties[Layer].Intensity * FMath::PerlinNoise3D(PerlinLayerProperties[Layer].Frequency * EvalLocation);
				}
				double Intensity = IntensityFunc(vid, Position, Normal);
				return Position + (TotalNoiseValue * Intensity * Normal);
			});
		}

		void Map(const FDynamicMesh3& Mesh,
			const TArray<FVector3d>& Positions, 
			const FMeshNormals& Normals,
			TFunctionRef<float(int32, const FVector3d&, const FVector3d&)> IntensityFunc,
			const FSampledScalarField2f& DisplaceField,
			const FSampledScalarField2f& CullField,
			TArray<FVector3d>& DisplacedPositions,
			TArray<int>& TriCullIDs,
			float DisplaceFieldBaseValue = 128.0/255, // value that corresponds to zero displacement
			FVector2f UVScale = FVector2f(1, 1),
			FVector2f UVOffset = FVector2f(0,0),
			FRichCurve* AdjustmentCurve = nullptr)
		{
			const FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->GetUVLayer(0);

			// We set things up such that DisplaceField goes from 0 to 1 in the U direction,
			// but the V direction may be shorter or longer if the texture is not square
			// (it will be 1/AspectRatio)
			float VHeight = DisplaceField.Height() * DisplaceField.CellDimensions.Y;
			
			for (int tid : Mesh.TriangleIndicesItr())
			{
				FIndex3i Tri = Mesh.GetTriangle(tid);
				FIndex3i UVTri = UVOverlay->GetTriangle(tid);
				float CullWeight=0;
				for (int j = 0; j < 3; ++j)
				{
					int vid = Tri[j];
					FVector2f UV = UVOverlay->GetElement(UVTri[j]);

					// Adjust UV value and tile it. 
					// Note that we're effectively stretching the texture to be square before tiling, since this
					// seems to be what non square textures do by default in UE. If we decide to tile without 
					// stretching by default someday, we'd do UV - FVector2f(FMath::Floor(UV.X), FMath:Floor(UV.Y/VHeight)*VHeight)
					// without multiplying by VHeight afterward.
					UV = UV * UVScale + UVOffset;
					UV = UV - FVector2f(FMath::Floor(UV.X), FMath::Floor(UV.Y));
					UV.Y *= VHeight;

					double Offset = DisplaceField.BilinearSampleClamped(UV);
					float  Mask = CullField.BilinearSampleClamped(UV);
					if(Mask<1.0f)
					{
						CullWeight+=1.0f;
					}
					if (AdjustmentCurve)
					{
						Offset = AdjustmentCurve->Eval(Offset);
					}
					Offset -= DisplaceFieldBaseValue;

					double Intensity = IntensityFunc(vid, Positions[vid], Normals[vid]);
					DisplacedPositions[vid] = Positions[vid] + (Offset * Intensity * Normals[vid]);
				}

				if(CullWeight/3.0f > 0.33333f)
				{
					TriCullIDs.Add(tid);
				}
			}
		}
		
		void Sine(const FDynamicMesh3& Mesh,
			const TArray<FVector3d>& Positions,
			const FMeshNormals& Normals,
			TFunctionRef<float(int32, const FVector3d&, const FVector3d&)> IntensityFunc,
			double Frequency,
			double PhaseShift,
			const FVector3d& Direction,
			TArray<FVector3d>& DisplacedPositions)
		{
			FQuaterniond RotateToDirection(Direction, { 0.0, 0.0, 1.0 });

			ParallelDisplace(Mesh, Positions, Normals, DisplacedPositions,
				[&](int32 vid, const FVector3d& Position, const FVector3d& Normal)
			{
				FVector3d RotatedPosition = RotateToDirection * Position;
				double DistXY = FMath::Sqrt(RotatedPosition.X * RotatedPosition.X + RotatedPosition.Y * RotatedPosition.Y);
				double Intensity = IntensityFunc(vid, Position, Normal);
				FVector3d Offset = Intensity * FMath::Sin(Frequency * DistXY + PhaseShift) * Direction;
				return Position + Offset;

			});
		}

	}

	enum class EDisplaceMeshToolSubdivisionType : uint8  
	{
		/** Subdivide the mesh using loop-style subdivision. */
		Flat,
	
		/** Subdivide the mesh using PN triangles which replace each original flat triangle by a curved shape that is 
			retriangulated into a number of small subtriangles. The geometry of a PN triangle is defined as one cubic Bezier 
			patch using control points. The patch matches the point and normal information at the vertices of the original 
			flat triangle.*/
		PNTriangles,
	};
	
	class FSubdivideMeshOp : public FDynamicMeshOperator
	{
	public:
		FSubdivideMeshOp(const FDynamicMesh3& SourceMesh, EDisplaceMeshToolSubdivisionType SubdivisionTypeIn, int SubdivisionsCountIn, TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMap);
		void CalculateResult(FProgressCancel* Progress) final;
	private:
		EDisplaceMeshToolSubdivisionType SubdivisionType;
		int SubdivisionsCount;
	};

	FSubdivideMeshOp::FSubdivideMeshOp(const FDynamicMesh3& SourceMesh, EDisplaceMeshToolSubdivisionType SubdivisionTypeIn, int SubdivisionsCountIn, TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMap)
		: SubdivisionType(SubdivisionTypeIn), SubdivisionsCount(SubdivisionsCountIn)
	{
		ResultMesh->Copy(SourceMesh);

		// If we have a WeightMap, initialize VertexUV.X with weightmap value. Note that we are going to process .Y anyway,
		// we could (for exmaple) speculatively compute another weightmap, or store previous weightmap values there, to support
		// fast switching between two...
		ResultMesh->EnableVertexUVs(FVector2f::Zero());
		if (WeightMap != nullptr)
		{
			for (int32 vid : ResultMesh->VertexIndicesItr())
			{
				ResultMesh->SetVertexUV(vid, FVector2f(WeightMap->GetValue(vid), 0));
			}
		}
		else
		{
			for (int32 vid : ResultMesh->VertexIndicesItr())
			{
				ResultMesh->SetVertexUV(vid, FVector2f::One());
			}
		}
	}

	void FSubdivideMeshOp::CalculateResult(FProgressCancel* ProgressCancel)
	{
		if (SubdivisionType == EDisplaceMeshToolSubdivisionType::Flat) 
		{
			FDpUniformTessellate Tessellator(ResultMesh.Get());
			Tessellator.Progress = ProgressCancel;
			Tessellator.TessellationNum = SubdivisionsCount;
						
			if (Tessellator.Validate() == EOperationValidationResult::Ok) 
			{
				Tessellator.Compute();
			}
		}
		else if (SubdivisionType == EDisplaceMeshToolSubdivisionType::PNTriangles) 
		{
			DpFPNTriangles PNTriangles(ResultMesh.Get());
			PNTriangles.Progress = ProgressCancel;
			PNTriangles.TessellationLevel = SubdivisionsCount;

			if (PNTriangles.Validate() == EOperationValidationResult::Ok)
			{
				PNTriangles.Compute(); 
			}
		}
		else 
		{
			// Unsupported subdivision type
			checkNoEntry();
		}

		
	}

	class FSubdivideMeshOpFactory : public IDynamicMeshOperatorFactory
	{
	public:
		FSubdivideMeshOpFactory(FDynamicMesh3& SourceMeshIn,
			EDisplaceMeshToolSubdivisionType SubdivisionTypeIn,
			int SubdivisionsCountIn,
			TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMapIn)
			: SourceMesh(SourceMeshIn), SubdivisionType(SubdivisionTypeIn), SubdivisionsCount(SubdivisionsCountIn), WeightMap(WeightMapIn)
		{
		}

		void SetSubdivisionType(EDisplaceMeshToolSubdivisionType SubdivisionTypeIn);
		EDisplaceMeshToolSubdivisionType GetSubdivisionType() const;

		void SetSubdivisionsCount(int SubdivisionsCountIn);
		int  GetSubdivisionsCount() const;

		void SetWeightMap(TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMapIn);

		TUniquePtr<FDynamicMeshOperator> MakeNewOperator() final
		{
			return MakeUnique<FSubdivideMeshOp>(SourceMesh, SubdivisionType, SubdivisionsCount, WeightMap);
		}
	private:
		const FDynamicMesh3& SourceMesh;
		EDisplaceMeshToolSubdivisionType SubdivisionType;
		int SubdivisionsCount;
		TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMap;
	};

	void FSubdivideMeshOpFactory::SetSubdivisionType(EDisplaceMeshToolSubdivisionType SubdivisionTypeIn) 
	{
		SubdivisionType = SubdivisionTypeIn;
	}

	EDisplaceMeshToolSubdivisionType FSubdivideMeshOpFactory::GetSubdivisionType() const
	{
		return SubdivisionType;
	}

	void FSubdivideMeshOpFactory::SetSubdivisionsCount(int SubdivisionsCountIn)
	{
		SubdivisionsCount = SubdivisionsCountIn;
	}

	int FSubdivideMeshOpFactory::GetSubdivisionsCount() const
	{
		return SubdivisionsCount;
	}

	void FSubdivideMeshOpFactory::SetWeightMap(TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMapIn)
	{
		WeightMap = WeightMapIn;
	}

	// A collection of parameters to avoid having excess function parameters
	struct DisplaceMeshParameters
	{
		~DisplaceMeshParameters()
		{
			UE_LOG(LogTemp,Warning,TEXT("~DisplaceMeshParameters()"));
		}
		float DisplaceIntensity = 0.0f;
		int RandomSeed = 0;
		UTexture2D* DisplacementMap = nullptr;
		float SineWaveFrequency = 0.0f;
		float SineWavePhaseShift = 0.0f;
		FVector SineWaveDirection = { 0.0f, 0.0f, 0.0f };
		bool bEnableFilter = false;
		FVector FilterDirection = { 0.0f, 0.0f, 0.0f };
		float FilterWidth = 0.0f;
		FSampledScalarField2f DisplaceField;
		FSampledScalarField2f CullField;
		TArray<FPerlinLayerProperties> PerlinLayerProperties;
		bool bRecalculateNormals = true;

		// Used in texture map displacement
		int32 DisplacementMapChannel = 0;
		float DisplacementMapBaseValue = 128.0/255; // i.e., what constitutes no displacement
		FVector2f UVScale = FVector2f(1,1);
		FVector2f UVOffset = FVector2f(0, 0);
		// This gets used by worker threads, so do not try to change an existing curve- make
		// a new one each time.
		TSharedPtr<FRichCurve, ESPMode::ThreadSafe> AdjustmentCurve;

		TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMap;
		TFunction<float(const FVector3d&, const FIndexedWeightMap)> WeightMapQueryFunc;
	};

	class FDisplaceMeshThread : public FRunnable
	{
	public:
		FDisplaceMeshThread(TSharedPtr<FDpMeshOptional> MeshOpIn,FDynamicMesh3* SourceMeshIn,
						TSharedPtr<DisplaceMeshParameters> DisplaceParametersIn,
						EDisplaceMeshToolDisplaceType DisplacementTypeIn,
						EDisplaceMeshToolSubdivisionType SubdivisionTypeIn,
						int SubdivisionsCountIn,
						TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMapIn);
		virtual  ~FDisplaceMeshThread()
		{
			UE_LOG(LogTemp,Warning,TEXT("~FDisplaceMeshThread"));
		}
		void CalculateResult(FProgressCancel* Progress);

		TUniquePtr<FDynamicMesh3> CalculateSubdivisionResult(FProgressCancel* Progress);

		virtual bool Init() override
		{
			return true;
		}
		virtual uint32 Run() override
		{
			double StartTime = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64());
			CalculateResult(nullptr);
			double EndTime = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64());
			UE_LOG(LogTemp,Warning,TEXT("2K_DpRA Subdivision And Dispalcement consuming：%f"),EndTime-StartTime);
			
			return 0;
		}
		virtual void Exit() override
		{
			AsyncTask(ENamedThreads::GameThread,[this]()
			{
				MeshOp->SaveMeshToStaticMesh();
			});
			
			UE_LOG(LogTemp,Warning,TEXT("FDisplaceMeshThread::Exit()"));
		}

		void UpdateDisplaceMap()
		{
			Parameters->DisplacementMapChannel=0;
			if (Parameters->DisplacementMap == nullptr ||
				Parameters->DisplacementMap->GetPlatformData() == nullptr ||
				Parameters->DisplacementMap->GetPlatformData()->Mips.Num() < 1)
			{
				Parameters->DisplaceField = FSampledScalarField2f();
				Parameters->DisplaceField.GridValues.AssignAll(0);

				Parameters->CullField = FSampledScalarField2f();
				Parameters->CullField.GridValues.AssignAll(0);
				return;
			}

			TImageBuilder<FVector4f> DisplacementMapValues;
			if (!UE::AssetUtils::ReadTexture(Parameters->DisplacementMap, DisplacementMapValues,
			                                 // need bPreferPlatformData to be true to respond to non-destructive changes to the texture in the editor
			                                 true))
			{
				Parameters->DisplaceField = FSampledScalarField2f();
				Parameters->DisplaceField.GridValues.AssignAll(0);

				Parameters->CullField = FSampledScalarField2f();
				Parameters->CullField.GridValues.AssignAll(0);
			}
			else
			{
				const FImageDimensions DisplacementMapDimensions = DisplacementMapValues.GetDimensions();
				int64 TextureWidth = DisplacementMapDimensions.GetWidth();
				int64 TextureHeight = DisplacementMapDimensions.GetHeight();
				Parameters->DisplaceField.Resize(TextureWidth, TextureHeight, 0.0f);
				Parameters->CullField.Resize(TextureWidth, TextureHeight, 0.0f);

				// Note that the height of the texture will not be 1.0 if it was not square. This should be kept in mind when sampling it later.
				Parameters->DisplaceField.SetCellSize(1.0f / (float)TextureWidth);
				Parameters->CullField.SetCellSize(1.0f / (float)TextureWidth);
				for (int64 y = 0; y < TextureHeight; ++y)
				{
					for (int64 x = 0; x < TextureWidth; ++x)
					{
						Parameters->DisplaceField.GridValues[y * TextureWidth + x] =
							DisplacementMapValues.GetPixel(y * TextureWidth + x)[Parameters->DisplacementMapChannel];

						Parameters->CullField.GridValues[y * TextureWidth + x] =
							DisplacementMapValues.GetPixel(y * TextureWidth + x)[2];
					}
				}
			}
		}

		bool IsDone()
		{
			return bDone;
		}

	private:
		FDynamicMesh3* SourceMesh;
		TSharedPtr<DisplaceMeshParameters>  Parameters;
		EDisplaceMeshToolDisplaceType DisplacementType;
		TArray<FVector3d> SourcePositions;
		FMeshNormals SourceNormals;
		TArray<FVector3d> DisplacedPositions;
		TArray<float> CullMask;
		EDisplaceMeshToolSubdivisionType SubdivisionType;
		int SubdivisionsCount;
		TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMap;
		TUniquePtr<FDynamicMesh3> ResultMesh;
		bool bDone=false;
		TSharedPtr<FDpMeshOptional> MeshOp;
	};

	FDisplaceMeshThread::FDisplaceMeshThread(TSharedPtr<FDpMeshOptional> MeshOpIn,FDynamicMesh3* SourceMeshIn,
									 TSharedPtr<DisplaceMeshParameters>  DisplaceParametersIn,
									 EDisplaceMeshToolDisplaceType DisplacementTypeIn,
									EDisplaceMeshToolSubdivisionType SubdivisionTypeIn,
									int SubdivisionsCountIn,
						TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMapIn)
		:MeshOp(MeshOpIn)
		,SourceMesh(SourceMeshIn)
		,Parameters(DisplaceParametersIn)
		,DisplacementType(DisplacementTypeIn)
		,SubdivisionType(SubdivisionTypeIn)
		,SubdivisionsCount(SubdivisionsCountIn)
	    ,WeightMap(WeightMapIn)
	{
		bDone=false;
		//MeshOp=nullptr;
		ResultMesh = MakeUnique<FDynamicMesh3>();
	}
	

	TUniquePtr<FDynamicMesh3> FDisplaceMeshThread::CalculateSubdivisionResult(FProgressCancel* Progress)
	{
		TUniquePtr<FDynamicMesh3> SubdivisionedMesh=MakeUnique<FDynamicMesh3>();
		SubdivisionedMesh->Copy(*SourceMesh);
		// If we have a WeightMap, initialize VertexUV.X with weightmap value. Note that we are going to process .Y anyway,
		// we could (for exmaple) speculatively compute another weightmap, or store previous weightmap values there, to support
		// fast switching between two...
		SubdivisionedMesh->EnableVertexUVs(FVector2f::Zero());
		if (WeightMap != nullptr)
		{
			for (int32 vid : SubdivisionedMesh->VertexIndicesItr())
			{
				SubdivisionedMesh->SetVertexUV(vid, FVector2f(WeightMap->GetValue(vid), 0));
			}
		}
		else
		{
			for (int32 vid : SubdivisionedMesh->VertexIndicesItr())
			{
				SubdivisionedMesh->SetVertexUV(vid, FVector2f::One());
			}
		}
		
		if (SubdivisionType == EDisplaceMeshToolSubdivisionType::Flat) 
		{
			FDpUniformTessellate Tessellator(SubdivisionedMesh.Get());
			Tessellator.Progress = Progress;
			Tessellator.TessellationNum = SubdivisionsCount;
						
			if (Tessellator.Validate() == EOperationValidationResult::Ok) 
			{
				Tessellator.Compute();
			}
		}
		else if (SubdivisionType == EDisplaceMeshToolSubdivisionType::PNTriangles) 
		{
			DpFPNTriangles PNTriangles(SubdivisionedMesh.Get());
			PNTriangles.Progress = Progress;
			PNTriangles.TessellationLevel = SubdivisionsCount;

			if (PNTriangles.Validate() == EOperationValidationResult::Ok)
			{
				PNTriangles.Compute(); 
			}
		}
		else 
		{
			// Unsupported subdivision type
			checkNoEntry();
		}

		
		return SubdivisionedMesh;
	}

	void FDisplaceMeshThread::CalculateResult(FProgressCancel* Progress)
	{	
		TUniquePtr<FDynamicMesh3> SubdivisionMesh= CalculateSubdivisionResult(Progress);
		ResultMesh->Copy(*SubdivisionMesh);
		bool bMapOk=false;
		AsyncTask(ENamedThreads::GameThread,[this,&bMapOk]()
		{
			UpdateDisplaceMap();
			bMapOk=true;
		});

		while (!bMapOk)
		{
			FPlatformProcess::Sleep(0.01);
		}
		
		if (Progress && Progress->Cancelled()) return;

		
		if (DisplacementType == EDisplaceMeshToolDisplaceType::DisplacementMap && !Parameters->DisplacementMap)
		{
			return;
		}

		
		SourceNormals = FMeshNormals(SubdivisionMesh.Get());
		SourceNormals.ComputeVertexNormals();

		if (Progress && Progress->Cancelled()) return;
		// cache initial positions
		SourcePositions.SetNum(SubdivisionMesh->MaxVertexID());
		for (int vid : ResultMesh->VertexIndicesItr())
		{
			SourcePositions[vid] = SubdivisionMesh->GetVertex(vid);
		}

		if (Progress && Progress->Cancelled()) return;
		DisplacedPositions.SetNum(SubdivisionMesh->MaxVertexID());
		CullMask.SetNum(SubdivisionMesh->MaxVertexID());
		TArray<int> TriCullIDs;
		if (Progress && Progress->Cancelled()) return;

		ComputeDisplacement::FDirectionalFilter DirectionalFilter{ Parameters->bEnableFilter,
			FVector3d(Parameters->FilterDirection),
			Parameters->FilterWidth };
		double Intensity = Parameters->DisplaceIntensity;

		TUniqueFunction<float(int32 vid, const FVector3d&)> WeightMapQueryFunc = [&](int32, const FVector3d&) { return 1.0f; };
		if (Parameters->WeightMap.IsValid())
		{
			if (SubdivisionMesh->IsCompactV() && SubdivisionMesh->VertexCount() == Parameters->WeightMap->Num())
			{
				WeightMapQueryFunc = [&](int32 vid, const FVector3d& Pos) { return Parameters->WeightMap->GetValue(vid); };
			}
			else
			{
				// disable input query function as it uses expensive AABBTree lookup
				//WeightMapQueryFunc = [&](int32 vid, const FVector3d& Pos) { return Parameters->WeightMapQueryFunc(Pos, *Parameters->WeightMap); };
				WeightMapQueryFunc = [&](int32 vid, const FVector3d& Pos) { return SubdivisionMesh->GetVertexUV(vid).X; };
			}
		}
		auto IntensityFunc = [&](int32 vid, const FVector3d& Position, const FVector3d& Normal) 
		{
			return Intensity * DirectionalFilter.FilterValue(Normal) * WeightMapQueryFunc(vid, Position);
		};



		FDynamicMesh3 CopyMesh;
		CopyMesh.Copy(*SubdivisionMesh);
		// compute Displaced positions in PositionBuffer
		switch (DisplacementType)
		{
		default:
		case EDisplaceMeshToolDisplaceType::Constant:
			ComputeDisplacement::Constant(*SubdivisionMesh, 
				SourcePositions, 
				SourceNormals,
				IntensityFunc,
				DisplacedPositions);
			break;

		case EDisplaceMeshToolDisplaceType::RandomNoise:
			ComputeDisplacement::RandomNoise(*SubdivisionMesh, 
				SourcePositions, 
				SourceNormals,
				IntensityFunc,
				Parameters->RandomSeed,
				DisplacedPositions);
			break;
			
		case EDisplaceMeshToolDisplaceType::PerlinNoise:
			ComputeDisplacement::PerlinNoise(*SubdivisionMesh,
				SourcePositions,
				SourceNormals,
				IntensityFunc,
				Parameters->PerlinLayerProperties,	
				Parameters->RandomSeed,
				DisplacedPositions);
			break;

		case EDisplaceMeshToolDisplaceType::DisplacementMap:
			ComputeDisplacement::Map(CopyMesh, 
				SourcePositions, 
				SourceNormals,
				IntensityFunc,
				Parameters->DisplaceField,
				Parameters->CullField,
				DisplacedPositions,
				TriCullIDs,
				Parameters->DisplacementMapBaseValue,
				Parameters->UVScale,
				Parameters->UVOffset,
				Parameters->AdjustmentCurve.Get());
			break;

		case EDisplaceMeshToolDisplaceType::SineWave:
			ComputeDisplacement::Sine(*SubdivisionMesh, 
				SourcePositions, 
				SourceNormals,
				IntensityFunc,
				Parameters->SineWaveFrequency,
				Parameters->SineWavePhaseShift,
				(FVector3d)Parameters->SineWaveDirection,
				DisplacedPositions);
			break;
		}
		// update preview vertex positions
		for (int vid : CopyMesh.VertexIndicesItr())
		{
			CopyMesh.SetVertex(vid, DisplacedPositions[vid]);
		}

		// Cull Mesh
		for(int cid:TriCullIDs)
		{
			CopyMesh.RemoveTriangle(cid);
		}
		// recalculate normals
		if (Parameters->bRecalculateNormals)
		{
			if (CopyMesh.HasAttributes())
			{
				FMeshNormals Normals(&CopyMesh);
				FDynamicMeshNormalOverlay* NormalOverlay = CopyMesh.Attributes()->PrimaryNormals();
				Normals.RecomputeOverlayNormals(NormalOverlay);
				Normals.CopyToOverlay(NormalOverlay);
			}
			else
			{
				FMeshNormals::QuickComputeVertexNormals(CopyMesh);
			}
		}

		MeshOp->ReMesh.Copy(CopyMesh);
		bDone=true;
	}
	
	class FSubdivideDisplaceMeshOp : public FDynamicMeshOperator
	{
	public:
		FSubdivideDisplaceMeshOp(FDynamicMesh3* SourceMeshIn,
						const DisplaceMeshParameters& DisplaceParametersIn,
						EDisplaceMeshToolDisplaceType DisplacementTypeIn,
						EDisplaceMeshToolSubdivisionType SubdivisionTypeIn,
						int SubdivisionsCountIn,
						TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMapIn);
		void CalculateResult(FProgressCancel* Progress) final;

		void SaveSubdivisionResult();

		TUniquePtr<FDynamicMesh3> CalculateSubdivisionResult(FProgressCancel* Progress);

	private:
		FDynamicMesh3* SourceMesh;
		DisplaceMeshParameters Parameters;
		EDisplaceMeshToolDisplaceType DisplacementType;
		TArray<FVector3d> SourcePositions;
		FMeshNormals SourceNormals;
		TArray<FVector3d> DisplacedPositions;
		TArray<float> CullMask;
		EDisplaceMeshToolSubdivisionType SubdivisionType;
		int SubdivisionsCount;
		TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMap;
	};

	FSubdivideDisplaceMeshOp::FSubdivideDisplaceMeshOp(FDynamicMesh3* SourceMeshIn,
									 const DisplaceMeshParameters& DisplaceParametersIn,
									 EDisplaceMeshToolDisplaceType DisplacementTypeIn,
									EDisplaceMeshToolSubdivisionType SubdivisionTypeIn,
									int SubdivisionsCountIn,
						TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMapIn)
		:SourceMesh(SourceMeshIn) 
		,Parameters(DisplaceParametersIn)
		,DisplacementType(DisplacementTypeIn)
		,SubdivisionType(SubdivisionTypeIn)
		,SubdivisionsCount(SubdivisionsCountIn)
	    ,WeightMap(WeightMapIn)
	{

	}

	void FSubdivideDisplaceMeshOp::SaveSubdivisionResult()
	{

	}

	TUniquePtr<FDynamicMesh3> FSubdivideDisplaceMeshOp::CalculateSubdivisionResult(FProgressCancel* Progress)
	{
		TUniquePtr<FDynamicMesh3> SubdivisionedMesh=MakeUnique<FDynamicMesh3>();
		SubdivisionedMesh->Copy(*SourceMesh);
		// If we have a WeightMap, initialize VertexUV.X with weightmap value. Note that we are going to process .Y anyway,
		// we could (for exmaple) speculatively compute another weightmap, or store previous weightmap values there, to support
		// fast switching between two...
		SubdivisionedMesh->EnableVertexUVs(FVector2f::Zero());
		if (WeightMap != nullptr)
		{
			for (int32 vid : SubdivisionedMesh->VertexIndicesItr())
			{
				SubdivisionedMesh->SetVertexUV(vid, FVector2f(WeightMap->GetValue(vid), 0));
			}
		}
		else
		{
			for (int32 vid : SubdivisionedMesh->VertexIndicesItr())
			{
				SubdivisionedMesh->SetVertexUV(vid, FVector2f::One());
			}
		}
		
		if (SubdivisionType == EDisplaceMeshToolSubdivisionType::Flat) 
		{
			FDpUniformTessellate Tessellator(SubdivisionedMesh.Get());
			Tessellator.Progress = Progress;
			Tessellator.TessellationNum = SubdivisionsCount;
						
			if (Tessellator.Validate() == EOperationValidationResult::Ok) 
			{
				Tessellator.Compute();
			}
		}
		else if (SubdivisionType == EDisplaceMeshToolSubdivisionType::PNTriangles) 
		{
			DpFPNTriangles PNTriangles(SubdivisionedMesh.Get());
			PNTriangles.Progress = Progress;
			PNTriangles.TessellationLevel = SubdivisionsCount;

			if (PNTriangles.Validate() == EOperationValidationResult::Ok)
			{
				PNTriangles.Compute(); 
			}
		}
		else 
		{
			// Unsupported subdivision type
			checkNoEntry();
		}

		
		return SubdivisionedMesh;
	}

	void FSubdivideDisplaceMeshOp::CalculateResult(FProgressCancel* Progress)
	{	
		FString Msg=TEXT("Converting DpTexture To Nanite...");
		FScopedSlowTask SlowComputation(100, FText::FromString(Msg));
		SlowComputation.EnterProgressFrame(10);		
		TUniquePtr<FDynamicMesh3> SubdivisionMesh= CalculateSubdivisionResult(Progress);
		ResultMesh->Copy(*SubdivisionMesh);
		if (Progress && Progress->Cancelled()) return;
		SlowComputation.EnterProgressFrame(10);	
		SaveSubdivisionResult();
		SlowComputation.EnterProgressFrame(30);
		
		if (DisplacementType == EDisplaceMeshToolDisplaceType::DisplacementMap && !Parameters.DisplacementMap)
		{
			return;
		}

		
		SourceNormals = FMeshNormals(SubdivisionMesh.Get());
		SourceNormals.ComputeVertexNormals();

		if (Progress && Progress->Cancelled()) return;
		// cache initial positions
		SourcePositions.SetNum(SubdivisionMesh->MaxVertexID());
		for (int vid : ResultMesh->VertexIndicesItr())
		{
			SourcePositions[vid] = SubdivisionMesh->GetVertex(vid);
		}

		if (Progress && Progress->Cancelled()) return;
		DisplacedPositions.SetNum(SubdivisionMesh->MaxVertexID());
		CullMask.SetNum(SubdivisionMesh->MaxVertexID());
		TArray<int> TriCullIDs;
		SlowComputation.EnterProgressFrame(10);
		if (Progress && Progress->Cancelled()) return;

		ComputeDisplacement::FDirectionalFilter DirectionalFilter{ Parameters.bEnableFilter,
			FVector3d(Parameters.FilterDirection),
			Parameters.FilterWidth };
		double Intensity = Parameters.DisplaceIntensity;

		TUniqueFunction<float(int32 vid, const FVector3d&)> WeightMapQueryFunc = [&](int32, const FVector3d&) { return 1.0f; };
		if (Parameters.WeightMap.IsValid())
		{
			if (SubdivisionMesh->IsCompactV() && SubdivisionMesh->VertexCount() == Parameters.WeightMap->Num())
			{
				WeightMapQueryFunc = [&](int32 vid, const FVector3d& Pos) { return Parameters.WeightMap->GetValue(vid); };
			}
			else
			{
				// disable input query function as it uses expensive AABBTree lookup
				//WeightMapQueryFunc = [&](int32 vid, const FVector3d& Pos) { return Parameters.WeightMapQueryFunc(Pos, *Parameters.WeightMap); };
				WeightMapQueryFunc = [&](int32 vid, const FVector3d& Pos) { return SourceMesh->GetVertexUV(vid).X; };
			}
		}
		auto IntensityFunc = [&](int32 vid, const FVector3d& Position, const FVector3d& Normal) 
		{
			return Intensity * DirectionalFilter.FilterValue(Normal) * WeightMapQueryFunc(vid, Position);
		};



		// compute Displaced positions in PositionBuffer
		switch (DisplacementType)
		{
		default:
		case EDisplaceMeshToolDisplaceType::Constant:
			ComputeDisplacement::Constant(*SubdivisionMesh, 
				SourcePositions, 
				SourceNormals,
				IntensityFunc,
				DisplacedPositions);
			break;

		case EDisplaceMeshToolDisplaceType::RandomNoise:
			ComputeDisplacement::RandomNoise(*SubdivisionMesh, 
				SourcePositions, 
				SourceNormals,
				IntensityFunc,
				Parameters.RandomSeed,
				DisplacedPositions);
			break;
			
		case EDisplaceMeshToolDisplaceType::PerlinNoise:
			ComputeDisplacement::PerlinNoise(*SubdivisionMesh,
				SourcePositions,
				SourceNormals,
				IntensityFunc,
				Parameters.PerlinLayerProperties,	
				Parameters.RandomSeed,
				DisplacedPositions);
			break;

		case EDisplaceMeshToolDisplaceType::DisplacementMap:
			ComputeDisplacement::Map(*SubdivisionMesh, 
				SourcePositions, 
				SourceNormals,
				IntensityFunc,
				Parameters.DisplaceField,
				Parameters.CullField,
				DisplacedPositions,
				TriCullIDs,
				Parameters.DisplacementMapBaseValue,
				Parameters.UVScale,
				Parameters.UVOffset,
				Parameters.AdjustmentCurve.Get());
			break;

		case EDisplaceMeshToolDisplaceType::SineWave:
			ComputeDisplacement::Sine(*SubdivisionMesh, 
				SourcePositions, 
				SourceNormals,
				IntensityFunc,
				Parameters.SineWaveFrequency,
				Parameters.SineWavePhaseShift,
				(FVector3d)Parameters.SineWaveDirection,
				DisplacedPositions);
			break;
		}
		SlowComputation.EnterProgressFrame(20);
		// update preview vertex positions
		for (int vid : ResultMesh->VertexIndicesItr())
		{
			ResultMesh->SetVertex(vid, DisplacedPositions[vid]);
		}

		// Cull Mesh
		for(int cid:TriCullIDs)
		{
			ResultMesh->RemoveTriangle(cid);
		}
		SlowComputation.EnterProgressFrame(10);
		// recalculate normals
		if (Parameters.bRecalculateNormals)
		{
			if (ResultMesh->HasAttributes())
			{
				FMeshNormals Normals(ResultMesh.Get());
				FDynamicMeshNormalOverlay* NormalOverlay = ResultMesh->Attributes()->PrimaryNormals();
				Normals.RecomputeOverlayNormals(NormalOverlay);
				Normals.CopyToOverlay(NormalOverlay);
			}
			else
			{
				FMeshNormals::QuickComputeVertexNormals(*ResultMesh);
			}
		}
	}

	class FDisplaceMeshOpFactory : public IDynamicMeshOperatorFactory
	{
	public:
		FDisplaceMeshOpFactory(FDynamicMesh3* SourceMeshIn,
			const DisplaceMeshParameters& DisplaceParametersIn,
			EDisplaceMeshToolDisplaceType DisplacementTypeIn,
			EDisplaceMeshToolSubdivisionType SubdivisionTypeIn,
			int SubdivisionsCountIn,
			TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMapIn)
			: SourceMesh(SourceMeshIn)
			, SubdivisionType(SubdivisionTypeIn)
			, SubdivisionsCount(SubdivisionsCountIn)
			, WeightMap(WeightMapIn)
		{
			SetIntensity(DisplaceParametersIn.DisplaceIntensity);
			SetRandomSeed(DisplaceParametersIn.RandomSeed);
			SetDisplacementMap(DisplaceParametersIn.DisplacementMap, DisplaceParametersIn.DisplacementMapChannel); // Calls UpdateMap
			SetFrequency(DisplaceParametersIn.SineWaveFrequency);
			SetPhaseShift(DisplaceParametersIn.SineWavePhaseShift);
			SetSineWaveDirection(DisplaceParametersIn.SineWaveDirection);
			SetEnableDirectionalFilter(DisplaceParametersIn.bEnableFilter);
			SetFilterDirection(DisplaceParametersIn.FilterDirection);
			SetFilterFalloffWidth(DisplaceParametersIn.FilterWidth);
			SetPerlinNoiseLayerProperties(DisplaceParametersIn.PerlinLayerProperties);
			SetDisplacementType(DisplacementTypeIn);

			Parameters.WeightMap = DisplaceParametersIn.WeightMap;
			Parameters.WeightMapQueryFunc = DisplaceParametersIn.WeightMapQueryFunc;

			Parameters.DisplacementMapBaseValue = DisplaceParametersIn.DisplacementMapBaseValue;
			Parameters.UVScale = DisplaceParametersIn.UVScale;
			Parameters.UVOffset = DisplaceParametersIn.UVOffset;

			Parameters.AdjustmentCurve = DisplaceParametersIn.AdjustmentCurve;
		}
		void SetIntensity(float IntensityIn);
		void SetRandomSeed(int RandomSeedIn);
		void SetDisplacementMap(UTexture2D* DisplacementMapIn, int32 ChannelIn);
		void SetDisplacementMapUVAdjustment(const FVector2f& UVScale, const FVector2f& UVOffset);
		void SetDisplacementMapBaseValue(float DisplacementMapBaseValue);
		void SetAdjustmentCurve(UCurveFloat* CurveFloat);
		void SetFrequency(float FrequencyIn);
		void SetPhaseShift(float PhaseShiftIn);
		void SetSineWaveDirection(const FVector& Direction);
		void SetDisplacementType(EDisplaceMeshToolDisplaceType TypeIn);
		void SetEnableDirectionalFilter(bool EnableDirectionalFilter);
		void SetFilterDirection(const FVector& Direction);
		void SetFilterFalloffWidth(float FalloffWidth);
		void SetPerlinNoiseLayerProperties(const TArray<FPerlinLayerProperties>& PerlinLayerProperties);
		void SetWeightMap(TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMap);
		void SetRecalculateNormals(bool bRecalculateNormals);

		TUniquePtr<FDynamicMeshOperator> MakeNewOperator() final
		{
			return MakeUnique<FSubdivideDisplaceMeshOp>(SourceMesh, Parameters, DisplacementType, SubdivisionType,
									SubdivisionsCount,WeightMap);
		}
	private:
		void UpdateMap();

		DisplaceMeshParameters Parameters;
		EDisplaceMeshToolDisplaceType DisplacementType;

		FDynamicMesh3* SourceMesh;
		EDisplaceMeshToolSubdivisionType SubdivisionType;
		int SubdivisionsCount;
		TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMap;
	};

	void FDisplaceMeshOpFactory::SetIntensity(float IntensityIn)
	{
		Parameters.DisplaceIntensity = IntensityIn;
	}

	void FDisplaceMeshOpFactory::SetRandomSeed(int RandomSeedIn)
	{
		Parameters.RandomSeed = RandomSeedIn;
	}

	void FDisplaceMeshOpFactory::SetDisplacementMap(UTexture2D* DisplacementMapIn, int32 ChannelIn)
	{
		Parameters.DisplacementMap = DisplacementMapIn;
		Parameters.DisplacementMapChannel = ChannelIn;

		// Note that we do the update even if we got the same pointer, because the texture
		// may have been changed in the editor.
		UpdateMap();
	}

	void FDisplaceMeshOpFactory::SetDisplacementMapUVAdjustment(const FVector2f& UVScale, const FVector2f& UVOffset)
	{
		Parameters.UVScale = UVScale;
		Parameters.UVOffset = UVOffset;
	}

	void FDisplaceMeshOpFactory::SetDisplacementMapBaseValue(float DisplacementMapBaseValue)
	{
		// We could bake this into the displacement field, but that would require calling UpdateMap with
		// every slider change, which is slow. So we'll just pass this down to the calculation.
		Parameters.DisplacementMapBaseValue = DisplacementMapBaseValue;
	}

	void FDisplaceMeshOpFactory::SetAdjustmentCurve(UCurveFloat* CurveFloat)
	{
		Parameters.AdjustmentCurve = CurveFloat ? TSharedPtr<FRichCurve, ESPMode::ThreadSafe>(
			static_cast<FRichCurve*>(CurveFloat->FloatCurve.Duplicate()))
			: nullptr;
	}

	void FDisplaceMeshOpFactory::SetFrequency(float FrequencyIn)
	{
		Parameters.SineWaveFrequency = FrequencyIn;
	}

	void FDisplaceMeshOpFactory::SetPhaseShift(float PhaseShiftIn)
	{
		Parameters.SineWavePhaseShift = PhaseShiftIn;
	}

	void FDisplaceMeshOpFactory::SetSineWaveDirection(const FVector& Direction)
	{
		Parameters.SineWaveDirection = Direction.GetSafeNormal();
	}

	void FDisplaceMeshOpFactory::SetDisplacementType(EDisplaceMeshToolDisplaceType TypeIn)
	{
		DisplacementType = TypeIn;
	}

	void FDisplaceMeshOpFactory::UpdateMap()
	{
		if (Parameters.DisplacementMap == nullptr ||
			Parameters.DisplacementMap->GetPlatformData() == nullptr ||
			Parameters.DisplacementMap->GetPlatformData()->Mips.Num() < 1)
		{
			Parameters.DisplaceField = FSampledScalarField2f();
			Parameters.DisplaceField.GridValues.AssignAll(0);

			Parameters.CullField = FSampledScalarField2f();
			Parameters.CullField.GridValues.AssignAll(0);
			return;
		}

		TImageBuilder<FVector4f> DisplacementMapValues;
		if (!UE::AssetUtils::ReadTexture(Parameters.DisplacementMap, DisplacementMapValues,
			// need bPreferPlatformData to be true to respond to non-destructive changes to the texture in the editor
			true)) 
		{
			Parameters.DisplaceField = FSampledScalarField2f();
			Parameters.DisplaceField.GridValues.AssignAll(0);
			
			Parameters.CullField = FSampledScalarField2f();
			Parameters.CullField.GridValues.AssignAll(0);
		}
		else
		{
			const FImageDimensions DisplacementMapDimensions = DisplacementMapValues.GetDimensions();
			int64 TextureWidth = DisplacementMapDimensions.GetWidth();
			int64 TextureHeight = DisplacementMapDimensions.GetHeight();
			Parameters.DisplaceField.Resize(TextureWidth, TextureHeight, 0.0f);
			Parameters.CullField.Resize(TextureWidth, TextureHeight, 0.0f);
			// Note that the height of the texture will not be 1.0 if it was not square. This should be kept in mind when sampling it later.
			Parameters.DisplaceField.SetCellSize(1.0f / (float)TextureWidth);
			Parameters.CullField.SetCellSize(1.0f / (float)TextureWidth);

			for (int64 y = 0; y < TextureHeight; ++y)
			{
				for (int64 x = 0; x < TextureWidth; ++x)
				{
					Parameters.DisplaceField.GridValues[y * TextureWidth + x] = 
						DisplacementMapValues.GetPixel(y * TextureWidth + x)[Parameters.DisplacementMapChannel];
					
					Parameters.CullField.GridValues[y * TextureWidth + x]=
						DisplacementMapValues.GetPixel(y * TextureWidth + x)[2];
				}
			}
		}
	}

	void FDisplaceMeshOpFactory::SetEnableDirectionalFilter(bool EnableDirectionalFilter)
	{
		Parameters.bEnableFilter = EnableDirectionalFilter;
	}

	void FDisplaceMeshOpFactory::SetFilterDirection(const FVector& Direction)
	{
		Parameters.FilterDirection = Direction.GetSafeNormal();
	}

	void FDisplaceMeshOpFactory::SetFilterFalloffWidth(float FalloffWidth)
	{
		Parameters.FilterWidth = FalloffWidth;
	}

	void FDisplaceMeshOpFactory::SetPerlinNoiseLayerProperties(const TArray<FPerlinLayerProperties>& LayerProperties )
	{
		Parameters.PerlinLayerProperties = LayerProperties;
	}

	void FDisplaceMeshOpFactory::SetWeightMap(TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> WeightMapIn)
	{
		Parameters.WeightMap = WeightMapIn;
	}

	void FDisplaceMeshOpFactory::SetRecalculateNormals(bool RecalcNormalsIn)
	{
		Parameters.bRecalculateNormals = RecalcNormalsIn;
	}

} // namespace



FDpMeshOptional::FDpMeshOptional(UMeshComponent* OpTarget,UTexture2D* Texture)
	: Target(OpTarget)
	, DpTexture(Texture)
{
}

FDpMeshOptional::~FDpMeshOptional()
{
	UE_LOG(LogTemp,Warning,TEXT("~FDpMeshOptional"));
}

void FDpMeshOptional::StartOptional()
{
	TargetWorld = Target->GetWorld();
	
	UpdateActiveWeightMap();
	
	FActorSpawnParameters SpawnInfo;
	PreviewMeshActor = TargetWorld->SpawnActor<AInternalToolFrameworkActor>(
		FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	DynamicMeshComponent = NewObject<UDynamicMeshComponent>(PreviewMeshActor);
	DynamicMeshComponent->SetTangentsType(EDynamicMeshComponentTangentsMode::AutoCalculated);
	DynamicMeshComponent->SetupAttachment(PreviewMeshActor->GetRootComponent());
	DynamicMeshComponent->RegisterComponent();
	DynamicMeshComponent->SetWorldTransform(Target->GetComponentToWorld());
	DynamicMeshComponent->bExplicitShowWireframe = false; //show wireframe
	TArray<UMaterialInterface*> MaterialSet = Target->GetMaterials();
	// transfer materials
	for (int k = 0; k < MaterialSet.Num(); ++k)
	{
		DynamicMeshComponent->SetMaterial(k, MaterialSet[k]);
	}
	const FMeshDescription* FoundMeshDescription = nullptr;
	static bool bFirst = true;
	static FMeshDescription EmptyMeshDescription;
	if (bFirst)
	{
		FStaticMeshAttributes Attributes(EmptyMeshDescription);
		Attributes.Register();
		bFirst = false;
	}
	if (UStaticMesh* StaticMesh = Cast<UStaticMeshComponent>(Target)->GetStaticMesh())
	{
		EMeshLODIdentifier UseLOD = EMeshLODIdentifier::LOD0;


		FoundMeshDescription = (UseLOD == EMeshLODIdentifier::HiResSource)
			                       ? StaticMesh->GetHiResMeshDescription()
			                       : StaticMesh->GetMeshDescription((int32)UseLOD);

		if (FoundMeshDescription == nullptr)
		{
			FoundMeshDescription = &EmptyMeshDescription;
		}
	}

	using namespace DisplaceMeshToolLocals;
	//convert static mesh to dynamic mesh
	UE::Geometry::FDynamicMesh3 DynamicMesh;
	FMeshDescriptionToDynamicMesh Converter;
	Converter.Convert(FoundMeshDescription, DynamicMesh);
	DynamicMeshComponent->SetMesh(MoveTemp(DynamicMesh));
	OriginalMesh.Copy(*DynamicMeshComponent->GetMesh());
	OriginalMeshSpatial.SetMesh(&OriginalMesh, true);
	TSharedPtr<DisplaceMeshParameters> Parameters=MakeShared<DisplaceMeshParameters>();
	Parameters->DisplaceIntensity=5.0f;
	Parameters->DisplacementMap=DpTexture;
	Parameters->DisplacementMapChannel=0;
	Parameters->WeightMap = ActiveWeightMap;
	Parameters->WeightMapQueryFunc = [this](const FVector3d& Position, const FIndexedWeightMap& WeightMap) { return WeightMapQuery(Position, WeightMap);	};
	//hide target
	Target->SetVisibility(false);

	constexpr int MaxTriangles = 3000000;
	double NumTriangles = OriginalMesh.MaxTriangleID();
	int MaxSubdivisions = (int)(FMath::Sqrt(MaxTriangles/NumTriangles) - 1);

	//use Texel count as subdivisions;
	int Subdivisions;
	if(DpTexture->GetImportedSize().X>DpTexture->GetImportedSize().Y)
	{
		Subdivisions=DpTexture->GetImportedSize().X;
	}
	else
	{
		Subdivisions=DpTexture->GetImportedSize().Y;
	}
	
	TextureAspect=(float)DpTexture->GetImportedSize().X / (float)DpTexture->GetImportedSize().Y;


	using namespace  DisplaceMeshToolLocals;
	
	//Displacer = MakeUnique<FDisplaceMeshOpFactory>(&OriginalMesh, Parameters,  EDisplaceMeshToolDisplaceType::DisplacementMap
		//												,EDisplaceMeshToolSubdivisionType::PNTriangles,4096,ActiveWeightMap);
	//Subdivider = MakeUnique<FSubdivideMeshOpFactory>(OriginalMesh, EDisplaceMeshToolSubdivisionType::PNTriangles, 4096, ActiveWeightMap);

	UMaterialInterface* Material = LoadObject<UMaterial>(nullptr, TEXT("/MeshModelingToolsetExp/Materials/InProgressMaterial"));
	DynamicMeshComponent->SetOverrideRenderMaterial(Material);
	
	if(bNeedsDisplaced)
	{
		TSharedPtr<FDpMeshOptional> WarpPtr(this);
		 DisplaceMeshTask=new FDisplaceMeshThread (WarpPtr,&OriginalMesh, Parameters,  EDisplaceMeshToolDisplaceType::DisplacementMap
													   ,EDisplaceMeshToolSubdivisionType::PNTriangles,Subdivisions,ActiveWeightMap);
		FRunnableThread::Create(DisplaceMeshTask,TEXT("DpDisplaceTask"));
	}
	
}

void FDpMeshOptional::UpdateActiveWeightMap()
{
	// if (CommonProperties->WeightMap == FName(TEXT("None")))
	// {
	// 	ActiveWeightMap = nullptr;
	// }
	// else
	// {
	// 	TSharedPtr<FIndexedWeightMap, ESPMode::ThreadSafe> NewWeightMap = MakeShared<FIndexedWeightMap, ESPMode::ThreadSafe>();
	// 	const FMeshDescription* MeshDescription = UE::ToolTarget::GetMeshDescription(Target);
	// 	UE::WeightMaps::GetVertexWeightMap(MeshDescription, CommonProperties->WeightMap, *NewWeightMap, 1.0f);
	// 	if (CommonProperties->bInvertWeightMap)
	// 	{
	// 		NewWeightMap->InvertWeightMap();
	// 	}
	// 	ActiveWeightMap = NewWeightMap;
	// }

	ActiveWeightMap = nullptr;
}

void FDpMeshOptional::StartComputation()
{

	// if ( bNeedsSubdivided )
	// {
	// 	if (SubdivideTask)
	// 	{
	// 		SubdivideTask->CancelAndDelete();
	// 	}
	// 	SubdividedMesh = nullptr;
	// 	SubdivideTask = new FAsyncTaskExecuterWithAbort<TModelingOpTask<FDynamicMeshOperator>>(Subdivider->MakeNewOperator());
	// 	SubdivideTask->StartBackgroundTask();
	// 	bNeedsSubdivided = false;
	// 	DynamicMeshComponent->SetOverrideRenderMaterial(Material);
	// }
	// if (bNeedsDisplaced && DisplaceTask)
	// {
	// 	DisplaceTask->CancelAndDelete();
	// 	DisplaceTask = nullptr;
	// 	DynamicMeshComponent->SetOverrideRenderMaterial(Material);
	// }
	
	AdvanceComputation();
}



void FDpMeshOptional::AdvanceComputation()
{
	using namespace DisplaceMeshToolLocals;

	// if (bNeedsDisplaced)
	// {
	// 	// force update of contrast curve
	// 	//FDisplaceMeshOpFactory* DisplacerDownCast = static_cast<FDisplaceMeshOpFactory*>(Displacer.Get());
	// 	//DisplacerDownCast->SetAdjustmentCurve(TextureMapProperties->bApplyAdjustmentCurve ? TextureMapProperties->AdjustmentCurve : nullptr);
	//
	// 	DisplaceTask = new FAsyncTaskExecuterWithAbort<TModelingOpTask<FDynamicMeshOperator>>(
	// 		Displacer->MakeNewOperator());
	// 	DisplaceTask->StartBackgroundTask();
	// 	bNeedsDisplaced = false;
	// }
	
	

}

void FDpMeshOptional::SaveMeshToStaticMesh()
{
	// TUniquePtr<FDynamicMesh3> DisplacedMesh = DisplaceTask->GetTask().ExtractOperator()->ExtractResult();

	FString Msg=TEXT("Converting Mesh To Nanite...");
	FScopedSlowTask SlowTask(100, FText::FromString(Msg));
	
	DynamicMeshComponent->ClearOverrideRenderMaterial();
	DynamicMeshComponent->GetMesh()->Copy(ReMesh);
	DynamicMeshComponent->NotifyMeshUpdated();

	Target->SetVisibility(true);
	FTransform OriginTransform=Target->GetComponentTransform();
	OriginTransform.SetScale3D(TVector<double>(1.0f,1.0f/TextureAspect,1.0f));
	Target->SetWorldTransform(OriginTransform);
	UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Target);
	Component->Mobility=EComponentMobility::Static;
	UStaticMesh* StaticMesh = Component->GetStaticMesh();
	FMeshDescription* FoundMeshDescription = nullptr;
	FConversionToMeshDescriptionOptions ConversionOptions;
	ConversionOptions.bSetPolyGroups = true;
	ConversionOptions.bUpdatePositions = true;
	ConversionOptions.bUpdateNormals = true;
	ConversionOptions.bUpdateTangents = false;
	ConversionOptions.bUpdateUVs = false;
	ConversionOptions.bUpdateVtxColors = false;
	ConversionOptions.bTransformVtxColorsSRGBToLinear = false;
	FDynamicMeshToMeshDescription Converter(ConversionOptions);
	FoundMeshDescription = StaticMesh->GetMeshDescription(0);
	SlowTask.EnterProgressFrame(10);
	
	double StartTime = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64());
	Converter.Convert(DynamicMeshComponent->GetMesh(), *FoundMeshDescription);
	double EndTime = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64());
	
	UE_LOG(LogTemp,Warning,TEXT("2K_DpRA FDynamicMeshToMeshDescription::Convert consuming：%f"),EndTime-StartTime);
	
	SlowTask.EnterProgressFrame(20);

	double StartTime1 = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64());
	//save and convert to nanite
	StaticMesh->NaniteSettings.bEnabled = true;
	
	// make sure transactional flag is on for this asset
	StaticMesh->SetFlags(RF_Transactional);
	// mark as modified
	StaticMesh->Modify();
	StaticMesh->ModifyMeshDescription(0);
	StaticMesh->CommitMeshDescription(0);
	FStaticMeshSourceModel& ThisSourceModel = StaticMesh->GetSourceModel(0);
	ThisSourceModel.ReductionSettings.PercentTriangles = 1.f;
	ThisSourceModel.ReductionSettings.PercentVertices = 1.f;
	DynamicMeshComponent->UnregisterComponent();
	DynamicMeshComponent->DestroyComponent();
	DynamicMeshComponent = nullptr;
	StaticMesh->MarkPackageDirty();
	//StaticMesh->OnMeshChanged.Broadcast();
	SlowTask.EnterProgressFrame(20);
	StaticMesh->PostEditChange();
	Component->RecreatePhysicsState();

	double EndTime1 = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64());

	UE_LOG(LogTemp,Warning,TEXT("2K_DpRA StaticMesh Build to nanite consuming：%f"),EndTime1-StartTime1);
	
	if (PreviewMeshActor != nullptr)
	{
		PreviewMeshActor->Destroy();
		PreviewMeshActor = nullptr;
	}

	if(DisplaceMeshTask)
	{
		delete DisplaceTask;
		DisplaceTask=nullptr;
	}
}

float FDpMeshOptional::WeightMapQuery(const FVector3d& Position, const FIndexedWeightMap& WeightMap) const
{
	double NearDistSqr;
	int32 NearTID = OriginalMeshSpatial.FindNearestTriangle(Position, NearDistSqr);
	if (NearTID < 0)
	{
		return 1.0f;
	}
	FDistPoint3Triangle3d Distance = TMeshQueries<FDynamicMesh3>::TriangleDistance(OriginalMesh, NearTID, Position);
	FIndex3i Tri = OriginalMesh.GetTriangle(NearTID);
	return WeightMap.GetInterpValue(Tri, Distance.TriangleBaryCoords);
}

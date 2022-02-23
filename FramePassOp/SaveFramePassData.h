#pragma once
#include "ScreenPass.h"
#include "CoreMinimal.h"
//#include "../SceneRendering.h"
//#include "RHICommandList.h"
//#include "Shader.h"
//#include "RHIStaticStates.h"
//#include "../ScenePrivate.h"
//#include "RenderGraph.h"
//#include "PixelShaderUtils.h"
//#include "../SceneTextureParameters.h"
//#include "../DeferredShadingRenderer.h"
#include "GlobalShader.h"
#include "PixelShaderUtils.h"

//js//////////////
#include "Serialization/BufferArchive.h"
#include "Templates/SharedPointer.h"
#include "ImageWrapper/Public/IImageWrapper.h"
#include "ImageWrapper/Public/IImageWrapperModule.h"
#include "ImageUtils.h"

struct UnrealInsertFrameDataGather
{
	struct DepthPixel	//定义深度像素结构体
	{
		float depth;
		char stencil;
		char unused1;
		char unused2;
		char unused3;
	};

	struct VelocityPixel	//定义深度像素结构体
	{
		uint16 R;
		uint16 G;
		uint16 B;
		uint16 A;
	};


	static void MyWriteExrImage(const TArray64<uint8>& RawData, FArchive& Ar, FIntPoint BufferSize, int32 BitsPerPixel, ERGBFormat RGBFormat)
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::Get().LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

		TSharedPtr<IImageWrapper> EXRImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);

		EXRImageWrapper->SetRaw(RawData.GetData(), RawData.GetAllocatedSize(), BufferSize.X, BufferSize.Y, RGBFormat, BitsPerPixel);

		const TArray64<uint8>& Data = EXRImageWrapper->GetCompressed(100);

		Ar.Serialize((void*)Data.GetData(), Data.GetAllocatedSize());
	}

	template<typename DataFormat>
	static void OutImageByRenderTarget(FTexture2DRHIRef uTexRes, const FString& outImagePath, const FString& outDataFilePath, bool isVelocityData)
	{
		FIntPoint BufferSize = uTexRes->GetSizeXY();
		FIntRect Rect = FIntRect(0, 0, BufferSize.X, BufferSize.Y);
		///不可信，要看renderdoc输出
		//EPixelFormat Format = uTexRes->GetFormat();
		//int32 ImageBytes = CalculateImageBytes(BufferSize.X, BufferSize.Y, 0, Format);
		//int32 oneSize= GPixelFormats[Format].BlockBytes;
		///
		TArray64<DataFormat> Data;

		Data.AddUninitialized(BufferSize.X * BufferSize.Y * sizeof(DataFormat));
		uint32 Lolstrid = 0;
		void* buffer = RHILockTexture2D(uTexRes, 0, RLM_ReadOnly, Lolstrid, true);	// 加锁 获取可读Texture数组首地址
		memcpy(Data.GetData(), buffer, BufferSize.X * BufferSize.Y * sizeof(DataFormat));		//复制数据
		RHIUnlockTexture2D(uTexRes, 0, true);	//解锁

		//保存数据成图片
		TArray<FColor> ConvertToColor;
		for (DataFormat& color : Data)
		{
			if constexpr (std::is_same<DataFormat, DepthPixel>::value)
			{
				ConvertToColor.Add(FColor(color.depth * 255, color.depth * 255, color.depth * 255, 255));
			}

			if constexpr (std::is_same<DataFormat, VelocityPixel>::value)
			{
				ConvertToColor.Add(FColor(color.R / 65535.0f * 255, color.G / 65535.0f * 255, color.B / 65535.0f * 255, color.A / 65535.0f * 255));
			}

			if constexpr (std::is_same<DataFormat, FFloat16Color>::value)
			{
				ConvertToColor.Add(FColor(color.R.GetFloat() * 255, color.G.GetFloat() * 255, color.B.GetFloat() * 255, 255));
			}
		}

		TArray<uint8> compressedBitmapColor;
		FImageUtils::CompressImageArray(BufferSize.X, BufferSize.Y, ConvertToColor, compressedBitmapColor);
		FFileHelper::SaveArrayToFile(compressedBitmapColor, outImagePath.GetCharArray().GetData());


		//保存数据到文件
		//if (!outDataFilePath.IsEmpty())
		//{

		//	TArray<FString> DataFile;
		//	FString DataNote;
		//	if constexpr (std::is_same<DataFormat, DepthPixel>::value)
		//	{
		//		DataNote = FString::Printf(TEXT("### resolution:%d*%d  dataFormat:%ls"), BufferSize.X, BufferSize.Y, TEXT("D32_Float"));
		//	}
		//	else
		//	{
		//		DataNote = FString::Printf(TEXT("### resolution:%d*%d  dataFormat:%ls"), BufferSize.X, BufferSize.Y, TEXT("R16B16G16A16_Float"));
		//	}
		//	DataFile.Add(DataNote);
		//	for (int y = 0; y < BufferSize.Y; y++)
		//	{
		//		FString DataLine;
		//		for (int x = 0; x < BufferSize.X; x++)
		//		{
		//			if constexpr (std::is_same<DataFormat, DepthPixel>::value)
		//			{
		//				uint32 index = y * x + x;
		//				DataLine += FString::Printf(TEXT("%f,"), Data[index].depth);
		//			}
		//			else
		//			{
		//				uint32 index = y * x + x;
		//				if (isVelocityData)
		//				{
		//					DataLine += FString::Printf(TEXT("%f %f %f %f,"),
		//						Data[index].R.GetFloat(), Data[index].G.GetFloat(),
		//						Data[index].B.GetFloat(), Data[index].A.GetFloat());
		//				}
		//				else
		//				{
		//					DataLine += FString::Printf(TEXT("%f %f %f %f,"),
		//						Data[index].R.GetFloat(), Data[index].G.GetFloat(),
		//						Data[index].B.GetFloat(), 1.0f);
		//				}
		//			}
		//		}

		//		DataFile.Add(DataLine);
		//	}

		//	FFileHelper::SaveStringArrayToFile(DataFile, outDataFilePath.GetCharArray().GetData());
		//}

		//保存到指定格式
		if (!outDataFilePath.IsEmpty())
		{
			TArray64<uint8> RawData;
			TArray64<FLinearColor> temp;

			int32 ImageBytes = BufferSize.X * BufferSize.Y * sizeof(FLinearColor);
			RawData.AddUninitialized(ImageBytes);

			if constexpr (std::is_same<DataFormat, DepthPixel>::value)
			{
				for (const DepthPixel& dp : Data)
				{
					temp.Add(FLinearColor(dp.depth, dp.depth, dp.depth, 1.0));
				}

			}

			if constexpr (std::is_same<DataFormat, VelocityPixel>::value)
			{
				for (const VelocityPixel& vp : Data)
				{
					temp.Add(FLinearColor(vp.R / 65535.0f, vp.G / 65535.0f, vp.B / 65535.0f, vp.A / 65535.0f));
				}
			}

			if constexpr (std::is_same<DataFormat, FFloat16Color>::value)
			{
				for (const FFloat16Color& vp : Data)
				{
					temp.Add(FLinearColor(vp.R.GetFloat(), vp.G.GetFloat(), vp.B.GetFloat(), vp.A.GetFloat()));
				}
			}

			FMemory::Memcpy(RawData.GetData(), temp.GetData(), ImageBytes);
			FArchive* Ar = IFileManager::Get().CreateFileWriter(*outDataFilePath);
			if (Ar)
			{
				FBufferArchive Buffer;
				MyWriteHDRImage<FLinearColor>(RawData, Buffer, BufferSize);
				Ar->Serialize(const_cast<uint8*>(Buffer.GetData()), Buffer.Num());
				delete Ar;
			}
		}
	}

	bool ExportDataToHDR(FTexture2DRHIRef RenderTarget, const FString& outDataFilePath)
	{
		TArray64<uint8> RawData;

		EPixelFormat Format = RenderTarget->GetFormat();

		int32 ImageBytes = CalculateImageBytes(RenderTarget->GetSizeXY().X, RenderTarget->GetSizeXY().Y, 0, Format);

		RawData.AddUninitialized(ImageBytes);
		bool bReadSuccess = false;
		if (Format == EPixelFormat::PF_FloatRGBA || Format == EPixelFormat::PF_A32B32G32R32F)
		{
			FRenderTarget;
			TArray<FFloat16Color>* FloatColors;

			ENQUEUE_RENDER_COMMAND(ReadSurfaceFloatCommand)(
				[RenderTarget, FloatColors](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.ReadSurfaceFloatData(
						RenderTarget,
						FIntRect(0, 0, RenderTarget->GetSizeXY().X, RenderTarget->GetSizeXY().Y),
						*FloatColors,
						CubeFace_PosX,
						0,
						0
					);
				});
			FlushRenderingCommands();
			FMemory::Memcpy(RawData.GetData(), FloatColors->GetData(), ImageBytes);
		}
		else
		{
			TArray<FColor>* OutData;
			ENQUEUE_RENDER_COMMAND(ReadSceneColorCommand)(
				[RenderTarget, OutData](FRHICommandListImmediate& RHICmdList)
				{
					RHICmdList.ReadSurfaceData(
						RenderTarget,
						FIntRect(0, 0, RenderTarget->GetSizeXY().X, RenderTarget->GetSizeXY().Y),
						*OutData,
						FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)
					);
				});
			FlushRenderingCommands();
			FMemory::Memcpy(RawData.GetData(), OutData->GetData(), ImageBytes);
		}

		//FArchive* Ar = IFileManager::Get().CreateFileWriter(*outDataFilePath);
		//if (Ar)
		//{
		//	FBufferArchive Buffer;

		//	bool bSuccess = false;

		//	if (TextureRenderTarget->RenderTargetFormat == RTF_RGBA16f)
		//	{
		//		// Note == is case insensitive
		//		if (FPaths::GetExtension(TotalFileName) == TEXT("HDR"))
		//		{
		//			bSuccess = FImageUtils::ExportRenderTarget2DAsHDR(TextureRenderTarget, Buffer);
		//		}
		//		else
		//		{
		//			bSuccess = FImageUtils::ExportRenderTarget2DAsEXR(TextureRenderTarget, Buffer);
		//		}

		//	}
		//	else
		//	{
		//		bSuccess = FImageUtils::ExportRenderTarget2DAsPNG(TextureRenderTarget, Buffer);
		//	}

		//	if (bSuccess)
		//	{
		//		Ar->Serialize(const_cast<uint8*>(Buffer.GetData()), Buffer.Num());
		//	}

		//	delete Ar;
		//}
		//else
		//{
		//	check(false);
		//}

		return true;
	}

	static void MyWriteScanLine(FArchive& Ar, const TArray<uint8>& ScanLine)
	{
		const uint8* LineEnd = ScanLine.GetData() + ScanLine.Num();
		const uint8* LineSource = ScanLine.GetData();
		TArray<uint8> Output;
		Output.Reserve(ScanLine.Num() * 2);
		while (LineSource < LineEnd)
		{
			int32 CurrentPos = 0;
			int32 NextPos = 0;
			int32 CurrentRunLength = 0;
			while (CurrentRunLength <= 4 && NextPos < 128 && LineSource + NextPos < LineEnd)
			{
				CurrentPos = NextPos;
				CurrentRunLength = 0;
				while (CurrentRunLength < 127 && CurrentPos + CurrentRunLength < 128 && LineSource + NextPos < LineEnd && LineSource[CurrentPos] == LineSource[NextPos])
				{
					NextPos++;
					CurrentRunLength++;
				}
			}

			if (CurrentRunLength > 4)
			{
				// write a non run: LineSource[0] to LineSource[CurrentPos]
				if (CurrentPos > 0)
				{
					Output.Add(CurrentPos);
					for (int32 i = 0; i < CurrentPos; i++)
					{
						Output.Add(LineSource[i]);
					}
				}
				Output.Add((uint8)(128 + CurrentRunLength));
				Output.Add(LineSource[CurrentPos]);
			}
			else
			{
				// write a non run: LineSource[0] to LineSource[NextPos]
				Output.Add((uint8)(NextPos));
				for (int32 i = 0; i < NextPos; i++)
				{
					Output.Add((uint8)(LineSource[i]));
				}
			}
			LineSource += NextPos;
		}
		Ar.Serialize(Output.GetData(), Output.Num());
	}
	static void MyWriteHDRHeader(FArchive& Ar, FIntPoint BufferSize)
	{
		const int32 MaxHeaderSize = 256;
		char Header[MAX_SPRINTF];
		FCStringAnsi::Sprintf(Header, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n", BufferSize.Y, BufferSize.X);
		Header[MaxHeaderSize - 1] = 0;
		int32 Len = FMath::Min(FCStringAnsi::Strlen(Header), MaxHeaderSize);
		Ar.Serialize(Header, Len);
	}
	static FColor ToRGBEDithered(const FLinearColor& ColorIN, const FRandomStream& Rand)
	{
		const float R = ColorIN.R;
		const float G = ColorIN.G;
		const float B = ColorIN.B;
		const float Primary = FMath::Max3(R, G, B);
		FColor	ReturnColor;

		if (Primary < 1E-32)
		{
			ReturnColor = FColor(0, 0, 0, 0);
		}
		else
		{
			int32 Exponent;
			const float Scale = frexp(Primary, &Exponent) / Primary * 255.f;

			ReturnColor.R = FMath::Clamp(FMath::TruncToInt((R * Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.G = FMath::Clamp(FMath::TruncToInt((G * Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.B = FMath::Clamp(FMath::TruncToInt((B * Scale) + Rand.GetFraction()), 0, 255);
			ReturnColor.A = FMath::Clamp(FMath::TruncToInt(Exponent), -128, 127) + 128;
		}

		return ReturnColor;
	}
	template<typename TSourceColorType>
	static void MyWriteHDRBits(FArchive& Ar, TSourceColorType* SourceTexels, FIntPoint BufferSize)
	{
		const FRandomStream RandomStream(0xA1A1);
		const int32 NumChannels = 4;
		const int32 SizeX = BufferSize.X;
		const int32 SizeY = BufferSize.Y;
		TArray<uint8> ScanLine[NumChannels];
		for (int32 Channel = 0; Channel < NumChannels; Channel++)
		{
			ScanLine[Channel].Reserve(SizeX);
		}

		for (int32 y = 0; y < SizeY; y++)
		{
			// write RLE header
			uint8 RLEheader[4];
			RLEheader[0] = 2;
			RLEheader[1] = 2;
			RLEheader[2] = SizeX >> 8;
			RLEheader[3] = SizeX & 0xFF;
			Ar.Serialize(&RLEheader[0], sizeof(RLEheader));

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				ScanLine[Channel].Reset();
			}

			for (int32 x = 0; x < SizeX; x++)
			{
				FLinearColor LinearColor(*SourceTexels);
				FColor RGBEColor = ToRGBEDithered(LinearColor, RandomStream);

				FLinearColor lintest = RGBEColor.FromRGBE();
				ScanLine[0].Add(RGBEColor.R);
				ScanLine[1].Add(RGBEColor.G);
				ScanLine[2].Add(RGBEColor.B);
				ScanLine[3].Add(RGBEColor.A);
				SourceTexels++;
			}

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				MyWriteScanLine(Ar, ScanLine[Channel]);
			}
		}
	}
	template<typename DataFormat>
	static void MyWriteHDRImage(const TArray64<uint8>& RawData, FArchive& Ar, FIntPoint BufferSize)
	{
		MyWriteHDRHeader(Ar, BufferSize);
		MyWriteHDRBits(Ar, (DataFormat*)RawData.GetData(), BufferSize);
	}

	static void MyWriteHDRImage(const TArray<FLinearColor>& RawData, FArchive& Ar, FIntPoint BufferSize)
	{
		MyWriteHDRHeader(Ar, BufferSize);
		MyWriteHDRBits(Ar, (FLinearColor*)RawData.GetData(), BufferSize);
	}

	static void InsertFrameDataGather(const TArray<FViewInfo>& Views, const FSceneRenderTargets& SceneContext)
	{
		UGameViewportClient* GameViewport = GEngine->GetCurrentPlayWorld()->GetGameViewport();
		if (GEngine->GameViewport != nullptr)
		{
			static  uint32 frameNums = 0;

			if (frameNums < 11)
			{
				frameNums++;
				return;
			}

			if (Views.Num() > 1)
			{
				check(false);
				return;
			}


			//输出buffer
			//if (SceneContext.SceneVelocity != nullptr)
			//{
			//	FTexture2DRHIRef uVelocityTexRes = SceneContext.SceneVelocity->GetRenderTargetItem().TargetableTexture->GetTexture2D();
			//	FString CImgFile = FString::Printf(TEXT("F:/Outs/VelocityImages/VelocityImage_%d.png"), frameNums);
			//	FString CDataFile = FString::Printf(TEXT("F:/Outs/VelocityDataFile/VelocityData_%d.HDR"), frameNums);
			//	OutImageByRenderTarget<VelocityPixel>(uVelocityTexRes, CImgFile, CDataFile, true);
			//}

			if (SceneContext.GetSceneColorSurface())
			{
				FTexture2DRHIRef uTexRes = SceneContext.GetSceneColorSurface()->GetTexture2D();
				FString CImgFile = FString::Printf(TEXT("F:/Outs/SceneImage/SceneLinearImage_%d.png"), frameNums);
				FString CDataFile = FString::Printf(TEXT("F:/Outs/SceneDataFile/SceneLinearData_%d.HDR"), frameNums);
				OutImageByRenderTarget<FFloat16Color>(uTexRes, CImgFile, CDataFile, false);
			}
/*
			if (SceneContext.GetSceneDepthSurface())
			{
				FTexture2DRHIRef uTexRes = SceneContext.GetSceneDepthSurface();
				FString CImgFile = FString::Printf(TEXT("F:/Outs/DepthImages/DepthImage_%d.png"), frameNums);
				FString CDataFile = FString::Printf(TEXT("F:/Outs/DepthDataFile/DepthData_%d.HDR"), frameNums);
				OutImageByRenderTarget<DepthPixel>(uTexRes, CImgFile, CDataFile, false);
			}


			//输出MVP
			if (SceneContext.SceneVelocity && SceneContext.GetSceneDepthSurface() && SceneContext.GetSceneColorSurface())
			{
				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					const FViewInfo& View = Views[ViewIndex];
					FMatrix viewMatrix = View.ViewMatrices.GetViewMatrix();
					FMatrix projectMatrix = View.ViewMatrices.GetProjectionMatrix();
					TArray<FString> ViewMatData;
					TArray<FString> ProjectData;

					//viewmatrix info 
					FString Begin = FString::Printf(TEXT("%d {"), frameNums);
					ViewMatData.Add(Begin);
					FString line1 = FString::Printf(TEXT("%f,%f,%f,%f"),
						viewMatrix.M[0][0], viewMatrix.M[0][1], viewMatrix.M[0][2], viewMatrix.M[0][3]);
					ViewMatData.Add(line1);
					FString line2 = FString::Printf(TEXT("%f,%f,%f,%f"),
						viewMatrix.M[1][0], viewMatrix.M[1][1], viewMatrix.M[1][2], viewMatrix.M[1][3]);
					ViewMatData.Add(line2);
					FString line3 = FString::Printf(TEXT("%f,%f,%f,%f"),
						viewMatrix.M[2][0], viewMatrix.M[2][1], viewMatrix.M[2][2], viewMatrix.M[2][3]);
					ViewMatData.Add(line3);
					FString line4 = FString::Printf(TEXT("%f,%f,%f,%f"),
						viewMatrix.M[3][0], viewMatrix.M[3][1], viewMatrix.M[3][2], viewMatrix.M[3][3]);
					ViewMatData.Add(line4);
					FString End = TEXT("}");
					ViewMatData.Add(End);
					FFileHelper::SaveStringArrayToFile(ViewMatData, TEXT("F:/Outs/ViewMatrixData.txt"), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);

					//project matrix info
					ProjectData.Add(Begin);
					FString pline1 = FString::Printf(TEXT("%f,%f,%f,%f"),
						projectMatrix.M[0][0], projectMatrix.M[0][1], projectMatrix.M[0][2], projectMatrix.M[0][3]);
					ProjectData.Add(pline1);
					FString pline2 = FString::Printf(TEXT("%f,%f,%f,%f"),
						projectMatrix.M[1][0], projectMatrix.M[1][1], projectMatrix.M[1][2], projectMatrix.M[1][3]);
					ProjectData.Add(pline2);
					FString pline3 = FString::Printf(TEXT("%f,%f,%f,%f"),
						projectMatrix.M[2][0], projectMatrix.M[2][1], projectMatrix.M[2][2], projectMatrix.M[2][3]);
					ProjectData.Add(pline3);
					FString pline4 = FString::Printf(TEXT("%f,%f,%f,%f"),
						projectMatrix.M[3][0], projectMatrix.M[3][1], projectMatrix.M[3][2], projectMatrix.M[3][3]);
					ProjectData.Add(pline4);
					ProjectData.Add(End);
					FFileHelper::SaveStringArrayToFile(ProjectData, TEXT("F:/Outs/ProjectMatrixData.txt"), FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
				}
			}
*/

			frameNums++;
		}

	}
};
//js//////////////

FScreenPassTexture AddMyLightFlowPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FRDGTextureRef& InputTexture);

//struct LightFlow
//{
//
//	inline void LightFlowOutPass(FIntPoint BufferSize, const FViewInfo& View, FRDGBuilder& GraphBuilder, FRDGTextureRef SceneColorTexture)
//	{
//		RDG_EVENT_SCOPE(GraphBuilder, "LightFlowSampler(ViewId=%d)", ViewIndex);
//		FRDGTextureDesc RTDesc;
//		RTDesc.ClearValue = FClearValueBinding();
//		RTDesc.Dimension = ETextureDimension::Texture2D;
//		RTDesc.Flags = TexCreate_RenderTargetable;
//		RTDesc.Format = PF_R16G16B16A16_UNORM;
//		RTDesc.Extent = BufferSize;
//		FRDGTextureRef LightFlowRT = GraphBuilder.CreateTexture(RTDesc, TEXT("MyRt"));
//		FScreenPassRenderTarget Output = FScreenPassRenderTarget(LightFlowRT, ERenderTargetLoadAction::ENoAction);
//		const FScreenPassTextureViewport OutputViewport(Output);
//		const FScreenPassTextureViewport ColorViewport(FScreenPassTexture(SceneColorTexture));
//
//		FMyLightFlowSamplerPS::FParameters* LFPassParameters = GraphBuilder.AllocParameters<FMyLightFlowSamplerPS::FParameters>();
//		LFPassParameters->InputColorSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
//		LFPassParameters->InputColorTexture = SceneColorTexture;
//		LFPassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();
//		TShaderMapRef<FMyLightFlowSamplerPS> PixelShader(View.ShaderMap);
//
//		AddDrawScreenPass(
//			GraphBuilder,
//			RDG_EVENT_NAME("LightFlowSampler %dx%d", OutputViewport.Rect.Width(), OutputViewport.Rect.Height()),
//			View,
//			OutputViewport,
//			ColorViewport,
//			PixelShader,
//			LFPassParameters);
//
//		//GraphBuilder.AddPass(
//		//	RDG_EVENT_NAME("LightFlowSampler"),
//		//	LFPassParameters,
//		//	ERDGPassFlags::Raster,
//		//	[LFPassParameters, &View, PixelShader, BufferSize](FRHICommandList& RHICmdList)
//		//	{
//		//		FPixelShaderUtils::DrawFullscreenPixelShader(RHICmdList, View.ShaderMap, PixelShader, *LFPassParameters, FIntRect(0, 0, BufferSize.X, BufferSize.Y));
//		//	}
//		//);
//	}
//};

//void FDeferredShadingSceneRenderer::SimpleUseRenderGraph(FRHICommandListImmediate& RHICmdList)
//{
//	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
//
//	FRDGBuilder GraphBuilder(RHICmdList);
//
//	FIntPoint TexBufferSize = FIntPoint(SceneContext.GetSceneDepthTexture()->GetSizeX(), SceneContext.GetSceneDepthTexture()->GetSizeY());
//
//	FRDGTextureRef SceneColor = GraphBuilder.RegisterExternalTexture(SceneContext.GetSceneColor(), TEXT("SceneColor"));
//
//	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
//	{
//		FViewInfo& View = Views[ViewIndex];
//
//		FSceneTextureParameters SceneTextures;
//		SetupSceneTextureParameters(GraphBuilder, &SceneTextures);
//
//		RDG_EVENT_SCOPE(GraphBuilder, "SimpleUseRenderGraph(ViewId=%d)", ViewIndex);
//
//		TShaderMapRef<FSimpleUseRenderGraphPS> PixelShader(View.ShaderMap);
//
//		FSimpleUseRenderGraphPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FSimpleUseRenderGraphPS::FParameters>();
//		PassParameters->RenderTargets[0] = FRenderTargetBinding(SceneColor, ERenderTargetLoadAction::ELoad);
//		ClearUnusedGraphResources(*PixelShader, PassParameters);
//
//		GraphBuilder.AddPass(
//			RDG_EVENT_NAME("SimpleTest"),
//			PassParameters,
//			ERDGPassFlags::Raster,
//			[PassParameters, &View, PixelShader, TexBufferSize](FRHICommandList& RHICmdList)
//			{
//				FPixelShaderUtils::DrawFullscreenPixelShader(RHICmdList, View.ShaderMap, *PixelShader, *PassParameters, FIntRect(0, 0, TexBufferSize.X, TexBufferSize.Y));
//			});
//
//	}
//	GraphBuilder.Execute();
//}
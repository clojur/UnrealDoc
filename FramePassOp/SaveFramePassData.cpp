#include "SaveFramePassData.h"
//#include "SceneTextureParameters.h"

#include "CanvasTypes.h"
#include "RenderTargetTemp.h"
#include "ClearQuad.h"
#include "ScenePrivate.h"
#include "SceneRenderTargets.h"

namespace
{
	class FMyLightFlowSamplerPS : public FGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FMyLightFlowSamplerPS);
		SHADER_USE_PARAMETER_STRUCT(FMyLightFlowSamplerPS, FGlobalShader)

			BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
			SHADER_PARAMETER_SAMPLER(SamplerState, InputColorSampler)
			SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputColorTexture)
			RENDER_TARGET_BINDING_SLOTS()
			END_SHADER_PARAMETER_STRUCT()

			static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{

		}

	};

	IMPLEMENT_GLOBAL_SHADER(FMyLightFlowSamplerPS, "/Engine/Private/MyGS/MyRenderGraph.usf", "MainPS", SF_Pixel);
}
FScreenPassTexture AddMyLightFlowPass(FRDGBuilder& GraphBuilder, const FViewInfo& View, const FRDGTextureRef& InputTexture)
{
	//RDG_EVENT_SCOPE(GraphBuilder, "LightFlowSampler(ViewId=%d)", 0);
	FRDGTextureDesc RTDesc;
	RTDesc.ClearValue = FClearValueBinding();
	RTDesc.Dimension = ETextureDimension::Texture2D;
	RTDesc.Flags = TexCreate_RenderTargetable;
	RTDesc.Format = PF_A2B10G10R10;
	RTDesc.Extent = InputTexture->Desc.Extent;
	FRDGTextureRef LightFlowRT = GraphBuilder.CreateTexture(RTDesc, TEXT("MyRt"));
	FScreenPassRenderTarget MyOutput = FScreenPassRenderTarget(LightFlowRT, ERenderTargetLoadAction::EClear);

	const FScreenPassTextureViewport OutputViewport(MyOutput);
	//const FScreenPassTextureViewport ColorViewport(FScreenPassTexture((*Inputs.SceneTextures)->SceneColorTexture));
	const FScreenPassTextureViewport InputViewport = FScreenPassTextureViewport(FScreenPassTexture(InputTexture));

	FMyLightFlowSamplerPS::FParameters* LFPassParameters = GraphBuilder.AllocParameters<FMyLightFlowSamplerPS::FParameters>();
	LFPassParameters->InputColorSampler = TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	LFPassParameters->InputColorTexture = InputTexture;
	LFPassParameters->RenderTargets[0] = MyOutput.GetRenderTargetBinding();

	TShaderMapRef<FMyLightFlowSamplerPS> PixelShader(View.ShaderMap);
	TShaderMapRef<FScreenPassVS> VertexShader(View.ShaderMap);

	FRHIBlendState* BlendState = FScreenPassPipelineState::FDefaultBlendState::GetRHI();
	FRHIDepthStencilState* DepthStencilState = FScreenPassPipelineState::FDefaultDepthStencilState::GetRHI();
	const FScreenPassPipelineState PipelineState(VertexShader, PixelShader, BlendState, DepthStencilState);
	//GraphBuilder.AddPass(
	//	RDG_EVENT_NAME("LightFlowSampler %d x %d", OutputViewport.Rect.Width(), OutputViewport.Rect.Height()),
	//	LFPassParameters,
	//	ERDGPassFlags::Raster,
	//	[&View, OutputViewport, InputViewport, PipelineState, PixelShader, LFPassParameters, Flags](FRHICommandListImmediate& RHICmdList)
	//	{
	//		DrawScreenPass(RHICmdList, View, OutputViewport, InputViewport, PipelineState, Flags, [&](FRHICommandListImmediate&)
	//			{
	//				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *PixelShaderParameters);
	//			});
	//	});

	//AddDrawScreenPass(
	//	GraphBuilder,
	//	RDG_EVENT_NAME("LightFlowSampler %d x %d", OutputViewport.Rect.Width(), OutputViewport.Rect.Height()),
	//	View,
	//	OutputViewport,
	//	ColorViewport,
	//	PixelShader,
	//	LFPassParameters
	//);
	static uint32 FrameNum=0;
	FIntPoint BufferSize = InputTexture->Desc.Extent;
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("LightFlowSampler"),
		LFPassParameters,
		ERDGPassFlags::Raster,
		[LFPassParameters, &View, PixelShader, BufferSize, OutputViewport, InputViewport, PipelineState](FRHICommandListImmediate& RHICmdList)
		{
			//FPixelShaderUtils::DrawFullscreenPixelShader(RHICmdList, View.ShaderMap, PixelShader, *LFPassParameters, FIntRect(0, 0, BufferSize.X, BufferSize.Y));
			
			DrawScreenPass(RHICmdList, View, OutputViewport, InputViewport, PipelineState, EScreenPassDrawFlags::None, [&](FRHICommandListImmediate&)
			{
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *LFPassParameters);
			});

			// js insertframe data
			if (GEngine->GameViewport)
			{
				struct VelocityPixel
				{
					uint16 R;
					uint16 G;
					uint16 B;
					uint16 A;
				};

				FRHITexture2D* PassOutput = LFPassParameters->InputColorTexture->GetRHI()->GetTexture2D();
				FIntPoint BufferSize11 = PassOutput->GetSizeXY();
				FIntRect Rect = FIntRect(0, 0, BufferSize11.X, BufferSize11.Y);
				if (PassOutput->GetFormat() == PF_R16G16B16A16_UNORM)
				{
					uint32 Lolstrid = 0;
					char* buffer = (char*)RHILockTexture2D(PassOutput, 0, RLM_ReadOnly, Lolstrid, false);// 加锁 获取可读Texture数组首地址
					char* TexturePtr=buffer;
					//保存数据成图片
					TArray<FColor> ConvertToColor;
					TArray<FLinearColor> HDRDatas;
					for (int32 row = 0; row < BufferSize11.Y; ++row)
					{
						uint64* Pixelptr = (uint64*)buffer;
						
						for (int32 col = 0; col < BufferSize11.X; ++col)
						{
							uint64 Color = *Pixelptr;
							uint16 r=(Color & 0x000000000000ffff);
							uint16 g=(Color & 0x00000000ffff0000) >> 16 ;
							uint16 b=(Color & 0x0000ffff00000000) >> 32 ;
							uint16 a=(Color & 0xffff000000000000) >> 48 ;
							ConvertToColor.Add(FColor(r / 65535.0f * 255, g / 65535.0f * 255,
							                          b / 65535.0f * 255));
							HDRDatas.Add(FLinearColor(r / 65535.0f ,g / 65535.0f,b / 65535.0f,a / 65535.0f));
							
							Pixelptr++;
						}

						buffer += Lolstrid;
					}
					
					RHIUnlockTexture2D(PassOutput, 0, false); //解锁
					
					// TArray<uint8> compressedBitmapColor;
					// FImageUtils::CompressImageArray(BufferSize.X, BufferSize.Y, ConvertToColor, compressedBitmapColor);
					// FFileHelper::SaveArrayToFile(compressedBitmapColor, TEXT("F:/Outs/LightFlow/1.png"));
					
					FString OutImage=FString::Printf(TEXT("F:/Outs/BaseColorImage/BaseColor_%d.bmp"),FrameNum);
					FString OutHDR=FString::Printf(TEXT("F:/Outs/BaseColorData/BaseColor_%d.hdr"),FrameNum);
					FFileHelper::CreateBitmap(OutImage.GetCharArray().GetData(), BufferSize11.X, BufferSize11.Y, ConvertToColor.GetData());

					FArchive* Ar = IFileManager::Get().CreateFileWriter(*OutHDR);
					if (Ar)
					{
						FBufferArchive ArBuffer;
						UnrealInsertFrameDataGather::MyWriteHDRImage(HDRDatas, ArBuffer, BufferSize11);
						Ar->Serialize(const_cast<uint8*>(ArBuffer.GetData()), ArBuffer.Num());
						delete Ar;
					}
					
					FrameNum++;
				}
			}
			// js insertframe data
		}
	);

	return MoveTemp(MyOutput);

	//return FScreenPassRenderTarget();
}
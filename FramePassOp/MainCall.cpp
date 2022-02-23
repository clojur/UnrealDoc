FScreenPassTexture AddPostProcessMaterialChain(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FPostProcessMaterialInputs& InputsTemplate,
	const FPostProcessMaterialChain& Materials)
{
	FScreenPassTexture Outputs = InputsTemplate.GetInput(EPostProcessMaterialInput::SceneColor);

	bool bFirstMaterialInChain = true;
	for (const UMaterialInterface* MaterialInterface : Materials)
	{
		FPostProcessMaterialInputs Inputs = InputsTemplate;
		Inputs.SetInput(EPostProcessMaterialInput::SceneColor, Outputs);
		
		// Only the first material in the chain needs to decode the input color
		Inputs.bMetalMSAAHDRDecode = Inputs.bMetalMSAAHDRDecode && bFirstMaterialInChain;
		bFirstMaterialInChain = false;

		// Certain inputs are only respected by the final post process material in the chain.
		if (MaterialInterface != Materials.Last())
		{
			Inputs.OverrideOutput = FScreenPassRenderTarget();
			Inputs.bFlipYAxis = false;
		}

		Outputs = AddPostProcessMaterialPass(GraphBuilder, View, Inputs, MaterialInterface);
	}
	
	Outputs = AddMyLightFlowPass(GraphBuilder, View, Outputs.Texture);
	return Outputs;
}
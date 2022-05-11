/*
*  Copyright (c) 2016-2020 YeHaike(841660657@qq.com).
*  All rights reserved.
*  @ Date : 2020/01/26
*
*/

#include "MultiWindowsManager.h"
#include "Engine/Engine.h"
#include "Widgets/SOverlay.h"
#include "Slate/SGameLayerManager.h"
#include "Engine/GameEngine.h"
#include "Slate/SceneViewport.h"
#include "MultiWindowsGameViewportClient.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY(LogMultiWindowsManager)

//void UMultiWindowsManager::CreateWindow(FText WindowTitle, FVector2D WindowPosition, FVector2D WindowSize)
//{
//	CreateWindow_Internal(WindowTitle, WindowPosition, WindowSize);
//}

TSharedPtr<SWindow> UMultiWindowsManager::CreateWindow_Internal(FText WindowTitle, FVector2D WindowPosition, FVector2D WindowSize)
{
	FText NewWindowTitle = WindowTitle;
	if (NewWindowTitle.IsEmpty())
	{
		NewWindowTitle = FText::Format(NSLOCTEXT("MultiWindows4UE4", "NewWindow", "NewWindow-{Index}"), NumOfNewViewportWindow);
	}

	TSharedPtr<SWindow> NewWindow = SNew(SWindow)
		.Title(NewWindowTitle)
		.ScreenPosition(WindowPosition)
		.ClientSize(WindowSize)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.UseOSWindowBorder(true)
		.SaneWindowPlacement(false)
		.SizingRule(ESizingRule::UserSized);

	// Mac does not support parenting, do not keep on top
#if PLATFORM_MAC
	FSlateApplication::Get().AddWindow(NewWindow.ToSharedRef());
#else
	// TSharedRef<SWindow, ESPMode::Fast> MainWindow = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame")).GetParentWindow().ToSharedRef();
	FSlateApplication::Get().AddWindow(NewWindow.ToSharedRef());
#endif

	NumOfNewViewportWindow++;
	return NewWindow;
}

FSceneViewport* UMultiWindowsManager::GetSceneViewport(UGameViewportClient* GameViewportClient) const
{
	return GameViewportClient->GetGameViewport();
}

void UMultiWindowsManager::CreateSceneViewport(UWindow* Window)
{
	if (!Window || !Window->GameViewportClient)
	{
		return;
	}
	auto GameViewportClientWindow = Window->GameViewportClientWindow.Pin();
	if (!GameViewportClientWindow.IsValid())
	{
		return;
	}

	if (!Window->ViewportWidget.IsValid())
	{
		CreateViewportWidget(Window);
	}

	GameViewportClientWindow->SetOnWindowClosed(FOnWindowClosed::CreateUObject(Window, &UWindow::OnGameWindowClosed));

	// SAVEWINPOS tells us to load/save window positions to user settings (this is disabled by default)
	int32 SaveWinPos;
	if (GEngine && FParse::Value(FCommandLine::Get(), TEXT("SAVEWINPOS="), SaveWinPos) && SaveWinPos > 0)
	{
		//// Get WinX/WinY from GameSettings, apply them if valid.
		//FIntPoint PiePosition = GEngine->GetGameUserSettings()->GetWindowPosition();
		//if (PiePosition.X >= 0 && PiePosition.Y >= 0)
		//{
		//	int32 WinX = GEngine->GetGameUserSettings()->GetWindowPosition().X;
		//	int32 WinY = GEngine->GetGameUserSettings()->GetWindowPosition().Y;
		//	GameViewportClientWindow->MoveWindowTo(FVector2D(WinX, WinY));
		//}
		//GameViewportClientWindow->SetOnWindowMoved(FOnWindowMoved::CreateUObject(Window, &UWindow::OnGameWindowMoved));
	}

	TSharedRef<SViewport> ViewportWidgetRef = Window->ViewportWidget.ToSharedRef();

	TSharedPtr<class FSceneViewport> SceneViewport = MakeShareable(new FSceneViewport(Window->GameViewportClient, ViewportWidgetRef));
	Window->SceneViewport = SceneViewport;
	Window->GameViewportClient->Viewport = SceneViewport.Get();
	//GameViewportClient->CreateHighresScreenshotCaptureRegionWidget(); //  Disabled until mouse based input system can be made to work correctly.

	// The viewport widget needs an interface so it knows what should render
	ViewportWidgetRef->SetViewportInterface(SceneViewport.ToSharedRef());

	// FSlateApplication::Get().RegisterViewport(Window->ViewportWidget.ToSharedRef());

	FSceneViewport* ViewportFrame = SceneViewport.Get();

	Window->GameViewportClient->SetViewportFrame(ViewportFrame);

	Window->GameViewportClient->GetGameLayerManager()->SetSceneViewport(ViewportFrame);

	GameViewportClientWindow->SetContent(ViewportWidgetRef);

	FViewport::ViewportResizedEvent.AddUObject(Window, &UWindow::OnViewportResized);
}

void UMultiWindowsManager::CreateViewportWidget(UWindow* Window)
{
	if (!Window)
	{
		return;
	}
	UGameViewportClient* GameViewportClient = Window->GameViewportClient;
	if (!GameViewportClient)
	{
		return;
	}
	bool bRenderDirectlyToWindow = true;

	TSharedRef<SOverlay> ViewportOverlayWidgetRef = SNew(SOverlay);

	TSharedRef<SGameLayerManager> GameLayerManagerRef = SNew(SGameLayerManager)
		.SceneViewport_UObject(this, &UMultiWindowsManager::GetSceneViewport, GameViewportClient)
		[
			ViewportOverlayWidgetRef
		];

	const bool bStereoAllowed = false;

	TSharedPtr<SViewport> ViewportWidget =
		SNew(SViewport)
		// Render directly to the window backbuffer unless capturing a movie or getting screenshots
		// @todo TEMP
		.RenderDirectlyToWindow(bRenderDirectlyToWindow)
		// Gamma correction in the game is handled in post processing in the scene renderer
		.EnableGammaCorrection(false)
		.EnableStereoRendering(bStereoAllowed)
		[
			GameLayerManagerRef
		];

	Window->ViewportWidget = ViewportWidget;
	Window->GameViewportClient->SetViewportOverlayWidget(Window->GameViewportClientWindow.Pin(), ViewportOverlayWidgetRef);
	Window->GameViewportClient->SetGameLayerManager(GameLayerManagerRef);
}

UWindow* UMultiWindowsManager::CreateGameViewportClientWindow()
{
	if (!GEngine)
	{
		return nullptr;
	}
	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}
	if (!GIsClient)
	{
		return nullptr;
	}
	UWindow* Window = NewObject<UWindow>(this, UWindow::StaticClass());
	AncillaryWindows.Add(Window);
	// Initialize the viewport client.
	UGameViewportClient* GameViewportClient = nullptr;
	GameViewportClient = NewObject<UGameViewportClient>(GEngine, GEngine->GameViewportClientClass);
	Window->GameViewportClient = GameViewportClient;
	UMultiWindowsGameViewportClient* MultiWindowsGameViewportClient = Cast<UMultiWindowsGameViewportClient>(GameViewportClient);
	if (MultiWindowsGameViewportClient)
	{
		MultiWindowsGameViewportClient->Window = Window;
	}
	FWorldContext& NewWorldContext = GEngine->CreateNewWorldContext(GameInstance->GetWorldContext()->WorldType);
	FName ContextHandle = NewWorldContext.ContextHandle;
	FWorldContext* MainWorldContext = GameInstance->GetWorldContext();
	NewWorldContext = *MainWorldContext;
	NewWorldContext.ExternalReferences.Empty();
	NewWorldContext.ContextHandle = ContextHandle;
	NewWorldContext.GameViewport = GameViewportClient;
	//UWorld* CurrentWorld;
	//GameInstance->GetWorldContext()->AddRef(CurrentWorld);
	//NewWorldContext.SetCurrentWorld(CurrentWorld);
	/*UWorld* World = UWorld::CreateWorld(NewWorldContext.WorldType, false);
	NewWorldContext.SetCurrentWorld(World);*/
	GameViewportClient->Init(NewWorldContext, GameInstance);
	bAddedNewWindow = true;

	// GameInstance->GetWorldContext()->GameViewport = GameViewportClient;

	// Attach the viewport client to a new viewport.
	if (GameViewportClient)
	{
		/** The game viewport window. This must be created before any gameplay code adds widgets */
		TSharedPtr<SWindow> GameViewportClientWindow = CreateWindow_Internal(FText::FromString("NewGameViewportClientWindow"));

		Window->GameViewportClientWindow = GameViewportClientWindow;
	
		CreateSceneViewport(Window);

		FString Error;
		if (GameViewportClient->SetupInitialLocalPlayer(Error) == NULL)
		{
			// UE_LOG(LogMultiWindowsManager, Fatal, TEXT("%s"), *Error);
		}

		UGameViewportClient::OnViewportCreated().Broadcast();
	}

	OnAncillaryWindowsArrayChanged.Broadcast(this);

	return Window;
}

void UMultiWindowsManager::GetMainWindow(UWindow*& OutMainWindow)
{
	if (nullptr == MainWindow && GEngine && GEngine->GameViewport)
	{
		MainWindow = NewObject<UWindow>(this, UWindow::StaticClass());
	}

	{
		MainWindow->GameViewportClient = GEngine->GameViewport;
		MainWindow->GameViewportClientWindow = GEngine->GameViewport->GetWindow();
		// MainWindow->SceneViewport = MakeShareable(GEngine->GameViewport->GetGameViewport());
		MainWindow->ViewportWidget = GEngine->GameViewport->GetGameViewportWidget();

		UMultiWindowsGameViewportClient* MultiWindowsGameViewportClient = Cast<UMultiWindowsGameViewportClient>(MainWindow->GameViewportClient);
		if (MultiWindowsGameViewportClient)
		{
			MultiWindowsGameViewportClient->Window = MainWindow;
		}
	}

	OutMainWindow = this->MainWindow;
}

void UMultiWindowsManager::UpdateWorldContentBeforeTick(TIndirectArray<FWorldContext>& WorldList)
{
	if (!GEngine)
	{
		return;
	}

	if (!GEngine->GameViewport)
	{
		return;
	}

	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	FWorldContext* MainWorldContext = GameInstance->GetWorldContext();
	if (!MainWorldContext)
	{
		return;
	}

	//TempWorldList.Empty();
	TempGameViewportClients.Empty();

	for (int32 WorldIdx = WorldList.Num() - 1; WorldIdx >= 0; --WorldIdx)
	{
		FWorldContext& ThisContext = WorldList[WorldIdx];
		for (int32 WindowIndex = AncillaryWindows.Num()-1; WindowIndex>=0; --WindowIndex)
		{
			UWindow* AncillaryWindow = AncillaryWindows[WindowIndex];
			if (AncillaryWindow)
			{
				if (ThisContext.GameViewport == AncillaryWindow->GameViewportClient && AncillaryWindow->GameViewportClient && !MainWorldContext->TravelURL.IsEmpty())
				{
					ThisContext.SetCurrentWorld(nullptr);
					//TempWorldList.Add(&ThisContext);
					TempGameViewportClients.Add(AncillaryWindow->GameViewportClient);
					WorldList.RemoveAt(WorldIdx);
				}
			}
		}
	}
}

void UMultiWindowsManager::UpdateWorldContentAfterTick(TIndirectArray<FWorldContext>& WorldList)
{
	if (!GEngine)
	{
		return;
	}

	if (!GEngine->GameViewport)
	{
		return;
	}

	UGameInstance* GameInstance = GEngine->GameViewport->GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	FWorldContext* MainWorldContext = GameInstance->GetWorldContext();
	if (!MainWorldContext)
	{
		return;
	}

	for (int32 WorldIdx = TempGameViewportClients.Num() - 1; WorldIdx >= 0; --WorldIdx)
	{
		UGameViewportClient* ThisGameViewportClient = TempGameViewportClients[WorldIdx];
		for (int32 WindowIndex = AncillaryWindows.Num() - 1; WindowIndex >= 0; --WindowIndex)
		{
			UWindow* AncillaryWindow = AncillaryWindows[WindowIndex];
			if (AncillaryWindow)
			{
				if (ThisGameViewportClient == AncillaryWindow->GameViewportClient && AncillaryWindow->GameViewportClient)
				{
					FWorldContext& NewWorldContext = GEngine->CreateNewWorldContext(MainWorldContext->WorldType);
					FName ContextHandle = NewWorldContext.ContextHandle;
					NewWorldContext = *MainWorldContext;
					NewWorldContext.ExternalReferences.Empty();
					NewWorldContext.ContextHandle = ContextHandle;
					NewWorldContext.GameViewport = AncillaryWindow->GameViewportClient;

					AncillaryWindow->GameViewportClient->Init(NewWorldContext, GameInstance);
					TempGameViewportClients.RemoveAt(WorldIdx);
				}
			}
		}
	}
	//TempWorldList.Empty();
}
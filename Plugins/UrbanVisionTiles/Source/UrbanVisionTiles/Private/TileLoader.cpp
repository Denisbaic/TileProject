// Fill out your copyright notice in the Description page of Project Settings.


#include "TileLoader.h"
#include "Engine/Engine.h"
#include "Engine/Texture2DDynamic.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

// Sets default values for this component's properties
ATilesController::ATilesController()
{	
	PrimaryActorTick.bCanEverTick = true;
	mesh = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("Tiles mesh"), true);
	SetRootComponent(mesh);
}


// Called when the game starts
void ATilesController::BeginPlay()
{
	Super::BeginPlay();
	ClearMesh();
	CreateMesh();	
}


// Called every frame
void ATilesController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);	
	/*for (int i = 0; i < mesh->GetNumSections(); i++)
	{
		if (!mesh->IsMeshSectionVisible(i))
		{
			freeIndices.Add(i);
		}
	}*/
	int x0, y0;
	GetMercatorXYFromOffset(Offset, BaseLevel, x0, y0);
	
	CreateMeshAroundPoint(BaseLevel, x0, y0);

	//TArray<FIntVector> pendingDelete;
	//TArray<FIntVector, FHeapAllocator> pendingUpdate;	
	for (auto tile : TileIndecies)
	{
		if (SplitTiles.Contains(tile.Key)) continue;
		int x = tile.Key.X;
		int y = tile.Key.Y;
		x /= (1 << (tile.Key.Z - BaseLevel));
		y /= (1 << (tile.Key.Z - BaseLevel));
		
		if (tile.Key.Z == BaseLevel && FMath::Abs(x - x0) > BaseLevelSize || FMath::Abs(y - y0) > BaseLevelSize)
		{
			if (TileLoader->IsTextureLoaded(tile.Key)) {
				
				pendingDelete.Add(tile.Key);
			}
		} else
		{
			pendingUpdate.AddUnique(tile.Key);
		}
	}
	for (auto meta : pendingDelete)
	{
		if (!TileIndecies.Contains(meta)) continue;
		ClearTileMesh(meta);		
	}
	//auto manager = GEngine->GetFirstLocalPlayerController(GetWorld())->PlayerCameraManager;
	for (auto meta : pendingUpdate)
	{
		if (!TileIndecies.Contains(meta)) continue;
		/*auto pos = GetXYOffsetFromMercatorOffset(meta.Z, meta.X, meta.Y) + GetActorLocation();		
		
		float dist = (pos - manager->GetCameraLocation()).Size();
		float tan = FMath::Tan(manager->GetCameraCachePOV().DesiredFOV * PI / 180 / 2);
		float viewSize = 2 * dist * tan;
		float tileSize = 360 * EarthOneDegreeLengthOnEquatorMeters * FMath::Cos(CenterLat * PI / 180) / (1 << meta.Z);*/
		float tilePixelsSize = GetPixelSize(meta);
		if (tilePixelsSize > 256.0f && meta.Z <= MaxLevel)
		{			
			SplitTile(meta);			
		}
		FIntVector parentMeta = FIntVector{ meta.X / 2, meta.Y / 2, meta.Z - 1 };
		float parentTilePixelsSize = GetPixelSize(parentMeta);
		if (parentTilePixelsSize < 200.0f && meta.Z > BaseLevel)
		{

			int32 x = meta.X - (meta.X % 2);
			int32 y = meta.Y - (meta.Y % 2);

			bool flag = false;
			for (int i = 0; i < 4; i++)
			{
				auto meta1 = FIntVector{ x + i % 2, y + i / 2, meta.Z };
				if (!TileIndecies.Contains(meta1)) {										
					flag = true;
				}
			}
			if (!flag) {
				CreateTileMesh(parentMeta);
				SplitTiles.Remove(parentMeta);
				for (int i = 0; i < 4; i++)
				{
					FIntVector meta1 = { x + i % 2, y + i / 2, meta.Z };
					ClearTileMesh(meta1);					
				}
			}
		}
	}
	pendingDelete.Empty();
	pendingUpdate.Empty();
}

float ATilesController::GetPixelSize(FIntVector meta)
{
	auto pos = GetXYOffsetFromMercatorOffset(meta.Z, meta.X, meta.Y) + GetActorLocation();
	auto manager = GEngine->GetFirstLocalPlayerController(GetWorld())->PlayerCameraManager;
	float dist = (pos - manager->GetCameraLocation()).Size();
	float tan = FMath::Tan(manager->GetCameraCachePOV().DesiredFOV * PI / 180 / 2);
	float viewSize = 2 * dist * tan;
	float tileSize = 360 * EarthOneDegreeLengthOnEquatorMeters * FMath::Cos(CenterLat * PI / 180) / (1 << meta.Z);
	float tilePixelsSize = tileSize / viewSize * GEngine->GameViewport->Viewport->GetSizeXY().X;
	return tilePixelsSize;
}


void ATilesController::CreateMesh()
{
	if (!TileLoader) 
		
		TileLoader = NewObject<UTileTextureContainer>(this);

	const int z = BaseLevel;

	const int x0 = GetMercatorXFromDegrees(CenterLon) * (1 << z);
	const int y0 = GetMercatorYFromDegrees(CenterLat) * (1 << z);
	
	for (int x = 0; x < BaseLevelSize; x++)
	{
		for (int y = 0; y < BaseLevelSize; y++)
		{
			CreateTileMesh(x + x0 - BaseLevelSize/2, y + y0 - BaseLevelSize/2, z);
		}
	}
}

void ATilesController::ClearMesh()
{
	mesh->ClearAllMeshSections();
	TileIndecies.Empty();
}


void ATilesController::CreateMeshAroundPoint(int z, int x0, int y0)
{
	if (!TileLoader)	
		TileLoader = NewObject<UTileTextureContainer>(this);

	//mesh->ClearAllMeshSections();
	for (int x = 0; x < BaseLevelSize; x++)
	{
		for (int y = 0; y < BaseLevelSize; y++)
		{
			if (!SplitTiles.Contains(FIntVector{ x + x0 - BaseLevelSize / 2, y + y0 - BaseLevelSize / 2, z })) {
				CreateTileMesh(x + x0 - BaseLevelSize / 2, y + y0 - BaseLevelSize / 2, z);
			}
		}
	}
}

double ATilesController::EarthRadius  = 6378.137 * 100;
double ATilesController::EarthOneDegreeLengthOnEquatorMeters = 111152.8928 * 100;

void ATilesController::GetMercatorXYFromOffset(FVector offsetValue, int z, int& x, int& y)
{
	int dx, dy;
	GetMercatorXYOffsetFromOffset(offsetValue, z, dx, dy);
	int x0 = GetMercatorXFromDegrees(CenterLon) * (1 << z);
	int y0 = GetMercatorYFromDegrees(CenterLat) * (1 << z);

	x = x0 + dx;
	y = y0 + dy;
}

void ATilesController::GetMercatorXYOffsetFromOffset(FVector offsetValue, int z, int& dx, int& dy)
{
	float fdx = offsetValue.X * (1 << z) / 360 / EarthOneDegreeLengthOnEquatorMeters / FMath::Cos(CenterLat * PI / 180);
	float fdy = offsetValue.Y * (1 << z) / 360 / EarthOneDegreeLengthOnEquatorMeters / FMath::Cos(CenterLat * PI / 180);
	dx = static_cast<int>(FMath::RoundFromZero(fdx));
	dy = static_cast<int>(FMath::RoundFromZero(fdy));
}

FVector ATilesController::GetXYOffsetFromMercatorOffset(int z, int x, int y)
{
	int x0 = GetMercatorXFromDegrees(CenterLon) * (1 << z);
	int y0 = GetMercatorYFromDegrees(CenterLat) * (1 << z);
	float fdx = (x - x0) * 360 * EarthOneDegreeLengthOnEquatorMeters / (1 << z) * FMath::Cos(CenterLat * PI / 180);
	float fdy = (y - y0) * 360 * EarthOneDegreeLengthOnEquatorMeters / (1 << z) * FMath::Cos(CenterLat * PI / 180);
	return FVector(fdx, fdy, 0) + GetActorLocation();
}

int ATilesController::CreateTileMesh(int x, int y, int z)
{
	auto meta = FIntVector{ x, y, z };
	return CreateTileMesh(meta);
}

int ATilesController::CreateTileMesh(FIntVector meta)
{	
	if (TileIndecies.Contains(meta)) {
		//mesh->SetMeshSectionVisible(TileIndecies[meta], true);
		return TileIndecies[meta];
	}
	int x = meta.X;
	int y = meta.Y;
	int z = meta.Z;
	TArray<FVector> vertices;
	TArray<FVector> normals;
	TArray<int> triangles;
	TArray<FVector2D> uvs;
	
	float x0 = GetMercatorXFromDegrees(CenterLon) * (1 << z);
	float y0 = GetMercatorYFromDegrees(CenterLat) * (1 << z);
	float size = EarthOneDegreeLengthOnEquatorMeters / (1 << z) * 360 * FMath::Cos(CenterLat * PI / 180);
	
	FVector delta((x - x0) * size, (y - y0) * size, 0);

	vertices.Add(delta + FVector(0, 0, 0));
	vertices.Add(delta + FVector(size, 0, 0));
	vertices.Add(delta + FVector(size, size, 0));
	vertices.Add(delta + FVector(0, size, 0));

	normals.Add(FVector::UpVector);
	normals.Add(FVector::UpVector);
	normals.Add(FVector::UpVector);
	normals.Add(FVector::UpVector);

	uvs.Add(FVector2D(0, 0));
	uvs.Add(FVector2D(1, 0));
	uvs.Add(FVector2D(1, 1));
	uvs.Add(FVector2D(0, 1));

	triangles.Add(0);
	triangles.Add(2);
	triangles.Add(1);

	triangles.Add(0);
	triangles.Add(3);
	triangles.Add(2);


	int sectionIndex = -1;
	if (freeIndices.Num() > 0)
	{
		sectionIndex = freeIndices.Last();
		freeIndices.RemoveAt(freeIndices.Num() - 1);
	}
	else
	{		
		sectionIndex = mesh->GetNumSections();
	}	
	mesh->CreateMeshSection(sectionIndex, vertices, triangles, normals, uvs, TArray<FColor>(),
	                       TArray<FRuntimeMeshTangent>(), false);
	auto tile = TileLoader->GetTileMaterial(x, y, z, TileMaterial, this->GetOwner());
	mesh->SetMaterial(sectionIndex, tile->Material);	
	TileLoader->CachedTiles[meta]->IsActive = true;	
	TileIndecies.Add(meta, sectionIndex);
	Tiles.Add(meta, tile);
	return  sectionIndex;
}

void ATilesController::ClearTileMesh(FIntVector meta)
{
	//mesh->ClearMeshSection(index);
	int index = TileIndecies[meta];
	mesh->SetMeshSectionVisible(index, false);
	
	if (TileLoader->CachedTiles.Contains(meta)) {
		TileLoader->CachedTiles[meta]->IsActive = false;
		TileLoader->CachedTiles[meta]->lastAcessTime = FDateTime::Now();		
	} else
	{
		UE_LOG(LogTemp, Warning, TEXT("WTF"));
	}	
	freeIndices.Add(index);
	TileIndecies.Remove(meta);
	Tiles[meta]->IsActive = false;
	Tiles[meta]->lastAcessTime = FDateTime::Now();
}

bool ATilesController::IsTileSplit(int x, int y, int z)
{
	return
		TileIndecies.Contains(FIntVector{x * 2, y * 2, z + 1}) &&
		TileIndecies.Contains(FIntVector{x * 2 + 1, y * 2, z + 1}) &&
		TileIndecies.Contains(FIntVector{x * 2, y * 2 + 1, z + 1}) &&
		TileIndecies.Contains(FIntVector{x * 2 + 1, y * 2 + 1, z + 1});
}

void ATilesController::SplitTile(FIntVector m)
{
	SplitTile(m.X, m.Y, m.Z);
}

void ATilesController::SplitTile(int x, int y, int z)
{
	FIntVector parentMeta = { x, y, z };
	FIntVector childMeta1 = { x * 2, y * 2, z + 1 };
	FIntVector childMeta2 = { x * 2 + 1, y * 2, z + 1 };
	FIntVector childMeta3 = { x * 2, y * 2 + 1, z + 1 };
	FIntVector childMeta4 = { x * 2 + 1, y * 2 + 1, z + 1 };

	TileLoader->GetTileMaterial(childMeta1, TileMaterial, this);
	TileLoader->GetTileMaterial(childMeta2, TileMaterial, this);
	TileLoader->GetTileMaterial(childMeta3, TileMaterial, this);
	TileLoader->GetTileMaterial(childMeta4, TileMaterial, this);

	if (TileLoader->IsTextureLoaded(childMeta1) && TileLoader->IsTextureLoaded(childMeta2) &&
		TileLoader->IsTextureLoaded(childMeta3) && TileLoader->IsTextureLoaded(childMeta4))
	{
		
		
		CreateTileMesh(childMeta1);
		CreateTileMesh(childMeta2);
		CreateTileMesh(childMeta3);
		CreateTileMesh(childMeta4);
		ClearTileMesh(parentMeta);
		SplitTiles.Add(parentMeta);
		TileIndecies.Remove(parentMeta);
		Tiles[parentMeta]->IsActive = false;
		Tiles[parentMeta]->lastAcessTime = FDateTime::Now();
	}
}

void UTextureDownloader::StartDownloadingTile(FIntVector meta, FString url)
{
	TextureMeta = meta;	
	_loader = UAsyncTaskDownloadImage::DownloadImage(url);
	_loader->OnSuccess.AddDynamic(this, &UTextureDownloader::OnTextureLoaded);
	_loader->OnFail.AddDynamic(this, &UTextureDownloader::OnLoadFailed);
}

void UTextureDownloader::OnTextureLoaded(UTexture2DDynamic* Texture)
{
	if (!Texture->IsValidLowLevel())
	{
		TileContainer->FreeLoader(TextureMeta);
		UE_LOG(LogTemp, Warning, TEXT("Loaded texture is corrupt"));
	}
	if(!material->IsValidLowLevel())
	{
		TileContainer->FreeLoader(TextureMeta);
		UE_LOG(LogTemp, Warning, TEXT("Texture loaded for already destroyed tile"));
	}
	TileContainer->CacheTexture(TextureMeta, (UTexture*)(Texture));
	material->SetTextureParameterValue("Tile", (UTexture*)(Texture));
}

void UTextureDownloader::OnLoadFailed(UTexture2DDynamic* Texture)
{
	TileContainer->FreeLoader(TextureMeta);
	UE_LOG(LogTemp, Warning, TEXT("Load failed"));
}

UTileInfo* UTileTextureContainer::GetTileMaterial(int x, int y, int z, UMaterialInterface* mat, AActor* owner)
{
	auto meta = FIntVector{ x, y, z };
	return GetTileMaterial(meta, mat, owner);
}

UTileInfo* UTileTextureContainer::GetTileMaterial(FIntVector meta, UMaterialInterface* mat, AActor* owner)
{
	//UE_LOG(LogTemp, Warning, TEXT("Total cached textures: %i"), CachedTiles.Num());

	if (CachedTiles.Num() > 512)
	{
		TArray<FIntVector> pendingDelete;

		for (auto cached : CachedTiles)
		{
			if (!cached.Value->IsActive && (cached.Value->lastAcessTime - FDateTime::Now()).GetTotalSeconds() > 60)
			{
				pendingDelete.Add(cached.Key);
			}
		}
		for (auto tile : pendingDelete)
		{
			CachedTiles[tile]->IsLoaded = false;
			CachedTiles.Remove(tile);			 
		}
	}
	if (!CachedTiles.Contains(meta))
	{
		//if (loadingImages.Contains(meta)) return loadingImages[meta]->material;
		UMaterialInstanceDynamic* matInstance = UMaterialInstanceDynamic::Create(mat, owner);		
		auto url = FString::Format(*UrlString, { meta.Z, meta.X, meta.Y });
		UTextureDownloader* loader;
		loader = NewObject<UTextureDownloader>();		
		loader->TileContainer = this;		
		loader->StartDownloadingTile(meta, url);
		loader->material = matInstance;
		loadingImages.Add(meta, loader);
		//matInstance->SetTextureParameterValue("Tile", CachedTextures[meta]);
		auto TileInfo = NewObject<UTileInfo>();
		//TileInfo->Container = this;
		TileInfo->Material = matInstance;
		TileInfo->lastAcessTime = FDateTime::Now();
		CachedTiles.Add(meta, TileInfo);
		return TileInfo;
	}	
	else
	{		
		return CachedTiles[meta];
	}
}


void UTileTextureContainer::CacheTexture(FIntVector meta, UTexture* texture)
{
	if (CachedTiles.Contains(meta)) {
		CachedTiles[meta]->Texture = texture;
	}
}

void UTileTextureContainer::FreeLoader(FIntVector meta)
{
	if (!loadingImages.Contains(meta))
	{
		UE_LOG(LogTemp, Warning, TEXT("WTF?"));
	}		
	//auto loader = loadingImages[meta];
	//loadingImages.Remove(meta);
	//unusedDownloaders.Add(loader);
}

bool UTileTextureContainer::IsTextureLoaded(FIntVector meta)
{
	return CachedTiles.Contains(meta) && CachedTiles[meta]->Texture && CachedTiles[meta]->Texture->IsValidLowLevel();
}

void UTileTextureContainer::Clear()
{
	CachedTiles.Empty();
}

//int32 UTileTextureContainer2::SectionIndex = 0;


ATilesController3::ATilesController3()
{
	PrimaryActorTick.bCanEverTick = true;
	mesh = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("Tiles mesh"), true);
	SetRootComponent(mesh);
}

void ATilesController3::BeginPlay()
{
	Super::BeginPlay();

	UTileInfo3::TileContainer = &this->TileContainer;
	UTextureDownloader3::TileContainer = &this->TileContainer;
	UTextureDownloader3::DownloaderContainer = &this->DownloaderContainer;

	PlayerController = GetWorld()->GetFirstPlayerController();
}

void ATilesController3::Tick(float DeltaTime)
{
	TArray<FIntVector> Coords;
	FVector Location;
	FRotator Rotator;
	PlayerController->GetPlayerViewPoint(Location, Rotator);

	GetCircleCoords(int32(Location.X / 256.f), int32(Location.Y / 256.f), 5, Coords);
	Coords.RemoveAll([](FIntVector const & Vec)
	{
		if(Vec.Z==0)
		{
			return  Vec.X != 0 || Vec.Y != 0;
		}
		return Vec.X < 0 || Vec.Y < 0 || Vec.Z < 0 || Vec.X>(4<<(Vec.Z-1))/2 || Vec.Y>(4 << (Vec.Z - 1))/2;
	});

	UE_LOG(LogTemp, Warning, TEXT("Coords lenght %d"), Coords.Num());
	for (auto& Element : Coords)
	{
		DrawDebugSphere(GetWorld(), FVector(Element.X*256.f,Element.Y*256.f,0), 10, 4, FColor::Red);
	}

	TilesToSections(Coords);

	TArray<FIntVector> TileKeys;
	for (auto& Element : TileContainer)
	{
		if((FDateTime::Now() - Element.Value->LastTimeAccess).GetSeconds()>5)
		{
			TileKeys.Add(Element.Key);
		}
	}
	for (auto& Element : TileKeys)
	{
		mesh->ClearMeshSection(TileContainer.FindAndRemoveChecked(Element)->SectionIndex);
	}
}

TArray<FIntVector> ATilesController3::line(int32 x1, int32 x2, int32 y)
{
	TArray<FIntVector> TempArr;
	for (int i = x1; i <= x2; ++i)
	{
		TempArr.Emplace( i,y ,5);
	}
	return TempArr;
}

void ATilesController3::GetCircleCoords(int32 cx, int32 cy, int32 radius, TArray<FIntVector>& CoordsOut)
{
	int x = radius, y = 0;
	int d = 3 - 2 * radius;

	while (y <= x)
	{
		CoordsOut.Append(line(cx - x, cx + x, cy - y));
		CoordsOut.Append(line(cx - x, cx + x, cy + y));
		CoordsOut.Append(line(cx - y, cx + y, cy + x));
		CoordsOut.Append(line(cx - y, cx + y, cy - x));

		if (d <= 0)
		{
			d += 4 * y + 6;
			++y;
		}
		else
		{
			d += 4 * (y - x) + 10;
			--x, ++y;
		}
	}
}

bool ATilesController3::IsPointInsidePolygon(TArray<FVector2D>& Points, float X, float Y)
{
	//TODO
	return true;
}

void ATilesController3::TilesToSections(TArray<FIntVector>& Tiles)
{
	TArray<int32> Triangles;
	TArray<FRuntimeMeshVertexSimple> _Vertices;
	//TArray<FVector2D> TilePoints;
	for (auto& Element : Tiles)
	{
		//TileToPoints(TilePoints,Element.X,Element.Y);
		if(TileContainer.Contains(Element) || DownloaderContainer.Contains(Element))
		{
			continue;
		}
		// First vertex
		_Vertices.Add(FRuntimeMeshVertexSimple(FVector(256.0f*Element.X, 256.0f*(Element.Y + 1), 0), FVector(0, 0, 1), FRuntimeMeshTangent(0, -1, 0), FColor::White, FVector2D(0, 0)));
		// Second vertex
		_Vertices.Add(FRuntimeMeshVertexSimple(FVector(256.0f*(Element.X + 1), 256.0f*(Element.Y + 1), 0), FVector(0, 0, 1), FRuntimeMeshTangent(0, -1, 0), FColor::White, FVector2D(0, 1)));
		// Third vertex
		_Vertices.Add(FRuntimeMeshVertexSimple(FVector(256.0f*(Element.X + 1), 256.0f*Element.Y, 0), FVector(0, 0, 1), FRuntimeMeshTangent(0, -1, 0), FColor::White, FVector2D(1, 1)));
		// Fourth vertex
		_Vertices.Add(FRuntimeMeshVertexSimple(FVector(256.0f*Element.X, 256.0f*Element.Y, 0), FVector(0, 0, 1), FRuntimeMeshTangent(0, -1, 0), FColor::White, FVector2D(1, 0)));

		Triangles.Add(0);
		Triangles.Add(1);
		Triangles.Add(2);
		Triangles.Add(0);
		Triangles.Add(2);
		Triangles.Add(3);

		mesh->CreateMeshSection(++SectionIndex, _Vertices, Triangles);
		UTextureDownloader3* LoaderPtr = NewObject<UTextureDownloader3>();
		UMaterialInstanceDynamic * matInstance = UMaterialInstanceDynamic::Create(TileMaterial, this);
		LoaderPtr->TileCoords = Element;
		LoaderPtr->material = matInstance;
		mesh->SetMaterial(SectionIndex, matInstance);

		
		auto url = FString::Format(*UrlString, { Element.Z, Element.X, Element.Y });
		LoaderPtr->StartDownloadingTile(LoaderPtr->TileCoords, url);
		DownloaderContainer.Add(LoaderPtr->TileCoords, LoaderPtr);
		_Vertices.Empty();
		Triangles.Empty();
		
		//mesh->CreateMeshSection(FCString::Atoi(*(FString::FromInt(Element.X) + FString::FromInt(Element.Y) + FString::FromInt(MaxLevel))), _Vertices, Triangles);
	}
}

bool ATilesController3::IsTileAccepted(TArray<FVector2D>& Tile, TArray<FVector2D>& Polygon)
{
	//TODO
	return true;
}


TMap<FIntVector, UTileInfo3*>* UTileInfo3::TileContainer = nullptr;
TMap<FIntVector, UTileInfo3*>* UTextureDownloader3::TileContainer = nullptr;
TMap<FIntVector, UTextureDownloader3*>* UTextureDownloader3::DownloaderContainer = nullptr;
URuntimeMeshComponent* ATilesController3::mesh = nullptr;

void UTextureDownloader3::StartDownloadingTile(FIntVector _TileCoords, FString url)
{
	UE_LOG(LogTemp, Warning, TEXT("start loading %d %d %d"), TileCoords.X, TileCoords.Y, TileCoords.Z);
	this->TileCoords = _TileCoords;
	loader = UAsyncTaskDownloadImage::DownloadImage(url);
	loader->OnSuccess.AddDynamic(this, &UTextureDownloader3::OnTextureLoaded);
	loader->OnFail.AddDynamic(this, &UTextureDownloader3::OnLoadFailed);
}

void UTextureDownloader3::OnTextureLoaded(UTexture2DDynamic* Texture)
{
	UE_LOG(LogTemp, Warning, TEXT("Loaded %d %d %d"), TileCoords.X, TileCoords.Y, TileCoords.Z);
	if (!Texture->IsValidLowLevel())
	{
		DownloaderContainer->Remove(TileCoords);
		UE_LOG(LogTemp, Warning, TEXT("Loaded texture is corrupt"));
		return;
	}
	if (!material->IsValidLowLevel())
	{
		DownloaderContainer->Remove(TileCoords);
		UE_LOG(LogTemp, Warning, TEXT("Texture loaded for already destroyed tile"));
		return;
	}
	DownloaderContainer->Remove(TileCoords);
	//UMaterial
	material->SetTextureParameterValue("Tile", (UTexture*)(Texture));

	UTileInfo3* TempTile = NewObject<UTileInfo3>();
	TempTile->TileCoords = TileCoords;
	TempTile->material = material;
	TempTile->texture = static_cast<UTexture*>(Texture);
	TempTile->SectionIndex = SectionIndex;
	TempTile->LastTimeAccess= FDateTime::Now();
	//TempTile->SetTimer();
	
	TileContainer->Add(TileCoords, TempTile);
}

void UTextureDownloader3::OnLoadFailed(UTexture2DDynamic* Texture)
{
	DownloaderContainer->Remove(TileCoords);
	UE_LOG(LogTemp, Warning, TEXT("Load failed %d %d %d"),TileCoords.X,TileCoords.Y,TileCoords.Z);
}
/*
void UTileInfo3::SetTimer()
{
	//if (GetWorld()->GetTimerManager().IsTimerActive(TimerHandle))
	//GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	FTimerHandle TimerHandle;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &UTileInfo3::OnDelete, 60.f, false);
	//GWorld->GetTimerManager().SetTimer(TimerHandle, this, &FTileInfo3::OnDelete,30.f);
}

void UTileInfo3::OnDelete()
{
	UE_LOG(LogTemp, Warning, TEXT("Deleted %d %d %d"), TileCoords.X, TileCoords.Y, TileCoords.Z);
	ATilesController3::mesh->ClearMeshSection(SectionIndex);
	TileContainer->Remove(TileCoords);
}
*/
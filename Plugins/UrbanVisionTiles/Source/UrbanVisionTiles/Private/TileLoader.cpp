// Fill out your copyright notice in the Description page of Project Settings.


#include "TileLoader.h"
#include "Engine/Engine.h"
#include "Engine/Texture2DDynamic.h"

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

	TArray<FTileTextureMeta> pendingDelete;
	TArray<FTileTextureMeta, FHeapAllocator> pendingUpdate;	
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
		FTileTextureMeta parentMeta = FTileTextureMeta{ meta.X / 2, meta.Y / 2, meta.Z - 1 };
		float parentTilePixelsSize = GetPixelSize(parentMeta);
		if (parentTilePixelsSize < 200.0f && meta.Z > BaseLevel)
		{

			auto x = meta.X - (meta.X % 2);
			auto y = meta.Y - (meta.Y % 2);

			bool flag = false;
			for (int i = 0; i < 4; i++)
			{
				auto meta1 = FTileTextureMeta{ x + i % 2, y + i / 2, meta.Z };
				if (!TileIndecies.Contains(meta1)) {										
					flag = true;
				}
			}
			if (!flag) {
				CreateTileMesh(parentMeta);
				SplitTiles.Remove(parentMeta);
				for (int i = 0; i < 4; i++)
				{
					auto meta1 = FTileTextureMeta{ x + i % 2, y + i / 2, meta.Z };
					ClearTileMesh(meta1);					
				}
			}

			
			
		}
		
	}
	for(auto meta : pendingDelete)
	{
		if (!TileIndecies.Contains(meta)) continue;		
			
	}
}

float ATilesController::GetPixelSize(FTileTextureMeta meta)
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
	if (!TileLoader) TileLoader = NewObject<UTileTextureContainer>(this);

	int z = BaseLevel;

	int x0 = GetMercatorXFromDegrees(CenterLon) * (1 << z);
	int y0 = GetMercatorYFromDegrees(CenterLat) * (1 << z);
	
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
	if (!TileLoader)	TileLoader = NewObject<UTileTextureContainer>(this);

	//mesh->ClearAllMeshSections();
	for (int x = 0; x < BaseLevelSize; x++)
	{
		for (int y = 0; y < BaseLevelSize; y++)
		{
			if (!SplitTiles.Contains(FTileTextureMeta{ x + x0 - BaseLevelSize / 2, y + y0 - BaseLevelSize / 2, z })) {
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
	dx = (int)FMath::RoundFromZero(fdx);
	dy = (int)FMath::RoundFromZero(fdy);
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
	auto meta = FTileTextureMeta{ x, y, z };
	return CreateTileMesh(meta);
}

int ATilesController::CreateTileMesh(FTileTextureMeta meta)
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

void ATilesController::ClearTileMesh(FTileTextureMeta meta)
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
		TileIndecies.Contains(FTileTextureMeta{x * 2, y * 2, z + 1}) &&
		TileIndecies.Contains(FTileTextureMeta{x * 2 + 1, y * 2, z + 1}) &&
		TileIndecies.Contains(FTileTextureMeta{x * 2, y * 2 + 1, z + 1}) &&
		TileIndecies.Contains(FTileTextureMeta{x * 2 + 1, y * 2 + 1, z + 1});
}

void ATilesController::SplitTile(FTileTextureMeta m)
{
	SplitTile(m.X, m.Y, m.Z);
}

void ATilesController::SplitTile(int x, int y, int z)
{
	auto parentMeta = FTileTextureMeta{ x, y, z };
	auto childMeta1 = FTileTextureMeta{ x * 2, y * 2, z + 1 };
	auto childMeta2 = FTileTextureMeta{ x * 2 + 1, y * 2, z + 1 };
	auto childMeta3 = FTileTextureMeta{ x * 2, y * 2 + 1, z + 1 };
	auto childMeta4 = FTileTextureMeta{ x * 2 + 1, y * 2 + 1, z + 1 };

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

void UTextureDownloader::StartDownloadingTile(FTileTextureMeta meta, FString url)
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
	auto meta = FTileTextureMeta{ x, y, z };
	return GetTileMaterial(meta, mat, owner);
}

UTileInfo* UTileTextureContainer::GetTileMaterial(FTileTextureMeta meta, UMaterialInterface* mat, AActor* owner)
{
	//UE_LOG(LogTemp, Warning, TEXT("Total cached textures: %i"), CachedTiles.Num());

	if (CachedTiles.Num() > 512)
	{
		TArray<FTileTextureMeta> pendingDelete;

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
		TileInfo->Container = this;
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


void UTileTextureContainer::CacheTexture(FTileTextureMeta meta, UTexture* texture)
{
	if (CachedTiles.Contains(meta)) {
		CachedTiles[meta]->Texture = texture;
	}
	
}

void UTileTextureContainer::FreeLoader(FTileTextureMeta meta)
{
	if (!loadingImages.Contains(meta))
	{
		UE_LOG(LogTemp, Warning, TEXT("WTF?"));
	}		
	//auto loader = loadingImages[meta];
	//loadingImages.Remove(meta);
	//unusedDownloaders.Add(loader);
}

bool UTileTextureContainer::IsTextureLoaded(FTileTextureMeta meta)
{
	return CachedTiles.Contains(meta) && CachedTiles[meta]->Texture && CachedTiles[meta]->Texture->IsValidLowLevel();
}

void UTileTextureContainer::Clear()
{
	CachedTiles.Empty();
}
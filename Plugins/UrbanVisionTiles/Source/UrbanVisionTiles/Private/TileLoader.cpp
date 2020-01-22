// Fill out your copyright notice in the Description page of Project Settings.


#include "TileLoader.h"
#include "Engine/Engine.h"
#include "Engine/Texture2DDynamic.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"


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

	//TArray<FTileTextureMeta> pendingDelete;
	//TArray<FTileTextureMeta, FHeapAllocator> pendingUpdate;	
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
	pendingDelete.Empty();
	pendingUpdate.Empty();
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

//int32 UTileTextureContainer2::SectionIndex = 0;

ATilesController2::ATilesController2()
{
	PrimaryActorTick.bCanEverTick = true;
	mesh = CreateDefaultSubobject<URuntimeMeshComponent>(TEXT("Tiles mesh"), true);
	
	
	//TileMaterial = NewObject<UMaterialInterface>();
	
	SetRootComponent(mesh);
}

void ATilesController2::BeginPlay()
{
	Super::BeginPlay();
	MapSize = MaxLevel ? 256 * 2 << MaxLevel : 256;
	/*
	TArray<int32> Triangles;
	TArray<FRuntimeMeshVertexSimple> _Vertices;

	// First vertex
	_Vertices.Add(FRuntimeMeshVertexSimple(FVector(0, 256, 0), FVector(0, 0, 1), FRuntimeMeshTangent(0, -1, 0), FColor::White, FVector2D(0, 0)));
	// Second vertex
	_Vertices.Add(FRuntimeMeshVertexSimple(FVector(256, 256, 0), FVector(0, 0, 1), FRuntimeMeshTangent(0, -1, 0), FColor::White, FVector2D(0, 1)));
	// Third vertex
	_Vertices.Add(FRuntimeMeshVertexSimple(FVector(256, 0, 0), FVector(0, 0, 1), FRuntimeMeshTangent(0, -1, 0), FColor::White, FVector2D(1, 1)));
	// Fourth vertex
	_Vertices.Add(FRuntimeMeshVertexSimple(FVector(0, 0, 0), FVector(0, 0, 1), FRuntimeMeshTangent(0, -1, 0), FColor::White, FVector2D(1, 0)));

	Triangles.Add(0);
	Triangles.Add(1);
	Triangles.Add(2);
	Triangles.Add(0);
	Triangles.Add(2);
	Triangles.Add(3);

	// Create the section bounding box
	//FBox SectionBoundingBox(FVector(0, 0, 0), FVector(256, 256, 0));

	mesh->CreateMeshSection(0, _Vertices, Triangles);
	*/
	PlayerController = GetWorld()->GetFirstPlayerController();
	//UE_LOG(LogTemp, Warning, TEXT("%s"), *PlayerController->GetName());
	CurrentLevel= (mesh->GetComponentLocation() - PlayerController->GetPawn()->GetActorLocation()).Size();

	UE_LOG(LogTemp, Warning, TEXT("%d"), PlayerController->PlayerCameraManager->GetFOVAngle());
}

void ATilesController2::Tick(float DeltaTime)
{
	FVector Location;
	FRotator Rotation;
	PlayerController->GetPlayerViewPoint(Location, Rotation);

	//UE_LOG(LogTemp, Warning, TEXT("%s"), *Location.ToString());
	//DrawDebugLine(GetWorld(), Location, Location + Rotation.Vector()*100.f, FColor::Red, false, -1.f, 0.f, 10.f);

	//FHitResult Hit;
	//GetWorld()->LineTraceSingleByChannel(Hit, Location, Location + Rotation.Vector()*1000000.f, ECC_Visibility);
	//DrawDebugSphere(GetWorld(), Hit.Location, 50.f, 8, FColor::Red);
	/*
	FHitResult LeftDown,LeftUp,RightUp,RightDown;
	
	FRotator RLeftDown= Rotation+FRotator(-45.f, -45.f, -45.f), RLeftUp= Rotation+FRotator(45.f, -45.f, -45.f), 
			 RRightUp= Rotation + FRotator(45.f, 45.f, 0), RRightDown= Rotation + FRotator(-45.f, 45.f, -45.f);

	GetWorld()->LineTraceSingleByChannel(LeftDown, Location, Location + RLeftDown.Vector()*1000000.f, ECC_Visibility);
	GetWorld()->LineTraceSingleByChannel(LeftUp, Location, Location + RLeftUp.Vector()*1000000.f, ECC_Visibility);
	GetWorld()->LineTraceSingleByChannel(RightUp, Location, Location + RRightUp.Vector()*1000000.f, ECC_Visibility);
	GetWorld()->LineTraceSingleByChannel(RightDown, Location, Location + RRightDown.Vector()*1000000.f, ECC_Visibility);

	//DrawDebugLine(GetWorld(), Location, Location + RLeftDown.Vector()*1000.f, FColor::Blue, false, -1.f, 0.f, 3.f);
	DrawDebugSphere(GetWorld(), LeftDown.Location, 100.f, 8, FColor::Blue);
	DrawDebugSphere(GetWorld(), LeftUp.Location, 100.f, 8, FColor::Blue);
	DrawDebugSphere(GetWorld(), RightUp.Location, 100.f, 8, FColor::Blue);
	DrawDebugSphere(GetWorld(), RightDown.Location, 100.f, 8, FColor::Blue);
	DrawDebugSphere(GetWorld(), (RightDown.Location+ LeftDown.Location+ LeftUp.Location+ RightUp.Location)/4, 100.f, 8, FColor::Red);
	
	
	TArray<FVector2D> Polygon = { {LeftDown.Location.X,LeftDown.Location.Y},
		{LeftUp.Location.X,LeftUp.Location.Y},
		{RightUp.Location.X,RightUp.Location.Y},
		{RightDown.Location.X,RightDown.Location.Y} };

	//�������� ��� ����, ����� ������ ����� ����� �������������
	int32 BeginX=int(((RightDown.Location + LeftDown.Location + LeftUp.Location + RightUp.Location)/4).X)>>8, BeginY= int(((RightDown.Location + LeftDown.Location + LeftUp.Location + RightUp.Location) / 4).Y) >> 8;
	//TODO �������� � ��������� � ������ �����
	//��������� ����� ������
	//TArray<FVector2D> CheckPoints={{256*BeginX,256*BeginY},{256 * BeginX,256 * (BeginY+1)}, {256 * (BeginX+1),256 * (BeginY+1)}, {256 * (BeginX+1),256 * BeginY}};
	TArray<FIntPoint> Tiles = { {BeginX, BeginY} };
	//TArray<FIntPoint> TilesToCheck={{BeginX, BeginY}};

	TArray<FVector2D> CheckPoints;

	//����� ��������� ����
	//��� ������ �� �����, ����� �����
	//��� �����, ���� ���� ���� �� ���� ������� ����� ��������� ������� �������
	//���� ������� ���� �������, ������ ���� ����� � ����� ������ ������

	*/

	
	FIntPoint BeginTile = { FMath::Abs<int32>(Location.X/256.0),FMath::Abs<int32>(Location.Y / 256.0)};
	TSet<FIntPoint> Tiles = { BeginTile, {FMath::Abs<int32>(BeginTile.X-1),FMath::Abs<int32>(BeginTile.Y-1)}, 
		{FMath::Abs<int32>(BeginTile.X+1),FMath::Abs<int32>(BeginTile.Y - 1)},
		{FMath::Abs<int32>(BeginTile.X - 1),FMath::Abs<int32>(BeginTile.Y + 1)},
		{FMath::Abs<int32>(BeginTile.X + 1),FMath::Abs<int32>(BeginTile.Y + 1)}};




	for (auto elem : Tiles)
	{
		UE_LOG(LogTemp, Warning, TEXT("Tile : %d %d"), elem.X,elem.Y);
	}
	TilesToSections(Tiles);
	////////////////////////////////////////////////////////////
	//UE_LOG(LogTemp, Warning, TEXT("Tick"));
	/*
	auto Distance = (mesh->GetComponentLocation() - PlayerController->GetPawn()->GetActorLocation()).Size();
	int level = Distance / 200.f;
	if(level==CurrentLevel)
		return;
	CurrentLevel = level;
	
	int z = FMath::Clamp(MaxLevel - CurrentLevel,0,MaxLevel);
	int y; int x = y = 0;
	LoaderPtr = NewObject<UTextureDownloader2>();
	LoaderPtr->TextureMeta = { 0,0,z };

	UMaterialInstanceDynamic* matInstance = UMaterialInstanceDynamic::Create(TileMaterial,this);
	LoaderPtr->material = matInstance;
	mesh->SetMaterial(0, matInstance);
	//loadingImages.Add(LoaderPtr->TextureMeta, LoaderPtr);
	UE_LOG(LogTemp, Warning, TEXT("%s"), *LoaderPtr->TextureMeta.ToString());
	auto url = FString::Format(*UrlString, { z, 0, 0});
	UE_LOG(LogTemp, Warning, TEXT("%s"), *url);
	LoaderPtr->StartDownloadingTile(LoaderPtr->TextureMeta, url);
	*/
}


bool ATilesController2::IsPointInsidePolygon(TArray<FVector2D>& Points, float X, float Y)
{
	int i1, i2, n, N, S, S1, S2, S3;
	bool flag=false;
	N = Points.Num();
	for (n = 0; n < N; n++)
	{
		flag = false;
		i1 = n < N - 1 ? n + 1 : 0;
		while (!flag)
		{
			i2 = i1 + 1;
			if (i2 >= N)
				i2 = 0;
			if (i2 == (n < N - 1 ? n + 1 : 0))
				break;
			S = abs(Points[i1].X * (Points[i2].Y - Points[n].Y) +Points[i2].X * (Points[n].Y - Points[i1].Y) +Points[n].X  * (Points[i1].Y - Points[i2].Y));
			S1 = abs(Points[i1].X * (Points[i2].Y - Y) +	Points[i2].X * (Y - Points[i1].Y) +	X * (Points[i1].Y - Points[i2].Y));
			S2 = abs(Points[n].X * (Points[i2].Y - Y) + Points[i2].X * (Y - Points[n].Y) +	X * (Points[n].Y - Points[i2].Y));
			S3 = abs(Points[i1].X * (Points[n].Y - Y) + Points[n].X * (Y - Points[i1].Y) +	X * (Points[i1].Y - Points[n].Y));
			
			if (S == S1 + S2 + S3)
			{
				flag = true;
				break;
			}
			i1 = i1 + 1;
			if (i1 >= N)
			{
				i1 = 0;
			}
		}
		if (!flag)
			break;
	}
	return flag;
}

void ATilesController2::TilesToSections(TSet<FIntPoint>& Tiles)
{
	TArray<int32> Triangles;
	TArray<FRuntimeMeshVertexSimple> _Vertices;
	TArray<FVector2D> TilePoints;
	for (auto& Element : Tiles)
	{
		//TileToPoints(TilePoints,Element.X,Element.Y);

		// First vertex
		_Vertices.Add(FRuntimeMeshVertexSimple(FVector(256.0f*Element.X, 256.0f*(Element.Y+1), 0), FVector(0, 0, 1), FRuntimeMeshTangent(0, -1, 0), FColor::White, FVector2D(0, 0)));
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
		UTextureDownloader2* LoaderPtr=NewObject<UTextureDownloader2>();
		UMaterialInstanceDynamic * matInstance = UMaterialInstanceDynamic::Create(TileMaterial, this);
		LoaderPtr->material = matInstance;
		mesh->SetMaterial(SectionIndex, matInstance);
		
		loadingImages.Add({ Element.X,Element.Y,MaxLevel }, LoaderPtr);
		auto url = FString::Format(*UrlString, { MaxLevel, Element.X, Element.Y });
		LoaderPtr->StartDownloadingTile(LoaderPtr->TextureMeta, url);

		_Vertices.Empty();
		Triangles.Empty();
		//mesh->CreateMeshSection(FCString::Atoi(*(FString::FromInt(Element.X) + FString::FromInt(Element.Y) + FString::FromInt(MaxLevel))), _Vertices, Triangles);
	}
}

bool ATilesController2::IsTileAccepted(TArray<FVector2D> & Tile, TArray<FVector2D>& Polygon)
{
	bool AllPointsNotInViewArea = false;
	for (auto point : Tile)
	{
		if(IsPointInsidePolygon(Polygon,point.X,point.Y))
		{
			AllPointsNotInViewArea = true;
			break;
		}
	}
	return AllPointsNotInViewArea;
}

void UTextureDownloader2::StartDownloadingTile(FTileTextureMeta meta,  FString url)
{
	//TextureMeta = meta;
	//__Texture = _Texture;
	UE_LOG(LogTemp, Warning, TEXT("In StartDownloadingTile"));
	_loader = UAsyncTaskDownloadImage::DownloadImage(url);
	_loader->OnSuccess.AddDynamic(this, &UTextureDownloader2::OnTextureLoaded);
	_loader->OnFail.AddDynamic(this, &UTextureDownloader2::OnLoadFailed);
}

void UTextureDownloader2::OnTextureLoaded(UTexture2DDynamic* Texture)
{
	//material=mesh->CreateAndSetMaterialInstanceDynamic(0);
	material->SetTextureParameterValue(FName("Tile"), (UTexture*)(Texture));	
	UE_LOG(LogTemp, Warning, TEXT("%s"), *TextureMeta.ToString());
	
}

void UTextureDownloader2::OnLoadFailed(UTexture2DDynamic * Texture)
{
	UE_LOG(LogTemp, Warning, TEXT("Load failed"));
}

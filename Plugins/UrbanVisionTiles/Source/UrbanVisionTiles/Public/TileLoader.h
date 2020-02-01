// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TextureResource.h"
#include "ProceduralMeshComponent.h"
#include "AsyncTaskDownloadImage.h"
#include "GameFramework/PlayerController.h"
#include "RuntimeMeshComponent.h"
#include "TileLoader.generated.h"

//class UTileTextureContainer2;
class UTileTextureContainer;

class UTileInfo;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class URBANVISIONTILES_API ATilesController : public AActor
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ATilesController();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
		TArray<FIntVector> pendingDelete;
		TArray<FIntVector, FHeapAllocator> pendingUpdate;
public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	float GetPixelSize(FIntVector meta);

	UFUNCTION(BlueprintCallable, Category = "Tiles", meta = (CallInEditor = "true"))
	void CreateMesh();

	UFUNCTION(BlueprintCallable, Category = "Tiles", meta = (CallInEditor = "true"))
	void ClearMesh();

	UFUNCTION(BlueprintCallable, Category = "Tiles")
		void CreateMeshAroundPoint(int z, int x0, int y0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
		UMaterialInterface* TileMaterial;	
	
	URuntimeMeshComponent* mesh;
	UTileTextureContainer* TileLoader;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
	float CenterLon;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
	float CenterLat;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
	TMap<FIntVector, UTileInfo*> Tiles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
	APlayerController* PlayerController;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
	FVector Offset;

	UPROPERTY()
	TArray<int> freeIndices;
	UPROPERTY()
	TMap<FIntVector, int> TileIndecies;
	

	UPROPERTY()
	TSet<FIntVector> SplitTiles;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
	int BaseLevel = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
	int BaseLevelSize = 8;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
		int MaxLevel = 18;

	UFUNCTION(BlueprintCallable, Category = "Math")
	void GetMercatorXYFromOffset(FVector offsetValue, int z, int& x, int& y);

	UFUNCTION(BlueprintCallable, Category = "Math")
	void GetMercatorXYOffsetFromOffset(FVector offsetValue, int z, int& x, int& y);

	UFUNCTION(BlueprintCallable, Category = "Math")
	FVector GetXYOffsetFromMercatorOffset(int z, int x, int y);

private:
	static float GetMercatorXFromDegrees(double lon)
	{
		return ((lon / 180 * PI) + PI) / 2 / PI;
	}	

	static float GetMercatorYFromDegrees(double lat)
	{
		return (PI - FMath::Loge(FMath::Tan(PI / 4 + lat * PI / 180 / 2))) / 2 / PI;
	}

	
	static double EarthRadius;// = 6378.137;
	static double EarthOneDegreeLengthOnEquatorMeters;// = 111152.8928;

	int CreateTileMesh(int x, int y, int z);

	int CreateTileMesh(FIntVector meta);

	void ClearTileMesh(FIntVector meta);

	bool IsTileSplit(int x, int y, int z);

	void SplitTile(FIntVector meta);
	
	void SplitTile(int x, int y, int z);
};


UCLASS()
class UTextureDownloader : public UObject
{
	GENERATED_BODY()
public:
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	FIntVector TextureMeta;
	UAsyncTaskDownloadImage* _loader;

	UFUNCTION(BlueprintCallable, Category = "Default")
	void StartDownloadingTile(FIntVector meta, FString url);

	UTileTextureContainer* TileContainer;
	UMaterialInstanceDynamic* material;

	UFUNCTION(BlueprintCallable, Category = "Default")
	void OnTextureLoaded(UTexture2DDynamic* Texture);

	UFUNCTION(BlueprintCallable, Category = "Default")
	void OnLoadFailed(UTexture2DDynamic* Texture);
};

UCLASS()
class UTileInfo : public UObject
{
	GENERATED_BODY()
public:
	//UPROPERTY()
	//UTileTextureContainer* Container;

	UPROPERTY()
		bool IsActive;

	UPROPERTY()
		bool IsLoaded;
	
	UPROPERTY()
	FDateTime lastAcessTime;

	UPROPERTY()
	UTexture* Texture;

	UPROPERTY()
	UMaterialInstanceDynamic* Material;
};

UCLASS()
class UTileTextureContainer : public UObject
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FIntVector, UTextureDownloader*> loadingImages;
public:
	UPROPERTY()
		TMap< FIntVector, UTileInfo*> CachedTiles;	
	
	UPROPERTY()
	FString UrlString = TEXT("http://a.tile.openstreetmap.org/{0}/{1}/{2}.png");

	//UFUNCTION(BlueprintCallable, Category = "Default")
	UTileInfo* GetTileMaterial(int x, int y, int z, UMaterialInterface* mat, AActor* owner);

	UFUNCTION(BlueprintCallable, Category = "Default")
	UTileInfo* GetTileMaterial(FIntVector meta, UMaterialInterface* mat, AActor* owner);

	UFUNCTION(BlueprintCallable, Category = "Default")
	void CacheTexture(FIntVector meta, UTexture* texture);

	UFUNCTION(BlueprintCallable, Category = "Default")
	void FreeLoader(FIntVector meta);

	UFUNCTION(BlueprintCallable, Category = "Default")
	bool IsTextureLoaded(FIntVector meta);

	UFUNCTION(BlueprintCallable, Category = "Default")
	void Clear();
private:
};


///////////////////////////////////////////////////////
///
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class URBANVISIONTILES_API ATilesController3 : public AActor
{
	GENERATED_BODY()
public:
	// Sets default values for this component's properties
	ATilesController3();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
		UMaterialInterface* TileMaterial;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
		int32 MaxLevel = 10;
	UPROPERTY()
		FString UrlString = TEXT("http://a.tile.openstreetmap.org/{0}/{1}/{2}.png");

	//static ATilesController3* TileControllerInstance;
	static URuntimeMeshComponent* mesh;

	TMap<FIntVector, class UTileInfo3*> TileContainer;
	TMap<FIntVector, class UTextureDownloader3*> DownloaderContainer;

	int32 SectionIndex = 0;

	int32 MapSize;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Tiles")
		APlayerController* PlayerController;
protected:
	FORCEINLINE TArray<FIntVector> line( int32 x1, int32 x2, int32 y);
	void GetCircleCoords(int32 cx, int32 cy, int32 radius, TArray<FIntVector>& CoordsOut);
private:
	static void TileToPoints(TArray<FVector2D>& Points, int32 X, int32 Y)
	{
		Points.Empty();
		Points = { {256.0f * X,256.0f * (Y + 1)}, {256.0f * (X + 1),256.0f * (Y + 1)}, {256.0f * (X + 1),256.0f * Y},{256.0f * X,256.0f * Y} };
	}
	static bool IsPointInsidePolygon(TArray<FVector2D>& Points, float X, float Y);
	void TilesToSections(TArray<FIntVector>& Tiles);
	bool IsTileAccepted(TArray<FVector2D> & Tile, TArray<FVector2D>& Polygon);
};
UCLASS()
class UTextureDownloader3 : public UObject
{
	GENERATED_BODY()
public:
	static TMap<FIntVector, class UTileInfo3*>*			 TileContainer;
	static TMap<FIntVector, UTextureDownloader3*>*		 DownloaderContainer;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
		FIntVector TileCoords;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
		int32 SectionIndex;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Default")
		UAsyncTaskDownloadImage* loader;
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Default")
		UMaterialInstanceDynamic* material;

	UFUNCTION(BlueprintCallable, Category = "Default")
		void StartDownloadingTile(FIntVector _TileCoords, FString url);

	UFUNCTION(BlueprintCallable, Category = "Default")
		void OnTextureLoaded(UTexture2DDynamic* Texture);

	UFUNCTION(BlueprintCallable, Category = "Default")
		void OnLoadFailed(UTexture2DDynamic* Texture);
};
UCLASS()
class UTileInfo3 : public UObject
{
	GENERATED_BODY()
public:
	static TMap<FIntVector, UTileInfo3*>*  TileContainer;
	FIntVector TileCoords;
	int32 SectionIndex;
	UMaterialInstanceDynamic* material;
	UTexture* texture;
	FDateTime LastTimeAccess;
};

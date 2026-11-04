#include "GbGreyboxMaterials.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

namespace GbGreybox
{
	void SetupMesh(UInstancedStaticMeshComponent* Mesh, EShape Shape)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

		if (!Mesh) return;
		UStaticMesh* Chosen = nullptr;
		switch (Shape)
		{
		case EShape::Cube: Chosen = Cube.Object; break;
		case EShape::Cone: Chosen = Cone.Object; break;
		case EShape::Cylinder: Chosen = Cylinder.Object; break;
		case EShape::Sphere: Chosen = Sphere.Object; break;
		}
		if (Chosen) Mesh->SetStaticMesh(Chosen);
		if (Material.Object) Mesh->SetMaterial(0, Material.Object);
	}

	void Paint(UInstancedStaticMeshComponent* Mesh, const FLinearColor& Color, UObject* Outer)
	{
		if (!Mesh) return;
		UMaterialInterface* Base = Mesh->GetMaterial(0);
		// Re-painting: tint the parent, not a dynamic instance of a dynamic instance.
		if (UMaterialInstanceDynamic* Existing = Cast<UMaterialInstanceDynamic>(Base))
		{
			Existing->SetVectorParameterValue(TEXT("Color"), Color);
			return;
		}
		if (!Base) return;
		UMaterialInstanceDynamic* Tinted = UMaterialInstanceDynamic::Create(Base, Outer ? Outer : Mesh);
		Tinted->SetVectorParameterValue(TEXT("Color"), Color);
		Mesh->SetMaterial(0, Tinted);
	}

	void FillGrassBlades(UInstancedStaticMeshComponent* Mesh, FIntPoint SizeMetres, int32 PerMetre, int32 Seed)
	{
		if (!Mesh) return;
		Mesh->ClearInstances();
		FRandomStream Rand(Seed);
		const float HalfX = SizeMetres.X * 50.f;
		const float HalfY = SizeMetres.Y * 50.f;
		const int32 Count = SizeMetres.X * SizeMetres.Y * PerMetre;
		for (int32 i = 0; i < Count; ++i)
		{
			const float Height = Rand.FRandRange(0.35f, 0.6f);
			const float Width = Rand.FRandRange(0.06f, 0.1f);
			const FVector Location(Rand.FRandRange(-HalfX, HalfX), Rand.FRandRange(-HalfY, HalfY), Height * 50.f);
			const FRotator Lean(Rand.FRandRange(-12.f, 12.f), Rand.FRandRange(0.f, 360.f), Rand.FRandRange(-12.f, 12.f));
			Mesh->AddInstance(FTransform(Lean, Location, FVector(Width, Width, Height)));
		}
	}
}
